#!/usr/bin/env bash
# oom_monitor.sh — Monitor a process for memory leaks with OOM detection
#
# Usage: ./oom_monitor.sh <PID> [--threshold-mb N] [--interval-sec N] [--output FILE]
#
# Tracks RSS/VSZ over time and alerts if memory exceeds threshold.

set -euo pipefail

PID=""
THRESHOLD_MB=2048
INTERVAL_SEC=10
OUTPUT_FILE=""

usage() {
    echo "Usage: $0 <PID> [--threshold-mb N] [--interval-sec N] [--output FILE]"
    echo ""
    echo "Options:"
    echo "  --threshold-mb N   Alert if RSS exceeds N MB (default: 2048)"
    echo "  --interval-sec N   Check interval in seconds (default: 10)"
    echo "  --output FILE      Write CSV to FILE (default: stdout)"
    exit 1
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --threshold-mb) THRESHOLD_MB="$2"; shift 2 ;;
        --interval-sec) INTERVAL_SEC="$2"; shift 2 ;;
        --output) OUTPUT_FILE="$2"; shift 2 ;;
        --help|-h) usage ;;
        *) PID="$1"; shift ;;
    esac
done

if [[ -z "$PID" ]]; then
    echo "Error: PID required"
    usage
fi

if ! kill -0 "$PID" 2>/dev/null; then
    echo "Error: Process $PID does not exist"
    exit 1
fi

THRESHOLD_KB=$((THRESHOLD_MB * 1024))

# Header
HEADER="timestamp_s,rss_kb,vsz_kb,rss_mb"
if [[ -n "$OUTPUT_FILE" ]]; then
    echo "$HEADER" > "$OUTPUT_FILE"
else
    echo "$HEADER"
fi

START_TIME=$(date +%s)
INITIAL_RSS=""

while kill -0 "$PID" 2>/dev/null; do
    ELAPSED=$(( $(date +%s) - START_TIME ))

    if RSS_VSZ=$(ps -o rss=,vsz= -p "$PID" 2>/dev/null); then
        RSS=$(echo "$RSS_VSZ" | awk '{print $1}')
        VSZ=$(echo "$RSS_VSZ" | awk '{print $2}')
        RSS_MB=$((RSS / 1024))

        LINE="${ELAPSED},${RSS},${VSZ},${RSS_MB}"

        if [[ -n "$OUTPUT_FILE" ]]; then
            echo "$LINE" >> "$OUTPUT_FILE"
        else
            echo "$LINE"
        fi

        # Track initial RSS for growth detection
        if [[ -z "$INITIAL_RSS" ]]; then
            INITIAL_RSS="$RSS"
        fi

        # OOM threshold check
        if [[ "$RSS" -gt "$THRESHOLD_KB" ]]; then
            echo "ALERT: RSS=${RSS_MB}MB exceeds threshold ${THRESHOLD_MB}MB at ${ELAPSED}s" >&2
            echo "Consider killing process $PID" >&2
        fi

        # Growth rate check (warn if >2x initial)
        if [[ "$INITIAL_RSS" -gt 0 && "$RSS" -gt $((INITIAL_RSS * 2)) ]]; then
            echo "WARNING: RSS has doubled since start (${INITIAL_RSS}KB -> ${RSS}KB)" >&2
        fi
    fi

    sleep "$INTERVAL_SEC"
done

echo "Process $PID exited. Monitoring complete." >&2
