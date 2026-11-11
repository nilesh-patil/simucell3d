#!/usr/bin/env bash
# Three short growth runs that differ only in the contact cutoff (2e-7, 5e-7, 1e-6 m), to see
# how the cutoff relative to the 1e-6 m minimum edge length affects cell interpenetration.
# One single-threaded run per core (cores 1-3, with sibling hyperthreads core+4 on a
# 4-core/8-thread machine). The parameter files simulation_results/overlap_test/cutoff_<c>.xml
# are not in the repository: copy parameters/longrun_fast_growth.xml and change
# contact_cutoff_adhesion and contact_cutoff_repulsion.
set -euo pipefail
# Configs (cutoff_*.xml) and outputs live in simulation_results/overlap_test/ (untracked).
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
# To compare two builds on one cutoff, start the same command with the other build on core 0:
#   taskset -c 0,4 <other build>/simucell3d ... cutoff_1e-6.xml > run_1e-6_other.log
