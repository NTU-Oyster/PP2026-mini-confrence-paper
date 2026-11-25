#!/usr/bin/env bash
# Usage: scripts/build.sh            -> build/       (release)
#        TSAN=1 scripts/build.sh     -> build-tsan/  (ThreadSanitizer)
set -euo pipefail
cd "$(dirname "$0")/.."
dir=build; opt=OFF
[[ "${TSAN:-0}" == 1 ]] && { dir=build-tsan; opt=ON; }
cmake -S . -B "$dir" -DTSAN=$opt
cmake --build "$dir" -j "${JOBS:-4}"
