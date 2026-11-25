// RQ1/RQ2: strong-scaling runs. One CSV row per search.
#include <algorithm>
#include <cstdio>

#include "game/state_io.hpp"
#include "search/runner.hpp"
#include "util/cli.hpp"

using namespace pm;

int main(int argc, char** argv) {
    Cli cli(argc, argv);
    const auto states = load_states(cli.str("states", "results/states.txt"));
    const int count = static_cast<int>(cli.num("count", 100));
    const int rep0 = static_cast<int>(cli.num("rep-begin", 0));
    const int rep1 = static_cast<int>(cli.num("rep-end", 5));
    SearchConfig cfg;
    cfg.ucb_c = cli.num("c", cfg.ucb_c);
    cfg.virtual_loss = cli.num("vl", cfg.virtual_loss);
    cfg.playouts = static_cast<int>(cli.num("playouts", 1));
    std::vector<int> threads;
    for (auto& t : cli.list("threads", "1,2,4,8,16")) threads.push_back(std::stoi(t));
    const auto budgets = cli.list("budgets", "100000");
    const auto schemes = cli.list(
        "schemes", "serial,tree:F64_LOCK,tree:F64_CAS,tree:F32_CAS,tree:FIX64,tree:BF16_CAS,root,pimc");
    FILE* out = std::fopen(cli.str("out", "/dev/stdout").c_str(), "w");
    if (!out) return std::perror("out"), 1;
    std::fprintf(out, "scheme,format,threads,budget,state_id,seed,wall_time_s,rollouts_per_s,"
                      "cas_retries,lock_waits,node_count,bytes_per_node,updates,lost_expansions\n");

    const int pmax = *std::max_element(threads.begin(), threads.end());
    Runner runner(cfg, pmax);
    const auto split = [](const std::string& x) {
        const auto k = x.find(':');
        return k == std::string::npos ? std::make_pair(x, std::string("F64_PLAIN"))
                                      : std::make_pair(x.substr(0, k), x.substr(k + 1));
    };
    const size_t step = std::max<size_t>(1, states.size() / count);
    // Warm-up: touch every tree and start the thread team before timing anything.
    for (auto& sc : schemes)
        runner.run(split(sc).first, split(sc).second, states[0].state, 100000, pmax, 1);

    for (int rep = rep0; rep < rep1; ++rep)
        for (int i = 0; i < count; ++i) {
            const auto& d = states[(i * step + i % 4) % states.size()];
            const uint64_t seed = 1000 + static_cast<uint64_t>(rep);
            // Configurations of one state run back to back, so slow drift in clock
            // frequency or background load is shared by all of them.
            for (auto& b : budgets)
                for (auto& sc : schemes) {
                    const auto [scheme, format] = split(sc);
                    for (int p : threads) {
                        if (scheme == "serial" && p != threads[0]) continue;
                        const auto budget = static_cast<uint64_t>(std::stod(b));
                        const auto r = runner.run(scheme, format, d.state, budget, p, seed);
                        std::fprintf(out, "%s,%s,%d,%lu,%d,%lu,%.9f,%.1f,%lu,%lu,%lu,%zu,%lu,%lu\n",
                                     scheme.c_str(), format.c_str(), scheme == "serial" ? 1 : p,
                                     budget, d.id, seed, r.wall_s, budget / r.wall_s,
                                     r.counters.cas_retries, r.counters.lock_waits,
                                     r.counters.nodes, r.bytes_per_node, r.counters.updates,
                                     r.counters.lost_expansions);
                    }
                }
            std::fflush(out);
        }
    std::fclose(out);
}
