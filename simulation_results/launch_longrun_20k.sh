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

# OpenMP environment matching the benchmark runner
export OMP_NUM_THREADS=8
export OMP_PROC_BIND=close
export OMP_WAIT_POLICY=passive
export OMP_DYNAMIC=false

# No wall-clock cap: run indefinitely (beyond 24h) until it completes or is stopped manually.
# Pinned to all 8 cores. run.log lives OUTSIDE the output dir because the binary
# wipes --output-dir at startup.
nohup taskset -c 0-7 \
    /usr/bin/time -v ./simucell3d \
    --schedule=adaptive \
    --output-dir="$OUT" \
    --diagnostics-csv="$OUT/performance_diagnostics.csv" \
    "$PARAM" \
    > "$LOGDIR/run.log" 2>&1 &

PID=$!
echo "$PID" > "$LOGDIR/sim.pid"
disown
echo "LAUNCHED_WRAPPER_PID=$PID"
