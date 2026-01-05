#!/bin/bash
#
# SimuCell3D Benchmark Visualization Generator v2.0
#
# Generates publication-quality visualizations for benchmark data with:
# - Automatic scheduler detection (supports N schedulers)
# - Data validation and cleaning with audit trail
# - CVD-safe colormaps and Tufte-style minimal ink design
#
# Usage:
#   ./scripts/generate_visualizations.sh <benchmark_directory> [options]
#
# Options:
#   --quality MODE      publication (300 DPI) or draft (150 DPI) [default: publication]
#   --plots SELECTION   all, biological, computational, or comma-separated IDs
#   --formats FORMATS   png, pdf, svg (comma-separated) [default: png]
#   --validate-only     Run data validation without generating plots
#   --skip-sim-stats    Skip large simulation_statistics.csv (faster, fewer plots)
#   --no-latex          Disable LaTeX rendering (faster)
#   --force             Regenerate all plots (ignore cache)
#   --verbose           Enable verbose output
#   --legacy            Run legacy scripts instead of unified script
#   --all-scripts       Run all three plotting scripts (legacy, updated, unified)
#   --skip-preprocess   Skip preprocessing step (use existing metrics/)
#   --help              Show this help
#
# Examples:
#   ./scripts/generate_visualizations.sh simulation_results/parallel_benchmark_20260126_094659
#   ./scripts/generate_visualizations.sh simulation_results/parallel_benchmark_20260126_094659 --quality draft
#   ./scripts/generate_visualizations.sh simulation_results/parallel_benchmark_20260126_094659 --validate-only
#   ./scripts/generate_visualizations.sh simulation_results/parallel_benchmark_20260126_094659 --all-scripts
#   ./scripts/generate_visualizations.sh simulation_results/parallel_benchmark_20260126_094659 --legacy

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default options
QUALITY="publication"
PLOTS="all"
FORMATS="png"
VALIDATE_ONLY=false
SKIP_SIM_STATS=false
NO_LATEX=false
FORCE=false
VERBOSE=false
LEGACY=false
ALL_SCRIPTS=false
SKIP_PREPROCESS=false

usage() {
    cat << EOF
SimuCell3D Benchmark Visualization Generator v2.0

Usage: $0 <benchmark_directory> [options]

Options:
  --quality MODE      publication (300 DPI) or draft (150 DPI) [default: publication]
  --plots SELECTION   all, biological, computational, or comma-separated IDs (01,04,08)
  --formats FORMATS   png, pdf, svg (comma-separated) [default: png]
  --validate-only     Run data validation without generating plots
  --skip-sim-stats    Skip large simulation_statistics.csv (faster, fewer plots)
  --no-latex          Disable LaTeX rendering (faster)
  --force             Regenerate all plots (ignore cache)
  --verbose           Enable verbose output
  --legacy            Run legacy scripts instead of unified script
  --all-scripts       Run all three plotting scripts (legacy, updated, unified)
  --skip-preprocess   Skip preprocessing step (use existing metrics/)
  --help              Show this help

Examples:
  $0 simulation_results/parallel_benchmark_20260126_094659
  $0 simulation_results/parallel_benchmark_20260126_094659 --quality draft --no-latex
  $0 simulation_results/parallel_benchmark_20260126_094659 --validate-only
  $0 simulation_results/parallel_benchmark_20260126_094659 --plots biological
  $0 simulation_results/parallel_benchmark_20260126_094659 --all-scripts
  $0 simulation_results/parallel_benchmark_20260126_094659 --legacy

Output Directories:
  plots-unified/        Unified script output (default)
  plots/                Original comprehensive script output
  plots-updated/        Extended narrative script output
EOF
}

# Parse arguments
BENCH_DIR=""
while [[ $# -gt 0 ]]; do
    case $1 in
        --quality)
            QUALITY="$2"
            shift 2
            ;;
        --plots)
            PLOTS="$2"
            shift 2
            ;;
        --formats)
            FORMATS="$2"
            shift 2
            ;;
        --validate-only)
            VALIDATE_ONLY=true
            shift
            ;;
        --skip-sim-stats)
            SKIP_SIM_STATS=true
            shift
            ;;
        --no-latex)
            NO_LATEX=true
            shift
            ;;
        --force)
            FORCE=true
            shift
            ;;
        --verbose|-v)
            VERBOSE=true
            shift
            ;;
        --legacy)
            LEGACY=true
            shift
            ;;
        --all-scripts)
            ALL_SCRIPTS=true
            shift
            ;;
        --skip-preprocess)
            SKIP_PREPROCESS=true
            shift
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        -*)
            echo -e "${RED}Error: Unknown option: $1${NC}"
            usage
            exit 1
            ;;
        *)
            if [ -z "$BENCH_DIR" ]; then
                BENCH_DIR="$1"
            else
                echo -e "${RED}Error: Unexpected argument: $1${NC}"
                usage
                exit 1
            fi
            shift
            ;;
    esac
done

# Check benchmark directory
if [ -z "$BENCH_DIR" ]; then
    echo -e "${RED}Error: No benchmark directory specified${NC}"
    usage
    exit 1
fi

# Verify directory exists
if [ ! -d "$BENCH_DIR" ]; then
    echo -e "${RED}Error: Directory not found: $BENCH_DIR${NC}"
    exit 1
fi

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo -e "${BLUE}============================================================${NC}"
echo -e "${BLUE}SimuCell3D Benchmark Visualization Generator v2.0${NC}"
echo -e "${BLUE}============================================================${NC}"
echo "Benchmark directory: $BENCH_DIR"
echo "Quality mode: $QUALITY"
if [ "$VALIDATE_ONLY" = true ]; then
    echo "Mode: Validation only"
fi
if [ "$ALL_SCRIPTS" = true ]; then
    echo "Mode: All scripts (legacy + updated + unified)"
fi
echo ""

# Step 1: Preprocess raw simulation data into metrics format
if [ "$SKIP_PREPROCESS" = false ]; then
    echo -e "${BLUE}------------------------------------------------------------${NC}"
    echo -e "${BLUE}Step 1: Preprocessing raw simulation data${NC}"
    echo -e "${BLUE}------------------------------------------------------------${NC}"
    if [ -f "$SCRIPT_DIR/preprocess_benchmark_metrics.py" ]; then
        python3 "$SCRIPT_DIR/preprocess_benchmark_metrics.py" "$BENCH_DIR"
        echo ""
    else
        echo -e "${YELLOW}Warning: preprocess_benchmark_metrics.py not found, skipping${NC}"
        echo ""
    fi
fi

# Step 2: Run visualization scripts
echo -e "${BLUE}------------------------------------------------------------${NC}"
echo -e "${BLUE}Step 2: Generating visualizations${NC}"
echo -e "${BLUE}------------------------------------------------------------${NC}"
echo ""

if [ "$ALL_SCRIPTS" = true ]; then
    # Run all three scripts
    echo -e "${GREEN}Running all three visualization scripts...${NC}"
    echo ""

    # Script 1: Legacy original
    if [ -f "$SCRIPT_DIR/plot_benchmark_results.py" ]; then
        echo -e "${BLUE}[1/3] Running legacy script (plot_benchmark_results.py)...${NC}"
        echo "      Output: $BENCH_DIR/plots/"
        python3 "$SCRIPT_DIR/plot_benchmark_results.py" "$BENCH_DIR" || true
        echo ""
    else
        echo -e "${YELLOW}[1/3] Skipped: plot_benchmark_results.py not found${NC}"
    fi

    # Script 2: Updated script
    if [ -f "$SCRIPT_DIR/plot_benchmark_results_updated.py" ]; then
        echo -e "${BLUE}[2/3] Running updated script (plot_benchmark_results_updated.py)...${NC}"
        echo "      Output: $BENCH_DIR/plots-updated/"

        CMD2="python3 $SCRIPT_DIR/plot_benchmark_results_updated.py $BENCH_DIR"

        if [ "$QUALITY" = "publication" ]; then
            CMD2="$CMD2 --publication"
        fi

        if [ "$NO_LATEX" = true ]; then
            CMD2="$CMD2 --no-latex"
        fi

        CMD2="$CMD2 --formats $FORMATS"

        eval "$CMD2" || true
        echo ""
    else
        echo -e "${YELLOW}[2/3] Skipped: plot_benchmark_results_updated.py not found${NC}"
    fi

    # Script 3: Unified script
    if [ -f "$SCRIPT_DIR/plot_benchmark_unified.py" ]; then
        echo -e "${BLUE}[3/3] Running unified script (plot_benchmark_unified.py)...${NC}"
        echo "      Output: $BENCH_DIR/plots-unified/"

        CMD="python3 $SCRIPT_DIR/plot_benchmark_unified.py $BENCH_DIR"
        CMD="$CMD --quality $QUALITY"
        CMD="$CMD --formats $FORMATS"

        if [ "$PLOTS" != "all" ]; then
            CMD="$CMD --plots $PLOTS"
        else
            CMD="$CMD --plots all"
        fi

        if [ "$SKIP_SIM_STATS" = true ]; then
            CMD="$CMD --skip-sim-stats"
        fi

        if [ "$NO_LATEX" = true ]; then
            CMD="$CMD --no-latex"
        fi

        if [ "$FORCE" = true ]; then
            CMD="$CMD --force"
        fi

        if [ "$VERBOSE" = true ]; then
            CMD="$CMD -v"
        fi

        eval "$CMD" || true
        echo ""
    else
        echo -e "${YELLOW}[3/3] Skipped: plot_benchmark_unified.py not found${NC}"
    fi

elif [ "$LEGACY" = true ]; then
    # Run legacy scripts
    echo -e "${YELLOW}Running legacy visualization scripts...${NC}"
    echo ""

    # Check if legacy scripts exist
    if [ ! -f "$SCRIPT_DIR/plot_benchmark_results.py" ]; then
        echo -e "${RED}Warning: Legacy script plot_benchmark_results.py not found${NC}"
    else
        echo -e "${BLUE}[1/2] Running original visualization script...${NC}"
        echo "      Output: $BENCH_DIR/plots/"
        echo "------------------------------------------------------------"
        python3 "$SCRIPT_DIR/plot_benchmark_results.py" "$BENCH_DIR" || true
        echo ""
    fi

    if [ ! -f "$SCRIPT_DIR/plot_benchmark_results_updated.py" ]; then
        echo -e "${RED}Warning: Legacy script plot_benchmark_results_updated.py not found${NC}"
    else
        echo -e "${BLUE}[2/2] Running updated visualization script...${NC}"
        echo "      Output: $BENCH_DIR/plots-updated/"
        echo "------------------------------------------------------------"

        CMD2="python3 $SCRIPT_DIR/plot_benchmark_results_updated.py $BENCH_DIR"

        if [ "$QUALITY" = "publication" ]; then
            CMD2="$CMD2 --publication"
        fi

        if [ "$NO_LATEX" = true ]; then
            CMD2="$CMD2 --no-latex"
        fi

        CMD2="$CMD2 --formats $FORMATS"

        eval "$CMD2" || true
        echo ""
    fi
else
    # Run unified script
    echo -e "${GREEN}Running unified visualization script...${NC}"
    echo "      Output: $BENCH_DIR/plots-unified/"
    echo ""

    # Build command
    CMD="python3 $SCRIPT_DIR/plot_benchmark_unified.py $BENCH_DIR"
    CMD="$CMD --quality $QUALITY"
    CMD="$CMD --formats $FORMATS"

    if [ "$PLOTS" != "all" ]; then
        CMD="$CMD --plots $PLOTS"
    else
        CMD="$CMD --plots all"
    fi

    if [ "$VALIDATE_ONLY" = true ]; then
        CMD="$CMD --validate-only"
    fi

    if [ "$SKIP_SIM_STATS" = true ]; then
        CMD="$CMD --skip-sim-stats"
    fi

    if [ "$NO_LATEX" = true ]; then
        CMD="$CMD --no-latex"
    fi

    if [ "$FORCE" = true ]; then
        CMD="$CMD --force"
    fi

    if [ "$VERBOSE" = true ]; then
        CMD="$CMD -v"
    fi

    # Execute
    eval "$CMD"
fi

echo ""
echo -e "${GREEN}============================================================${NC}"
echo -e "${GREEN}Complete!${NC}"
echo -e "${GREEN}============================================================${NC}"
if [ "$ALL_SCRIPTS" = true ]; then
    echo "Generated visualizations in:"
    echo "  - $BENCH_DIR/plots/         (Original comprehensive suite)"
    echo "  - $BENCH_DIR/plots-updated/ (Extended narrative suite)"
    echo "  - $BENCH_DIR/plots-unified/ (Latest statistical improvements)"
    echo ""
    # Count plots in each directory
    LEGACY_COUNT=$(ls "$BENCH_DIR/plots/"*.png 2>/dev/null | wc -l || echo 0)
    UPDATED_COUNT=$(ls "$BENCH_DIR/plots-updated/"*.png 2>/dev/null | wc -l || echo 0)
    UNIFIED_COUNT=$(ls "$BENCH_DIR/plots-unified/"*.png 2>/dev/null | wc -l || echo 0)
    echo "Plot counts:"
    echo "  - plots/:         $LEGACY_COUNT plots"
    echo "  - plots-updated/: $UPDATED_COUNT plots"
    echo "  - plots-unified/: $UNIFIED_COUNT plots"
    echo ""
    echo "Total: $((LEGACY_COUNT + UPDATED_COUNT + UNIFIED_COUNT)) plots generated"
elif [ "$LEGACY" = true ]; then
    echo "Generated visualizations in:"
    echo "  - $BENCH_DIR/plots/         (Original comprehensive suite)"
    echo "  - $BENCH_DIR/plots-updated/ (Extended narrative suite)"
else
    echo "Generated visualizations in:"
    echo "  - $BENCH_DIR/plots-unified/"
    echo ""
    echo "Key outputs:"
    echo "  - validation_report.html    Data quality report"
    echo "  - audit_trail.json          Record of data corrections"
    echo "  - manifest.json             Reproducibility metadata"
fi
echo ""
