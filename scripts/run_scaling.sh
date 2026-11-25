#!/usr/bin/env bash
# Inside a Slurm job (or on a machine without Slurm). Resumable; see worklist.py.
exec "$(dirname "$0")/run_queue.sh" scaling
