#!/usr/bin/env python3
"""Generate the fixed decision-state set (thin wrapper around main_gen_states)."""
import argparse
import pathlib
import subprocess

ap = argparse.ArgumentParser()
ap.add_argument("--count", type=int, default=1000)
ap.add_argument("--seed", type=int, default=2026)
ap.add_argument("--out", default="results/states.txt")
a = ap.parse_args()
root = pathlib.Path(__file__).resolve().parent.parent
exe = root / "build" / "main_gen_states"
subprocess.run([str(exe), "--count", str(a.count), "--seed", str(a.seed), "--out", a.out],
               check=True, cwd=root)
