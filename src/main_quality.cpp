// RQ3: root decisions and values. Two modes share one row format:
//   --threads 1   many states at once, one sequential search each (no timing use)
//   --threads P   one state at a time with P threads, so the search is parallel
#include <atomic>
#include <cstdio>

#include "game/state_io.hpp"
#include "search/runner.hpp"
#include "util/cli.hpp"

using namespace pm;

int main(int argc, char** argv) {
    Cli cli(argc, argv);
    const auto states = load_states(cli.str("states", "results/states.txt"));
    const size_t b = static_cast<size_t>(cli.num("begin", 0));
    const size_t e = std::min(states.size(), static_cast<size_t>(cli.num("end", 1e9)));
    const int p = static_cast<int>(cli.num("threads", 1));
    const int workers = static_cast<int>(cli.num("workers", 1));  // concurrent states, p = 1
    const auto budgets = cli.list("budgets", "10000");
    const auto formats = cli.list("formats", "F64_CAS");
    const int seed0 = static_cast<int>(cli.num("seed-begin", 0));
    const int seed1 = static_cast<int>(cli.num("seed-end", 1));
    SearchConfig cfg;
    cfg.ucb_c = cli.num("c", cfg.ucb_c);
    cfg.virtual_loss = cli.num("vl", cfg.virtual_loss);
    FILE* out = std::fopen(cli.str("out", "/dev/stdout").c_str(), "w");
    if (!out) return std::perror("out"), 1;
    std::fprintf(out, "format,threads,budget,state_id,seed,action,value,"
                      "visits_fold,visits_call,visits_raise,mean_fold,mean_call,mean_raise,"
                      "wall_time_s\n");
    if (p != 1 && workers != 1) return std::fprintf(stderr, "threads and workers exclusive\n"), 1;

    const auto do_state = [&](Runner& runner, size_t i) {
        for (auto& bs : budgets)
            for (auto& f : formats)
                for (int k = seed0; k < seed1; ++k) {
                    const auto budget = static_cast<uint64_t>(std::stod(bs));
                    const auto r = runner.run("tree", f, states[i].state, budget, p,
                                              static_cast<uint64_t>(k) * 1000003 + 7);
                    std::fprintf(out, "%s,%d,%lu,%d,%d,%d,%.17g,%lu,%lu,%lu,%.17g,%.17g,%.17g,%.6f\n",
                                 f.c_str(), p, budget, states[i].id, k, r.best(), r.value(),
                                 r.visits[0], r.visits[1], r.visits[2], r.mean(0), r.mean(1),
                                 r.mean(2), r.wall_s);
                }
    };
    if (workers == 1) {
        Runner runner(cfg, p);
        for (size_t i = b; i < e; ++i) do_state(runner, i), std::fflush(out);
    } else {
        std::atomic<size_t> next{b};
        // stdio locks each fprintf call, so rows from different workers never mix.
        run_team(workers, [&](int) {
            Runner runner(cfg, 1);
            for (size_t i; (i = next.fetch_add(1)) < e;) do_state(runner, i);
        });
    }
    std::fclose(out);
}
