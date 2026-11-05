#!/usr/bin/env bash
# Reproduce the iteration-~9640 segfault of the paper-config run under gdb,
# using a Release-equivalent binary built with debug symbols (build-dbg/).
# Prints all-thread backtraces on SIGSEGV; also enables core dumps as backup.
#
# Usage: bash simulation_results/launch_crash_repro.sh [build-dir] [run-name]
set -euo pipefail

PROJECT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${1:-build-dbg}"
RUN="${2:-crash_repro_dbg}"
OUT="$PROJECT/simulation_results/$RUN"
LOGDIR="$PROJECT/simulation_results/${RUN}_logs"
PARAM="$PROJECT/simulation_results/longrun_20k_fast_growth.xml"

pkill -9 -f "simucell3d --schedule=adaptive --output-dir=$OUT" 2>/dev/null || true
sleep 1
rm -rf "$OUT"/* "$LOGDIR"
mkdir -p "$OUT" "$LOGDIR"

cd "$PROJECT/$BUILD_DIR"

# Same OpenMP environment as the production launcher: the binary picks thread count and
# placement itself (one pinned thread per physical core); THREADS=N overrides.
export OMP_WAIT_POLICY=passive
export OMP_DYNAMIC=false
THREADS_FLAG=""
if [[ -n "${THREADS:-}" ]]; then THREADS_FLAG="--threads=$THREADS"; fi
# Let ASan builds report and abort on the first error, and keep going past leaks
export ASAN_OPTIONS=abort_on_error=1:detect_leaks=0:halt_on_error=1
export UBSAN_OPTIONS=print_stacktrace=1

ulimit -c unlimited

nohup gdb -batch -q \
        -ex "set pagination off" \
        -ex "set print thread-events off" \
        -ex "handle SIGUSR1 SIGUSR2 nostop noprint" \
        -ex "run" \
        -ex "echo \n=== CRASH: info ===\n" \
        -ex "info signal SIGSEGV" \
        -ex "echo \n=== CRASH: current thread backtrace ===\n" \
        -ex "bt full 15" \
        -ex "echo \n=== CRASH: all threads ===\n" \
        -ex "thread apply all bt 25" \
        -ex "echo \n=== CRASH: generating core ===\n" \
        -ex "generate-core-file $LOGDIR/core" \
        --args ./simucell3d \
            --schedule=adaptive \
            --output-dir="$OUT" \
            --diagnostics-csv="$OUT/performance_diagnostics.csv" \
            $THREADS_FLAG \
            "$PARAM" \
    > "$LOGDIR/run.log" 2>&1 &

PID=$!
echo "$PID" > "$LOGDIR/sim.pid"
disown
echo "LAUNCHED_GDB_PID=$PID"
