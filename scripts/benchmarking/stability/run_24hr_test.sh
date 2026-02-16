#!/usr/bin/env bash
# run_24hr_test.sh — Run SimuCell3D for 24 hours and monitor stability
#
# Usage: ./run_24hr_test.sh <parameter_file> [--duration-hours N] [--build-dir DIR]
#
# Runs a long-duration simulation with periodic memory and stability checks.
# Outputs a summary report at the end.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-./build}"
DURATION_HOURS=24
PARAM_FILE=""
LOG_DIR="/tmp/simucell3d_24hr_test_$$"
CHECK_INTERVAL=300  # seconds between health checks

usage() {
    echo "Usage: $0 <parameter_file> [--duration-hours N] [--build-dir DIR]"
    echo ""
    echo "Options:"
    echo "  --duration-hours N   Test duration in hours (default: 24)"
    echo "  --build-dir DIR      Build directory (default: ./build)"
    exit 1
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --duration-hours) DURATION_HOURS="$2"; shift 2 ;;
        --build-dir) BUILD_DIR="$2"; shift 2 ;;
        --help|-h) usage ;;
        *) PARAM_FILE="$1"; shift ;;
    esac
done

if [[ -z "$PARAM_FILE" ]]; then
    echo "Error: parameter file required"
    usage
fi

if [[ ! -f "$BUILD_DIR/simucell3d" ]]; then
    echo "Error: $BUILD_DIR/simucell3d not found. Build first."
    exit 1
fi

if [[ ! -f "$PARAM_FILE" ]]; then
    echo "Error: Parameter file '$PARAM_FILE' not found."
    exit 1
fi

mkdir -p "$LOG_DIR"
DURATION_SECS=$((DURATION_HOURS * 3600))

echo "=== SimuCell3D 24-Hour Stability Test ==="
echo "Parameter file: $PARAM_FILE"
echo "Duration: ${DURATION_HOURS}h (${DURATION_SECS}s)"
echo "Build dir: $BUILD_DIR"
echo "Log dir: $LOG_DIR"
echo "Started: $(date -Iseconds)"
echo ""

# Start simulation in background
"$BUILD_DIR/simucell3d" "$PARAM_FILE" \
    > "$LOG_DIR/simulation.log" 2>&1 &
SIM_PID=$!

echo "Simulation PID: $SIM_PID"
echo "$SIM_PID" > "$LOG_DIR/sim.pid"

# Memory tracking file
echo "timestamp_s,rss_kb,vsz_kb" > "$LOG_DIR/memory.csv"

# Cleanup function
cleanup() {
    if kill -0 "$SIM_PID" 2>/dev/null; then
        echo "Stopping simulation (PID $SIM_PID)..."
        kill "$SIM_PID" 2>/dev/null || true
        wait "$SIM_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT

# Monitor loop
START_TIME=$(date +%s)
CHECK_NUM=0

while true; do
    ELAPSED=$(( $(date +%s) - START_TIME ))

    # Check if duration exceeded
    if [[ $ELAPSED -ge $DURATION_SECS ]]; then
        echo "[$(date -Iseconds)] Duration reached (${DURATION_HOURS}h). Stopping."
        break
    fi

    # Check if process is still alive
    if ! kill -0 "$SIM_PID" 2>/dev/null; then
        wait "$SIM_PID" || EXIT_CODE=$?
        echo "[$(date -Iseconds)] Simulation exited with code ${EXIT_CODE:-0} after ${ELAPSED}s"
        break
    fi

    # Record memory usage
    if RSS_VSZ=$(ps -o rss=,vsz= -p "$SIM_PID" 2>/dev/null); then
        RSS=$(echo "$RSS_VSZ" | awk '{print $1}')
        VSZ=$(echo "$RSS_VSZ" | awk '{print $2}')
        echo "${ELAPSED},${RSS},${VSZ}" >> "$LOG_DIR/memory.csv"

        CHECK_NUM=$((CHECK_NUM + 1))
        if [[ $((CHECK_NUM % 12)) -eq 0 ]]; then
            # Print status every hour
            RSS_MB=$((RSS / 1024))
            echo "[$(date -Iseconds)] Running: ${ELAPSED}s elapsed, RSS=${RSS_MB}MB"
        fi
    fi

    sleep "$CHECK_INTERVAL"
done

# Generate summary report
echo ""
echo "=== Summary Report ==="
echo "Ended: $(date -Iseconds)"
echo "Total runtime: ${ELAPSED}s"

if [[ -f "$LOG_DIR/memory.csv" ]]; then
    # Compute peak memory from CSV
    PEAK_RSS=$(tail -n +2 "$LOG_DIR/memory.csv" | cut -d, -f2 | sort -n | tail -1)
    PEAK_VSZ=$(tail -n +2 "$LOG_DIR/memory.csv" | cut -d, -f3 | sort -n | tail -1)
    FIRST_RSS=$(tail -n +2 "$LOG_DIR/memory.csv" | head -1 | cut -d, -f2)
    LAST_RSS=$(tail -n +2 "$LOG_DIR/memory.csv" | tail -1 | cut -d, -f2)

    if [[ -n "$PEAK_RSS" ]]; then
        echo "Peak RSS: $((PEAK_RSS / 1024)) MB"
        echo "Peak VSZ: $((PEAK_VSZ / 1024)) MB"
        if [[ -n "$FIRST_RSS" && -n "$LAST_RSS" && "$FIRST_RSS" -gt 0 ]]; then
            GROWTH=$(( (LAST_RSS - FIRST_RSS) * 100 / FIRST_RSS ))
            echo "RSS growth: ${GROWTH}%"
            if [[ $GROWTH -gt 20 ]]; then
                echo "WARNING: Memory growth >20% — possible memory leak"
            fi
        fi
    fi
fi

echo "Logs: $LOG_DIR/"
echo "Memory CSV: $LOG_DIR/memory.csv"
echo "Simulation log: $LOG_DIR/simulation.log"
