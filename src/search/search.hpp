// The search schemes: sequential, tree-parallel, root-parallel and PIMC.
#pragma once
#include <vector>

#include "search/tree.hpp"
#include "util/parallel.hpp"
#include "util/timer.hpp"

namespace pm {

inline uint64_t share(uint64_t total, int parts, int i) {
    return total / parts + (static_cast<uint64_t>(i) < total % parts ? 1 : 0);
}

// A search stops after a fixed number of iterations or, if seconds > 0, at a deadline.
struct Budget {
    uint64_t iterations = 0;
    double seconds = 0;
    Budget(uint64_t n) : iterations(n) {}
    static Budget time(double s) {
        Budget b(0);
        b.seconds = s;
        return b;
    }
};

// Run one worker's part; returns the number of iterations done.
template <class Step>
uint64_t run_part(const Budget& b, uint64_t iterations, double seconds, Step&& step) {
    if (b.seconds <= 0) {
        for (uint64_t i = 0; i < iterations; ++i) step();
        return iterations;
    }
    const double end = now_s() + seconds;
    uint64_t n = 0;
    do {
        for (int i = 0; i < 16; ++i) step();
        n += 16;
    } while (now_s() < end);
    return n;
}

// Sequential ISMCTS. With virtual loss it performs exactly the arithmetic of the
// one-thread tree-parallel search; without it, it is the true sequential baseline.
template <class G, bool VirtualLoss = false>
RootStats serial_search(Tree<G, PlainF64>& tree, const G& s, int me, Budget budget,
                        uint64_t seed) {
    tree.reset();
    typename Tree<G, PlainF64>::Worker w(seed, 0);
    const double t0 = now_s();
    const uint64_t n = run_part(budget, budget.iterations, budget.seconds, [&] {
        tree.template iterate<true, VirtualLoss>(s, me, w);
    });
    const double t1 = now_s();
    RootStats r = tree.root_stats();
    r.counters = w.c;
    r.iterations = n;
    r.wall_s = t1 - t0;
    return r;
}

// Tree parallelization: p threads share one tree stored in format P.
template <class G, class P>
RootStats tree_search(Tree<G, P>& tree, const G& s, int me, Budget budget, int p,
                      uint64_t seed) {
    tree.reset();
    std::vector<Counters> counters(p);
    std::vector<uint64_t> done(p);
    const double t0 = now_s();
    run_team(p, [&](int tid) {
        typename Tree<G, P>::Worker w(seed, tid);
        done[tid] = run_part(budget, share(budget.iterations, p, tid), budget.seconds,
                             [&] { tree.template iterate<true, true>(s, me, w); });
        counters[tid] = w.c;
    });
    const double t1 = now_s();
    RootStats r = tree.root_stats();
    for (auto& c : counters) r.counters += c;
    for (auto n : done) r.iterations += n;
    r.wall_s = t1 - t0;
    return r;
}

// Trees owned by individual threads; reused between searches. Each tree is
// constructed by the thread that uses it.
template <class G>
class TreeSet {
public:
    TreeSet(const SearchConfig& cfg, int p) : trees_(p) {
        run_team(p, [&](int t) { trees_[t] = std::make_unique<Tree<G, PlainF64>>(cfg); });
    }
    Tree<G, PlainF64>& operator[](int t) { return *trees_[t]; }

private:
    std::vector<std::unique_ptr<Tree<G, PlainF64>>> trees_;
};

// Root parallelization: independent trees, root statistics summed at the end.
template <class G>
RootStats root_parallel_search(TreeSet<G>& trees, const G& s, int me, Budget budget, int p,
                               uint64_t seed) {
    std::vector<RootStats> part(p);
    const double t0 = now_s();
    run_team(p, [&](int tid) {
        Tree<G, PlainF64>& tree = trees[tid];
        tree.reset();
        typename Tree<G, PlainF64>::Worker w(seed, tid);
        const uint64_t n = run_part(budget, share(budget.iterations, p, tid), budget.seconds,
                                    [&] { tree.template iterate<true, false>(s, me, w); });
        part[tid] = tree.root_stats();
        part[tid].iterations = n;
        part[tid].counters = w.c;
    });
    RootStats r;
    for (auto& x : part) r.merge(x);
    r.wall_s = now_s() - t0;
    return r;
}

// PIMC: K sampled worlds, each searched with perfect-information UCT. World k
// uses RNG stream k, so the result does not depend on the number of threads.
template <class G>
RootStats pimc_search(TreeSet<G>& trees, const G& s, int me, Budget budget, int worlds, int p,
                      uint64_t seed) {
    std::vector<RootStats> part(worlds);
    auto one_world = [&](int k, Tree<G, PlainF64>& tree) {
        tree.reset();
        typename Tree<G, PlainF64>::Worker w(seed, k);
        G world = s;
        world.determinize(me, w.rng);
        // Under a deadline each world gets an equal slice of the p * T thread-seconds.
        const uint64_t n = run_part(budget, share(budget.iterations, worlds, k),
                                    budget.seconds * p / worlds,
                                    [&] { tree.template iterate<false, false>(world, me, w); });
        part[k] = tree.root_stats();
        part[k].iterations = n;
        part[k].counters = w.c;
    };
    const double t0 = now_s();
#ifdef PM_STD_THREAD
    std::atomic<int> next{0};
    run_team(p, [&](int tid) {
        for (int k; (k = next.fetch_add(1)) < worlds;) one_world(k, trees[tid]);
    });
#else
#pragma omp parallel for schedule(dynamic) num_threads(p)
    for (int k = 0; k < worlds; ++k) one_world(k, trees[omp_get_thread_num()]);
#endif
    RootStats r;
    for (auto& x : part) r.merge(x);
    r.wall_s = now_s() - t0;
    return r;
}

}  // namespace pm
