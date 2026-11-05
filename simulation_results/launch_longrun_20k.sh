#!/usr/bin/env bash
# Launch the 24h wall-clock-capped, adaptive-schedule SimuCell3D run that
# reproduces the ~20K-cell fast-growth benchmark (parallel_benchmark_20260124_091702).
set -euo pipefail

PROJECT=/home/nilesh-patil/projects/simucell3d
RUN=longrun_20k_fast_growth
OUT="$PROJECT/simulation_results/$RUN"
LOGDIR="$PROJECT/simulation_results/${RUN}_logs"
PARAM="$PROJECT/simulation_results/$RUN.xml"

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

# No wall-clock cap: run indefinitely (beyond 24h) until it completes or is stopped manually.
# run.log lives OUTSIDE the output dir because the binary wipes --output-dir at startup.
nohup /usr/bin/time -v ./simucell3d \
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
