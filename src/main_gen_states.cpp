// Writes a fixed set of HULH decision states (same number per street).
// Histories are random non-folding actions, so both seats and many bet levels occur.
#include <cstdio>
#include <fstream>

#include "game/state_io.hpp"
#include "util/cli.hpp"
#include "util/slurm_guard.hpp"

using namespace pm;

int main(int argc, char** argv) {
    Cli cli(argc, argv);
    const int n = static_cast<int>(cli.num("count", 1000));
    Rng rng(static_cast<uint64_t>(cli.num("seed", 2026)), 77);
    std::ofstream out(cli.str("out", "results/states.txt"));
    if (!out) return std::fprintf(stderr, "cannot write output\n"), 1;
    out << "# id street history hole board\n";
    for (int id = 0; id < n; ++id) {
        const int target = id % 4;
        for (;;) {
            DecisionState d;
            d.id = id;
            d.state = Hulh::deal(rng);
            Hulh& s = d.state;
            bool done = false;
            while (!s.terminal() && s.street <= target) {
                if (s.street == target && rng.below(3) == 0) { done = true; break; }
                const int a = (s.legal_mask() >> kRaise & 1) && rng.below(2) ? kRaise : kCall;
                d.history += a == kRaise ? 'r' : 'c';
                s.apply(a);
            }
            if (!done) continue;
            const std::string line = format_state(d);
            parse_state(line);  // round-trip check, throws if inconsistent
            out << line << '\n';
            break;
        }
    }
}
