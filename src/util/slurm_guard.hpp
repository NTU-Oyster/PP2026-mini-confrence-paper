// Refuse to run on a cluster login node: if Slurm is installed but we are not inside
// a job, exit at once. On machines without Slurm this does nothing.
#pragma once
#include <unistd.h>

#include <cstdio>
#include <cstdlib>

namespace pm {
struct SlurmGuard {
    SlurmGuard() {
        if (!std::getenv("SLURM_JOB_ID") && access("/usr/bin/sbatch", X_OK) == 0) {
            std::fprintf(stderr, "refusing to run outside a Slurm job (use srun/sbatch)\n");
            std::_Exit(2);
        }
    }
};
inline const SlurmGuard slurm_guard;
}  // namespace pm
