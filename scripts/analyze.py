#!/usr/bin/env python3
"""Raw CSVs -> results/summary/*.csv. Every number in the paper comes from these."""
import collections

import numpy as np

from common import SERIES, boot_ci, load, median_iqr, series_of, write_csv


def scaling():
    rows = load("scaling/*.csv")
    serial = {}
    for r in rows:
        if r["scheme"] == "serial":
            serial[r["playouts"], r["budget"], r["state_id"], r["seed"]] = float(r["wall_time_s"])
    group = collections.defaultdict(list)
    own1 = {}
    for r in rows:
        key = (r["playouts"], r["budget"], series_of(r), int(r["threads"]))
        group[key].append(r)
        if r["threads"] == "1":
            own1[r["playouts"], r["budget"], series_of(r), r["state_id"], r["seed"]] = float(
                r["wall_time_s"])
    out = []
    for (m, b, s, p), g in sorted(group.items(), key=lambda kv: str(kv[0])):
        if s == "serial":
            continue
        t = np.array([float(r["wall_time_s"]) for r in g])
        sp = [serial[m, b, r["state_id"], r["seed"]] / float(r["wall_time_s"]) for r in g]
        self_sp = [own1[m, b, s, r["state_id"], r["seed"]] / float(r["wall_time_s"]) for r in g]
        up = sum(int(r["updates"]) for r in g)
        med, lo, hi = median_iqr(sp)
        smed, slo, shi = median_iqr(self_sp)
        tmed, tlo, thi = median_iqr(t)
        out.append(dict(
            playouts=m, budget=b, series=s, threads=p, runs=len(g),
            speedup_med=med, speedup_q1=lo, speedup_q3=hi,
            efficiency_med=med / p, efficiency_q1=lo / p, efficiency_q3=hi / p,
            self_speedup_med=smed, self_speedup_q1=slo, self_speedup_q3=shi,
            time_med_s=tmed, time_q1_s=tlo, time_q3_s=thi,
            rollouts_per_s_med=np.median([float(r["rollouts_per_s"]) for r in g]),
            cas_retries_per_update=sum(int(r["cas_retries"]) for r in g) / up,
            lock_waits_per_update=sum(int(r["lock_waits"]) for r in g) / up,
            updates_per_iteration=up / sum(int(r["budget"]) for r in g),
            # Tree searches update a node twice per visit (virtual loss, then result). Formats
            # and schemes walk paths of different length, so time per visit is the fair
            # unit when comparing update costs; speedup alone mixes the two effects.
            visits_per_iteration=up / sum(int(r["budget"]) for r in g)
            / (1 if s in ("root", "pimc") else 2),
            ns_per_visit_med=np.median([
                float(r["wall_time_s"]) * 1e9
                / (int(r["updates"]) / (1 if s in ("root", "pimc") else 2)) for r in g]),
            nodes_med=np.median([int(r["node_count"]) for r in g]),
            bytes_per_node=g[0]["bytes_per_node"]))
    write_csv("scaling.csv", out)
    # The serial baseline itself, in absolute terms.
    ser = collections.defaultdict(list)
    for r in rows:
        if r["scheme"] == "serial":
            ser[r["playouts"], r["budget"]].append(float(r["rollouts_per_s"]))
    write_csv("serial.csv", [dict(playouts=m, budget=b, runs=len(v),
                                  rollouts_per_s_med=np.median(v))
                             for (m, b), v in sorted(ser.items())])
    # Per street, to show how the tree size changes contention.
    st = collections.defaultdict(list)
    for r in rows:
        if r["scheme"] == "tree" and r["threads"] == "8":
            st[r["playouts"], r["budget"], r["format"], r["street"]].append(
                serial[r["playouts"], r["budget"], r["state_id"], r["seed"]]
                / float(r["wall_time_s"]))
    write_csv("scaling_by_street.csv",
              [dict(playouts=k[0], budget=k[1], series=k[2], street=k[3], runs=len(v),
                    speedup_med=np.median(v)) for k, v in sorted(st.items())])


def reference():
    """Majority action per state over the three reference seeds. If all three differ,
    the action with the largest total visit count. Value: mean over the seeds."""
    by = collections.defaultdict(list)
    for r in load("quality/ref_*.csv"):
        by[int(r["state_id"])].append(r)
    ref, unanimous = {}, 0
    for sid, rs in by.items():
        acts = [int(r["action"]) for r in rs]
        top, n = collections.Counter(acts).most_common(1)[0]
        if n == 1:
            v = np.zeros(3)
            for r in rs:
                v += [int(r["visits_" + k]) for k in ("fold", "call", "raise")]
            top = int(v.argmax())
        unanimous += n == len(rs)
        ref[sid] = (top, np.mean([float(r["value"]) for r in rs]))
    write_csv("reference.csv", [dict(states=len(ref), seeds=len(rs), unanimous=unanimous,
                                     unanimous_frac=unanimous / len(ref))])
    return ref


def per_state_table(rows, ref):
    """{key: {state: (agreement over seeds, abs value error, rollouts/s, spread)}}"""
    acc = collections.defaultdict(lambda: collections.defaultdict(list))
    for r in rows:
        sid = int(r["state_id"])
        if sid not in ref:
            continue
        # iterations is what the search really ran; playouts turns it into rollouts.
        rps = int(r["iterations"]) * int(r["playouts"]) / float(r["wall_time_s"])
        acc[r["format"]][sid].append((float(int(r["action"]) == ref[sid][0]),
                                      abs(float(r["value"]) - ref[sid][1]), rps))
    return acc


def quality(ref):
    rng = np.random.default_rng(1)
    groups = collections.defaultdict(list)
    for r in load("quality/p1_*.csv") + load("quality/p8_*.csv"):
        groups[r["threads"], r["budget"]].append(r)
    out = []
    for (p, b), rows in sorted(groups.items(), key=lambda kv: (int(kv[0][0]), int(kv[0][1]))):
        table = per_state_table(rows, ref)
        base = {s: np.mean([x[0] for x in v]) for s, v in table["F64_CAS"].items()}
        for f in SERIES:
            if f not in table:
                continue
            states = sorted(table[f])
            agree = [np.mean([x[0] for x in table[f][s]]) for s in states]
            err = [np.mean([x[1] for x in table[f][s]]) for s in states]
            a, lo, hi = boot_ci(agree, rng)
            e, elo, ehi = boot_ci(err, rng)
            d, dlo, dhi = boot_ci([np.mean([x[0] for x in table[f][s]]) - base[s] for s in states], rng)
            out.append(dict(threads=p, budget=b, series=f, states=len(states),
                            seeds=len(table[f][states[0]]), agreement=a, agree_lo=lo, agree_hi=hi,
                            value_mae=e, value_lo=elo, value_hi=ehi,
                            agree_minus_f64=d, diff_lo=dlo, diff_hi=dhi))
    write_csv("quality.csv", out)


def pareto(ref):
    rng = np.random.default_rng(2)
    groups = collections.defaultdict(list)
    for r in load("quality/t*.csv"):
        groups[r["seconds_limit"], r["playouts"]].append(r)
    out = []
    for (t, m), rows in sorted(groups.items()):
        table = per_state_table(rows, ref)
        for f in SERIES:
            if f not in table:
                continue
            states = sorted(table[f])
            a, lo, hi = boot_ci([np.mean([x[0] for x in table[f][s]]) for s in states], rng)
            e = np.mean([np.mean([x[1] for x in table[f][s]]) for s in states])
            rps = np.median([x[2] for s in states for x in table[f][s]])
            out.append(dict(seconds=t, playouts=m, series=f, states=len(states), agreement=a,
                            agree_lo=lo, agree_hi=hi, value_mae=e, rollouts_per_s_med=rps))
    write_csv("pareto.csv", out)


def match():
    rows = load("match/*.csv")
    write_csv("match.csv", rows)


if __name__ == "__main__":
    scaling()
    ref = reference()
    quality(ref)
    pareto(ref)
    match()
