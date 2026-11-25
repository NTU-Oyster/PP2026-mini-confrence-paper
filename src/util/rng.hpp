// xoshiro256** seeded by splitmix64(seed, stream). One instance per thread.
#pragma once
#include <cstdint>

namespace pm {

inline uint64_t splitmix64(uint64_t& x) {
    uint64_t z = (x += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

class Rng {
public:
    explicit Rng(uint64_t seed = 1, uint64_t stream = 0) {
        // Mix the stream id through splitmix64 first, so that (seed, stream)
        // and (seed + 1, stream - 1) do not yield overlapping state.
        uint64_t sid = stream;
        uint64_t x = seed ^ splitmix64(sid);
        for (auto& w : s_) w = splitmix64(x);
    }
    uint64_t next() {
        const uint64_t r = rotl(s_[1] * 5, 7) * 9;
        const uint64_t t = s_[1] << 17;
        s_[2] ^= s_[0];
        s_[3] ^= s_[1];
        s_[1] ^= s_[2];
        s_[0] ^= s_[3];
        s_[2] ^= t;
        s_[3] = rotl(s_[3], 45);
        return r;
    }
    // Uniform integer in [0, n). Multiply-shift on 32 bits; the bias is at most
    // n / 2^32, negligible for n <= 52.
    uint32_t below(uint32_t n) {
        return static_cast<uint32_t>(((next() >> 32) * n) >> 32);
    }
    double uniform() { return (next() >> 11) * 0x1.0p-53; }

private:
    static uint64_t rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }
    uint64_t s_[4];
};

}  // namespace pm
