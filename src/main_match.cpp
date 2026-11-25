// Section 7.4: duplicate matches against baseline agents, reported in mbb/hand.
#include <cstdio>

#include "search/agents.hpp"
#include "util/cli.hpp"

using namespace pm;

static std::function<Agent<Hulh>()> make(const std::string& spec, const SearchConfig& cfg) {
    if (spec == "random") return [] { return random_agent<Hulh>(); };
    if (spec == "call") return [] { return call_agent<Hulh>(); };
    if (spec == "rule") return [] { return rule_agent(); };
    const auto k = spec.find(':');
    const std::string name = spec.substr(0, k);
    const auto n = static_cast<uint64_t>(std::stod(spec.substr(k + 1)));
    if (name == "ismcts") return [=] { return ismcts_agent<Hulh>(n, cfg); };
    if (name == "pimc") return [=] { return pimc_agent<Hulh>(n, kActions * 16, cfg); };
    throw std::runtime_error("unknown agent " + spec);
}

int main(int argc, char** argv) {
    Cli cli(argc, argv);
    const int pairs = static_cast<int>(cli.num("pairs", 2000));
    const int p = static_cast<int>(cli.num("threads", 8));
    const auto seed = static_cast<uint64_t>(cli.num("seed", 1));
    FILE* out = std::fopen(cli.str("out", "/dev/stdout").c_str(), "w");
    if (!out) return std::perror("out"), 1;
    std::fprintf(out, "agent,opponent,ucb_c,pairs,seed,mbb_per_hand,ci95\n");
    for (auto& cs : cli.list("c", "0.25")) {
        SearchConfig cfg;
        cfg.ucb_c = std::stod(cs);
        for (auto& a : cli.list("agents", "ismcts:10000"))
            for (auto& o : cli.list("opponents", "random,call,rule")) {
                const auto r = duplicate_match<Hulh>(make(a, cfg), make(o, cfg), pairs, p, seed);
                // net chips are half small-bets; one big blind is two of them
                std::fprintf(out, "%s,%s,%s,%d,%lu,%.1f,%.1f\n", a.c_str(), o.c_str(), cs.c_str(),
                             pairs, seed, r.mean / 2 * 1000, r.ci95 / 2 * 1000);
                std::fflush(out);
            }
    }
}
