#!/bin/bash
################################################################################
# Pre-Flight Check for 24-Hour Performance Comparison
#
# Validates system configuration and prerequisites before launching
# the 12-hour comparison
################################################################################

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

PASS=0
WARN=0
FAIL=0

echo -e "${BLUE}════════════════════════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}  Pre-Flight Check: 24-Hour Performance Comparison${NC}"
echo -e "${BLUE}════════════════════════════════════════════════════════════════════════════${NC}"
echo ""

# Configuration
PROJECT_ROOT="/home/nilesh-patil/projects/version-cpp-next"
V1_BUILD_DIR="/home/nilesh-patil/projects/version-cpp-v1.0/build"
CURRENT_BUILD_DIR="$PROJECT_ROOT/build"
PARAM_FILE="$PROJECT_ROOT/parameters/progressive_scaling/parameters_vesicle_progbench_128k.xml"
MESH_FILE="$PROJECT_ROOT/data/input_meshes/fig_3_vesicle.vtk"

echo -e "${YELLOW}[1] System Resources${NC}"
echo ""

# Check CPU cores
NUM_CORES=$(nproc)
echo -n "  CPU Cores: $NUM_CORES ... "
if [ "$NUM_CORES" -ge 16 ]; then
    echo -e "${GREEN}PASS${NC} (16+ required)"
    PASS=$((PASS+1))
else
    echo -e "${RED}FAIL${NC} (16+ required, found $NUM_CORES)"
    echo "    Action: Upscale VM to at least 16 CPU cores"
    FAIL=$((FAIL+1))
fi

# Check memory
TOTAL_MEM_GB=$(free -g | grep "Mem:" | awk '{print $2}')
echo -n "  Total Memory: ${TOTAL_MEM_GB}GB ... "
if [ "$TOTAL_MEM_GB" -ge 4 ]; then
    echo -e "${GREEN}PASS${NC} (4GB+ required)"
    PASS=$((PASS+1))
else
    echo -e "${RED}FAIL${NC} (4GB+ recommended, found ${TOTAL_MEM_GB}GB)"
    FAIL=$((FAIL+1))
fi

# Check disk space
FREE_SPACE_GB=$(df -BG "$PROJECT_ROOT" | tail -1 | awk '{print $4}' | sed 's/G//')
echo -n "  Free Disk Space: ${FREE_SPACE_GB}GB ... "
if [ "$FREE_SPACE_GB" -ge 10 ]; then
    echo -e "${GREEN}PASS${NC} (15GB+ recommended)"
    PASS=$((PASS+1))
elif [ "$FREE_SPACE_GB" -ge 5 ]; then
    echo -e "${YELLOW}WARN${NC} (15GB+ recommended, found ${FREE_SPACE_GB}GB)"
    WARN=$((WARN+1))
else
    echo -e "${RED}FAIL${NC} (15GB+ recommended, found ${FREE_SPACE_GB}GB)"
    FAIL=$((FAIL+1))
fi

echo ""
echo -e "${YELLOW}[2] Binaries and Build Configuration${NC}"
echo ""

# Check v1.0 binary
echo -n "  v1.0 binary ... "
if [ -f "$V1_BUILD_DIR/simucell3d" ]; then
    echo -e "${GREEN}PASS${NC}"
    echo "    Location: $V1_BUILD_DIR/simucell3d"
    PASS=$((PASS+1))
else
    echo -e "${RED}FAIL${NC}"
    echo "    Expected: $V1_BUILD_DIR/simucell3d"
    FAIL=$((FAIL+1))
fi

# Check current binary
echo -n "  Current binary ... "
if [ -f "$CURRENT_BUILD_DIR/simucell3d" ] || [ -L "$CURRENT_BUILD_DIR/simucell3d" ]; then
    echo -e "${GREEN}PASS${NC}"
    echo "    Location: $CURRENT_BUILD_DIR/simucell3d"
    PASS=$((PASS+1))
else
    echo -e "${RED}FAIL${NC}"
    echo "    Expected: $CURRENT_BUILD_DIR/simucell3d"
    FAIL=$((FAIL+1))
fi

# Check v1.0 is Release build
echo -n "  v1.0 Release build ... "
if [ -f "$V1_BUILD_DIR/CMakeCache.txt" ] && grep -q "CMAKE_BUILD_TYPE:STRING=Release" "$V1_BUILD_DIR/CMakeCache.txt"; then
    echo -e "${GREEN}PASS${NC}"
    PASS=$((PASS+1))
else
    echo -e "${RED}FAIL${NC}"
    echo "    Action: Rebuild v1.0 with: cmake -DCMAKE_BUILD_TYPE=Release .."
    FAIL=$((FAIL+1))
fi

# Check current is Release build
echo -n "  Current Release build ... "
if [ -f "$CURRENT_BUILD_DIR/CMakeCache.txt" ] && grep -q "CMAKE_BUILD_TYPE:STRING=Release" "$CURRENT_BUILD_DIR/CMakeCache.txt"; then
    echo -e "${GREEN}PASS${NC}"
    PASS=$((PASS+1))
else
    echo -e "${RED}FAIL${NC}"
    echo "    Action: Rebuild current with: cmake -DCMAKE_BUILD_TYPE=Release .."
    FAIL=$((FAIL+1))
fi

echo ""
echo -e "${YELLOW}[3] Input Files${NC}"
echo ""

# Check parameter file
echo -n "  Parameter file ... "
if [ -f "$PARAM_FILE" ]; then
    echo -e "${GREEN}PASS${NC}"
    echo "    Location: $PARAM_FILE"
    FILE_SIZE=$(ls -lh "$PARAM_FILE" | awk '{print $5}')
    echo "    Size: $FILE_SIZE"
    PASS=$((PASS+1))
else
    echo -e "${RED}FAIL${NC}"
    echo "    Expected: $PARAM_FILE"
    FAIL=$((FAIL+1))
fi

# Check mesh file
echo -n "  Input mesh ... "
if [ -f "$MESH_FILE" ]; then
    echo -e "${GREEN}PASS${NC}"
    echo "    Location: $MESH_FILE"
    FILE_SIZE=$(ls -lh "$MESH_FILE" | awk '{print $5}')
    echo "    Size: $FILE_SIZE"
    PASS=$((PASS+1))
else
    echo -e "${RED}FAIL${NC}"
    echo "    Expected: $MESH_FILE"
    FAIL=$((FAIL+1))
fi

echo ""
echo -e "${YELLOW}[4] Environment Check${NC}"
echo ""

# Check for running simulations
echo -n "  No conflicting simulations ... "
RUNNING=$(pgrep -f simucell3d | wc -l)
if [ "$RUNNING" -eq 0 ]; then
    echo -e "${GREEN}PASS${NC}"
    PASS=$((PASS+1))
else
    echo -e "${YELLOW}WARN${NC} ($RUNNING simucell3d processes running)"
    echo "    Running PIDs: $(pgrep -f simucell3d | tr '\n' ' ')"
    echo "    Action: Stop existing simulations or ensure CPU resources available"
    WARN=$((WARN+1))
fi

# Check taskset availability
echo -n "  taskset command ... "
if command -v taskset &> /dev/null; then
    echo -e "${GREEN}PASS${NC}"
    PASS=$((PASS+1))
else
    echo -e "${RED}FAIL${NC}"
    echo "    Action: Install util-linux package"
    FAIL=$((FAIL+1))
fi

# Check timeout availability
echo -n "  timeout command ... "
if command -v timeout &> /dev/null; then
    echo -e "${GREEN}PASS${NC}"
    PASS=$((PASS+1))
else
    echo -e "${RED}FAIL${NC}"
    echo "    Action: Install coreutils package"
    FAIL=$((FAIL+1))
fi

# Check /usr/bin/time availability
echo -n "  /usr/bin/time command ... "
if [ -f "/usr/bin/time" ]; then
    echo -e "${GREEN}PASS${NC}"
    PASS=$((PASS+1))
else
    echo -e "${YELLOW}WARN${NC}"
    echo "    Note: Timing metrics will be limited"
    WARN=$((WARN+1))
fi

echo ""
echo -e "${YELLOW}[5] Script Files${NC}"
echo ""

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Check launch script
echo -n "  Launch script ... "
if [ -f "$SCRIPT_DIR/launch_24hour_comparison.sh" ]; then
    echo -e "${GREEN}PASS${NC}"
    PASS=$((PASS+1))
else
    echo -e "${RED}FAIL${NC}"
    FAIL=$((FAIL+1))
fi

# Check monitor script
echo -n "  Monitor script ... "
if [ -f "$SCRIPT_DIR/monitor_24hour.sh" ]; then
    echo -e "${GREEN}PASS${NC}"
    PASS=$((PASS+1))
else
    echo -e "${YELLOW}WARN${NC} (optional)"
    WARN=$((WARN+1))
fi

# Check analysis script
echo -n "  Analysis script ... "
if [ -f "$SCRIPT_DIR/analyze_results.sh" ]; then
    echo -e "${GREEN}PASS${NC}"
    PASS=$((PASS+1))
else
    echo -e "${YELLOW}WARN${NC} (optional)"
    WARN=$((WARN+1))
fi

# Check scripts are executable
if [ -f "$SCRIPT_DIR/launch_24hour_comparison.sh" ]; then
    echo -n "  Scripts executable ... "
    if [ -x "$SCRIPT_DIR/launch_24hour_comparison.sh" ]; then
        echo -e "${GREEN}PASS${NC}"
        PASS=$((PASS+1))
    else
        echo -e "${YELLOW}WARN${NC}"
        echo "    Action: chmod +x $SCRIPT_DIR/*.sh"
        WARN=$((WARN+1))
    fi
fi

echo ""
echo -e "${BLUE}════════════════════════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}  Summary${NC}"
echo -e "${BLUE}════════════════════════════════════════════════════════════════════════════${NC}"
echo ""
echo -e "  ${GREEN}PASS:${NC} $PASS"
echo -e "  ${YELLOW}WARN:${NC} $WARN"
echo -e "  ${RED}FAIL:${NC} $FAIL"
echo ""

if [ "$FAIL" -eq 0 ]; then
    echo -e "${GREEN}✓ System is ready for 12-hour comparison!${NC}"
    echo ""
    echo "To launch the comparison:"
    echo "  cd $SCRIPT_DIR"
    echo "  ./launch_24hour_comparison.sh"
    echo ""
    exit 0
elif [ "$FAIL" -le 2 ] && [ "$NUM_CORES" -lt 16 ]; then
    echo -e "${YELLOW}⚠ System needs CPU cores upscaling${NC}"
    echo ""
    echo "Primary issue: Insufficient CPU cores ($NUM_CORES / 16 required)"
    echo ""
    echo "Action required:"
    echo "  1. Upscale VM to 16+ CPU cores"
    echo "  2. Re-run this preflight check"
    echo "  3. Launch the comparison"
    echo ""
    exit 1
else
    echo -e "${RED}✗ System is NOT ready for 12-hour comparison${NC}"
    echo ""
    echo "Please address the failures listed above before proceeding."
    echo ""
    exit 1
fi
