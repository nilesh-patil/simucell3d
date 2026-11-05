#!/usr/bin/env bash
# Short paper-config runs (to it ~9,000 / ~2.4k cells) comparing contact cutoffs 2e-7 / 5e-7 / 1e-6 m.
# Question: is the cell interpenetration seen in every long run caused by the 2e-7 m cutoff
# paired with 1e-6 m edges? One single-threaded run per physical core (cores 1-3).
set -euo pipefail
P="$(cd "$(dirname "$0")/../.." && pwd)"; T=$P/simulation_results/overlap_test
export OMP_WAIT_POLICY=passive OMP_DYNAMIC=false
cd $P   # input mesh path is relative to the repo root
core=1
for c in 2e-7 5e-7 1e-6; do
  O=$T/out_$c; rm -rf "$O"; mkdir -p "$O"
  nohup taskset -c $core,$((core+4)) /usr/bin/time -v $P/build/simucell3d --schedule=adaptive --threads=1 \
    --output-dir="$O" --diagnostics-csv="$O/performance_diagnostics.csv" "$T/cutoff_$c.xml" > "$T/run_$c.log" 2>&1 &
  echo "$c pid $! core $core"; core=$((core+1))
done
# Results reported in PR #115 were made with the pre-fix cell divider. A fourth run repeated
# cutoff_1e-6.xml with the multi-loop divider fix (now in this branch) on core 0:
#   taskset -c 0,4 <fixed build>/simucell3d ... cutoff_1e-6.xml > run_1e-6_divfix.log
