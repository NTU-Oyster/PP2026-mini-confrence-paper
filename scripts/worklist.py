#!/usr/bin/env python3
"""Print the shards of the experiments, one per line:  est_seconds <TAB> output <TAB> command.

Usage: worklist.py {scaling|quality|match|all}. Shards are independent and resumable;
scripts/run_queue.sh executes those whose output file does not exist yet.
"""
import sys

FORMATS = "F64_LOCK,F64_CAS,F32_CAS,FIX64,BF16_CAS"
UCB = "--c 0.25"


def scaling():
    # 100 states x 5 seeds x m in {1, 8, 64}; blocks of 25 states per shard. Ordered by
    # repetition so that a partial run is already balanced over m and states.
    est = {1: 300, 8: 360, 64: 720}
    for rep in range(5):
        for b in range(0, 100, 25):
            for m in (1, 8, 64):
                out = f"results/raw/scaling/m{m}_rep{rep}_b{b}.csv"
                yield est[m], out, (
                    f"./build/main_bench --count 100 --begin {b} --end {b + 25} --playouts {m} "
                    f"--rep-begin {rep} --rep-end {rep + 1} --budgets 100000,1000000 "
                    f"--threads 1,2,4,8,16 {UCB}")


def quality():
    for b in range(0, 1000, 250):
        # Reference: seeds 100..102 are disjoint from the test seeds 0..2.
        yield 400, f"results/raw/quality/ref_b{b}.csv", (
            f"./build/main_quality --begin {b} --end {b + 250} --workers 16 --formats F64_CAS "
            f"--budgets 10000000 --seed-begin 100 --seed-end 103 {UCB}")
    for budget, est in ((10000, 60), (100000, 60), (1000000, 120), (10000000, 1200)):
        for b in range(0, 1000, 250):
            yield est, f"results/raw/quality/p1_N{budget}_b{b}.csv", (
                f"./build/main_quality --begin {b} --end {b + 250} --workers 16 "
                f"--formats F64_CAS,F32_CAS,FIX64,BF16_CAS --budgets {budget} "
                f"--seed-begin 0 --seed-end 3 {UCB}")
    for budget, est in ((10000, 60), (100000, 180), (1000000, 600)):
        for b in range(0, 1000, 100):
            yield est, f"results/raw/quality/p8_N{budget}_b{b}.csv", (
                f"./build/main_quality --begin {b} --end {b + 100} --threads 8 "
                f"--formats {FORMATS},root,pimc --budgets {budget} --seed-begin 0 --seed-end 3 {UCB}")
    # 1e7 with 8 threads is restricted to 250 states and one seed (calibrated cost).
    for b in range(0, 250, 50):
        yield 900, f"results/raw/quality/p8_N10000000_b{b}.csv", (
            f"./build/main_quality --begin {b} --end {b + 50} --threads 8 "
            f"--formats {FORMATS},root,pimc --budgets 10000000 --seed-begin 0 --seed-end 1 {UCB}")
    # Equal wall-clock time, for the speed-quality Pareto plot.
    for m in (1, 8, 64):
        for t in (0.001, 0.01, 0.1):
            for b in range(0, 1000, 250):
                yield max(40, int(250 * 7 * t * 1.2)), f"results/raw/quality/t{t}_m{m}_b{b}.csv", (
                    f"./build/main_quality --begin {b} --end {b + 250} --threads 8 "
                    f"--formats {FORMATS},root,pimc --budgets 1 --seconds {t} --playouts {m} "
                    f"--seed-begin 0 --seed-end 1 {UCB}")


def match():
    # Seed differs from every calibration match.
    for agent in ("ismcts:10000", "ismcts:100000", "pimc:10000", "pimc:100000"):
        yield 400, f"results/raw/match/{agent.replace(':', '_')}.csv", (
            f"./build/main_match --agents {agent} --opponents random,call,rule --c 0.25 "
            f"--pairs 5000 --threads 16 --seed 777")


LISTS = {"scaling": scaling, "quality": quality, "match": match}
which = sys.argv[1] if len(sys.argv) > 1 else "all"
for name in LISTS if which == "all" else [which]:
    for est, out, cmd in LISTS[name]():
        print(est, out, cmd, sep="\t")
