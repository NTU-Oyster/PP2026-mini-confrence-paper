#!/usr/bin/env bash
# Run pending shards of an experiment list until LIMIT seconds have passed.
# Usage (inside a Slurm job): scripts/run_queue.sh {scaling|quality|match|all}
set -uo pipefail
cd "$(dirname "$0")/.."
source scripts/guard.sh
LIMIT=${LIMIT:-3300}
export OMP_PROC_BIND=close OMP_PLACES=cores
start=$SECONDS
mkdir -p results/raw/scaling results/raw/quality results/raw/match
[ -f results/states.txt ] || ./build/main_gen_states
pending=0
while IFS=$'\t' read -r est out cmd; do
  [ -f "$out" ] && continue
  if (( SECONDS - start + est > LIMIT )); then pending=1; continue; fi
  t0=$SECONDS
  load0=$(cut -d' ' -f1 /proc/loadavg)
  # A shard counts as done only once it has finished cleanly and been renamed.
  if $cmd --out "$out.part" < /dev/null; then
    mv "$out.part" "$out"
    echo "$(date +%F_%T) ok $((SECONDS - t0))s load $load0->$(cut -d' ' -f1 /proc/loadavg) cpus $(grep Cpus_allowed_list /proc/self/status | cut -f2) $out" >> results/raw/queue.log
  else
    echo "$(date +%F_%T) FAILED $out" >> results/raw/queue.log
  fi
done < <(python3 scripts/worklist.py "${1:-all}")
echo "pending=$pending after $((SECONDS - start))s"
