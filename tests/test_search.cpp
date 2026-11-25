// CLAUDE.md section 6, items 3-5.
#include <cmath>
#include <utility>

#include "check.hpp"
#include "game/hulh.hpp"
#include "game/leduc.hpp"
#include "search/agents.hpp"
#include "search/search.hpp"

using namespace pm;

static Hulh some_state(uint64_t seed, int street) {
    Rng rng(seed, 5);
    for (;;) {
        Hulh s = Hulh::deal(rng);
        while (!s.terminal() && s.street < street) {
            const unsigned m = s.legal_mask();
            int a;
            do a = 1 + rng.below(2);
            while (!(m >> a & 1));
            s.apply(a);
        }
        if (!s.terminal()) return s;
    }
}

template <class P>
static RootStats tree_run(const Hulh& s, uint64_t n, int p, uint64_t seed) {
    Tree<Hulh, P> t{SearchConfig{}};
    return tree_search<Hulh, P>(t, s, s.player(), n, p, seed);
}

static void same(const RootStats& a, const RootStats& b) {
    for (int k = 0; k < kActions; ++k) {
        CHECK(a.visits[k] == b.visits[k]);
        CHECK(a.sum[k] == b.sum[k]);  // bitwise
    }
    CHECK(a.counters.nodes == b.counters.nodes);
}

// Item 4: one-thread tree-parallel equals sequential, bit for bit.
static void test_serial_equivalence() {
    Tree<Hulh, PlainF64> st{SearchConfig{}};
    for (int street = 0; street < 4; ++street)
        for (uint64_t seed = 1; seed <= 5; ++seed) {
            const Hulh s = some_state(seed, street);
            const auto a = serial_search<Hulh, true>(st, s, s.player(), 20000, seed);
            same(a, tree_run<F64Cas>(s, 20000, 1, seed));
            same(a, tree_run<F64Lock>(s, 20000, 1, seed));
            uint64_t n = 0;
            for (auto v : a.visits) n += v;
            CHECK(n == 20000);
        }
}

// F64 decides every selection; format Q accumulates the same rewards alongside.
// Any difference is therefore accumulation error alone.
template <class Q>
struct Shadow {
    template <class T> using Counter = Plain<T>;
    using Aux = NoAux;
    struct Cell {
        double v;
        typename Q::Cell q;
        typename Q::Aux qaux;
        void set(double r) { v = r, q.set(r); }
        double get() const { return v; }
        void add(double r, Aux&, Counters& c) {
            ++c.updates;
            v += r;
            q.add(r, qaux, c);
        }
    };
};
// F64 with every reward scaled by (1 + 2^-40): a perturbation far below F32 rounding.
struct Jitter64 {
    template <class T> using Counter = Plain<T>;
    using Aux = NoAux;
    struct Cell {
        double v;
        void set(double r) { v = r; }
        double get() const { return v; }
        void add(double r, Aux&, Counters& c) {
            ++c.updates;
            v += r * (1.0 + 0x1p-40);
        }
    };
};

template <class F>
static void for_states(F f) {
    for (int street = 0; street < 4; ++street)
        for (uint64_t seed = 1; seed <= 25; ++seed) f(some_state(seed, street), seed);
}

// Largest root-mean gap between two searches, over all actions and 100 states.
template <class F>
static void report_gap(const char* label, F run_pair) {
    double worst = 0, worst_value = 0, sum_value = 0;
    int over = 0;
    for_states([&](const Hulh& s, uint64_t seed) {
        const std::pair<RootStats, RootStats> r = run_pair(s, seed);
        double gap = 0;
        for (int a = 0; a < kActions; ++a)
            gap = std::max(gap, std::fabs(r.first.mean(a) - r.second.mean(a)));
        const double v = std::fabs(r.first.value() - r.second.value());
        worst = std::max(worst, gap);
        worst_value = std::max(worst_value, v);
        sum_value += v;
        over += gap > 1e-4;
    });
    std::printf("free  %-22s max mean gap %.2e, value gap max %.2e avg %.2e, %d/100 > 1e-4\n",
                label, worst, worst_value, sum_value / 100, over);
}

template <class Q>
static double shadow_gap(const char* label, uint64_t n) {
    double worst = 0;
    for_states([&](const Hulh& s, uint64_t seed) {
        Tree<Hulh, Shadow<Q>> t{SearchConfig{}};
        typename Tree<Hulh, Shadow<Q>>::Worker w(seed, 0);
        for (uint64_t i = 0; i < n; ++i) t.template iterate<true, true>(s, s.player(), w);
        for (int a = 0; a < kActions; ++a)
            if (auto* c = t.root_child(a))
                worst = std::max(worst, std::fabs(c->sum.v - c->sum.q.get()) /
                                            static_cast<double>(c->visits.load(kRelaxed)));
    });
    std::printf("forced %-8s N=%lu: max |mean gap| %.2e\n", label,
                static_cast<unsigned long>(n), worst);
    return worst;
}

static void test_formats() {
    CHECK(Bf16Cas::decode(Bf16Cas::encode(1.0f)) == 1.0f);
    CHECK(Bf16Cas::decode(Bf16Cas::encode(-0.5f)) == -0.5f);
    CHECK(Bf16Cas::encode(1.0f + 1.0f / 256) == Bf16Cas::encode(1.0f));  // tie to even
    std::printf("sizeof(Node): LOCK %zu F64 %zu F32 %zu FIX64 %zu BF16 %zu\n",
                sizeof(Node<F64Lock>), sizeof(Node<F64Cas>), sizeof(Node<F32Cas>),
                sizeof(Node<Fix64>), sizeof(Node<Bf16Cas>));
    const uint64_t n = 10000;
    // Same trajectory: this is the 1e-4 criterion.
    CHECK(shadow_gap<F32Cas>("F32_CAS", n) < 1e-4);
    CHECK(shadow_gap<Fix64>("FIX64", n) < 1e-4);
    shadow_gap<Bf16Cas>("BF16_CAS", n);  // informational: not expected to agree
    // Free-running searches, with two F64-only controls for comparison.
    Tree<Hulh, Jitter64> tj{SearchConfig{}};
    const auto f64 = [n](const Hulh& s, uint64_t seed) { return tree_run<F64Cas>(s, n, 1, seed); };
    report_gap("F32_CAS", [&](const Hulh& s, uint64_t k) {
        return std::make_pair(f64(s, k), tree_run<F32Cas>(s, n, 1, k));
    });
    report_gap("FIX64", [&](const Hulh& s, uint64_t k) {
        return std::make_pair(f64(s, k), tree_run<Fix64>(s, n, 1, k));
    });
    report_gap("F64 jitter 2^-40", [&](const Hulh& s, uint64_t k) {
        return std::make_pair(f64(s, k), tree_search<Hulh, Jitter64>(tj, s, s.player(), n, 1, k));
    });
    report_gap("F64 other seed", [&](const Hulh& s, uint64_t k) {
        return std::make_pair(f64(s, k), f64(s, k + 1000));
    });
    report_gap("BF16_CAS", [&](const Hulh& s, uint64_t k) {
        return std::make_pair(f64(s, k), tree_run<Bf16Cas>(s, n, 1, k));
    });
}

// Accounting invariants that must survive any interleaving.
template <class P>
static void parallel_invariants(int p) {
    const Hulh s = some_state(3, 0);
    const uint64_t n = 40001;
    const auto r = tree_run<P>(s, n, p, 9);
    uint64_t v = 0;
    for (auto x : r.visits) v += x;
    CHECK(v == n);
    CHECK(r.counters.nodes <= 16583);
    for (int a = 0; a < kActions; ++a) CHECK(std::fabs(r.mean(a)) < 0.6);
}
static void test_parallel() {
    for (int p : {2, 4, 8}) {
        parallel_invariants<F64Lock>(p);
        parallel_invariants<F64Cas>(p);
        parallel_invariants<F32Cas>(p);
        parallel_invariants<Fix64>(p);
        parallel_invariants<Bf16Cas>(p);
        const Hulh s = some_state(4, 1);
        TreeSet<Hulh> trees(SearchConfig{}, p);
        const auto rp = root_parallel_search<Hulh>(trees, s, s.player(), 30001, p, 3);
        const auto pm1 = pimc_search<Hulh>(trees, s, s.player(), 30001, 16, 1, 3);
        const auto pmp = pimc_search<Hulh>(trees, s, s.player(), 30001, 16, p, 3);
        uint64_t v = 0;
        for (auto x : rp.visits) v += x;
        CHECK(v == 30001);
        same(pm1, pmp);  // PIMC is deterministic regardless of the thread count
    }
}

// Item 3: ISMCTS clearly beats a random player at Leduc.
static void test_leduc_strength() {
    const auto mk = [] { return ismcts_agent<Leduc>(2000, SearchConfig{}); };
    const auto r = duplicate_match<Leduc>(mk, [] { return random_agent<Leduc>(); }, 3000, 4, 1);
    std::printf("Leduc ISMCTS vs random: %+.3f +- %.3f chips/hand\n", r.mean, r.ci95);
    CHECK(r.mean - r.ci95 > 0);
    const auto c = duplicate_match<Leduc>(mk, [] { return call_agent<Leduc>(); }, 3000, 4, 2);
    std::printf("Leduc ISMCTS vs call:   %+.3f +- %.3f chips/hand\n", c.mean, c.ci95);
    CHECK(c.mean - c.ci95 > 0);
}

// River, facing a bet: fold ends at -contrib exactly; call ends at showdown, so its
// mean must equal the exact equity payoff over the 990 opponent hands.
static void test_exact_river() {
    int checked = 0, compared = 0;
    for (uint64_t seed = 1; seed <= 400 && checked < 25; ++seed) {
        Hulh s = some_state(seed, 3);
        s.apply(kRaise);  // the first player bets, so the other one faces a bet
        const int me = s.player();
        if (!(s.legal_mask() >> kFold & 1)) continue;
        ++checked;
        uint64_t used = 1ULL << s.hole[me][0] | 1ULL << s.hole[me][1];
        for (auto c : s.board) used |= 1ULL << c;
        double ev = 0;
        int n = 0;
        for (int i = 0; i < 52; ++i)
            for (int j = i + 1; j < 52; ++j) {
                if (used >> i & 1 || used >> j & 1) continue;
                Hulh t = s;
                t.hole[1 - me][0] = static_cast<uint8_t>(i);
                t.hole[1 - me][1] = static_cast<uint8_t>(j);
                t.apply(kCall);
                CHECK(t.terminal());
                ev += t.payoff(me);
                ++n;
            }
        CHECK(n == 990);
        ev /= n;
        const auto r = tree_run<F64Cas>(s, 200000, 1, seed);
        CHECK(std::fabs(r.mean(kFold) + s.contrib[me] / Hulh::kMaxPot) < 1e-9);
        if (r.visits[kCall] > 2000) {
            ++compared;
            const double se = (s.contrib[1 - me] / Hulh::kMaxPot) / std::sqrt(static_cast<double>(r.visits[kCall]));
            if (std::fabs(r.mean(kCall) - ev) > 4 * se)
                std::printf("river call EV mismatch: exact %.4f search %.4f\n", ev, r.mean(kCall));
            CHECK(std::fabs(r.mean(kCall) - ev) <= 4 * se);
        }
    }
    std::printf("exact river states checked: %d, call EV compared: %d\n", checked, compared);
    CHECK(checked >= 10 && compared >= 5);
}

int main() {
    test_exact_river();
    test_serial_equivalence();
    test_formats();
    test_parallel();
    test_leduc_strength();
    TEST_MAIN_END();
}
