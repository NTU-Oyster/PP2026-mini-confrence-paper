#!/usr/bin/env bash
# Record hardware and software environment into results/env/. Run inside a Slurm job.
set -u
cd "$(dirname "$0")/.."
mkdir -p results/env
out=results/env/env_$(date +%Y%m%d_%H%M%S).txt
{
  echo "date: $(date)"
  echo "kernel: $(uname -sr)"   # hostname deliberately omitted (double-blind)
  echo "allowed cpus: $(grep Cpus_allowed_list /proc/self/status | cut -f2)"
  env | grep -E '^(OMP|SLURM_(JOB_QOS|CPUS|MEM))' | sort
  echo "--- compiler"; c++ --version | head -1; cmake --version | head -1
  grep -E 'CMAKE_(CXX_FLAGS|BUILD_TYPE|CXX_FLAGS_RELEASE)(:|=)|NODE_ALIGN' build/CMakeCache.txt
  echo "--- lscpu"; lscpu
  echo "--- memory"; free -g
  echo "--- numa"; numactl -H 2>/dev/null
  echo "--- perf_event_paranoid: $(cat /proc/sys/kernel/perf_event_paranoid)"
} > "$out"
echo "$out"
