// Open-loop search tree and one ISMCTS iteration. A node stands for a public
// action sequence; the same code runs sequentially (PlainF64) or shared by a
// team (atomic policies). Rewards are stored from the searching player's view.
#pragma once
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <type_traits>

#include "stats/policies.hpp"
#include "util/counters.hpp"
#include "util/rng.hpp"

namespace pm {

constexpr int kActions = 3;

struct SearchConfig {
    double ucb_c = 0.25;          // exploration constant, in normalised-reward units
    double virtual_loss = 1.0;    // pessimistic reward charged while a visit is in flight
    uint32_t pool_nodes = 1u << 16;
    int playouts = 1;             // rollouts per expanded leaf; 1 in every main experiment
};

struct RootStats {
    uint64_t visits[kActions] = {0, 0, 0};
    double sum[kActions] = {0, 0, 0};
    Counters counters;
    double wall_s = 0;
    size_t bytes_per_node = 0;

    double mean(int a) const { return visits[a] ? sum[a] / static_cast<double>(visits[a]) : 0.0; }
    double value() const {
        double s = 0, n = 0;
        for (int a = 0; a < kActions; ++a) s += sum[a], n += static_cast<double>(visits[a]);
        return n > 0 ? s / n : 0.0;
    }
    int best() const {  // most visited; ties broken by the higher mean
        int b = -1;
        for (int a = 0; a < kActions; ++a) {
            if (!visits[a]) continue;
            if (b < 0 || visits[a] > visits[b] || (visits[a] == visits[b] && mean(a) > mean(b)))
                b = a;
        }
        return b;
    }
    void merge(const RootStats& o) {
        for (int a = 0; a < kActions; ++a) visits[a] += o.visits[a], sum[a] += o.sum[a];
        counters += o.counters;
        bytes_per_node = o.bytes_per_node;
    }
};

// PM_NODE_ALIGN=64 is a diagnostic build that gives every node its own cache line.
#ifndef PM_NODE_ALIGN
#define PM_NODE_ALIGN 8
#endif
template <class P>
struct alignas(PM_NODE_ALIGN) Node {
    typename P::template Counter<uint64_t> visits;
    typename P::template Counter<uint32_t> child[kActions];  // pool index, 0 = absent
    [[no_unique_address]] typename P::Aux aux;
    typename P::Cell sum;
};

template <class G, class P>
class Tree {
public:
    using N = Node<P>;
    static constexpr uint32_t kRoot = 1;
    static constexpr uint32_t kBlock = 64;  // nodes handed to a thread at a time

    explicit Tree(const SearchConfig& cfg)
        : cfg_(cfg), pool_(static_cast<N*>(zeroed(cfg.pool_nodes * sizeof(N)))) {
        if (!pool_) std::abort();
        reset();
    }
    void reset() {
        // Children of fresh nodes are set at allocation, so only the root needs it.
        next_.store(kBlock, kRelaxed);
        init(pool_.get()[kRoot], 0.0, 0);
    }

    // Per-thread state: RNG, counters and the thread's slice of the node pool.
    struct Worker {
        Rng rng;
        Counters c;
        uint32_t cur = 0, end = 0;
        Worker(uint64_t seed, int tid) : rng(seed, static_cast<uint64_t>(tid)) {}
    };

    // Determinize = false searches the given (fully known) state: the PIMC world.
    template <bool Determinize, bool VirtualLoss>
    void iterate(const G& root, int me, Worker& w) {
        G s = root;
        if constexpr (Determinize) s.determinize(me, w.rng);
        N* path[G::kMaxDepth];
        double loss[G::kMaxDepth];
        int depth = 0;
        N* node = &at(kRoot);

        while (!s.terminal()) {
            const int mover = s.player();
            const double vl = VirtualLoss ? (mover == me ? -cfg_.virtual_loss : cfg_.virtual_loss) : 0.0;
            const unsigned legal = s.legal_mask();
            uint32_t kid[kActions];
            unsigned missing = 0;
            for (int a = 0; a < kActions; ++a) {
                kid[a] = (legal >> a & 1) ? node->child[a].load(std::memory_order_acquire) : 0;
                if ((legal >> a & 1) && !kid[a]) missing |= 1u << a;
            }
            const bool expand = missing != 0;
            const int a = expand ? nth_bit(missing, w.rng.below(__builtin_popcount(missing)))
                                 : select(kid, mover == me);
            N* ch;
            bool created = false;
            if (expand) {
                ch = install(*node, a, vl, w, created);
            } else {
                ch = &at(kid[a]);
            }
            if (!created) {
                ch->visits.fetch_add(1, kRelaxed);
                if constexpr (VirtualLoss) ch->sum.add(vl, ch->aux, w.c);
            }
            path[depth] = ch;
            loss[depth++] = vl;
            s.apply(a);
            node = ch;
            if (expand) break;
        }

        double r = 0;
        for (int k = 0; k < cfg_.playouts; ++k) {
            G t = s;
            while (!t.terminal()) {  // uniform random rollout
                const unsigned m = t.legal_mask();
                t.apply(nth_bit(m, w.rng.below(__builtin_popcount(m))));
            }
            r += t.payoff(me);
        }
        if (cfg_.playouts > 1) r /= cfg_.playouts;
        for (int i = depth; i-- > 0;) path[i]->sum.add(r - loss[i], path[i]->aux, w.c);
    }

    RootStats root_stats() const {
        RootStats out;
        const N& root = at(kRoot);
        for (int a = 0; a < kActions; ++a) {
            const uint32_t k = root.child[a].load(std::memory_order_acquire);
            if (!k) continue;
            out.visits[a] = at(k).visits.load(kRelaxed);
            out.sum[a] = at(k).sum.get();
        }
        out.bytes_per_node = sizeof(N);
        return out;
    }
    const SearchConfig& config() const { return cfg_; }
    const N* root_child(int a) const {  // nullptr if absent; for tests
        const uint32_t k = at(kRoot).child[a].load(std::memory_order_acquire);
        return k ? &at(k) : nullptr;
    }

private:
    static void* zeroed(size_t bytes) {
        void* m = std::aligned_alloc(64, (bytes + 63) & ~size_t{63});
        if (m) std::memset(m, 0, bytes);
        return m;
    }
    struct Free {
        void operator()(N* p) const { std::free(p); }
    };

    N& at(uint32_t i) { return pool_.get()[i]; }
    const N& at(uint32_t i) const { return pool_.get()[i]; }

    static int nth_bit(unsigned m, uint32_t k) {
        while (k--) m &= m - 1;
        return __builtin_ctz(m);
    }
    static void init(N& n, double sum, uint64_t visits) {
        n.visits.store(visits, kRelaxed);
        for (auto& c : n.child) c.store(0, kRelaxed);
        if constexpr (std::is_same_v<P, F64Lock>) n.aux.held.store(0, kRelaxed);
        n.sum.set(sum);
    }

    // UCB1 over fully expanded children. The parent count is the sum of the child
    // counts just read, which avoids a shared counter at every node on the path.
    int select(const uint32_t* kid, bool maximise) const {
        double n[kActions] = {0, 0, 0}, total = 0;
        for (int a = 0; a < kActions; ++a)
            if (kid[a]) total += n[a] = static_cast<double>(at(kid[a]).visits.load(kRelaxed));
        const double lg = std::log(total);
        int best = -1;
        double bv = 0;
        for (int a = 0; a < kActions; ++a) {
            if (!kid[a]) continue;
            double q = at(kid[a]).sum.get() / n[a];
            if (!maximise) q = -q;
            const double u = q + cfg_.ucb_c * std::sqrt(lg / n[a]);
            if (best < 0 || u > bv) best = a, bv = u;
        }
        return best;
    }

    // Add child a with CAS. On failure use the winner's node and recycle ours.
    N* install(N& parent, int a, double vl, Worker& w, bool& created) {
        if (w.cur == w.end) {
            w.cur = next_.fetch_add(kBlock, kRelaxed);
            w.end = w.cur + kBlock;
            if (w.end > cfg_.pool_nodes) {
                std::fprintf(stderr, "node pool exhausted\n");
                std::abort();
            }
        }
        const uint32_t idx = w.cur;
        N& fresh = at(idx);
        init(fresh, vl, 1);  // born with its first visit (and virtual loss) applied
        uint32_t expect = 0;
        if (parent.child[a].compare_exchange_strong(expect, idx, std::memory_order_acq_rel,
                                                    std::memory_order_acquire)) {
            ++w.cur;
            ++w.c.nodes;
            created = true;
            return &fresh;
        }
        ++w.c.lost_expansions;
        return &at(expect);
    }

    SearchConfig cfg_;
    std::unique_ptr<N[], Free> pool_;
    std::atomic<uint32_t> next_{kBlock};
};

}  // namespace pm
