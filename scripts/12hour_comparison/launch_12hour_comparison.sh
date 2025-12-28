#!/bin/bash
################################################################################
# 12-Hour Performance Comparison: v1.0 vs Current (Adaptive)
#
# This script launches a 12-hour performance comparison between v1.0 and
# current version with 8 cores allocated to each simulation.
#
# Prerequisites:
#   - VM upscaled to 16+ CPU cores
#   - Both simucell3d binaries built in Release mode
#   - At least 10GB free disk space
#
# Usage:
#   ./launch_12hour_comparison.sh
#
################################################################################

set -e  # Exit on error

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}════════════════════════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}  12-Hour Performance Comparison: v1.0 vs Current (Adaptive)${NC}"
echo -e "${BLUE}════════════════════════════════════════════════════════════════════════════${NC}"
echo ""

# Configuration
DURATION_SECONDS=43200  # 12 hours
V1_CORES="0-7"
CURRENT_CORES="8-15"
V1_NUM_THREADS=8
CURRENT_NUM_THREADS=8
PARAM_FILE="parameters/progressive_scaling/parameters_vesicle_progbench_128k.xml"
MESH_FILE="data/input_meshes/fig_3_vesicle.vtk"

# Paths
PROJECT_ROOT="/home/nilesh-patil/projects/version-cpp-next"
V1_BUILD_DIR="/home/nilesh-patil/projects/version-cpp-v1.0/build"
CURRENT_BUILD_DIR="$PROJECT_ROOT/build"
OUTPUT_DIR="$PROJECT_ROOT/doc/working/12hour_comparison_$(date +%Y%m%d_%H%M%S)"

echo -e "${YELLOW}[1/9] Pre-flight checks...${NC}"

# Check CPU cores
NUM_CORES=$(nproc)
if [ "$NUM_CORES" -lt 16 ]; then
    echo -e "${RED}ERROR: Insufficient CPU cores!${NC}"
    echo "  Required: 16 cores (8 for v1.0 + 8 for current)"
    echo "  Available: $NUM_CORES cores"
    echo ""
    echo "Please upscale your VM before running this script."
    exit 1
fi
echo -e "${GREEN}  ✓ CPU cores: $NUM_CORES (sufficient for 8+8 allocation)${NC}"

# Check binaries exist
if [ ! -f "$V1_BUILD_DIR/simucell3d" ]; then
    echo -e "${RED}ERROR: v1.0 binary not found at $V1_BUILD_DIR/simucell3d${NC}"
    exit 1
fi
echo -e "${GREEN}  ✓ v1.0 binary found${NC}"

if [ ! -f "$CURRENT_BUILD_DIR/simucell3d" ]; then
    echo -e "${RED}ERROR: Current binary not found at $CURRENT_BUILD_DIR/simucell3d${NC}"
    exit 1
fi
echo -e "${GREEN}  ✓ Current binary found${NC}"

# Check Release builds
if ! grep -q "CMAKE_BUILD_TYPE:STRING=Release" "$V1_BUILD_DIR/CMakeCache.txt"; then
    echo -e "${RED}ERROR: v1.0 is not a Release build${NC}"
    exit 1
fi
echo -e "${GREEN}  ✓ v1.0 is Release build${NC}"

if ! grep -q "CMAKE_BUILD_TYPE:STRING=Release" "$CURRENT_BUILD_DIR/CMakeCache.txt"; then
    echo -e "${RED}ERROR: Current is not a Release build${NC}"
    exit 1
fi
echo -e "${GREEN}  ✓ Current is Release build${NC}"

# Check parameter file and mesh
if [ ! -f "$PROJECT_ROOT/$PARAM_FILE" ]; then
    echo -e "${RED}ERROR: Parameter file not found: $PARAM_FILE${NC}"
    exit 1
fi
echo -e "${GREEN}  ✓ Parameter file found${NC}"

if [ ! -f "$PROJECT_ROOT/$MESH_FILE" ]; then
    echo -e "${RED}ERROR: Mesh file not found: $MESH_FILE${NC}"
    exit 1
fi
echo -e "${GREEN}  ✓ Input mesh found${NC}"

# Check disk space (need at least 10GB)
FREE_SPACE_GB=$(df -BG "$PROJECT_ROOT" | tail -1 | awk '{print $4}' | sed 's/G//')
if [ "$FREE_SPACE_GB" -lt 10 ]; then
    echo -e "${YELLOW}WARNING: Low disk space (${FREE_SPACE_GB}GB free)${NC}"
    echo "  Recommendation: At least 10GB free"
    read -p "Continue anyway? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
else
    echo -e "${GREEN}  ✓ Disk space: ${FREE_SPACE_GB}GB free${NC}"
fi

echo ""
echo -e "${YELLOW}[2/9] Creating output directory structure...${NC}"
mkdir -p "$OUTPUT_DIR"/{logs,metrics/{v1,current},sim_{v1,current}}
echo -e "${GREEN}  ✓ Created: $OUTPUT_DIR${NC}"

# Save configuration
cat > "$OUTPUT_DIR/config.txt" <<EOF
12-Hour Performance Comparison: v1.0 vs Current (Adaptive)
===========================================================

Configuration:
  Parameter:     $PARAM_FILE
  Duration:      43200 seconds (12 hours)
  CPU Cores:     v1.0=$V1_CORES ($V1_NUM_THREADS threads), current=$CURRENT_CORES ($CURRENT_NUM_THREADS threads)
  Scheduling:    v1.0=static (default), current=adaptive

System:
  Total Cores:   $NUM_CORES
  Free Space:    ${FREE_SPACE_GB}GB

Start Time:      $(date)
Expected End:    $(date -d '+12 hours')

Paths:
  v1.0 binary:   $V1_BUILD_DIR/simucell3d
  Current binary: $CURRENT_BUILD_DIR/simucell3d
  Output dir:    $OUTPUT_DIR
EOF

echo ""
echo -e "${YELLOW}[3/9] Preparing parameter files with absolute paths...${NC}"

# Create parameter files with absolute paths
MESH_ABS_PATH="$PROJECT_ROOT/$MESH_FILE"
cp "$PROJECT_ROOT/$PARAM_FILE" /tmp/params_v1_12hour.xml
cp "$PROJECT_ROOT/$PARAM_FILE" /tmp/params_current_12hour.xml

# Update paths for v1.0
sed -i "s|<input_mesh_file_path>.*</input_mesh_file_path>|<input_mesh_file_path>$MESH_ABS_PATH</input_mesh_file_path>|g" /tmp/params_v1_12hour.xml
sed -i "s|<output_mesh_folder_path>.*</output_mesh_folder_path>|<output_mesh_folder_path>$OUTPUT_DIR/sim_v1</output_mesh_folder_path>|g" /tmp/params_v1_12hour.xml

# Update paths for current
sed -i "s|<input_mesh_file_path>.*</input_mesh_file_path>|<input_mesh_file_path>$MESH_ABS_PATH</input_mesh_file_path>|g" /tmp/params_current_12hour.xml
sed -i "s|<output_mesh_folder_path>.*</output_mesh_folder_path>|<output_mesh_folder_path>$OUTPUT_DIR/sim_current</output_mesh_folder_path>|g" /tmp/params_current_12hour.xml

echo -e "${GREEN}  ✓ Parameter files prepared${NC}"

echo ""
echo -e "${YELLOW}[4/9] Launching v1.0 simulation (cores $V1_CORES)...${NC}"

cd "$V1_BUILD_DIR"
export OMP_NUM_THREADS=$V1_NUM_THREADS
export OMP_WAIT_POLICY=passive
export OMP_DYNAMIC=false
export OMP_PROC_BIND=close
export OMP_SCHEDULE="static,100"

nohup timeout $DURATION_SECONDS taskset -c $V1_CORES \
  /usr/bin/time -v ./simucell3d /tmp/params_v1_12hour.xml \
  > "$OUTPUT_DIR/logs/v1.log" 2>&1 &

V1_PID=$!
echo $V1_PID > "$OUTPUT_DIR/v1.pid"
echo -e "${GREEN}  ✓ v1.0 launched: PID=$V1_PID${NC}"

# Wait for startup
sleep 3

# Verify it's running
if ! kill -0 $V1_PID 2>/dev/null; then
    echo -e "${RED}ERROR: v1.0 simulation failed to start!${NC}"
    echo "Check logs: tail $OUTPUT_DIR/logs/v1.log"
    exit 1
fi

echo ""
echo -e "${YELLOW}[5/9] Launching current version simulation (cores $CURRENT_CORES)...${NC}"

cd "$CURRENT_BUILD_DIR"
export OMP_NUM_THREADS=$CURRENT_NUM_THREADS
export OMP_WAIT_POLICY=passive
export OMP_DYNAMIC=false
export OMP_PROC_BIND=close

nohup timeout $DURATION_SECONDS taskset -c $CURRENT_CORES \
  /usr/bin/time -v ./simucell3d \
  --schedule=adaptive \
  --output-dir="$OUTPUT_DIR/sim_current" \
  --diagnostics-csv="$OUTPUT_DIR/sim_current/performance_diagnostics.csv" \
  /tmp/params_current_12hour.xml \
  > "$OUTPUT_DIR/logs/current.log" 2>&1 &

CURRENT_PID=$!
echo $CURRENT_PID > "$OUTPUT_DIR/current.pid"
echo -e "${GREEN}  ✓ Current launched: PID=$CURRENT_PID${NC}"

# Wait for startup
sleep 3

# Verify it's running
if ! kill -0 $CURRENT_PID 2>/dev/null; then
    echo -e "${RED}ERROR: Current simulation failed to start!${NC}"
    echo "Check logs: tail $OUTPUT_DIR/logs/current.log"
    # Kill v1.0 if current failed
    kill $V1_PID 2>/dev/null
    exit 1
fi

echo ""
echo -e "${YELLOW}[6/9] Starting metrics collection daemon...${NC}"

# Source sampling utilities and launch metrics daemon
SCRIPT_DIR="$(dirname "$(readlink -f "$0")")"
UTILS_DIR="$(dirname "$SCRIPT_DIR")/comparison_utils"

if [ -f "$UTILS_DIR/metrics_daemon.sh" ]; then
    nohup "$UTILS_DIR/metrics_daemon.sh" "$OUTPUT_DIR" > "$OUTPUT_DIR/logs/metrics_daemon.log" 2>&1 &
    METRICS_PID=$!
    echo $METRICS_PID > "$OUTPUT_DIR/metrics.pid"
    echo -e "${GREEN}  ✓ Metrics daemon launched: PID=$METRICS_PID${NC}"
    echo -e "${GREEN}    Per-minute metrics will be saved to: $OUTPUT_DIR/metrics/${NC}"
else
    echo -e "${YELLOW}  ⚠ Metrics daemon script not found (optional)${NC}"
    echo -e "${YELLOW}    Expected at: $UTILS_DIR/metrics_daemon.sh${NC}"
fi

echo ""
echo -e "${YELLOW}[7/9] Verifying simulations are running...${NC}"

sleep 5

# Check processes
V1_RUNNING=$(kill -0 $V1_PID 2>/dev/null && echo "YES" || echo "NO")
CURRENT_RUNNING=$(kill -0 $CURRENT_PID 2>/dev/null && echo "YES" || echo "NO")

if [ "$V1_RUNNING" != "YES" ]; then
    echo -e "${RED}ERROR: v1.0 simulation not running!${NC}"
    tail -20 "$OUTPUT_DIR/logs/v1.log"
    exit 1
fi

if [ "$CURRENT_RUNNING" != "YES" ]; then
    echo -e "${RED}ERROR: Current simulation not running!${NC}"
    tail -20 "$OUTPUT_DIR/logs/current.log"
    kill $V1_PID 2>/dev/null
    exit 1
fi

echo -e "${GREEN}  ✓ Both simulations running${NC}"

# Check CPU affinity
echo ""
echo "  CPU Affinity:"
V1_SIMUCELL_PID=$(pgrep -P $V1_PID | head -1)
if [ -n "$V1_SIMUCELL_PID" ]; then
    AFFINITY=$(taskset -cp $V1_SIMUCELL_PID 2>/dev/null | grep -oP 'list: \K.*')
    echo "    v1.0:    cores $AFFINITY"
fi

CURRENT_SIMUCELL_PID=$(pgrep -P $CURRENT_PID | head -1)
if [ -n "$CURRENT_SIMUCELL_PID" ]; then
    AFFINITY=$(taskset -cp $CURRENT_SIMUCELL_PID 2>/dev/null | grep -oP 'list: \K.*')
    echo "    Current: cores $AFFINITY"
fi

# Check logs have output
echo ""
echo "  Log files:"
V1_LOG_LINES=$(wc -l < "$OUTPUT_DIR/logs/v1.log")
CURRENT_LOG_LINES=$(wc -l < "$OUTPUT_DIR/logs/current.log")
echo "    v1.0:    $V1_LOG_LINES lines"
echo "    Current: $CURRENT_LOG_LINES lines"

echo ""
echo -e "${YELLOW}[8/9] Starting monitoring...${NC}"

# Launch monitor script
SCRIPT_DIR="$(dirname "$(readlink -f "$0")")"
if [ -f "$SCRIPT_DIR/monitor_12hour.sh" ]; then
    nohup "$SCRIPT_DIR/monitor_12hour.sh" "$OUTPUT_DIR" > "$OUTPUT_DIR/monitor_output.log" 2>&1 &
    MONITOR_PID=$!
    echo $MONITOR_PID > "$OUTPUT_DIR/monitor.pid"
    echo -e "${GREEN}  ✓ Monitor launched: PID=$MONITOR_PID${NC}"
else
    echo -e "${YELLOW}  ⚠ Monitor script not found (optional)${NC}"
fi

echo ""
echo -e "${YELLOW}[9/9] Creating status file...${NC}"

cat > "$OUTPUT_DIR/STATUS.md" <<EOF
# 12-Hour Performance Comparison - IN PROGRESS

## Status: RUNNING ✓

Both simulations launched successfully at $(date)!

### Simulation Details

| Version | PID | CPU Cores | Threads | Status |
|---------|-----|-----------|---------|--------|
| v1.0 (static) | $V1_PID | $V1_CORES | $V1_NUM_THREADS | Running |
| Current (adaptive) | $CURRENT_PID | $CURRENT_CORES | $CURRENT_NUM_THREADS | Running |

### Configuration

- **Start Time**: $(date)
- **Expected End**: $(date -d '+12 hours')
- **Duration**: 43200 seconds (12 hours)
- **Parameter File**: $PARAM_FILE (128k iterations max)
- **Output Directory**: $OUTPUT_DIR

### Monitor Progress

\`\`\`bash
# Check current iterations
grep -oP 'iteration: \\K[0-9]+' $OUTPUT_DIR/logs/v1.log | tail -1
grep -oP 'Iteration \\K[0-9]+' $OUTPUT_DIR/logs/current.log | tail -1

# Watch logs
tail -f $OUTPUT_DIR/logs/v1.log
tail -f $OUTPUT_DIR/logs/current.log

# Check monitor output (if running)
tail -f $OUTPUT_DIR/monitor_output.log

# Check process status
ps aux | grep simucell3d | grep -v grep
\`\`\`

### Expected Completion

The simulations will automatically terminate after 12 hours or when they reach 128,000 iterations (whichever comes first).

Based on previous benchmarks (~1.54x speedup), we expect:
- v1.0 to complete ~80-90k iterations in 12 hours
- Current to complete ~120-140k iterations in 12 hours (all 128k if fast enough)

---
Generated by launch_12hour_comparison.sh
EOF

echo -e "${GREEN}  ✓ Status file created${NC}"

echo ""
echo -e "${GREEN}════════════════════════════════════════════════════════════════════════════${NC}"
echo -e "${GREEN}  ✓ 12-Hour Comparison Successfully Launched!${NC}"
echo -e "${GREEN}════════════════════════════════════════════════════════════════════════════${NC}"
echo ""
echo -e "${BLUE}Output directory:${NC} $OUTPUT_DIR"
echo ""
echo -e "${BLUE}Quick status check:${NC}"
echo "  tail -f $OUTPUT_DIR/logs/v1.log"
echo "  tail -f $OUTPUT_DIR/logs/current.log"
echo ""
echo -e "${BLUE}Process IDs:${NC}"
echo "  v1.0:    $V1_PID"
echo "  Current: $CURRENT_PID"
echo ""
echo -e "${YELLOW}Expected completion: $(date -d '+12 hours')${NC}"
echo ""
echo "The simulations are now running. Check back in 12 hours for results!"
echo ""
