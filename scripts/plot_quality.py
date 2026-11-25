#!/usr/bin/env python3
"""Figures 5-6: agreement against budget, and the speed-quality plane."""
import csv

from common import COLOR, MARK, NAME, SERIES, SUMMARY, save, style

plt = style()
rows = list(csv.DictReader(open(SUMMARY / "quality.csv")))
fig, axes = plt.subplots(1, 2, figsize=(7.0, 2.6), sharey=True)
for ax, p, title in zip(axes, ["1", "8"], ["1 thread", "8 threads"]):
    for s in SERIES:
        r = [x for x in rows if x["threads"] == p and x["series"] == s]
        if not r:
            continue
        x = [int(v["budget"]) for v in r]
        y = [float(v["agreement"]) for v in r]
        err = [[a - float(v["agree_lo"]) for a, v in zip(y, r)],
               [float(v["agree_hi"]) - a for a, v in zip(y, r)]]
        ax.errorbar(x, y, yerr=err, color=COLOR[s], marker=MARK[s], capsize=2, lw=1.2,
                    label=NAME[s] + (" (noise floor)" if s == "F64_CAS" else ""))
    ax.set_xscale("log")
    ax.set_xlabel("Iterations per decision")
    ax.set_title(title)
axes[0].set_ylabel("Agreement with reference")
h, l = axes[1].get_legend_handles_labels()
fig.legend(h, l, loc="outside upper center", ncol=4, frameon=False)
save(fig, "fig5_agreement")

pr = list(csv.DictReader(open(SUMMARY / "pareto.csv")))
fig, axes = plt.subplots(1, 3, figsize=(7.0, 2.6), sharey=True)
T = "0.01"
for ax, m in zip(axes, ["1", "8", "64"]):
    for s in SERIES:
        r = [x for x in pr if x["playouts"] == m and x["series"] == s and x["seconds"] == T]
        if not r:
            continue
        v = r[0]
        ax.scatter([float(v["rollouts_per_s_med"])], [float(v["agreement"])], s=28, zorder=3,
                   color=COLOR[s], marker=MARK[s], label=NAME[s], edgecolor="white", lw=0.4)
    ax.set_xscale("log")
    ax.set_xlabel("Rollouts per second")
    ax.set_title(f"m = {m}")
axes[0].set_ylabel("Agreement with reference")
h, l = axes[0].get_legend_handles_labels()
fig.legend(h, l, loc="outside upper center", ncol=4, frameon=False)
save(fig, "fig6_pareto")
