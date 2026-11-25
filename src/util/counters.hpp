// Per-thread event counters, summed after the search. Never shared while running.
#pragma once
#include <cstdint>

namespace pm {

struct alignas(64) Counters {
    uint64_t updates = 0;      // reward-cell read-modify-writes
    uint64_t cas_retries = 0;  // failed compare-exchange attempts on reward cells
    uint64_t lock_waits = 0;   // lock acquisitions that found the lock held
    uint64_t nodes = 0;        // nodes installed into the tree
    uint64_t lost_expansions = 0;
    Counters& operator+=(const Counters& o) {
        updates += o.updates;
        cas_retries += o.cas_retries;
        lock_waits += o.lock_waits;
        nodes += o.nodes;
        lost_expansions += o.lost_expansions;
        return *this;
    }
};

}  // namespace pm
