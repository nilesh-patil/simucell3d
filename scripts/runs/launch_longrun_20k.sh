#!/usr/bin/env bash
# Launch the long-run growth simulation (parameters/longrun_fast_growth.xml) in the
# background with the adaptive schedule and a diagnostics CSV. It grows a tissue from one
# cell; the run continues until the simulation ends or the process is stopped.
#
# Usage: bash scripts/runs/launch_longrun_20k.sh     (THREADS=N to pass --threads=N)
# Output: simulation_results/longrun_20k_fast_growth/ (deleted at start),
#         log and pid in simulation_results/longrun_20k_fast_growth_logs/
set -euo pipefail

PROJECT="$(cd "$(dirname "$0")/../.." && pwd)"
RUN=longrun_20k_fast_growth
OUT="$PROJECT/simulation_results/$RUN"
LOGDIR="$PROJECT/simulation_results/${RUN}_logs"
PARAM="$PROJECT/parameters/longrun_fast_growth.xml"

# Make sure no stale instance is running
pkill -9 -f "simucell3d --schedule=adaptive --output-dir=$OUT" 2>/dev/null || true
sleep 1

# Clean output dir + log dir for a fresh run
rm -rf "$OUT"/* "$LOGDIR"
mkdir -p "$OUT" "$LOGDIR"

cd "$PROJECT/build"

# Thread count and placement are chosen by the binary itself: one pinned thread per
# physical core the process is allowed to use (see include/cpu_topology.hpp).
# Do NOT set OMP_PROC_BIND/OMP_PLACES or taskset here: OMP_PROC_BIND=close without
# OMP_PLACES used to land 4 threads on 2 physical cores, and taskset would cap the
# core count on bigger machines. Override with THREADS=N to use hyperthreads.
export OMP_WAIT_POLICY=passive
export OMP_DYNAMIC=false
THREADS_FLAG=""
if [[ -n "${THREADS:-}" ]]; then THREADS_FLAG="--threads=$THREADS"; fi

# No wall-clock cap: the run continues until it completes or is stopped manually.
# run.log lives OUTSIDE the output dir because the binary wipes --output-dir at startup.
# stdbuf -oL -eL makes run.log line-buffered. Redirected to a file, stdout is otherwise
# flushed in ~4 KB blocks, i.e. 10-25 min apart at ~30 s/iteration, which looks like a stall.
nohup /usr/bin/time -v stdbuf -oL -eL ./simucell3d \
    --schedule=adaptive \
    --output-dir="$OUT" \
    --diagnostics-csv="$OUT/performance_diagnostics.csv" \
    $THREADS_FLAG \
    "$PARAM" \
    > "$LOGDIR/run.log" 2>&1 &

PID=$!
echo "$PID" > "$LOGDIR/sim.pid"
disown
echo "LAUNCHED_WRAPPER_PID=$PID"
