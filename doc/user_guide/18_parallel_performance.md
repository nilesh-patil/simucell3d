(ug-parallel-performance)=
# Parallel performance

SimuCell3D runs on one shared-memory machine and parallelizes each iteration over the cells
with OpenMP. This chapter explains the choices that affect run time: the number of threads and
where they run, the OpenMP {term}`scheduling mode`, the contact detection algorithm, and the
parameters that determine the cost of an iteration. The thread count, the scheduling mode and
the contact detection algorithm do not change the model; with more than one thread, results
vary slightly between runs in any case (see [Reproducibility](#reproducibility)).

Implementation details of the scheduler and the performance monitor are in the developer
guide ({doc}`/developers/performance_internals`).

## Threads

By default, `simucell3d` starts one OpenMP thread per physical core and pins thread $i$ to one
CPU, so that no two threads share a core:

- **Which cores.** The cores are those of the CPUs the process is allowed to run on (its
  affinity mask). The mask is set by `taskset`, by a container runtime with a CPU set, or by a
  batch scheduler that binds jobs to CPUs. Physical cores are recognised from
  `/sys/devices/system/cpu/`; where that is not available, every logical CPU counts as a core.
- **Placement.** Threads go first to one logical CPU of each physical core, alternating
  between sockets on multi-socket machines, and only then to the second hyperthread of each
  core.
- **Override.** `--threads=N` sets the number of threads. With more threads than physical
  cores, the extra threads are placed on hyperthreads.
- **Pinning off.** `SIMUCELL3D_PIN_THREADS=0`, or any value of `OMP_PLACES`, switches
  pinning off and leaves placement to the OpenMP runtime.
- `OMP_NUM_THREADS` is ignored.

The start of every run reports the decision, for example:

```text
Thread count: Using 8 physical cores (out of 16 allowed logical CPUs, 2 threads/core, 1 socket(s))
Thread pinning: enabled, thread i -> CPU 0,1,2,3,4,5,6,7
```

:::{versionadded} 2.1
Thread pinning and the physical-core default. Before 2.1, threads could end up in pairs on
the two hyperthreads of one core while other cores were idle: on a machine with 4 cores and
8 hardware threads, runs used two of the four cores.
:::

### Several runs on one machine

Each run pins its threads starting from the first CPU of its affinity mask. Two runs started
side by side with the default settings therefore pin their threads to the **same** CPUs and
compete for them. Give each run its own set of CPUs:

```console
$ taskset -c 0-3 ./simucell3d ../parameters/core/parameters_sheet.xml --output-dir=../results/sheet &
$ taskset -c 4-7 ./simucell3d ../parameters/core/parameters_tube.xml --output-dir=../results/tube &
```

Each run then uses the physical cores among its CPUs. On a machine with hyperthreads, check
with `lscpu --extended` which logical CPUs belong to the same core, and give each run whole
cores. Alternatively, set `SIMUCELL3D_PIN_THREADS=0` and `--threads=N` for every run and let the
operating system distribute them.

On a cluster, request whole cores from the batch system and let `simucell3d` count them; pass
`--threads=N` if the batch system does not restrict the affinity mask.

## Scheduling modes

The loops over cells are not balanced: cells differ in size, number of faces and number of
contacts. The OpenMP schedule decides how the cells are distributed over the threads. It is set
with `--schedule`.

`--schedule` governs only the loops whose schedule is chosen at run time. Several loops have a
fixed schedule that no option changes: in the default build (contact model 2 with the spatial
grid), the contact-detection loops and the division loop are fixed, and the time integration
runs on one thread. With contact models 0 and 1, or with `sweep_and_prune`, the option also
governs contact detection and time integration.

`adaptive` (default)
: Each phase of an iteration gets its own schedule: dynamic for contact detection, guided for
  polarization, internal forces and time integration, and static for the face-type update and
  the mesh refinement. A base schedule for the remaining loops is chosen at start from the
  number of cells and an estimate of how unequal the work per cell is, and is changed when the
  run moves between the phases `INITIALIZATION` (fewer than 10 cells), `GROWTH` (more than 1% of
  the cells divided in the last 50 iterations) and `HOMEOSTASIS`. Messages starting with
  `[Iteration N]` on standard output report these changes.

`static`, `dynamic`, `guided`
: One OpenMP schedule of that kind for all loops whose schedule is chosen at run time.

In every mode the chunk size is adjusted on the iteration after cells divide, and halved when
more than 10% of the cells divided at once. With `dynamic` and `guided` it is also recomputed
every 50 iterations when the workload estimate has changed by more than 20%.

The workload estimate is the coefficient of variation of a per-cell cost model (number of
faces, contacts, growth and other terms). It is written as `cov` in the
{term}`diagnostics CSV` ({doc}`/user_guide/14_output_files`).

`adaptive` is the recommended mode. If a run spends most of its time in one phase (see
[Finding where the time goes](#finding-where-the-time-goes)), a short comparison of the four
modes on the same tissue, with the same thread count, shows whether a fixed schedule is faster
for it.

:::{versionchanged} 2.1
`--schedule=auto` and `--enable-per-loop-scheduling` were removed; `adaptive` covers both
({doc}`/user_guide/13_running_simulations`).
:::

## Contact detection algorithm

Finding the pairs of nearby faces of different cells is the broad phase of contact detection.
It is chosen with `<contact_detection_algorithm>` in the parameter file
({doc}`/user_guide/11_parameter_file`):

`uspg` (default)
: A uniform spatial partitioning grid (USPG): space is divided into cubic voxels, each face is
  binned into the voxels that its bounding box (enlarged by the contact cutoff) overlaps, and
  only faces that share a voxel are compared.

`sweep_and_prune`
: Sweep-and-Prune on the bounding boxes of the cells (enlarged by the contact cutoff), sorted
  along one axis, to find pairs of cells that may touch; only the faces of those pairs are then
  compared.

`adaptive`
: The grid below 500 cells and Sweep-and-Prune from 500 cells on.

The choice applies only to contact model 2 (face-face coupling, the default build); contact
models 0 and 1 always use the grid. The choice affects only how candidate face pairs are
found; the contact forces are computed by the same code. The shipped parameter files all use
the default. Which algorithm is faster depends on the tissue; compare them on a short run.

## What determines the cost of an iteration

The work per iteration grows with the number of cells and with the number of faces per cell.

- **Number of cells.** In the growth benchmark the wall time of one iteration grows as about
  $N^{1.10}$ in the number of cells $N$, and the cumulative wall time to reach $N$ cells as
  $N^{1.333}$ (see [Measured run times](#measured-run-times)). The second exponent matches the
  $\mathcal{O}(N^{4/3})$ that Runser, Vetter & Iber (2024) report for a growth simulation; the
  two differ because a growing tissue needs more iterations per doubling as it gets larger.
- **Faces per cell.** The mesh refiner keeps edge lengths between `min_edge_length` and three
  times that value ({doc}`/user_guide/05_mesh_refinement`), so the number of faces of a cell
  of area $A$ grows as $A / l_{\min}^2$. Halving `min_edge_length` roughly quadruples the
  faces per cell and the cost per cell.
- **Contacts.** Larger contact cutoffs give more candidate pairs per face. Contact detection is
  the largest phase: in the growth benchmark it takes about 60% of the timed iteration at
  100 cells and 75% at 145,000 cells.
- **Time step.** The run needs `simulation_duration / time_step` iterations, so the wall time per
  simulated second is close to proportional to $1/\Delta t$. The largest stable time step
  depends on the tissue and the mesh resolution; a time step that is too large shows up as
  unstable cells being removed. See [Time step](13_running_simulations.md#time-step) and
  {ref}`benchmarks-time-step`.
- **Output.** Every mesh pair is written as ASCII text; a short `sampling_period` slows the run
  and fills the disk ({doc}`/user_guide/14_output_files`).

(reference-figures)=
(ug-parallel-measured)=
### Measured run times

The November 2026 benchmark measured 2.1.0 on 8-core cloud machines (GCP c2-standard-16,
8 threads on 8 physical cores, 64 GB of memory, one run per machine). The figures below come
from it; they hold for that hardware and those files, and are not guarantees for other
machines. The full tables, the method and the comparison with earlier versions are in
{ref}`benchmarks-performance`.

**Growth benchmark** (the lab's `parameters_benchmarck.xml` of release 1.0, contact model 1,
time step `2.5e-8` s; three runs):

| Quantity | 2.1.0, measured |
|---|---:|
| Wall time to 10,000 cells | 1.28 ± 0.01 h |
| Wall time to 125,000 cells | 37.7 ± 0.8 h |
| Cumulative wall time to $N$ cells, from 10,000 cells on | grows as $N^{1.333 \pm 0.010}$; locally $N^{1.36}$ above 30,000 cells |
| Wall time per iteration | about 2.6 s at 10,000 cells, about 44 s at 140,000 cells; grows as $N^{1.10}$ |
| Memory (maximum resident set) | 0.13 GiB per 1,000 cells; 18.9 GiB at 145,000 cells |
| Share of the 8 cores kept busy over a 48 h run | 83% |

**Fixed-length tissues** (contact model 2, shipped meshes, time step `1e-7` s):

| Run | 2.1.0, measured |
|---|---:|
| Vesicle, 100,000 iterations (434 cells) | 6.20 ± 0.95 h |
| Tube, 100,000 iterations (192 cells) | 3.7 h |
| Mono- or multilayer vesicle, 1,736 iterations (paper protocol) | 0.18 h |

### Sizing a run

These rules of thumb are fitted to the measurements above. Use them to choose a machine and a
wall-time limit, then check them with a short run of your own file.

- **Wall time of a growth run.** On the benchmark hardware, the growth benchmark at
  `time_step` `2.5e-8` s needs about $T(N) \approx 1.28\ \text{h} \times (N/10{,}000)^{4/3}$
  to reach $N$ cells: about 5.5 h for 30,000 cells, 14 h for 60,000 and 37 h for 125,000. At
  another time step, multiply by `2.5e-8` s divided by your `time_step`. A different tissue, mesh
  resolution or contact model changes the prefactor; measure it from the first few thousand
  cells of your run and extrapolate with the same exponent.
- **Wall time of a fixed-length run.** Measure the wall time per iteration from two rows of the
  {term}`diagnostics CSV` (difference of `wall_epoch` divided by 100) and multiply by
  `simulation_duration / time_step`. For a tissue that does not grow, the cost per iteration
  stays nearly constant.
- **Memory.** Allow about 0.13 GiB per 1,000 cells of the benchmark tissue (about 140 KB per
  cell), plus room for the operating system. The largest measured point, 18.9 GiB at 145,000
  cells, used less than a third of the 64 GB machine. Cells with more faces (a smaller `min_edge_length`) need proportionally more.
- **Disk.** Each mesh pair is written in full; estimate the size of one pair from the first
  output file and multiply by `simulation_duration / sampling_period`
  ({doc}`/user_guide/14_output_files`).
- **Cores in use.** `/usr/bin/time -v` reports "Percent of CPU this job got". Divided by the
  number of threads, it gives the share of the cores kept busy; 2.1.0 reached 83% on 8 threads in
  the growth benchmark. A much lower value points to other processes on the same cores (see
  [Several runs on one machine](#several-runs-on-one-machine)).

:::{note}
The benchmark ran every case with 8 threads, so it does not measure how the run time changes
with the thread count. Runser, Vetter & Iber (2024) report, for release 1.0, that the parallel
efficiency follows Amdahl's law (their Supplementary Fig. 6) and that growth from one cell to
125,000 cells took one day on an Intel Xeon W-2245 (eight cores, 3.9 GHz) with 16 threads and a
time step of `1e-7` s. That configuration is not stable in 2.1.0 on the benchmark file
({ref}`benchmarks-time-step`), and the machine, thread count and time step differ from the
benchmark's, so the two figures are not a like-for-like comparison.
:::

Where the speed of 2.x comes from, as measured: compared with 1.0 with two crash fixes, 2.1.0
reaches 10,000 cells in about half the wall time (1.28 h against 2.53 h), mostly because it
keeps 83% of the cores busy against 49%, and it needs 39% less memory per cell. On the vesicle
and tube (contact model 2), 2.0 was 1.5-1.8 times faster than 1.0 with two crash fixes, and the
2.1 change to face-face contact (the compact face grid) added a further factor of 1.4 on the
vesicle and 1.2 on the tube. That change does not affect contact model 1, which the growth
benchmark uses. Because 2.x also changes the physics ({doc}`/whats_new/v2.1`), these are
comparisons of cost for the same input files, not of identical computations.

## Finding where the time goes

Two outputs show where a run spends its time:

- The {term}`diagnostics CSV` gives, every 100 iterations, the mean time per iteration of mesh
  refinement, contact detection, polarization and internal forces, and time integration, with
  the number of cells. The difference of `wall_epoch` between rows, divided by 100, is the
  wall-clock time per iteration, including output and divisions.
- At the end of a run, standard output shows a performance report with the time spent in each
  of these phases.

To measure the effect of a setting, run the same parameter file for a few hundred iterations
(reduce `simulation_duration`), once per setting, with the same `--threads`, and compare the
per-phase times.

(ug-parallel-reproducibility)=
## Reproducibility

Runs of more than one cell are not bit-identical from one run to the next, in 1.0 as in 2.1.
There are two sources:

- **A clock-seeded random generator.** The surface sampling of the initial triangulation and of
  every division interface, and the growth rates and division volumes when their standard
  deviation is not zero, are drawn from generators seeded from the system clock. A run that
  uses any of them differs from the next one even with `--threads=1`.
- **Threading.** With more than one thread, runs differ even when nothing random is drawn. The
  likely cause is the order in which contact forces are added, which depends on thread timing;
  the mechanism has not been isolated.

A run is repeatable value by value only on one thread, without divisions, with standard
deviations of 0 and with `perform_initial_triangulation` off (see {doc}`04_time_integration`
and {doc}`/faq`).

The November 2026 benchmark measured the size of the differences with three
{term}`replicates <replicate>` per case on 8 threads ({ref}`benchmarks-reproducibility`):

- Growth benchmark: replicates differ from the first iteration, because the initial
  triangulation draws random points. The wall time to 60,000 cells varies by up to 2.1% between
  replicates, and the final cell count of 48 h runs by about 2%.
- Vesicle (no division, no random draws): replicates differ from the second sampled iteration
  on. Mean pressure varies by 0.1-1.0% between replicates and median sphericity by less than
  0.1%.
- Single-cell analytic cases (no initial triangulation, no division, standard deviations 0):
  the statistics files were identical byte for byte in every 2.x build. The benchmark did not
  repeat these runs within one build.

Compare runs statistically, with at least three replicates for any effect of the order of a few
percent.

:::{seealso}
- {doc}`/user_guide/13_running_simulations`: `--threads`, `--schedule` and the environment
  variables.
- {doc}`/developers/performance_internals`: how the adaptive mode and the pinning are
  implemented.
- {doc}`/developers/benchmarking`: the benchmark scripts.
:::
