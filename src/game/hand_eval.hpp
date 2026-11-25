// 7-card hand evaluator, written from scratch (no third-party code or tables).
// Card = rank * 4 + suit, rank 0..12 (deuce..ace), suit 0..3.
// Value = category << 20 | up to five rank nibbles; larger is better.
#pragma once
#include <array>
#include <cstdint>

namespace pm {

enum HandCategory : uint32_t {
    kHighCard = 1, kPair, kTwoPair, kTrips, kStraight, kFlush, kFullHouse, kQuads,
    kStraightFlush
};

namespace detail {

constexpr int hi_bit(uint32_t m) { return 31 - __builtin_clz(m); }

constexpr uint32_t top_bits(uint32_t mask, int k) {
    uint32_t e = 0;
    while (k-- > 0) {
        const int b = hi_bit(mask);
        e = (e << 4) | static_cast<uint32_t>(b);
        mask ^= 1u << b;
    }
    return e;
}

struct EvalTables {
    std::array<uint8_t, 8192> straight{};  // high-card rank of the straight, 0 = none
    std::array<uint32_t, 8192> top5{};     // five highest ranks as nibbles
};

inline const EvalTables& tables() {
    static const EvalTables t = [] {
        EvalTables x;
        for (uint32_t m = 0; m < 8192; ++m) {
            for (int h = 12; h >= 4; --h) {
                const uint32_t run = 0x1fu << (h - 4);
                if ((m & run) == run) { x.straight[m] = static_cast<uint8_t>(h); break; }
            }
            const uint32_t wheel = (1u << 12) | 0xfu;  // A-2-3-4-5: high card is the five
            if (!x.straight[m] && (m & wheel) == wheel) x.straight[m] = 3;
            if (__builtin_popcount(m) >= 5) x.top5[m] = top_bits(m, 5);
        }
        return x;
    }();
    return t;
}

}  // namespace detail

inline uint32_t eval7(const uint8_t* c) {
    using detail::hi_bit;
    using detail::top_bits;
    const auto& T = detail::tables();
    uint32_t m[4] = {0, 0, 0, 0};
    for (int i = 0; i < 7; ++i) m[c[i] & 3] |= 1u << (c[i] >> 2);

    uint32_t flush = 0;
    for (uint32_t s : m)
        if (__builtin_popcount(s) >= 5) flush = s;
    if (flush && T.straight[flush]) return kStraightFlush << 20 | T.straight[flush];

    const uint32_t any = m[0] | m[1] | m[2] | m[3];
    const uint32_t a = m[0] & m[1], b = m[2] & m[3];
    const uint32_t x = m[0] | m[1], y = m[2] | m[3];
    const uint32_t p2 = a | b | (x & y);           // ranks held at least twice
    const uint32_t p3 = (a & y) | (b & x);         // at least three times
    const uint32_t p4 = a & b;

    if (p4) {
        const int q = hi_bit(p4);
        return kQuads << 20 | q << 4 | hi_bit(any & ~(1u << q));
    }
    if (p3) {
        const int t = hi_bit(p3);
        const uint32_t rest = p2 & ~(1u << t);
        if (rest) return kFullHouse << 20 | t << 4 | hi_bit(rest);
    }
    if (flush) return kFlush << 20 | T.top5[flush];
    if (T.straight[any]) return kStraight << 20 | T.straight[any];
    if (p3) {
        const int t = hi_bit(p3);
        return kTrips << 20 | t << 8 | top_bits(any & ~(1u << t), 2);
    }
    if (p2) {
        const int h = hi_bit(p2);
        const uint32_t rest = p2 & ~(1u << h);
        if (rest) {
            const int l = hi_bit(rest);
            return kTwoPair << 20 | h << 8 | l << 4 | hi_bit(any & ~(1u << h) & ~(1u << l));
        }
        return kPair << 20 | h << 12 | top_bits(any & ~(1u << h), 3);
    }
    return kHighCard << 20 | T.top5[any];
}

}  // namespace pm
