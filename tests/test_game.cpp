// CLAUDE.md section 6, items 1 and 2: game rules and hand evaluator.
#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

#include "check.hpp"
#include "game/hulh.hpp"
#include "game/leduc.hpp"

using namespace pm;

// Deliberately naive 5-card evaluator: sort, count, compare. Same value encoding.
static uint32_t slow5(const int* c) {
    std::vector<std::pair<int, int>> groups;  // (count, rank)
    int cnt[13] = {0};
    bool flush = true;
    for (int i = 0; i < 5; ++i) {
        ++cnt[c[i] / 4];
        flush = flush && c[i] % 4 == c[0] % 4;
    }
    for (int r = 0; r < 13; ++r)
        if (cnt[r]) groups.push_back({cnt[r], r});
    std::sort(groups.rbegin(), groups.rend());
    int straight = -1;
    if (groups.size() == 5) {
        if (groups[0].second - groups[4].second == 4) straight = groups[0].second;
        if (groups[0].second == 12 && groups[1].second == 3) straight = 3;  // wheel
    }
    uint32_t kick = 0;
    for (auto& g : groups) kick = kick * 16 + static_cast<uint32_t>(g.second);
    if (flush && straight >= 0) return kStraightFlush << 20 | straight;
    if (groups[0].first == 4) return kQuads << 20 | kick;
    if (groups[0].first == 3 && groups[1].first == 2) return kFullHouse << 20 | kick;
    if (flush) return kFlush << 20 | kick;
    if (straight >= 0) return kStraight << 20 | straight;
    if (groups[0].first == 3) return kTrips << 20 | kick;
    if (groups[0].first == 2 && groups[1].first == 2) return kTwoPair << 20 | kick;
    if (groups[0].first == 2) return kPair << 20 | kick;
    return kHighCard << 20 | kick;
}
static uint32_t slow7(const uint8_t* c) {
    uint32_t best = 0;
    for (int i = 0; i < 7; ++i)
        for (int j = i + 1; j < 7; ++j) {
            int h[5], n = 0;
            for (int k = 0; k < 7; ++k)
                if (k != i && k != j) h[n++] = c[k];
            best = std::max(best, slow5(h));
        }
    return best;
}

static void play(Hulh& s, const char* h) {
    for (; *h; ++h) {
        const int a = *h == 'f' ? kFold : *h == 'c' ? kCall : kRaise;
        CHECK(!s.terminal());
        CHECK(s.legal_mask() >> a & 1);
        s.apply(a);
    }
}

static void test_rules() {
    Hulh s;
    CHECK(s.player() == 0);
    CHECK(s.legal_mask() == 7u);
    play(s, "c");  // limp: the big blind keeps the option
    CHECK(s.street == 0 && s.player() == 1 && s.legal_mask() == 6u);
    play(s, "c");
    CHECK(s.street == 1 && s.player() == 1 && s.bets == 0 && s.contrib[0] == 2);
    play(s, "r");
    CHECK(s.contrib[1] == 4 && s.player() == 0);
    play(s, "rrr");
    CHECK(s.bets == 4 && s.contrib[0] == 10);
    CHECK(s.legal_mask() == 3u);  // cap reached: fold or call only
    play(s, "c");
    CHECK(s.street == 2 && s.contrib[0] == 10 && s.contrib[1] == 10);
    play(s, "r");
    CHECK(s.contrib[1] == 14);  // turn bets are doubled
    play(s, "f");
    CHECK(s.terminal() && s.net_chips(0) == -10 && s.net_chips(1) == 10);

    Hulh cap;
    play(cap, "rrr");  // blind + three raises = four bets
    CHECK(cap.contrib[0] == 8 && !(cap.legal_mask() >> kRaise & 1));
    play(cap, "crrrrcrrrrcrrrrc");
    CHECK(cap.terminal() && cap.contrib[0] == Hulh::kMaxContrib);
    CHECK(cap.contrib[1] == Hulh::kMaxContrib);

    Hulh sb;
    play(sb, "f");
    CHECK(sb.net_chips(0) == -1 && sb.net_chips(1) == 1);

    // Showdown: winner, loser and a split pot.
    auto showdown = [](std::vector<int> h0, std::vector<int> h1, std::vector<int> b) {
        Hulh g;
        for (int i = 0; i < 2; ++i) g.hole[0][i] = h0[i], g.hole[1][i] = h1[i];
        for (int i = 0; i < 5; ++i) g.board[i] = b[i];
        play(g, "cccccccc");
        CHECK(g.terminal());
        return g;
    };
    Hulh w = showdown({48, 49}, {44, 45}, {0, 5, 10, 23, 28});
    CHECK(w.net_chips(0) == 2 && w.net_chips(1) == -2);
    Hulh tie = showdown({0, 1}, {5, 6}, {33, 36, 42, 47, 49});  // T-J-Q-K-A on board
    CHECK(tie.net_chips(0) == 0 && tie.net_chips(1) == 0);
    CHECK(std::fabs(w.payoff(0) - 2.0 / 96) < 1e-15);

    // Zero-sum and bounds over random play.
    Rng rng(7);
    for (int i = 0; i < 20000; ++i) {
        Hulh g = Hulh::deal(rng);
        int depth = 0;
        while (!g.terminal()) {
            int a;
            do a = rng.below(3);
            while (!(g.legal_mask() >> a & 1));
            g.apply(a);
            ++depth;
        }
        CHECK(depth <= Hulh::kMaxDepth);
        CHECK(g.net_chips(0) + g.net_chips(1) == 0);
        CHECK(std::abs(g.net_chips(0)) <= Hulh::kMaxContrib);
    }

    // Determinization keeps every visible card and never duplicates a card.
    for (int i = 0; i < 5000; ++i) {
        Hulh g = Hulh::deal(rng), d;
        const int target = rng.below(4);
        while (g.street < target) g.apply(kCall);
        d = g;
        const int p = rng.below(2);
        d.determinize(p, rng);
        CHECK(d.hole[p][0] == g.hole[p][0] && d.hole[p][1] == g.hole[p][1]);
        for (int k = 0; k < Hulh::visible_board(g.street); ++k) CHECK(d.board[k] == g.board[k]);
        uint64_t seen = 0;
        int n = 0;
        for (auto& h : d.hole)
            for (auto c : h) seen |= 1ULL << c, ++n;
        for (auto c : d.board) seen |= 1ULL << c, ++n;
        CHECK(__builtin_popcountll(seen) == n);
    }
}

static void test_tree_size() {
    // Size of the complete open-loop betting tree; bounds the node pool.
    std::function<long(const Hulh&)> count = [&](const Hulh& s) -> long {
        if (s.terminal()) return 1;
        long n = 1;
        for (int a = 0; a < 3; ++a)
            if (s.legal_mask() >> a & 1) {
                Hulh t = s;
                t.apply(a);
                n += count(t);
            }
        return n;
    };
    std::printf("HULH betting-tree nodes by street:");
    Hulh s;
    for (int street = 0; street < 4; ++street) {
        while (s.street < street) s.apply(kCall);
        std::printf(" %ld", count(s));
    }
    std::printf("\n");
}

static void test_evaluator() {
    Rng rng(2026);
    std::vector<int> hist(10, 0);
    for (int i = 0; i < 100000; ++i) {
        uint64_t used = 0;
        uint8_t c[7];
        for (auto& x : c) x = Hulh::draw(used, rng);
        const uint32_t f = eval7(c);
        CHECK(f == slow7(c));
        ++hist[f >> 20];
    }
    std::printf("categories (high..sf):");
    for (int k = 1; k <= 9; ++k) std::printf(" %d", hist[k]);
    std::printf("\n");

    // Rare categories are barely hit by uniform sampling: draw from a short deck too.
    for (int i = 0; i < 100000; ++i) {
        uint8_t c[7];
        uint32_t used = 0;
        const int base = rng.below(10), width = 4;  // ranks base..base+3 -> quads, full houses
        for (auto& x : c) {
            uint32_t k;
            do k = rng.below(width * 4);
            while (used >> k & 1);
            used |= 1u << k;
            x = static_cast<uint8_t>(base * 4 + k);
        }
        CHECK(eval7(c) == slow7(c));
    }
    for (int i = 0; i < 100000; ++i) {
        uint8_t c[7];
        uint64_t used = 0;
        const int suit = rng.below(4);
        for (int k = 0; k < 7; ++k) {  // five or more suited cards -> (straight) flushes
            do {
                c[k] = k < 5 ? static_cast<uint8_t>(rng.below(13) * 4 + suit)
                             : static_cast<uint8_t>(rng.below(52));
            } while (used >> c[k] & 1);
            used |= 1ULL << c[k];
        }
        CHECK(eval7(c) == slow7(c));
    }

    // AA vs KK preflop, about 82%.
    double eq = 0;
    const int n = 400000;
    for (int i = 0; i < n; ++i) {
        uint64_t used = 0;
        uint8_t a[7], b[7];
        a[0] = 48, a[1] = 49, b[0] = 44, b[1] = 45;
        used = 1ULL << 48 | 1ULL << 49 | 1ULL << 44 | 1ULL << 45;
        for (int k = 2; k < 7; ++k) a[k] = b[k] = Hulh::draw(used, rng);
        const uint32_t x = eval7(a), y = eval7(b);
        eq += x > y ? 1.0 : x == y ? 0.5 : 0.0;
    }
    eq /= n;
    std::printf("AA vs KK equity %.4f\n", eq);
    CHECK(std::fabs(eq - 0.82) < 0.01);
}

static void test_leduc() {
    Leduc s;
    s.hole[0] = 0, s.hole[1] = 4, s.board = 1;  // player 0 pairs the board
    for (int a : {kRaise, kRaise, kCall, kRaise, kRaise, kCall}) {
        CHECK(s.legal_mask() >> a & 1);
        s.apply(a);
    }
    CHECK(s.terminal() && s.contrib[0] == Leduc::kMaxContrib);
    CHECK(s.net_chips(0) == 13 && s.net_chips(1) == -13);
}

int main() {
    test_rules();
    test_tree_size();
    test_evaluator();
    test_leduc();
    TEST_MAIN_END();
}
