"""Shared loading, statistics and plot style. Standard library + numpy only."""
import csv
import glob
import pathlib

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parent.parent
RAW = ROOT / "results" / "raw"
SUMMARY = ROOT / "results" / "summary"
FIG = ROOT / "paper" / "figures"

# Fixed order and colour per series: colour follows the entity in every figure.
SERIES = ["F64_LOCK", "F64_CAS", "F32_CAS", "FIX64", "BF16_CAS", "root", "pimc"]
COLOR = dict(zip(SERIES, ["#2a78d6", "#eb6834", "#1baf7a", "#eda100", "#e87ba4", "#008300",
                          "#6250d6"]))
MARK = dict(zip(SERIES, ["o", "s", "^", "D", "v", "P", "X"]))
NAME = {"F64_LOCK": "F64 lock", "F64_CAS": "F64 CAS", "F32_CAS": "F32 CAS", "FIX64": "FIX64",
        "BF16_CAS": "BF16 CAS", "root": "Root-parallel", "pimc": "PIMC"}


def load(pattern):
    """All rows of the matching raw CSVs, as dicts of strings."""
    rows = []
    for path in sorted(glob.glob(str(RAW / pattern))):
        with open(path) as f:
            rows += list(csv.DictReader(f))
    return rows


def series_of(row):
    """Series key of a scaling row: the format for tree searches, else the scheme."""
    return row["format"] if row["scheme"] == "tree" else row["scheme"]


def median_iqr(x):
    q = np.percentile(x, [25, 50, 75])
    return q[1], q[0], q[2]


def boot_ci(per_state, rng, n=2000):
    """Mean and 95% percentile bootstrap interval, resampling states.
    per_state: (states,) array of state means."""
    per_state = np.asarray(per_state, float)
    idx = rng.integers(0, len(per_state), (n, len(per_state)))
    means = per_state[idx].mean(1)
    return per_state.mean(), *np.percentile(means, [2.5, 97.5])


def write_csv(name, rows):
    SUMMARY.mkdir(parents=True, exist_ok=True)
    with open(SUMMARY / name, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0]))
        w.writeheader()
        w.writerows(rows)
    print(name, len(rows))


def style():
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    plt.rcParams.update({
        "font.size": 8, "axes.labelsize": 8, "axes.titlesize": 8, "xtick.labelsize": 8,
        "ytick.labelsize": 8, "legend.fontsize": 8, "pdf.fonttype": 42,
        "axes.spines.top": False, "axes.spines.right": False, "axes.linewidth": 0.6,
        "axes.grid": True, "grid.color": "#dddddd", "grid.linewidth": 0.5,
        "axes.axisbelow": True, "lines.linewidth": 1.3, "lines.markersize": 4,
        "figure.constrained_layout.use": True,
    })
    return plt


def save(fig, name):
    FIG.mkdir(parents=True, exist_ok=True)
    fig.savefig(FIG / f"{name}.pdf")
    fig.savefig(FIG / f"{name}.png", dpi=150)
    print("wrote", name)
