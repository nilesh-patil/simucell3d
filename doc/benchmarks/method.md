(benchmarks-method)=
# Method

This page states once how the November 2026 benchmark was run and read. The other pages of the
section refer to it.

## Machines and threads

| Item | Value |
|---|---|
| Machine | GCP c2-standard-16 (Intel Cascade Lake), 64 GB RAM, one run at a time per machine |
| Threads | 8 OpenMP threads, pinned one per physical core (`taskset -c 0-7`, `OMP_PLACES={0},...,{7}`) |
| 2.0 | Launched with `taskset -c 0-15` and the same `OMP_PLACES`: 2.0 has no `--threads` option and derives its thread count from the affinity mask, which on this machine would give 4 threads |
| Period | 18 to 22 November 2026 |
| Size | 243 runs, 1,827 run-hours from launch to end (GNU `time` elapsed: 1,827.0 h); one run of 1.0 with two crash fixes failed on a harness error (missing input file), was rerun, and is left out of all counts |

Runs were launched in waves. The replicate runs of 1.0 with two crash fixes and 2.1.0 ran on
different machines from the first wave, and on the vesicle the 2.1.0 replicates were 11 to 19%
faster than its first run. Speed comparisons between builds are therefore made within one wave
where it matters ({ref}`benchmarks-performance`).

## Builds

| Name in these pages | Commit | What it is |
|---|---|---|
| 1.0 | `38af451` (tag `v1.0`) | The code published with the paper |
| 1.0 with two crash fixes | `38af451` plus two patches | The local-id resync after a cell removal (which also moves daughter appends out of the parallel division loop) and the face-winding fix in edge splitting: the smallest change that lets 1.0 survive its first cell removal |
| 2.0 | `343bd98` (tag `v2.0`) | First maintenance release |
| Intermediate builds | `9acfe36`, `ba74eed`, `f08df16`, `a331169` | Four commits between 2.0 and 2.1.0, used to locate where a change arrived |
| 2.1.0 | `f824dfd` (tag `v2.1.0`) | Release documented here, 18 November 2026 |

Every build was compiled twice: once with the shipped compile-time switches of
`include/global_configuration.hpp` (contact model 2, polarization mode 1) for the vesicle, tube and
analytic cases, and once with the switches written in the header of the growth benchmark file
(contact model 1, polarization mode 0, `WRITE_NORMALS_IN_OUTPUT_MESH_FILE false`) for the growth
benchmark ({doc}`/user_guide/10_compile_time_configuration`). The parameter-file schema is the same
in all eight builds: each requires the same 31 tags, so one input file runs unchanged on all of them.

## Tissues and input files

| Tissue | Input file | Mesh | In the 2.1.0 tree |
|---|---|---|---|
| Growth benchmark (paper Fig. 1e, Table 2) | `parameters_benchmarck.xml` | `data/input_meshes/sphere.vtk` (one cell) | No. The file is the lab's, in the `v1.0` tag only |
| Vesicle ε sweep | `parameters/benchmarks/fig3e_monolayer.xml`, apical tension set to ε × 4e-4 N/m and growth rate set to 0 | `data/input_meshes/fig_3_vesicle.vtk` (432 epithelial cells, lumen, ECM shell) | Yes (the file; the five ε values are edits) |
| Mono- and multilayer vesicle | `parameters/benchmarks/fig3e_monolayer.xml` and `fig3e_multilayer.xml` | `fig_3_vesicle.vtk` | Yes |
| Tube | `parameters/core/parameters_tube.xml` | `data/input_meshes/fig_1_tube.vtk` (192 cells) | Yes |
| Analytic one- and two-cell cases | `test/test_scientific_benchmarks/params_single_sphere.xml`, `params_laplace_pressure.xml`, `params_two_spheres_stable.xml`, `params_two_spheres_contact.xml` | `sphere.vtk`; `test/test_io/test_simulation_initializer/2_spheres.vtk` | Yes |

To repeat the growth benchmark with 2.1.0, extract the lab's file from the `v1.0` tag and build
with the switches in its header:

```console
$ git show v1.0:parameters_benchmarck.xml > parameters_benchmarck.xml
$ md5sum parameters_benchmarck.xml
10b02d4fb776a9efadecbca355d90bde  parameters_benchmarck.xml
```

Its mesh path `data/input_meshes/sphere.vtk` is relative and resolves from the repository root.
It is a different configuration from `parameters/longrun_fast_growth.xml`, the reconstructed
paper-like configuration of the growth example ({doc}`/examples/growth_from_one_cell`), which uses
contact model 2. Every growth-benchmark number in this section belongs to the lab's file with
contact model 1. Its contact cutoff is 0.5 µm; the paper's Table 2 default is 0.2 µm.

The edits made to the shipped files, all applied identically in every build:

- The output folder and absolute mesh paths, for every case.
- The time step of the growth benchmark: `1e-7` s as shipped, plus `5e-8` s, `2.5e-8` s and `1.25e-8` s.
- The output sampling period of the tube, from `1e-6` s to `1e-4` s, and of the paper-protocol
  mono- and multilayer runs, from `1e-4` s to `1e-5` s. The shipped long vesicle files already
  sample every `1e-4` s and were left unchanged. Sampling changes only how many mesh files are
  written; the statistics file is written every 50 iterations regardless.
- The mono- and multilayer vesicle "paper protocol" runs end at `1.736e-4` s, the time at which the
  mean target volume has doubled, as the paper describes ("grown ... until they had doubled in
  size"). The shipped files run for `1e-2` s; those are kept as separate cases ("shipped file").
- The "finer mesh" variants of the two-cell cases set `min_edge_length` to 1.5e-7 m.
- Three of the analytic files end with a stray `</output>` tag after which the XML reader stops
  silently; it was dropped.

## The 20 common cases

All builds ran the same 20 cases with the paper's parameters: the growth benchmark at four time
steps, the vesicle at five values of ε (1.0, 1.5, 2.0, 3.0, 4.0), the mono- and multilayer vesicle
by the paper protocol and with the shipped files, the tube, and six analytic one- and two-cell
cases. The four time steps and five ε values were chosen for the benchmark; two of the growth
cases are 3-hour runs. A further 29 runs used a second, literature-based parameter set on four builds
(time step `5e-8` s, except four growth runs at `2.5e-8` s); this section reports the paper-parameter runs unless a line says otherwise.

Stopping rules: the growth benchmark stops at 150,000 cells or 48 hours of wall time (3 hours for
the time-step sweep runs); the vesicle sweep and the tube run 100,000 iterations (`1e-2` s); the
long vesicle files run to `1e-2` s or 48 hours.

Replicates: three runs per build where a result was compared between versions. That is the
growth benchmark at `2.5e-8` s (1.0 with two crash fixes and 2.1.0 to 48 hours; the four
intermediate builds one run to 48 hours and two to 60,000 cells) and the vesicle sweep (1.0 with
two crash fixes and 2.1.0, three runs at each ε). Other cases ran once per build.

## Definitions

Outcome of one run
: *crash*: killed by a signal (SIGSEGV or SIGABRT). *Abort*: exit status 1, in 1.0 with the
  mesh-refinement error. *Hang*: no progress, stopped by the harness. Otherwise the run reached its
  end or its planned time cap; it then counts as *removes cells* if the solver removed at least one
  cell as unstable or after a failed refinement, and as *loses the tissue* if in addition its final
  cell count is at most half its maximum.

Completed case
: Every run of the case reached its end or its planned cap with no cell removed by the solver. A
  case is judged by its worst run. Deaths at the minimum volume and refused divisions are part of
  the model and do not count against completion; they are reported on
  {ref}`benchmarks-validity`.

Usable run
: One run that reached its last iteration with no cell removed. Usable means the run finished
  cleanly. It does not mean the tissue is biologically right.

Cell cycle
: Simulated time of the growth benchmark divided by `1e-5` s, the cell-cycle time used in the
  analysis. It compares growth between versions independent of wall time.

## How the numbers were read

- **Wall time of a whole run:** GNU `time -v` elapsed.
- **Wall time to a cell count:** from the time stamp of iteration 0 to the first iteration at which
  the running maximum of the cell count reaches that size, from the per-iteration lines of the run
  log.
- **Memory:** maximum resident set size from GNU `time -v`, in GiB ($2^{30}$ bytes).
- **Core utilisation:** (user + system CPU time) / (elapsed × 8), for runs longer than 15 minutes.
- **Removals and refusals:** counted from the log lines of each run, not from the diagnostics
  file, which omits the last incomplete window ({ref}`ug-safeguards`).
- **Scaling exponents:** ordinary least squares on log–log data. For cumulative wall time T(N),
  30 log-spaced cell counts from 10,000 to the end of each run; for seconds per iteration, 12 bins
  per decade. "±" is the standard deviation over the three runs, not the fit's standard error,
  which understates the uncertainty of cumulative data.

## Data handling and checks

Every run's output was copied off its machine and checksum-verified (md5 of every file) before the
machine was deleted. The figures in this section are drawn from small extracts of those runs,
stored next to these pages in `doc/benchmarks/data/` and plotted by
`doc/benchmarks/make_figures.py`. The growth curves in `growth_wall.csv` are thinned for plotting;
the milestone times and scaling exponents in the tables come from the full per-iteration logs. The data files use short codes (B0 to B7 for the builds in the order of the table above, T1 to
T5 for the five tissues, arm a and b for the two parameter sets); `doc/benchmarks/data/README.txt`
explains every code, outcome value and column.

Two independent passes recomputed the published results from the raw logs, the per-cell
statistics, the mesh snapshots and the repository. The first corrected four numbers, among them
the total compute, which had been overstated about fivefold by summing the scheduled 48-hour
deadlines instead of the actual run times. The second found seven more that were wrong, among them
a statement that the benchmark draws no random numbers (it does; see
{ref}`benchmarks-reproducibility`), and made more than thirty others more precise. The numbers on
these pages are the corrected ones.
