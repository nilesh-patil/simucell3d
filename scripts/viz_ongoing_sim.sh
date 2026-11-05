#!/usr/bin/env bash
# Snapshot the paper-config growth runs and generate the full visualization suite.
# The benchmark viz pipeline expects a <bench>/sim_<name>/ layout with one dir per
# "scheduler"; we use that slot for one dir per run so every plot compares the runs:
#   local4c  - this machine, 4 physical cores, 79ad7fd (build-dbg: -O3 -g), stopped it 13245
#   paper8c  - c2-standard-16, 8 physical cores, 79ad7fd (ended 2026-09-12, 131k cells)
#   guard8c  - c2-standard-16, 8 cores, 459d02d bending guard (OOM-killed 2026-09-13, 120k cells)
#   fix113   - c2-standard-16, 8 cores, e4223cc guard + #113 (host error 2026-09-13, 64k cells)
#   fix113r2 - round 2 of fix113 (+128 GB swap), stopped 2026-09-26 at 93.6k cells when thrashing
#   v1r2     - round 2 of v1.0 unmodified (SIGSEGV 2026-09-23, 3.6k cells)
#   v1fix    - v1.0 + local-id fix only (v1.0 unstable-mesh abort 2026-09-24, 32k cells)
#   v1       - c2-standard-16, 8 cores, upstream tag v1.0 (SIGSEGV 2026-09-12, 2.7k cells);
#              v1.0 writes no performance_diagnostics.csv, so one is synthesised from the
#              timestamped run.log (wall-clock per 100 iterations; phase columns left empty)
# Any run whose directory is missing is skipped.
set -uo pipefail
cd "$(dirname "$0")/.."

VIZ=simulation_results/longrun_20k_viz
declare -A RUNS=(
  [local4c]=simulation_results/longrun_20k_fast_growth
  [paper8c]=simulation_results/longrun_20k_fast_growth_paper8c
  [guard8c]=simulation_results/longrun_20k_fast_growth_paper8c_guard
  [fix113]=simulation_results/longrun_20k_fast_growth_paper8c_fix113
  [v1]=simulation_results/longrun_20k_fast_growth_v1
  [fix113r2]=simulation_results/longrun_20k_fast_growth_paper8c_fix113_r2
  [v1r2]=simulation_results/longrun_20k_fast_growth_v1_r2
  [v1fix]=simulation_results/longrun_20k_fast_growth_v1fix
)

# Build a performance_diagnostics.csv (same header as the current solver's) from a v1.0
# run.log whose lines were prefixed with an ISO timestamp by the launcher's awk filter.
# One row per 100 iterations: total_iteration_ms is the window's mean wall time per iteration.
synth_v1_diag() {
  local LOG=$1 OUT=$2
  echo "sim_time,wall_epoch,iteration,cells,divisions,cov,phase,mesh_refinement_ms,contact_detection_ms,polarization_internal_forces_ms,time_integration_ms,total_iteration_ms,thread_imbalance_pct,unstable_cells_removed" > "$OUT"
  grep -aE '^[0-9T:Z-]+ Progression' "$LOG" \
    | sed -E 's/^([0-9-]+)T([0-9:]+)Z Progression [0-9]+%, iteration: ([0-9]+), file number [0-9]+, nb cells ([0-9]+)$/\1 \2 \3 \4/' \
    | awk 'BEGIN{prev_e=""; prev_i=-1}
           { gsub(/-/," ",$1); gsub(/:/," ",$2); e=mktime($1" "$2); it=$3; c=$4
             if (it%100==0) {
               ms = (prev_e=="" || it==prev_i) ? 0 : 1000.0*(e-prev_e)/(it-prev_i)
               printf "0.000,%d,%d,%d,0,,GROWTH,,,,,%.2f,,0\n", e, it, c, ms
               prev_e=e; prev_i=it } }' >> "$OUT"
}

# Facts the CSVs cannot carry, for the SUMMARY_STATS.md run table: code version, machine, how
# the run ended. The per-run counters (removals, /usr/bin/time -v peak RSS / elapsed) are pulled
# from run.log below.
declare -A COMMIT=( [local4c]=79ad7fd [paper8c]=79ad7fd [guard8c]=459d02d [fix113]=e4223cc [v1]="v1.0 (38af451)" [fix113r2]=e4223cc [v1r2]="v1.0 (38af451)" [v1fix]="v1.0 + local-id fix" )
declare -A CORES=( [local4c]="4 (local)" [paper8c]="8 (c2-standard-16)" [guard8c]="8 (c2-standard-16)" [fix113]="8 (c2-standard-16)" [v1]="8 (c2-standard-16)" [fix113r2]="8 (c2-16 + 128 GB swap)" [v1r2]="8 (c2-16 + 128 GB swap)" [v1fix]="8 (c2-16 + 128 GB swap)" )
declare -A OUTCOME=(
  [local4c]="stopped by SIGTERM under gdb at it 13245"
  [paper8c]="stopped by SIGTERM at it 13722 after 125k-cell endpoint"
  [guard8c]="OOM-killed (SIGKILL) at it 13589, RSS 36→64 GB in last ~24 h"
  [fix113]="GCE host error + auto-restart at it 13008 (run lost)"
  [v1]="SIGSEGV at it 9276, one iteration after its first below-min-volume cell erase"
  [fix113r2]="stopped by SIGTERM at it 13435 (user decision: swap thrashing); mass removal 106 @ 13147, crawl from 13200"
  [v1r2]="SIGSEGV in libmesh.so at it 9620 (reproduces v1.0 crash)"
  [v1fix]="v1.0 unstable-mesh abort (refinement failed, exit 1) at it 11796"
)

# Emit one JSON object per run into $VIZ/run_notes.json (read by plot_benchmark_results.py)
run_notes() {
  local NAME=$1 LOG=$2 sep=$3
  local removed rss elapsed cpu
  removed=$(grep -ac 'Removing numerically unstable' "$LOG" 2>/dev/null)   # prints 0 (rc 1) on no match
  rss=$(grep -aoE 'Maximum resident set size \(kbytes\): [0-9]+' "$LOG" 2>/dev/null | tail -1 | tr -dc 0-9)
  elapsed=$(grep -aoE 'Elapsed \(wall clock\) time \(h:mm:ss or m:ss\): [0-9:.]+' "$LOG" 2>/dev/null | tail -1 | awk '{print $NF}')
  cpu=$(grep -aoE 'Percent of CPU this job got: [0-9]+' "$LOG" 2>/dev/null | tail -1 | tr -dc 0-9)
  # time -v summary only exists for runs that ended under the wrapper (not host-error / local gdb kills)
  local elapsed_json=null
  [[ -n "$elapsed" ]] && elapsed_json="\"$elapsed\""
  printf '%s  "%s": {"commit": "%s", "cores": "%s", "outcome": "%s", "unstable_removals": %s, "peak_rss_kb": %s, "time_v_elapsed": %s, "time_v_cpu_pct": %s}' \
    "$sep" "$NAME" "${COMMIT[$NAME]}" "${CORES[$NAME]}" "${OUTCOME[$NAME]}" "${removed:-0}" \
    "${rss:-null}" "$elapsed_json" "${cpu:-null}"
}

rm -rf "$VIZ"
mkdir -p "$VIZ"
NOTES="$VIZ/run_notes.json"
echo "{" > "$NOTES"
SEP=""
for NAME in local4c paper8c guard8c fix113 fix113r2 v1 v1r2 v1fix; do
  SRC=${RUNS[$NAME]}
  [[ -f "$SRC/simulation_statistics.csv" ]] || { echo "skip $NAME: no $SRC"; continue; }
  STAGE="$VIZ/sim_$NAME"
  mkdir -p "$STAGE"
  # Point-in-time copies: an ongoing run keeps appending to the originals
  cp "$SRC/simulation_statistics.csv" "$STAGE/simulation_statistics.csv"
  if [[ -f "$SRC/performance_diagnostics.csv" ]]; then
    cp "$SRC/performance_diagnostics.csv" "$STAGE/performance_diagnostics.csv"
  else
    synth_v1_diag "${SRC}_logs/run.log" "$STAGE/performance_diagnostics.csv"
  fi
  ITER=$(grep -aoE 'iteration: [0-9]+' "${SRC}_logs/run.log" 2>/dev/null | tail -1 | tr -dc 0-9)
  echo "$NAME: iteration ~${ITER:-?} | stats rows: $(wc -l < "$STAGE/simulation_statistics.csv") | diag rows: $(wc -l < "$STAGE/performance_diagnostics.csv")"
  run_notes "$NAME" "${SRC}_logs/run.log" "$SEP" >> "$NOTES"
  SEP=$',\n'
done
printf '\n}\n' >> "$NOTES"
python3 -c "import json,sys; json.load(open(sys.argv[1]))" "$NOTES" || { echo "run_notes.json invalid"; exit 1; }
echo "============================================================"

# Run the full suite (all three plotting scripts), no LaTeX (cm-super/type1ec not installed)
# BENCH_ALIGN=iteration: these runs happened at different times, so align comparison.csv by iteration
BENCH_ALIGN=iteration bash scripts/generate_visualizations.sh "$VIZ" --all-scripts --no-latex
