(dev-benchmarking)=
# Benchmarking

This page describes how to measure the speed of SimuCell3D and compare two builds or two
{term}`scheduling modes <scheduling mode>`: the measurement method, the benchmark driver
script, the plotting tools, and how the November 2026 cross-version benchmark was run so that a
slice of it can be repeated. It does not list results: the benchmark results are on the
{ref}`benchmarks` pages, and the measurements recorded for a release are in `CHANGELOG.md` with
their conditions. The code that is being measured is described in
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
   the plots, `validation_report.json`, `validation_report.html`, `audit_trail.json` and
   `manifest.json` to
   `DIR/plots-unified/`. Options: `--quality {publication,draft}` (300 or 150 dpi),
   `--plots {all,biological,computational,<id>,...}`, `--formats png,pdf,svg`,
   `--validate-only`, `--skip-sim-stats`, `--no-latex`, `--force`, `--output-dir`, `-v`.

The unified script registers 17 plots, numbered 01 to 17 (with 02b, without 12), in two groups:
biological (pressure, energy, cell heterogeneity, population dynamics, energy conservation,
contact fraction, a summary dashboard) and computational (scaling, phase timing, roofline, load
balance, scheduler comparisons, cache efficiency, iteration-rate timeline). The helper package
`scripts/simucell3d_viz/` holds its data validation, cleaning and statistics code. The scaling
plot (`04`) fits a power law to the time per iteration against the cell count, with 95 %
prediction intervals and diagnostic panels (Q-Q, residuals, Cook's distance). It also draws a
separate $N^{4/3}$ reference line through the medians; that line is not a fit.

`generate_visualizations.sh --legacy` runs the two older scripts instead of the unified one:
`plot_benchmark_results.py` into `plots/` and `plot_benchmark_results_updated.py` into
`plots-updated/`. `--all-scripts` runs all three. `scripts/visualization/` holds single-purpose plotting scripts that take file
paths as positional arguments.

(dev-benchmarking-bench2)=
## The November 2026 cross-version benchmark

In November 2026 the releases 1.0, 2.0 and 2.1.0 and four intermediate builds were run on the
same machines and the same input files. The results, and the canonical description of the
method, are in {ref}`benchmarks-method`. This section says how the runs were set up, so that a
developer can repeat one case on one build. The harness that created the machines, launched the
runs and copied their output is not part of the repository; everything a single run needs is.

### Builds

Each build is a commit compiled in Release mode, once per set of compile-time switches.

| Build | Source |
|---|---|
| 1.0 | tag `v1.0` (38af451) |
| 1.0 with two crash fixes | `v1.0` plus the local-id resynchronisation after a cell removal (with the fix of the daughter-append race that it needs, as in f61b620) and the face-winding fix of `split_edge` (as in 3a0fd76); the patches are not in the repository |
| 2.0 | tag `v2.0` (343bd98) |
| four intermediate builds | 9acfe36, ba74eed, f08df16 (the merge of 1fe5966), a331169 |
| 2.1.0 | tag `v2.1.0` (f824dfd) |

Build a tagged version from a separate worktree, never by checking out over a working tree with
changes:

```console
$ git worktree add ../simucell3d-v2.1.0 v2.1.0
$ cd ../simucell3d-v2.1.0
$ cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
```

1.0 and 2.0 declare an older minimum CMake version; with CMake 4 add
`-DCMAKE_POLICY_VERSION_MINIMUM=3.5` to the configure command.

### Tissues and cases

Five tissues, all with the parameters of the paper or of the files shipped with the code:

| Tissue | Input | Compile-time switches |
|---|---|---|
| Growth benchmark | the lab's `parameters_benchmarck.xml` of 1.0 (`git show v1.0:parameters_benchmarck.xml`; not shipped since 2.0), at four time steps: `1e-7` (as shipped), `5e-8`, `2.5e-8` and `1.25e-8` s | `CONTACT_MODEL_INDEX 1`, `POLARIZATION_MODE_INDEX 0`, `WRITE_NORMALS_IN_OUTPUT_MESH_FILE false` |
| Vesicle, $\varepsilon$ sweep | `parameters/benchmarks/fig3e_monolayer.xml` with no growth and an apical surface tension of $\varepsilon$ × `4e-4` N/m, for $\varepsilon$ = 1, 1.5, 2, 3, 4; 100,000 iterations | defaults |
| Mono- and multilayer vesicles | `fig3e_monolayer.xml` and `fig3e_multilayer.xml` cut to the paper's protocol (1,736 iterations), and the two files as shipped (`1e-2` s) | defaults |
| Tube | `parameters/core/parameters_tube.xml` | defaults |
| Analytic cases | `params_single_sphere.xml`, `params_laplace_pressure.xml`, `params_two_spheres_stable.xml` and `params_two_spheres_contact.xml` in `test/test_scientific_benchmarks/`, and the two two-sphere files again with a minimum edge length of `1.5e-7` m | defaults |

The 20 cases that every build was given are: the growth benchmark at the four time steps, the
five $\varepsilon$ values, the four mono- and multilayer files, the tube, and six analytic files.
The growth benchmark is not the reconstructed configuration `parameters/longrun_fast_growth.xml`
(contact model 2) that the release growth run uses ({ref}`dev-release`); do not compare numbers
from the two.

For the growth benchmark file with 2.1.0, the recommended time step is `2.5e-8` s: it was clean
(no cell removed) in three of three 48-hour runs. This is a measurement, not a guarantee; the
time-step results of every build are in {ref}`benchmarks-time-step`.

The edits made to the shipped files (output paths, time steps, sampling periods, the end time of
the paper protocol, a stray XML tag) are listed in {ref}`benchmarks-method`. The two-sphere
variants exist because the shipped two-sphere meshes have edges of 0.18 to 0.31 µm, below the
shipped minimum edge length of `7.5e-7` m.

### Machine and launch

One run per machine, on Google Cloud `c2-standard-16` instances (8 physical cores, 16 logical
CPUs, 64 GB), with 8 OpenMP threads, one per physical core. The same pinning was used for
every build, because 1.0 has no `--threads` option and does not pin its threads:

```bash
export OMP_NUM_THREADS=8 OMP_PROC_BIND=true OMP_DYNAMIC=false OMP_WAIT_POLICY=passive
export OMP_PLACES="{0},{1},{2},{3},{4},{5},{6},{7}"
/usr/bin/time -v taskset -c 0-7 stdbuf -oL -eL \
    ./build/simucell3d run.xml --threads=8 --schedule=adaptive \
    --output-dir="$OUT" --diagnostics-csv="$OUT/performance_diagnostics.csv"
```

The flags after `run.xml` are for 2.1.0 and the intermediate builds; 2.0 has no `--threads`
and gets `taskset -c 0-15` (it derives its thread count from the affinity mask), and 1.0 takes
the parameter file as its only argument. On these machines CPUs 0 to 7 are on distinct physical
cores; check `/sys/devices/system/cpu/cpu0/topology/thread_siblings_list` before reusing the
command on another machine type. Because `OMP_PLACES` is set, 2.1.0 prints
`Thread pinning: disabled` and leaves the placement to the OpenMP runtime
({ref}`dev-performance-internals`). Each output line was prefixed with a UTC time stamp, and wall
times were taken from these stamps, not from a wall-clock limit.

The stopping rules (150,000 cells or 48 hours for the full growth runs, 3 hours for the
time-step sweep, a fixed number of iterations for the other tissues) and the replicates are in
{ref}`benchmarks-method`. Besides these, a run was stopped as a hang after two hours without
progress in its log and with the same function in repeated stack samples, and on swap
thrashing. Failed runs were not restarted: a crash is a
result.

### Outcomes

A case counts as **completed** when every run of it reached its end, or its planned time cap,
with no cell removed by the solver. Deaths at the minimum volume and refused divisions do not
count against it. A run is **usable** when it reaches its last iteration with no cell removed.
The full definitions are in {ref}`benchmarks-method`. Read the outcome of a run from:

- the end of `run.log`: an exception message, a signal, or a normal end;
- `grep -c "Removing numerically unstable cell" run.log` and
  `grep -c "Removing cell .* mesh refinement failed" run.log`;
- the last row of `simulation_statistics.csv` for the cell count and simulated time.

### Benchmark workflows

No workflow runs on a schedule; there is no nightly or weekly benchmark. Two workflows measure
performance, and both are informational ({ref}`dev-ci`):

- `perf.yml` times a pull request against its base on the same runner (20 iterations of the
  434-cell vesicle, five repetitions each) and comments the result.
- `long.yml` runs the micro-benchmarks (`ctest -L benchmark`) in its Release build and writes
  their output to the job summary.

Neither is a substitute for the cross-version benchmark, which needs dedicated machines and
hours to days per run.

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
