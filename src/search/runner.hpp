// Runs any scheme/format by name on a HULH state, keeping the trees between calls.
#pragma once
#include <map>
#include <memory>
#include <stdexcept>
#include <string>

#include "game/hulh.hpp"
#include "search/search.hpp"

namespace pm {

constexpr int kPimcWorlds = 64;  // a multiple of every thread count used (1..16)

class Runner {
public:
    Runner(const SearchConfig& cfg, int max_threads)
        : cfg_(cfg), serial_(cfg), lock_(cfg), f64_(cfg), f32_(cfg), fix_(cfg), bf16_(cfg) {
        // Per-thread trees exist only when a scheme needs them.
        max_threads_ = max_threads;
    }

    // scheme: serial | tree | root | pimc; format is used by "tree" only.
    RootStats run(const std::string& scheme, const std::string& format, const Hulh& s,
                  uint64_t budget, int p, uint64_t seed) {
        const int me = s.player();
        if (scheme == "serial") return serial_search<Hulh, false>(serial_, s, me, budget, seed);
        if (scheme == "root") return root_parallel_search<Hulh>(own(), s, me, budget, p, seed);
        if (scheme == "pimc")
            return pimc_search<Hulh>(own(), s, me, budget, kPimcWorlds, p, seed);
        if (scheme == "tree") {
            if (format == F64Lock::kName) return tree_search(lock_, s, me, budget, p, seed);
            if (format == F64Cas::kName) return tree_search(f64_, s, me, budget, p, seed);
            if (format == F32Cas::kName) return tree_search(f32_, s, me, budget, p, seed);
            if (format == Fix64::kName) return tree_search(fix_, s, me, budget, p, seed);
            if (format == Bf16Cas::kName) return tree_search(bf16_, s, me, budget, p, seed);
        }
        throw std::runtime_error("unknown scheme/format " + scheme + "/" + format);
    }

private:
    TreeSet<Hulh>& own() {
        if (!own_) own_ = std::make_unique<TreeSet<Hulh>>(cfg_, max_threads_);
        return *own_;
    }
    SearchConfig cfg_;
    int max_threads_;
    Tree<Hulh, PlainF64> serial_;
    Tree<Hulh, F64Lock> lock_;
    Tree<Hulh, F64Cas> f64_;
    Tree<Hulh, F32Cas> f32_;
    Tree<Hulh, Fix64> fix_;
    Tree<Hulh, Bf16Cas> bf16_;
    std::unique_ptr<TreeSet<Hulh>> own_;
};

}  // namespace pm
