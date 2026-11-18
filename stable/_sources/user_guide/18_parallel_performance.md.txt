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
the two hyperthreads of one core while other cores were idle.
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
with `--schedule`:

`adaptive` (default)
: Every phase of an iteration sets its own schedule for its parallel loops: dynamic for contact detection, guided for
  polarization, internal forces and time integration, static for the face-type update and the
  mesh refinement, and dynamic with small chunks for the division check. A base schedule for
  the remaining loops is chosen at start from the number of cells and an estimate of how
  unequal the work per cell is, and is changed when the run moves between the phases
  `INITIALIZATION` (fewer than 10 cells), `GROWTH` (more than 1% of the cells divided in the
  last 50 iterations) and `HOMEOSTASIS`. Messages starting with `[Iteration N]` on standard
  output report these changes.

`static`, `dynamic`, `guided`
: One OpenMP schedule of that kind for all loops. For `dynamic` and `guided`, the chunk size is
  recomputed every 50 iterations from the number of cells and the workload estimate, and
  reduced temporarily after many divisions.

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
: Sweep-and-Prune on the bounding boxes of the faces, sorted along the coordinate axes.

`adaptive`
: The grid below 500 cells and Sweep-and-Prune from 500 cells on.

The choice applies only to contact model 2 (face-face coupling, the default build); contact
models 0 and 1 always use the grid. The choice affects only how candidate face pairs are
found; the contact forces are computed by the same code. The shipped parameter files all use
the default. Which algorithm is faster depends on the tissue; compare them on a short run.

## What determines the cost of an iteration

The work per iteration grows with the number of cells and with the number of faces per cell.

- **Number of cells.** Runser, Vetter & Iber (2024) report that the total time complexity of a
  growth simulation is $\mathcal{O}(N_c^{4/3})$ in the number of cells $N_c$.
- **Faces per cell.** The mesh refiner keeps edge lengths between `min_edge_length` and three
  times that value ({doc}`/user_guide/05_mesh_refinement`), so the number of faces of a cell
  of area $A$ grows as $A / l_{\min}^2$. Halving `min_edge_length` roughly quadruples the
  faces per cell and the cost per cell.
- **Contacts.** Larger contact cutoffs give more candidate pairs per face.
- **Time step.** The run needs `simulation_duration / time_step` iterations. The largest
  stable time step depends on the mechanical parameters and the mesh resolution; a time step
  that is too large shows up as unstable cells being removed
  ({doc}`/user_guide/09_safeguards`).
- **Output.** Every mesh pair is written as ASCII text; a short `sampling_period` slows the run
  and fills the disk ({doc}`/user_guide/14_output_files`).

### Reference figures

The measurements below are the only ones given for this purpose; they come with their
conditions and are not guarantees for other machines.

- Runser, Vetter & Iber (2024), with release 1.0: growth from one cell to 125,000 cells in one
  day of computation on an Intel Xeon W-2245 (eight cores, 3.9 GHz) with 16 threads, for cells
  with 121 nodes and 238 triangular faces on average. The paper reports that the parallel
  efficiency follows Amdahl's law (its Supplementary Fig. 6).
- 2.1 (from `CHANGELOG.md`): the compact face grid of face-face contact halves the contact
  step, from 1,059 to 500 ms per iteration at 2.6k cells with 4 threads. In 8-core runs of the
  reconstructed paper-like growth configuration (`parameters/longrun_fast_growth.xml`, not the
  lab's configuration; {doc}`/examples/growth_from_one_cell`), 2.1 needs
  44-46% less time per iteration at 30k cells than the development builds that preceded it,
  and reaches 30k cells in about 4 hours.

Because 2.1 also changes the physics ({doc}`/whats_new/v2.1`), run times of 1.0 and 2.1 are not
a like-for-like comparison.

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

## Reproducibility

- With more than one thread, two runs with the same input are not bit-identical: contact
  forces are summed in an order that depends on the thread timing, and the rounding
  differences grow over time.
- Growth rates and division volumes with a non-zero standard deviation are drawn from a random
  generator seeded from the clock, so runs that use them differ even on one thread.
- Cell division (to triangulate the division interface) and the initial triangulation sample
  points with a generator that is also seeded from the clock. A run in which cells divide therefore differs from one run to the
  next even with `--threads=1` and standard deviations of 0: in a test with
  `longrun_fast_growth.xml` on one thread, two runs were identical up to the first division and
  differed afterwards (per-cell volumes by up to 0.5% after 6,000 iterations).

A run on one thread is repeatable value by value only when cells do not divide, the standard
deviations are 0 and `perform_initial_triangulation` is off (see
{doc}`04_time_integration` and {doc}`/faq`). Compare runs with divisions statistically.

:::{seealso}
- {doc}`/user_guide/13_running_simulations`: `--threads`, `--schedule` and the environment
  variables.
- {doc}`/developers/performance_internals`: how the adaptive mode and the pinning are
  implemented.
- {doc}`/developers/benchmarking`: the benchmark scripts.
:::
