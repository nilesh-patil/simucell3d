#!/usr/bin/env bash
# Snapshot the live ongoing simulation and generate the full visualization suite.
# The benchmark viz pipeline expects a <bench>/sim_<scheduler>/ layout, so we stage
# a point-in-time copy of the running sim's CSVs as sim_adaptive/.
set -uo pipefail
cd /home/nilesh-patil/projects/simucell3d

SRC=simulation_results/longrun_20k_fast_growth
VIZ=simulation_results/longrun_20k_viz
STAGE="$VIZ/sim_adaptive"

# Current iteration for provenance
ITER=$(grep Progression "${SRC}_logs/run.log" 2>/dev/null | tail -1 | sed -E 's/.*iteration: ([0-9]+).*/\1/')
echo "Snapshotting live sim at iteration ~${ITER:-unknown}"

rm -rf "$VIZ"
mkdir -p "$STAGE"
# Consistent point-in-time snapshot of the metric CSVs (the sim keeps appending to the originals)
cp "$SRC/simulation_statistics.csv"   "$STAGE/simulation_statistics.csv"
cp "$SRC/performance_diagnostics.csv" "$STAGE/performance_diagnostics.csv"

echo "stats rows: $(wc -l < "$STAGE/simulation_statistics.csv")  | diag rows: $(wc -l < "$STAGE/performance_diagnostics.csv")"
echo "============================================================"

# Run the full suite (all three plotting scripts), no LaTeX (cm-super/type1ec not installed)
bash scripts/generate_visualizations.sh "$VIZ" --all-scripts --no-latex
