// Storage formats for the accumulated reward of a node. Each policy provides
//   Cell  with set() (before publication), get(), and add(r, aux, counters)
//   Aux   extra per-node state (the spinlock), empty otherwise
//   Counter<T>  the type used for visit counts and child links.
// Visit counts are 64-bit in every policy; only the reward format changes.
#pragma once
#include <immintrin.h>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "util/counters.hpp"

namespace pm {

constexpr auto kRelaxed = std::memory_order_relaxed;

// Same interface as std::atomic, without atomicity; for the sequential searches.
template <class T>
struct Plain {
    T v;
    T load(std::memory_order) const { return v; }
    void store(T x, std::memory_order) { v = x; }
    T fetch_add(T d, std::memory_order) {
        const T o = v;
        v += d;
        return o;
    }
    bool compare_exchange_strong(T& expect, T desired, std::memory_order, std::memory_order) {
        if (v != expect) {
            expect = v;
            return false;
        }
        v = desired;
        return true;
    }
};

struct NoAux {};

struct PlainF64 {
    static constexpr const char* kName = "F64_PLAIN";
    template <class T> using Counter = Plain<T>;
    using Aux = NoAux;
    struct Cell {
        double v;
        void set(double r) { v = r; }
        double get() const { return v; }
        void add(double r, Aux&, Counters& c) {
            ++c.updates;
            v += r;
        }
    };
};

struct F64Lock {
    static constexpr const char* kName = "F64_LOCK";
    template <class T> using Counter = std::atomic<T>;
    struct Aux {
        std::atomic<uint32_t> held{0};
    };
    struct Cell {
        // The lock serialises writers. The value is still declared atomic (and
        // accessed with plain relaxed loads/stores) so that the lock-free reads
        // in UCB selection are not data races.
        std::atomic<double> v;
        void set(double r) { v.store(r, kRelaxed); }
        double get() const { return v.load(kRelaxed); }
        void add(double r, Aux& a, Counters& c) {
            ++c.updates;
            if (a.held.exchange(1, std::memory_order_acquire)) {
                ++c.lock_waits;
                do {
                    while (a.held.load(kRelaxed)) _mm_pause();
                } while (a.held.exchange(1, std::memory_order_acquire));
            }
            v.store(v.load(kRelaxed) + r, kRelaxed);
            a.held.store(0, std::memory_order_release);
        }
    };
};

template <class T>
struct FloatCasCell {
    std::atomic<T> v;
    void set(double r) { v.store(static_cast<T>(r), kRelaxed); }
    double get() const { return static_cast<double>(v.load(kRelaxed)); }
    void add(double r, NoAux&, Counters& c) {
        ++c.updates;
        T old = v.load(kRelaxed);
        while (!v.compare_exchange_weak(old, old + static_cast<T>(r), kRelaxed, kRelaxed))
            ++c.cas_retries;
    }
};

struct F64Cas {
    static constexpr const char* kName = "F64_CAS";
    template <class T> using Counter = std::atomic<T>;
    using Aux = NoAux;
    using Cell = FloatCasCell<double>;
};

struct F32Cas {
    static constexpr const char* kName = "F32_CAS";
    template <class T> using Counter = std::atomic<T>;
    using Aux = NoAux;
    using Cell = FloatCasCell<float>;
};

struct Fix64 {
    static constexpr const char* kName = "FIX64";
    static constexpr double kScale = 1 << 20;
    template <class T> using Counter = std::atomic<T>;
    using Aux = NoAux;
    struct Cell {
        std::atomic<int64_t> v;
        void set(double r) { v.store(std::llrint(r * kScale), kRelaxed); }
        double get() const { return static_cast<double>(v.load(kRelaxed)) * (1.0 / kScale); }
        void add(double r, Aux&, Counters& c) {
            ++c.updates;
            v.fetch_add(std::llrint(r * kScale), kRelaxed);
        }
    };
};

struct Bf16Cas {
    static constexpr const char* kName = "BF16_CAS";
    template <class T> using Counter = std::atomic<T>;
    using Aux = NoAux;
    static uint16_t encode(float f) {  // round to nearest, ties to even
        uint32_t u;
        std::memcpy(&u, &f, 4);
        u += 0x7fff + (u >> 16 & 1);
        return static_cast<uint16_t>(u >> 16);
    }
    static float decode(uint16_t h) {
        const uint32_t u = static_cast<uint32_t>(h) << 16;
        float f;
        std::memcpy(&f, &u, 4);
        return f;
    }
    struct Cell {
        std::atomic<uint16_t> v;
        void set(double r) { v.store(encode(static_cast<float>(r)), kRelaxed); }
        double get() const { return decode(v.load(kRelaxed)); }
        void add(double r, Aux&, Counters& c) {
            ++c.updates;
            const float d = static_cast<float>(r);
            uint16_t old = v.load(kRelaxed);
            while (!v.compare_exchange_weak(old, encode(decode(old) + d), kRelaxed, kRelaxed))
                ++c.cas_retries;
        }
    };
};

}  // namespace pm
