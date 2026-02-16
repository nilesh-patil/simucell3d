#!/usr/bin/env bash
# Reproduce Nature paper Figure 3e: Monolayer/Multilayer transition
#
# This script runs the vesicle simulation at different surface tension ratios
# to reproduce the monolayer-to-multilayer transition observed in Figure 3e.
#
# Expected result: Layering transition at dimensionless surface tension ratio
# gamma_tilde (apical_tension / lateral_tension) ~ 0.05
#
# Usage: ./reproduce_fig3e.sh [BUILD_DIR]
#   BUILD_DIR: path to build directory (default: PROJECT_ROOT/build)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="${1:-$PROJECT_ROOT/build}"
SIMUCELL3D="$BUILD_DIR/simucell3d"
RESULTS_BASE="$PROJECT_ROOT/simulation_results/nature_fig3e"

# Verify binary exists
if [[ ! -x "$SIMUCELL3D" ]]; then
    echo "ERROR: simucell3d binary not found at $SIMUCELL3D"
    echo "Please build first: cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j\$(nproc)"
    exit 1
fi

echo "========================================="
echo "Nature Figure 3e Reproduction"
echo "Monolayer/Multilayer Transition"
echo "========================================="
echo "Binary: $SIMUCELL3D"
echo "Results: $RESULTS_BASE"
echo ""

# Set OpenMP environment for reproducible results
export OMP_NUM_THREADS="${OMP_NUM_THREADS:-$(nproc)}"
export OMP_PROC_BIND=close
export OMP_PLACES=cores

echo "Using $OMP_NUM_THREADS threads"
echo ""

# Run monolayer configuration (low gamma_tilde)
echo "--- Running monolayer configuration ---"
MONOLAYER_DIR="$RESULTS_BASE/monolayer"
mkdir -p "$MONOLAYER_DIR"

PARAM_FILE="$PROJECT_ROOT/parameters/benchmarks/fig3e_monolayer.xml"
if [[ ! -f "$PARAM_FILE" ]]; then
    echo "WARNING: Parameter file not found: $PARAM_FILE"
    echo "Using default vesicle parameters instead"
    PARAM_FILE="$PROJECT_ROOT/parameters/core/parameters_vesicle.xml"
fi

echo "Parameter file: $PARAM_FILE"
echo "Output: $MONOLAYER_DIR"
time "$SIMUCELL3D" "$PARAM_FILE" --schedule=adaptive 2>&1 | tee "$MONOLAYER_DIR/simulation.log"
echo "Monolayer simulation complete."
echo ""

# Run multilayer configuration (high gamma_tilde)
echo "--- Running multilayer configuration ---"
MULTILAYER_DIR="$RESULTS_BASE/multilayer"
mkdir -p "$MULTILAYER_DIR"

PARAM_FILE="$PROJECT_ROOT/parameters/benchmarks/fig3e_multilayer.xml"
if [[ ! -f "$PARAM_FILE" ]]; then
    echo "WARNING: Parameter file not found: $PARAM_FILE"
    echo "Using default vesicle parameters instead"
    PARAM_FILE="$PROJECT_ROOT/parameters/core/parameters_vesicle.xml"
fi

echo "Parameter file: $PARAM_FILE"
echo "Output: $MULTILAYER_DIR"
time "$SIMUCELL3D" "$PARAM_FILE" --schedule=adaptive 2>&1 | tee "$MULTILAYER_DIR/simulation.log"
echo "Multilayer simulation complete."
echo ""

echo "========================================="
echo "Figure 3e reproduction complete!"
echo ""
echo "Results saved to:"
echo "  Monolayer:  $MONOLAYER_DIR"
echo "  Multilayer: $MULTILAYER_DIR"
echo ""
echo "To validate: Compare output VTK files with golden masters in"
echo "  baselines/nature_paper/fig3e/"
echo ""
echo "Expected observation:"
echo "  - Monolayer config: cells form a single-layer epithelium"
echo "  - Multilayer config: cells organize into multiple layers"
echo "  - Transition occurs at gamma_tilde ~ 0.05"
echo "========================================="
