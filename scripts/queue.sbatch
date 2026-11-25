#!/usr/bin/env bash
#SBATCH -J ismcts
#SBATCH -c 16
#SBATCH --mem=16G
#SBATCH -t 60
#SBATCH -d singleton
#SBATCH -o results/raw/slurm-%j.out
# Submit from the repository root: sbatch scripts/queue.sbatch [list]
./scripts/record_env.sh
./scripts/run_queue.sh "${1:-all}"
