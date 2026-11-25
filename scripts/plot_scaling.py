#!/usr/bin/env python3
"""Figures 2-4: speedup, efficiency and contention against threads."""
import csv

from common import COLOR, MARK, NAME, SERIES, SUMMARY, save, style

plt = style()
rows = list(csv.DictReader(open(SUMMARY / "scaling.csv")))
PS = [1, 2, 4, 8, 16]
MS = ["1", "8", "64"]
BUDGET = "1000000"


def pick(m, s):
    r = {int(x["threads"]): x for x in rows
         if x["playouts"] == m and x["series"] == s and x["budget"] == BUDGET}
    return r


def frame(ax, m):
    ax.set_xscale("log", base=2)
    ax.set_xticks(PS, PS)
    ax.set_xlabel("Threads (8 cores; 16 uses SMT)")
    ax.set_title(f"m = {m} rollout{'s' if m != '1' else ''} per leaf")


def panels(name, col, ylabel, ideal, ylog):
    fig, axes = plt.subplots(1, 3, figsize=(7.0, 2.5), sharey=ylog is not None)
    for ax, m in zip(axes, MS):
        frame(ax, m)
        if ideal:
            ax.plot(PS[:4], [ideal(p) for p in PS[:4]], color="#555555", lw=0.8, ls=":",
                    label="Ideal")
        for s in SERIES:
            r = pick(m, s)
            x = [p for p in PS if p in r]
            y = [float(r[p][col + "_med"]) for p in x]
            lo = [float(r[p][col + "_q1"]) for p in x]
            hi = [float(r[p][col + "_q3"]) for p in x]
            ax.plot(x, y, color=COLOR[s], marker=MARK[s], label=NAME[s])
            ax.fill_between(x, lo, hi, color=COLOR[s], alpha=0.12, lw=0)
        if ylog:
            ax.set_yscale("log", base=2)
    axes[0].set_ylabel(ylabel)
    h, l = axes[0].get_legend_handles_labels()
    fig.legend(h, l, loc="outside upper center", ncol=4, frameon=False)
    save(fig, name)


panels("fig2_speedup", "speedup", "Speedup over sequential", lambda p: p, True)
panels("fig3_efficiency", "efficiency", "Parallel efficiency", lambda p: 1, None)

fig, axes = plt.subplots(1, 3, figsize=(7.0, 2.3), sharey=True)
for ax, m in zip(axes, MS):
    frame(ax, m)
    for s in ["F64_CAS", "F32_CAS", "BF16_CAS", "F64_LOCK"]:
        r = pick(m, s)
        x = [p for p in PS if p in r]
        col = "lock_waits_per_update" if s == "F64_LOCK" else "cas_retries_per_update"
        ax.plot(x, [float(r[p][col]) for p in x], color=COLOR[s], marker=MARK[s],
                label=NAME[s] + (" (lock waits)" if s == "F64_LOCK" else " (retries)"))
axes[0].set_ylabel("Events per reward update")
h, l = axes[0].get_legend_handles_labels()
fig.legend(h, l, loc="outside upper center", ncol=4, frameon=False)
save(fig, "fig4_contention")
