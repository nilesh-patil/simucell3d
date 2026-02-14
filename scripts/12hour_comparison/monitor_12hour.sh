#!/bin/bash
################################################################################
# 12-Hour Comparison Monitor Script
#
# Monitors progress of both simulations and displays periodic status updates
################################################################################

OUTPUT_DIR="$1"

if [ -z "$OUTPUT_DIR" ]; then
    echo "Usage: $0 <output_directory>"
    exit 1
fi

if [ ! -d "$OUTPUT_DIR" ]; then
    echo "ERROR: Output directory not found: $OUTPUT_DIR"
    exit 1
fi

V1_PID=$(cat "$OUTPUT_DIR/v1.pid" 2>/dev/null)
CURRENT_PID=$(cat "$OUTPUT_DIR/current.pid" 2>/dev/null)

if [ -z "$V1_PID" ] || [ -z "$CURRENT_PID" ]; then
    echo "ERROR: Could not read PID files"
    exit 1
fi

START_TIME=$(date +%s)
DURATION=43200  # 12 hours in seconds

echo "Starting monitor for 12-hour comparison"
echo "v1.0 PID: $V1_PID"
echo "Current PID: $CURRENT_PID"
echo "Start time: $(date)"
echo ""

# Counter for periodic detailed output
COUNTER=0

while true; do
    NOW=$(date +%s)
    ELAPSED=$((NOW - START_TIME))
    REMAINING=$((DURATION - ELAPSED))

    # Check if still running
    V1_RUNNING=$(kill -0 $V1_PID 2>/dev/null && echo "YES" || echo "NO")
    CURRENT_RUNNING=$(kill -0 $CURRENT_PID 2>/dev/null && echo "YES" || echo "NO")

    # Exit if both stopped
    if [[ "$V1_RUNNING" == "NO" && "$CURRENT_RUNNING" == "NO" ]]; then
        echo ""
        echo "==============================================================================="
        echo "Both simulations have completed!"
        echo "Completion time: $(date)"
        echo "Total elapsed: $((ELAPSED/3600))h $((ELAPSED%3600/60))m $((ELAPSED%60))s"
        echo "==============================================================================="
        break
    fi

    # Extract iterations from logs
    V1_ITER=$(grep -oP 'iteration: \K[0-9]+' "$OUTPUT_DIR/logs/v1.log" 2>/dev/null | tail -1)
    CURRENT_ITER=$(grep -oP 'Iteration \K[0-9]+' "$OUTPUT_DIR/logs/current.log" 2>/dev/null | tail -1)

    # Set defaults if not found
    V1_ITER=${V1_ITER:-0}
    CURRENT_ITER=${CURRENT_ITER:-0}

    # Read additional metrics from timeseries CSVs (if available)
    V1_CELLS=0
    V1_DIVS=0
    V1_PHASE="N/A"
    V1_COV=0
    V1_CPU=0
    CURRENT_CELLS=0
    CURRENT_DIVS=0
    CURRENT_PHASE="N/A"
    CURRENT_COV=0
    CURRENT_CPU=0

    # Parse v1 timeseries (CSV format: elapsed_sec,iteration,cells,divisions,iters_per_sec,phase,total_iteration_ms,mesh_ms,contact_ms,polar_ms,integ_ms,cov,thread_imbalance_pct,...)
    if [ -f "$OUTPUT_DIR/metrics/v1_timeseries.csv" ]; then
        V1_LAST=$(tail -1 "$OUTPUT_DIR/metrics/v1_timeseries.csv" 2>/dev/null)
        if [[ ! "$V1_LAST" =~ ^elapsed_sec ]]; then
            IFS=',' read -r _ _ V1_CELLS V1_DIVS _ V1_PHASE _ _ _ _ _ V1_COV _ _ _ _ _ _ V1_CPU _ <<< "$V1_LAST"
            V1_CELLS=${V1_CELLS:-0}
            V1_DIVS=${V1_DIVS:-0}
            V1_PHASE=${V1_PHASE:-"N/A"}
            V1_COV=${V1_COV:-0}
            V1_CPU=${V1_CPU:-0}
        fi
    fi

    # Parse current timeseries
    if [ -f "$OUTPUT_DIR/metrics/current_timeseries.csv" ]; then
        CURRENT_LAST=$(tail -1 "$OUTPUT_DIR/metrics/current_timeseries.csv" 2>/dev/null)
        if [[ ! "$CURRENT_LAST" =~ ^elapsed_sec ]]; then
            IFS=',' read -r _ _ CURRENT_CELLS CURRENT_DIVS _ CURRENT_PHASE _ _ _ _ _ CURRENT_COV _ _ _ _ _ _ CURRENT_CPU _ <<< "$CURRENT_LAST"
            CURRENT_CELLS=${CURRENT_CELLS:-0}
            CURRENT_DIVS=${CURRENT_DIVS:-0}
            CURRENT_PHASE=${CURRENT_PHASE:-"N/A"}
            CURRENT_COV=${CURRENT_COV:-0}
            CURRENT_CPU=${CURRENT_CPU:-0}
        fi
    fi

    # Calculate rates (iterations per second)
    if [ $ELAPSED -gt 0 ]; then
        V1_RATE=$(awk "BEGIN {printf \"%.2f\", $V1_ITER / $ELAPSED}")
        CURRENT_RATE=$(awk "BEGIN {printf \"%.2f\", $CURRENT_ITER / $ELAPSED}")
    else
        V1_RATE="0.00"
        CURRENT_RATE="0.00"
    fi

    # Calculate speedup
    if [ "$V1_ITER" -gt 0 ]; then
        SPEEDUP=$(awk "BEGIN {printf \"%.2f\", $CURRENT_ITER / $V1_ITER}")
    else
        SPEEDUP="N/A"
    fi

    # Estimate completion iterations
    if [ $REMAINING -gt 0 ]; then
        V1_PROJECTED=$(awk "BEGIN {printf \"%.0f\", $V1_ITER + ($V1_RATE * $REMAINING)}")
        CURRENT_PROJECTED=$(awk "BEGIN {printf \"%.0f\", $CURRENT_ITER + ($CURRENT_RATE * $REMAINING)}")
    else
        V1_PROJECTED=$V1_ITER
        CURRENT_PROJECTED=$CURRENT_ITER
    fi

    # Every 10 minutes (600 seconds) or on first iteration, show detailed output
    if [ $((COUNTER % 10)) -eq 0 ]; then
        clear
        echo "==============================================================================="
        echo "  12-Hour Comparison: v1.0 vs Current (Adaptive)"
        echo "  Time: $(date)"
        echo "==============================================================================="
        echo ""
        printf "%-15s %12s %12s %12s\n" "Metric" "v1.0" "Current" "Ratio"
        echo "-------------------------------------------------------------------------------"
        printf "%-15s %12s %12s %12s\n" "Status" "$V1_RUNNING" "$CURRENT_RUNNING" "-"
        printf "%-15s %12d %12d %12s\n" "Iterations" "$V1_ITER" "$CURRENT_ITER" "${SPEEDUP}x"
        printf "%-15s %12s %12s %12s\n" "Rate (it/s)" "$V1_RATE" "$CURRENT_RATE" "-"
        printf "%-15s %12d %12d %12s\n" "Cells" "$V1_CELLS" "$CURRENT_CELLS" "-"
        printf "%-15s %12d %12d %12s\n" "Divisions" "$V1_DIVS" "$CURRENT_DIVS" "-"
        printf "%-15s %12s %12s %12s\n" "Phase" "$V1_PHASE" "$CURRENT_PHASE" "-"
        printf "%-15s %12.3f %12.3f %12s\n" "CoV" "$V1_COV" "$CURRENT_COV" "-"
        printf "%-15s %12.1f %12.1f %12s\n" "CPU %" "$V1_CPU" "$CURRENT_CPU" "-"
        printf "%-15s %12d %12d %12s\n" "Projected" "$V1_PROJECTED" "$CURRENT_PROJECTED" "-"
        echo "-------------------------------------------------------------------------------"
        echo ""
        echo "Time elapsed:   $((ELAPSED/3600))h $((ELAPSED%3600/60))m $((ELAPSED%60))s / 12h 00m 00s"
        echo "Time remaining: $((REMAINING/3600))h $((REMAINING%3600/60))m $((REMAINING%60))s"
        echo ""
        echo "Progress: $(awk "BEGIN {printf \"%.1f\", ($ELAPSED / $DURATION) * 100}")% complete"
        echo ""

        # Show recent log entries
        echo "Recent v1.0 output:"
        tail -3 "$OUTPUT_DIR/logs/v1.log" 2>/dev/null | sed 's/^/  /'
        echo ""
        echo "Recent current output:"
        tail -3 "$OUTPUT_DIR/logs/current.log" 2>/dev/null | sed 's/^/  /'
        echo ""
        echo "==============================================================================="
    else
        # Brief status line every minute
        echo "[$(date +%H:%M:%S)] Elapsed: $((ELAPSED/3600))h$((ELAPSED%3600/60))m | v1.0: $V1_ITER it, $V1_CELLS cells ($V1_PHASE) | Current: $CURRENT_ITER it, $CURRENT_CELLS cells ($CURRENT_PHASE) | Speedup: ${SPEEDUP}x"
    fi

    COUNTER=$((COUNTER + 1))
    sleep 60  # Check every minute
done

# Final summary
echo ""
echo "Generating final summary..."

cat > "$OUTPUT_DIR/MONITOR_SUMMARY.txt" <<EOF
12-Hour Comparison Monitor Summary
==================================

Start time:      $(date -d "@$START_TIME")
End time:        $(date)
Total duration:  $((ELAPSED/3600))h $((ELAPSED%3600/60))m $((ELAPSED%60))s

Final Results:
  v1.0:
    Final iteration: $V1_ITER
    Average rate:    $V1_RATE it/s
    Status:          $V1_RUNNING

  Current:
    Final iteration: $CURRENT_ITER
    Average rate:    $CURRENT_RATE it/s
    Status:          $CURRENT_RUNNING

Performance:
  Speedup:         ${SPEEDUP}x

EOF

echo "Monitor summary saved to: $OUTPUT_DIR/MONITOR_SUMMARY.txt"
echo "Monitor script finished."
