// Heads-up limit Texas Hold'em. Chips are counted in half small-bets:
// SB = 1, BB = 2, bet = 2 (preflop, flop) or 4 (turn, river).
// At most four bets per street; the big blind is the first preflop bet.
// Player 0 is the button / small blind: first to act preflop, last afterwards.
#pragma once
#include <cstdint>

#include "game/hand_eval.hpp"
#include "util/rng.hpp"

namespace pm {

enum Action : int { kFold = 0, kCall = 1, kRaise = 2, kNumActions = 3 };

struct Hulh {
    static constexpr const char* kName = "hulh";
    static constexpr int kMaxBets = 4;
    static constexpr int kMaxContrib = 48;               // 8 + 8 + 16 + 16 per player
    static constexpr double kMaxPot = 2.0 * kMaxContrib;  // reward normaliser
    static constexpr int kMaxDepth = 32;

    uint8_t street = 0, to_act = 0, bets = 1, acted = 0;
    uint8_t contrib[2] = {1, 2};
    int8_t folded = -1;
    bool over = false;
    uint8_t hole[2][2] = {{0, 0}, {0, 0}};
    uint8_t board[5] = {0, 0, 0, 0, 0};

    static int visible_board(int street) { return street == 0 ? 0 : street + 2; }
    static int bet_size(int street) { return street < 2 ? 2 : 4; }

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
            if (street == 3) { over = true; return; }
            ++street;
            bets = 0;
            acted = 0;
            to_act = 1;
            return;
        }
        to_act = static_cast<uint8_t>(opp);
    }

    // Net chips (half small-bets) won by player p. Only valid at a terminal state.
    int net_chips(int p) const {
        if (folded >= 0) return folded == p ? -contrib[p] : contrib[1 - p];
        uint8_t c[2][7];
        for (int q = 0; q < 2; ++q) {
            c[q][0] = hole[q][0];
            c[q][1] = hole[q][1];
            for (int i = 0; i < 5; ++i) c[q][2 + i] = board[i];
        }
        const uint32_t a = eval7(c[p]), b = eval7(c[1 - p]);
        return a == b ? 0 : (a > b ? contrib[1 - p] : -contrib[p]);
    }
    double payoff(int p) const { return net_chips(p) / kMaxPot; }

    // Resample everything player p cannot see: opponent hole cards, future board.
    void determinize(int p, Rng& rng) {
        uint64_t used = 1ULL << hole[p][0] | 1ULL << hole[p][1];
        const int nb = visible_board(street);
        for (int i = 0; i < nb; ++i) used |= 1ULL << board[i];
        hole[1 - p][0] = draw(used, rng);
        hole[1 - p][1] = draw(used, rng);
        for (int i = nb; i < 5; ++i) board[i] = draw(used, rng);
    }

    static Hulh deal(Rng& rng) {
        Hulh s;
        uint64_t used = 0;
        for (auto& h : s.hole)
            for (auto& c : h) c = draw(used, rng);
        for (auto& c : s.board) c = draw(used, rng);
        return s;
    }

    static uint8_t draw(uint64_t& used, Rng& rng) {
        for (;;) {
            const uint32_t c = rng.below(52);
            if (!(used >> c & 1)) {
                used |= 1ULL << c;
                return static_cast<uint8_t>(c);
            }
        }
    }
};

}  // namespace pm
