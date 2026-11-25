#!/usr/bin/env bash
# Diagnosis of tree-parallel scaling: rollouts per leaf (grain) and node alignment.
# Run inside a Slurm job: srun -t 20 -c 16 --mem=16G scripts/run_diag.sh
set -euo pipefail
cd "$(dirname "$0")/.."
JOBS=8 ./scripts/build.sh > /dev/null
cmake -S . -B build-al64 -DNODE_ALIGN=64 > /dev/null
cmake --build build-al64 -j 8 --target main_bench > /dev/null
export OMP_PROC_BIND=close OMP_PLACES=cores
mkdir -p results/diag
[ -f results/states.txt ] || ./build/main_gen_states
for m in 1 8 64; do
  for al in 8 64; do
    bin=build; [ $al = 64 ] && bin=build-al64
    ./$bin/main_bench --count 8 --rep-end 1 --budgets $((200000 / m)) --playouts $m \
      --schemes serial,tree:F64_LOCK,tree:F64_CAS,tree:FIX64,root \
      --threads 1,2,4,8 --out results/diag/playouts${m}_align${al}.csv
  done
done
