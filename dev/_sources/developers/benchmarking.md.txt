(dev-benchmarking)=
# Benchmarking

This page describes how to measure the speed of SimuCell3D and compare two builds or two
{term}`scheduling modes <scheduling mode>`: the measurement method, the benchmark driver
script and the plotting tools. It does not list results; the measurements recorded for a release
are in `CHANGELOG.md` with their conditions. The code that is being measured is described in
{ref}`dev-performance-internals`.

## Method

Timings of a simulation depend on more than the code. A comparison is meaningful only when
the following hold:

**Same machine, same time.**
: Run both builds on the same machine, under the same load, ideally interleaved (A, B, A, B,
  ...). Timings from different machines, or from shared cloud runners at different times, are
  not comparable.

**Fixed work.**
: Compare the time per iteration at matching cell counts, or the time to complete a fixed
  number of iterations. A run cut off by a wall-clock limit does a different amount of work on a
  faster machine. In growth simulations the cost per iteration rises with the number of cells,
  so compare iterations at the same cell count (the `cells` and `total_iteration_ms` columns of
  the {term}`diagnostics CSV`), not at the same iteration number.

**Same physics.**
: Two builds that differ in the model (for example 1.0 and 2.1, see `CHANGELOG.md`) follow
  different trajectories. Their timings can be compared at equal cell counts, but the
  comparison then mixes code speed and model behaviour; say so when reporting it.

**Controlled threads.**
: Fix the thread count with `--threads=N`; keep the default pinning. Do not run two benchmarks
  on the same physical cores. When several runs share one machine, give each its own set of
  cores (with `taskset`, which the solver respects) and its own `--threads`.

**Little output.**
: Mesh output can dominate the run time and fill the disk. Use a parameter file with a large
  `sampling_period`, and write the output to local scratch space with `--output-dir`.

**Repetitions.**
: Repeat each configuration (at least three times for short runs) and report the median and
  the range. A difference smaller than the run-to-run range is not a result.

**Recorded environment.**
: Record the commit (`simucell3d --version`), compiler, build type, CPU model
  (`lscpu`), core and thread counts, the parameter file and the command line with every result.

Performance comparisons are informational: no CI check fails on a slowdown
({ref}`dev-ci`). A slowdown that matters is investigated with the phase timings and a profiler
({ref}`dev-performance-internals`).

## Quick comparison of two builds

`scripts/ci/perf_ab.py`, which the `perf.yml` workflow uses on pull requests, compares two
builds on the same machine with a fixed amount of work: it runs both executables on a copy of a
parameter file with a shortened `simulation_duration` and `sampling_period`, one unmeasured
warm-up run each, then alternates them for the given number of repetitions, with mesh output in a
temporary directory. It reports the median and range of the wall time of each build, and calls
the head build "slower" only when the median ratio exceeds `--threshold` (default 1.10) and the
ranges do not overlap. From the repository root, with the two builds in `build-a/` and
`build-b/`:

```console
$ python3 scripts/ci/perf_ab.py --base build-a/simucell3d --head build-b/simucell3d \
    --parameter-file parameters/core/parameters_vesicle_quick.xml \
    --simulation-duration 2e-6 --sampling-period 2e-6 --source-dir . --reps 5 --threads 4 \
    --markdown perf.md --json perf.json
```

This is a short, single-configuration check. For growth runs, where the cost per iteration
changes with the cell count, use the parallel benchmark driver below and compare the
diagnostics CSVs at equal cell counts.

## The parallel benchmark driver

`scripts/run_parallel_benchmark_comparison.sh` runs up to three simulations side by side on
disjoint sets of cores and monitors them: `v1` (a separate SimuCell3D 1.0 tree), `static` and
`adaptive` (this tree with `--schedule=static` and `--schedule=adaptive`). It is meant for runs
of hours to days.

Options:

| Option | Effect |
|---|---|
| `--modes=MODE1,MODE2` | Modes to run, from `v1`, `static`, `adaptive`. Default: `v1` and `adaptive`. |
| `--only=MODE` | Run a single mode. |
| `--all-modes` | Run all three modes. |
| `--skip-v1` | Exclude `v1` even if listed. |
| `--param-file=PATH` | Parameter file, relative to the repository root. The built-in default names a file that is not in the repository, so always pass this option. |
| `--v1-root=PATH` | Location of the 1.0 tree (default: `$V1_ROOT`, else `version-cpp-v1.0` next to this repository). |
| `--cores-per-sim=N` | Cores per simulation (default: divided automatically). |
| `--monitor-interval=N` | Seconds between status checks (default 60). |
| `--clean-rebuild` | Delete the contents of `build/` (and of the 1.0 tree's `build/`) and rebuild the enabled modes in Release before running. |
| `--no-auto-restart` | Do not restart a crashed or stalled simulation. |
| `--resume=DIR` | Resume monitoring of an interrupted benchmark directory. |
| `--quick-test` | Stop each simulation after 60 s (checks the setup). |
| `--dry-run` | Print the plan without running. |

Example, comparing the two scheduling modes of this tree on the long-run growth
configuration:

```console
$ cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
$ scripts/run_parallel_benchmark_comparison.sh --modes=static,adaptive \
    --param-file=parameters/longrun_fast_growth.xml --dry-run
```

Remove `--dry-run` to start. The script writes to `doc/working/parallel_benchmark_<timestamp>/`:

```text
parallel_benchmark_<timestamp>/
├── sim_static/, sim_adaptive/      simulation output and performance_diagnostics.csv per mode
├── logs/                           stdout and stderr of each simulation (<mode>.log)
├── metrics/health_log.csv          start, crash, stall, restart and completion events
├── state/                          process ids
├── metadata.json                   machine and configuration
├── checkpoint.json                 progress, for --resume
└── SUMMARY.md                      final table per mode
```

The simulations run in the background with `nohup`, so the script can be stopped and
monitoring resumed with `--resume=DIR`. `scripts/monitors/parallel_benchmark_monitor.sh DIR
--watch` (or `--latest --watch`) shows a read-only live view of a running benchmark.

:::{note}
The `v1` mode passes `--diagnostics-csv` to the 1.0 executable. Unmodified 1.0 requires exactly
one argument, the parameter file, and exits otherwise, so `v1` mode needs a 1.0 tree patched to
accept that flag.
1.0 also does not pin threads; the script binds each mode to its cores with `taskset`.
:::

## Processing and plots

After a benchmark (or on a running one), produce the plots with:

```console
$ scripts/generate_visualizations.sh doc/working/parallel_benchmark_<timestamp> --quality draft --no-latex
```

`generate_visualizations.sh` runs two steps, which can also be run separately:

1. `python3 scripts/preprocess_benchmark_metrics.py DIR` converts each
   `sim_<mode>/performance_diagnostics.csv` and `simulation_statistics.csv` into
   `metrics/<mode>/{computational,phase_timings,biological,workload}.csv` and
   `metrics/comparison.csv`.
2. `python3 scripts/plot_benchmark_unified.py DIR` validates and cleans the data, then writes
   the plots, `validation_report.json`, `audit_trail.json` and `manifest.json` to
   `DIR/plots-unified/`. Options: `--quality {publication,draft}` (300 or 150 dpi),
   `--plots {all,biological,computational,<id>,...}`, `--formats png,pdf,svg`,
   `--validate-only`, `--skip-sim-stats`, `--no-latex`, `--force`, `--output-dir`, `-v`.

The unified script registers 17 plots, numbered 01 to 17 (with 02b, without 12), in two groups:
biological (pressure, energy, cell heterogeneity, population dynamics, energy conservation,
contact fraction, a summary dashboard) and computational (scaling, phase timing, roofline, load
balance, scheduler comparisons, cache efficiency, iteration-rate timeline). The helper package
`scripts/simucell3d_viz/` holds its data validation, cleaning and statistics code. The scaling
plot draws an $N^{4/3}$ reference curve for comparison; it is not a fit.

`generate_visualizations.sh --legacy` runs the older `plot_benchmark_results.py` instead, and
`--all-scripts` runs the legacy, the intermediate `plot_benchmark_results_updated.py` and the
unified script. `scripts/visualization/` holds single-purpose plotting scripts that take file
paths as positional arguments.

## Other benchmark material

| Path | Content |
|---|---|
| `test/test_benchmarks/` | Micro-benchmarks registered as CTest targets `benchmark_*` ({ref}`dev-performance-internals`). |
| `scripts/benchmarking/latency/` | `perf_wrapper.sh` (hardware counters as JSON) and `roofline_analysis.py`. |
| `scripts/benchmarking/stability/` | Long-run memory monitoring and Valgrind ({ref}`dev-testing`). |
| `scripts/benchmarking/scientific/reproduce_fig3e.sh` | Runs `parameters/benchmarks/fig3e_monolayer.xml` and `fig3e_multilayer.xml` in sequence, logging to `simulation_results/nature_fig3e/<case>/`; the simulation output goes to the folder named in each parameter file. |
| `scripts/runs/launch_longrun_20k.sh` | Starts `parameters/longrun_fast_growth.xml` in the background with `--schedule=adaptive` and a diagnostics CSV; the run used for the release growth check ({ref}`dev-release`). |

:::{seealso}
- {doc}`/user_guide/18_parallel_performance` for the user-facing options.
- {ref}`dev-ci` for the performance job on pull requests.
:::
