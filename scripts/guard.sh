# Sourced by every script that runs programs.
if command -v sbatch >/dev/null 2>&1 && [ -z "${SLURM_JOB_ID:-}" ]; then
  echo "refusing to run outside a Slurm job (use srun/sbatch)" >&2
  exit 2
fi
