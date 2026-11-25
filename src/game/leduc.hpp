// Leduc Hold'em, used only to validate the search code. Six cards (three ranks,
// two suits), ante 1, bet 2 then 4, at most two bets per round.
#pragma once
#include <cstdint>

#include "game/hulh.hpp"

namespace pm {

struct Leduc {
    static constexpr const char* kName = "leduc";
    static constexpr int kMaxBets = 2;
    static constexpr int kMaxContrib = 13;  // 1 + 4 + 8
    static constexpr double kMaxPot = 2.0 * kMaxContrib;
    static constexpr int kMaxDepth = 12;

    uint8_t street = 0, to_act = 0, bets = 0, acted = 0;
    uint8_t contrib[2] = {1, 1};
    int8_t folded = -1;
    bool over = false;
    uint8_t hole[2] = {0, 0};
    uint8_t board = 0;

    static int bet_size(int street) { return street == 0 ? 2 : 4; }
    bool terminal() const { return over; }
    int player() const { return to_act; }

    unsigned legal_mask() const {
        unsigned m = 1u << kCall;
        if (contrib[to_act] < contrib[1 - to_act]) m |= 1u << kFold;
        if (bets < kMaxBets) m |= 1u << kRaise;
        return m;
    }

    void apply(int a) {
        const int me = to_act, opp = 1 - me;
        if (a == kFold) {
            folded = static_cast<int8_t>(me);
            over = true;
            return;
        }
        contrib[me] = contrib[opp];
        if (a == kRaise) {
            contrib[me] = static_cast<uint8_t>(contrib[me] + bet_size(street));
            ++bets;
            acted = 1;
        } else if (++acted == 2) {
            if (street == 1) { over = true; return; }
            street = 1;
            bets = 0;
            acted = 0;
            to_act = 0;
            return;
        }
        to_act = static_cast<uint8_t>(opp);
    }

    int strength(int p) const { return (hole[p] / 2 == board / 2 ? 10 : 0) + hole[p] / 2; }
    int net_chips(int p) const {
        if (folded >= 0) return folded == p ? -contrib[p] : contrib[1 - p];
        const int a = strength(p), b = strength(1 - p);
        return a == b ? 0 : (a > b ? contrib[1 - p] : -contrib[p]);
    }
    double payoff(int p) const { return net_chips(p) / kMaxPot; }

    void determinize(int p, Rng& rng) {
        uint32_t used = 1u << hole[p];
        if (street == 1) used |= 1u << board;
        hole[1 - p] = draw(used, rng);
        if (street == 0) board = draw(used, rng);
    }
    static Leduc deal(Rng& rng) {
        Leduc s;
        uint32_t used = 0;
        s.hole[0] = draw(used, rng);
        s.hole[1] = draw(used, rng);
        s.board = draw(used, rng);
        return s;
    }
    static uint8_t draw(uint32_t& used, Rng& rng) {
        for (;;) {
            const uint32_t c = rng.below(6);
            if (!(used >> c & 1)) {
                used |= 1u << c;
                return static_cast<uint8_t>(c);
            }
        }
    }
};

}  // namespace pm
