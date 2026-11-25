// Agents used for validation matches. An agent sees the full state object but must
// only use its own hole cards and the visible board; searching agents and the rule
// agent enforce this by re-determinizing the state first.
#pragma once
#include <cmath>
#include <functional>
#include <vector>

#include "game/hulh.hpp"
#include "search/search.hpp"

namespace pm {

template <class G>
using Agent = std::function<int(const G& s, int me, uint64_t seed)>;

template <class G>
Agent<G> random_agent() {
    return [](const G& s, int, uint64_t seed) {
        Rng rng(seed, 99);
        for (;;) {
            const int a = static_cast<int>(rng.below(3));
            if (s.legal_mask() >> a & 1) return a;
        }
    };
}

template <class G>
Agent<G> call_agent() {
    return [](const G&, int, uint64_t) { return 1; };
}

// Win probability against a uniformly random hand, by Monte Carlo.
inline double mc_equity(const Hulh& s, int me, int samples, Rng& rng) {
    double eq = 0;
    for (int i = 0; i < samples; ++i) {
        Hulh d = s;
        d.determinize(me, rng);
        uint8_t c[2][7];
        for (int q = 0; q < 2; ++q) {
            c[q][0] = d.hole[q][0];
            c[q][1] = d.hole[q][1];
            for (int k = 0; k < 5; ++k) c[q][2 + k] = d.board[k];
        }
        const uint32_t a = eval7(c[me]), b = eval7(c[1 - me]);
        eq += a > b ? 1.0 : a == b ? 0.5 : 0.0;
    }
    return eq / samples;
}

// Raise with a strong hand, fold when the equity is below the pot odds.
inline Agent<Hulh> rule_agent(int samples = 200) {
    return [samples](const Hulh& s, int me, uint64_t seed) {
        Rng rng(seed, 98);
        const double eq = mc_equity(s, me, samples, rng);
        const unsigned legal = s.legal_mask();
        const double to_call = s.contrib[1 - me] - s.contrib[me];
        const double pot = s.contrib[0] + s.contrib[1];
        if (eq > 0.6 && (legal >> kRaise & 1)) return static_cast<int>(kRaise);
        if (to_call > 0 && eq < to_call / (pot + to_call)) return static_cast<int>(kFold);
        return static_cast<int>(kCall);
    };
}

template <class G>
Agent<G> ismcts_agent(uint64_t budget, const SearchConfig& cfg) {
    auto tree = std::make_shared<Tree<G, PlainF64>>(cfg);
    return [=](const G& s, int me, uint64_t seed) {
        return serial_search<G, false>(*tree, s, me, budget, seed).best();
    };
}

template <class G>
Agent<G> pimc_agent(uint64_t budget, int worlds, const SearchConfig& cfg) {
    auto trees = std::make_shared<TreeSet<G>>(cfg, 1);
    return [=](const G& s, int me, uint64_t seed) {
        return pimc_search<G>(*trees, s, me, budget, worlds, 1, seed).best();
    };
}

struct MatchResult {
    double mean = 0, ci95 = 0;  // net chips per hand for agent A
    int pairs = 0;
};

// Duplicate match: every deal is played twice with the seats exchanged. Factories
// are called once per thread because searching agents own a tree.
template <class G>
MatchResult duplicate_match(const std::function<Agent<G>()>& make_a,
                            const std::function<Agent<G>()>& make_b, int pairs, int p,
                            uint64_t seed) {
    std::vector<double> x(pairs);
    run_team(p, [&](int tid) {
        Agent<G> a = make_a(), b = make_b();
        for (int i = tid; i < pairs; i += p) {
            Rng deal_rng(seed, static_cast<uint64_t>(i));
            const G deal = G::deal(deal_rng);
            Rng seeds(seed ^ 0x5eed, static_cast<uint64_t>(i));
            double net = 0;
            for (int seat = 0; seat < 2; ++seat) {  // seat of agent A
                G s = deal;
                while (!s.terminal()) {
                    const int me = s.player();
                    s.apply((me == seat ? a : b)(s, me, seeds.next()));
                }
                net += s.net_chips(seat);
            }
            x[i] = net / 2;
        }
    });
    MatchResult r;
    r.pairs = pairs;
    double sum = 0, sq = 0;
    for (double v : x) sum += v;
    r.mean = sum / pairs;
    for (double v : x) sq += (v - r.mean) * (v - r.mean);
    r.ci95 = 1.96 * std::sqrt(sq / (pairs - 1) / pairs);
    return r;
}

}  // namespace pm
