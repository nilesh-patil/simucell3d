#!/bin/bash
################################################################################
# Post-Run Analysis Script for 12-Hour Comparison
#
# Extracts metrics and generates comprehensive comparison summary
#
# Usage: ./analyze_results.sh <output_directory>
################################################################################

OUTPUT_DIR="$1"

if [ -z "$OUTPUT_DIR" ]; then
    echo "Usage: $0 <output_directory>"
    echo ""
    echo "Example:"
    echo "  ./analyze_results.sh /home/nilesh-patil/projects/version-cpp-next/doc/working/12hour_comparison_YYYYMMDD_HHMMSS"
    exit 1
fi

if [ ! -d "$OUTPUT_DIR" ]; then
    echo "ERROR: Output directory not found: $OUTPUT_DIR"
    exit 1
fi

echo "Analyzing 12-hour comparison results..."
echo "Output directory: $OUTPUT_DIR"
echo ""

# Extract metrics from logs
echo "Extracting iteration counts..."
V1_FINAL_ITER=$(grep -oP 'iteration: \K[0-9]+' "$OUTPUT_DIR/logs/v1.log" 2>/dev/null | tail -1)
CURRENT_FINAL_ITER=$(grep -oP 'Iteration \K[0-9]+' "$OUTPUT_DIR/logs/current.log" 2>/dev/null | tail -1)

if [ -z "$V1_FINAL_ITER" ]; then
    V1_FINAL_ITER="N/A"
    echo "  WARNING: Could not extract v1.0 iteration count"
else
    echo "  v1.0: $V1_FINAL_ITER iterations"
fi

if [ -z "$CURRENT_FINAL_ITER" ]; then
    CURRENT_FINAL_ITER="N/A"
    echo "  WARNING: Could not extract current iteration count"
else
    echo "  Current: $CURRENT_FINAL_ITER iterations"
fi

# Extract timing metrics from /usr/bin/time output
echo ""
echo "Extracting timing metrics..."
V1_WALL_TIME=$(grep "Elapsed (wall clock)" "$OUTPUT_DIR/logs/v1.log" 2>/dev/null | grep -oP '\d+:\d+:\d+' | tail -1)
CURRENT_WALL_TIME=$(grep "Elapsed (wall clock)" "$OUTPUT_DIR/logs/current.log" 2>/dev/null | grep -oP '\d+:\d+:\d+' | tail -1)

V1_CPU_PERCENT=$(grep "Percent of CPU" "$OUTPUT_DIR/logs/v1.log" 2>/dev/null | grep -oP '\d+' | tail -1)
CURRENT_CPU_PERCENT=$(grep "Percent of CPU" "$OUTPUT_DIR/logs/current.log" 2>/dev/null | grep -oP '\d+' | tail -1)

# Memory metrics (in KB)
V1_MAX_MEM=$(grep "Maximum resident set size" "$OUTPUT_DIR/logs/v1.log" 2>/dev/null | grep -oP '\d+' | tail -1)
CURRENT_MAX_MEM=$(grep "Maximum resident set size" "$OUTPUT_DIR/logs/current.log" 2>/dev/null | grep -oP '\d+' | tail -1)

# Convert to MB
if [ -n "$V1_MAX_MEM" ]; then
    V1_MAX_MEM_MB=$(awk "BEGIN {printf \"%.0f\", $V1_MAX_MEM / 1024}")
else
    V1_MAX_MEM_MB="N/A"
fi

if [ -n "$CURRENT_MAX_MEM" ]; then
    CURRENT_MAX_MEM_MB=$(awk "BEGIN {printf \"%.0f\", $CURRENT_MAX_MEM / 1024}")
else
    CURRENT_MAX_MEM_MB="N/A"
fi

# Calculate speedup
if [ "$V1_FINAL_ITER" != "N/A" ] && [ "$CURRENT_FINAL_ITER" != "N/A" ] && [ "$V1_FINAL_ITER" -gt 0 ]; then
    SPEEDUP=$(awk "BEGIN {printf \"%.2f\", $CURRENT_FINAL_ITER / $V1_FINAL_ITER}")
    SPEEDUP_PERCENT=$(awk "BEGIN {printf \"%.1f\", (($CURRENT_FINAL_ITER / $V1_FINAL_ITER) - 1) * 100}")
else
    SPEEDUP="N/A"
    SPEEDUP_PERCENT="N/A"
fi

# Calculate iteration rates
if [ "$V1_WALL_TIME" != "" ] && [ "$V1_FINAL_ITER" != "N/A" ]; then
    # Convert wall time to seconds
    V1_SECONDS=$(echo "$V1_WALL_TIME" | awk -F: '{ print ($1 * 3600) + ($2 * 60) + $3 }')
    V1_RATE=$(awk "BEGIN {printf \"%.2f\", $V1_FINAL_ITER / $V1_SECONDS}")
else
    V1_RATE="N/A"
fi

if [ "$CURRENT_WALL_TIME" != "" ] && [ "$CURRENT_FINAL_ITER" != "N/A" ]; then
    CURRENT_SECONDS=$(echo "$CURRENT_WALL_TIME" | awk -F: '{ print ($1 * 3600) + ($2 * 60) + $3 }')
    CURRENT_RATE=$(awk "BEGIN {printf \"%.2f\", $CURRENT_FINAL_ITER / $CURRENT_SECONDS}")
else
    CURRENT_RATE="N/A"
fi

# Read configuration
START_TIME=$(grep "^Started:" "$OUTPUT_DIR/config.txt" 2>/dev/null | cut -d: -f2- | xargs)
if [ -z "$START_TIME" ]; then
    START_TIME="Unknown"
fi

# Generate comprehensive summary
echo ""
echo "Generating summary report..."

cat > "$OUTPUT_DIR/SUMMARY.md" <<EOF
# 12-Hour Performance Comparison: v1.0 vs Current

## Executive Summary

This report presents the results of a 12-hour performance comparison between SimuCell3D v1.0 and the current version (version-cpp-next) with adaptive OpenMP scheduling.

**Key Finding:** The current version achieved **${SPEEDUP}x speedup** (${SPEEDUP_PERCENT}% improvement) over v1.0.

---

## Configuration

| Parameter | Value |
|-----------|-------|
| **Parameter File** | parameters/progressive_scaling/parameters_vesicle_progbench_128k.xml |
| **Duration** | 12 hours (43,200 seconds) |
| **CPU Allocation** | v1.0: cores 0-7 (8 threads), current: cores 8-15 (8 threads) |
| **Scheduling** | v1.0: static (default), current: adaptive |
| **Start Time** | $START_TIME |

---

## Results

### Iteration Throughput

| Metric | v1.0 | Current | Speedup |
|--------|------|---------|---------|
| **Final Iteration** | $V1_FINAL_ITER | $CURRENT_FINAL_ITER | **${SPEEDUP}x** |
| **Wall Time** | $V1_WALL_TIME | $CURRENT_WALL_TIME | - |
| **Avg Rate (iter/sec)** | $V1_RATE | $CURRENT_RATE | - |

### Resource Usage

| Metric | v1.0 | Current |
|--------|------|---------|
| **CPU Utilization** | ${V1_CPU_PERCENT}% | ${CURRENT_CPU_PERCENT}% |
| **Max Memory (MB)** | $V1_MAX_MEM_MB | $CURRENT_MAX_MEM_MB |

---

## Interpretation

EOF

# Add interpretation based on speedup
if [ "$SPEEDUP" != "N/A" ]; then
    if (( $(echo "$SPEEDUP >= 1.5" | bc -l) )); then
        cat >> "$OUTPUT_DIR/SUMMARY.md" <<EOF
✅ **Strong Performance Improvement**

The current version demonstrates a significant **${SPEEDUP}x speedup** over v1.0, representing a **${SPEEDUP_PERCENT}% improvement** in iteration throughput. This validates the effectiveness of the adaptive OpenMP scheduling strategy combined with bug fixes.

### Contributing Factors

1. **Adaptive Scheduling**: Dynamic per-loop optimization selects optimal scheduling strategies based on workload characteristics
2. **BUG-001 Fix**: Corrected stale momentum in semi-implicit Euler integration
3. **BUG-003 Fix**: Fixed assert statement in node reset functionality
4. **Per-Loop Optimization**: Different computational phases use specialized scheduling (contact=dynamic, integration=guided, mesh=static)

EOF
    elif (( $(echo "$SPEEDUP >= 1.2" | bc -l) )); then
        cat >> "$OUTPUT_DIR/SUMMARY.md" <<EOF
✅ **Moderate Performance Improvement**

The current version shows a **${SPEEDUP}x speedup** (**${SPEEDUP_PERCENT}% improvement**) over v1.0. While not as dramatic as the expected 1.54x from benchmarks, this still represents meaningful progress.

### Possible Factors

- Longer runtime may show different scaling characteristics
- Different workload distribution over 12 hours vs 1 hour
- Adaptive scheduling benefits may vary with simulation phase
- Bug fixes contribute to correctness and stability

EOF
    else
        cat >> "$OUTPUT_DIR/SUMMARY.md" <<EOF
⚠️ **Lower Than Expected Improvement**

The current version shows a **${SPEEDUP}x speedup** (**${SPEEDUP_PERCENT}% improvement**), which is below the expected 1.54x from shorter benchmarks.

### Possible Causes

- Long-running simulation characteristics differ from short benchmarks
- Resource contention or system load variations
- Different simulation phases may benefit less from adaptive scheduling
- Recommend investigation into performance characteristics

EOF
    fi
else
    cat >> "$OUTPUT_DIR/SUMMARY.md" <<EOF
⚠️ **Unable to Calculate Speedup**

Missing iteration count data prevented speedup calculation. Check log files for errors.

EOF
fi

cat >> "$OUTPUT_DIR/SUMMARY.md" <<EOF
---

## Key Differences (24 commits since v1.0)

The current version includes:

- **Adaptive OpenMP Scheduling**: Intelligent base schedule selection + per-loop optimization
- **Bug Fixes**:
  - BUG-001: Stale momentum in semi-implicit Euler time integration
  - BUG-003: Assert statement in node reset functionality
- **Enhanced Testing**: 40+ comprehensive unit tests across all modules
- **Performance Monitoring**: Built-in diagnostics and profiling capabilities
- **Documentation**: Comprehensive performance tuning guides

---

## Files and Logs

- **v1.0 Log**: \`$OUTPUT_DIR/logs/v1.log\`
- **Current Log**: \`$OUTPUT_DIR/logs/current.log\`
- **Performance Diagnostics**: \`$OUTPUT_DIR/sim_current/performance_diagnostics.csv\`
- **Monitor Output**: \`$OUTPUT_DIR/monitor_output.log\`
- **Configuration**: \`$OUTPUT_DIR/config.txt\`

---

## Verification Steps

### Check for Errors

\`\`\`bash
# Search for errors in logs
grep -i "error\|segmentation\|abort" $OUTPUT_DIR/logs/v1.log
grep -i "error\|segmentation\|abort" $OUTPUT_DIR/logs/current.log
\`\`\`

### Verify Completion

\`\`\`bash
# Check if simulations ran to completion or timed out
tail -50 $OUTPUT_DIR/logs/v1.log
tail -50 $OUTPUT_DIR/logs/current.log
\`\`\`

### Analyze Diagnostics

\`\`\`bash
# View performance diagnostics from current version
head -20 $OUTPUT_DIR/sim_current/performance_diagnostics.csv
\`\`\`

---

**Report Generated**: $(date)
**Analysis Script**: scripts/12hour_comparison/analyze_results.sh

EOF

echo -e "\n✓ Summary report generated: $OUTPUT_DIR/SUMMARY.md"

# Run timeseries analysis if metrics exist
echo ""
if [ -f "$OUTPUT_DIR/metrics/v1_timeseries.csv" ] && [ -f "$OUTPUT_DIR/metrics/current_timeseries.csv" ]; then
    echo "Running timeseries analysis..."
    SCRIPT_DIR="$(dirname "$(readlink -f "$0")")"
    UTILS_DIR="$(dirname "$SCRIPT_DIR")/comparison_utils"

    if [ -f "$UTILS_DIR/analyze_timeseries.sh" ]; then
        "$UTILS_DIR/analyze_timeseries.sh" "$OUTPUT_DIR"
        echo ""
    else
        echo "  WARNING: Timeseries analysis script not found (optional)"
        echo "  Expected at: $UTILS_DIR/analyze_timeseries.sh"
        echo ""
    fi
else
    echo "  NOTE: Per-minute metrics not found (timeseries analysis skipped)"
    echo "        This is expected if the comparison was run before metrics daemon was added."
    echo ""
fi

echo "You can view the summary with:"
echo "  cat $OUTPUT_DIR/SUMMARY.md"
echo ""

# Display summary to terminal
echo "==============================================================================="
cat "$OUTPUT_DIR/SUMMARY.md"
echo "==============================================================================="
