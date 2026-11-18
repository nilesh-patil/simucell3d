(dev-performance-internals)=
# Performance internals

This page explains how the solver chooses its thread count, places its threads, schedules its
OpenMP loops and measures itself, and how to profile it. The user-facing options (`--threads`,
`--schedule`, the contact detection algorithm) are described in
{doc}`/user_guide/18_parallel_performance`; how to run comparative benchmarks is described in
{ref}`dev-benchmarking`.

## Parallel structure

All parallelism is OpenMP over cells: each parallel loop in `solver::run_iteration()` iterates
over `cell_lst_`, and the work of one cell (refining its mesh, computing its internal forces,
integrating its nodes) is done by one thread. The cost of a cell grows with its number of faces
and with the number of its contacts, so the work per loop iteration is uneven, which is why the
loop schedule matters. The face-face contact model (index 2) additionally splits the contact
work of very large cells into several work units (`contact_face_face_via_coupling::build_work_units()`).

A loop follows the selected schedule only if it is written with `schedule(runtime)`; the
schedule is then set with `omp_set_schedule()` before the loop. The loops over cells in
`run_iteration()`, `local_mesh_refiner::refine_meshes()`,
`time_integration_scheme::update_nodes_positions()` and the sweep-and-prune path of the contact
model use `schedule(runtime)`. Other loops fix their schedule in the pragma: the face numbering
and grid construction of contact model 2 use `schedule(static)`, its grid-based contact
resolution uses `schedule(dynamic, 1)` over work units, and the loop in `cell_divider::run()` has
no schedule clause (implementation default). New parallel loops over cells should use
`schedule(runtime)` unless there is a reason to fix the schedule.

## Thread count and placement

Code: `solver::solver()` in `src/solver.cpp` and `include/cpu_topology.hpp`.

1. **Allowed CPUs.** `cpu_topology::allowed_cpus()` returns the logical CPUs the process may
   use: the OpenMP place list if one exists, otherwise the affinity mask
   (`sched_getaffinity`, which reflects `taskset` and cgroup CPU sets), otherwise all
   processors. The result is computed once per process and cached, because after the first
   solver has pinned the calling thread the affinity mask reports a single CPU.
2. **Topology.** `cpu_topology::detect()` groups the allowed CPUs into physical cores from
   `/sys/devices/system/cpu/cpu<N>/topology/{physical_package_id,core_id}`. Without sysfs,
   each logical CPU counts as a core. There is no parsing of Slurm variables or cgroup CPU
   quotas.
3. **Thread count.** One thread per physical core, unless `--threads=N` (or the `nb_threads`
   argument of the Python module) is positive. The solver calls `omp_set_num_threads()`, so
   `OMP_NUM_THREADS` has no effect on a run.
4. **Pinning.** Unless `SIMUCELL3D_PIN_THREADS=0` or `OMP_PLACES` is set, OpenMP thread $i$ is
   pinned with `pthread_setaffinity_np` to the $i$-th CPU of
   `cpu_topology::placement_order()`: first one CPU of every physical core, then the
   hyperthread siblings. The solver pins its own threads because libgomp reads
   `OMP_PROC_BIND`/`OMP_PLACES` before `main()` runs, and with `OMP_PROC_BIND=close` but no
   `OMP_PLACES` libgomp places consecutive threads on sibling hyperthreads, leaving physical
   cores idle.

The constructor prints the outcome:

```text
Thread count: Using 8 physical cores (out of 16 allowed logical CPUs, 2 threads/core, 1 socket(s))
Thread pinning: enabled, thread i -> CPU 0,1,2,3,4,5,6,7
```

The CPU list in the second line depends on how the kernel numbers sibling hyperthreads.

## Loop scheduling

Code: `solver::solver()`, `initialize_per_loop_schedules()`, `update_workload_heterogeneity()`,
`adaptive_schedule_update()`, `handle_division_event()` and the file-static helpers at the top
of `src/solver.cpp`.

### Workload heterogeneity

`calculate_workload_heterogeneity()` assigns each cell an estimated cost, starting from its face
count and adding terms for contacts (scaled by the fraction of coupled nodes and by the contact
model), time integration, polarization, growth and large meshes; static cells get a reduced
contact term and no integration term. The estimate is clamped to $[0.2, 2.5]$ times the face
count. The function returns the coefficient of variation (CoV, standard deviation divided by
mean) of these estimates over all cells. It is a model of the work, not a measurement. The
`cov` column of the diagnostics CSV is this value, and `thread_imbalance_pct` is the same value
multiplied by 100.

### Chunk size

`calculate_optimal_chunk_size()` returns 0 (OpenMP default) for static scheduling. Otherwise it
returns $\max(1, \lfloor N / (4T) \rfloor)$ for $N < 100$ cells and $T$ threads, and for larger
$N$ the value $\lfloor N / (T\,d) \rfloor$ clamped to $[1, 100]$, with $d = 20$ for
CoV $> 0.6$, $d = 10$ for CoV $> 0.4$ and $d = 4$ otherwise. Guided scheduling uses twice the
dynamic chunk size.

### Modes

`static`, `dynamic`, `guided`
: The schedule kind is fixed for the whole run. Every 50 iterations
  `update_workload_heterogeneity()` recomputes the CoV; if it changed by more than 20 % and the
  resulting chunk size differs by more than 20 %, the chunk size is updated.

`adaptive` (default)
: At construction, a base schedule is chosen from the initial cell count with the table in
  `lookup_benchmark_mode()` (13 cell-count bands). `multi_factor_heuristic()` computes a second
  opinion from the CoV and the number of cells per thread; when the two disagree, the table
  wins and the disagreement is printed. In addition, four per-loop schedules are fixed at
  construction and selected before the corresponding phase of each iteration:

  | Phase | Kind | Chunk size |
  |---|---|---|
  | contact detection | dynamic | base chunk, at least 1 |
  | polarization, internal forces, time integration | guided | 2 x base chunk |
  | face type updates, mesh refinement | static | default |
  | cell division | dynamic | base chunk / 2, at least 1 |

  The chunk-size update of `update_workload_heterogeneity()` is skipped in this mode. Every 50
  iterations, `adaptive_schedule_update()` classifies the run as INITIALIZATION (fewer than 10
  cells), GROWTH (more than 1 % of the cells divided in the last 50 iterations) or HOMEOSTASIS,
  and on a phase change sets a new global schedule (dynamic, guided or static, depending on
  the phase, the CoV and the cell count). After a division, `handle_division_event()` reduces
  the chunk size of the global schedule at the start of the next iteration. Because each phase
  of the iteration selects its own per-loop schedule first, these global updates affect only
  loops that run outside the four phases.

The selection is printed once at start-up, in this form:

```text
Using OpenMP adaptive (base: static, per-loop: ON) scheduling
  Cells: 434, Threads: 8, Chunk size: 0
  Workload heterogeneity (CoV): <value>
```

The phase reported in the diagnostics CSV comes from `detect_simulation_phase()`.

:::{note}
The band table in `lookup_benchmark_mode()` encodes measurements made during 2.0 development on
one machine. It is a heuristic: a mode that is fastest on one machine and cell count need not be
fastest on another. Compare modes on the target machine with {ref}`dev-benchmarking` before
changing the defaults.
:::

## Synchronisation

- Each node owns a `std::mutex` (`include/mesh/node.hpp`). Contact model 2 updates the coupling
  maps of two nodes under both locks and adds contact forces to nodes of other cells with
  `vec3::atomic_translate()` (through `node::add_force_atomic()`).
- `vec3::translate()` contains `#pragma omp atomic` directives, but they have no effect because
  the `math_modules` library is compiled without OpenMP. A vector that several threads add to
  must be updated with `vec3::atomic_translate()`, which uses the GCC/Clang `__atomic`
  builtins.
- Before cells are removed, contact models 1 and 2 pass through an empty parallel region that
  acts as a barrier, so no thread still holds a node lock of a cell being destroyed.
- `omp_set_schedule()` after a division is deferred to the next iteration
  (`pending_division_update_`), so it is never called while a parallel region may still read
  the cell list.
- Results are not bit-reproducible across thread counts: the order of floating-point additions
  into a node depends on the schedule. The determinism tests run single-threaded
  ({ref}`dev-testing`).

## Performance monitor and diagnostics

`PerformanceMonitor` (`include/performance_monitor.hpp`) accumulates wall-clock timings of named
sections; `PerformanceMonitor::ScopedTimer` times a scope:

```cpp
{
    PerformanceMonitor::ScopedTimer timer(perf_monitor_, "contact_detection");
    contact_model_ptr_->run(cell_lst_);
}
```

The solver times four sections: `mesh_refinement`, `contact_detection`,
`polarization_and_internal_forces` and `time_integration`. Every 100 iterations
`solver::export_diagnostics()` appends one row to the diagnostics CSV with the average of each
timer over the window, then resets the timers. The file is
`<output folder>/performance_diagnostics.csv`, or the path given with `--diagnostics-csv`. Its
columns:

| Column | Meaning |
|---|---|
| `sim_time` | simulation time, s |
| `wall_epoch` | wall-clock time, seconds since the Unix epoch |
| `iteration` | iteration number |
| `cells` | number of cells |
| `divisions` | divisions in the last 100 iterations |
| `cov` | workload CoV (see above) |
| `phase` | INITIALIZATION, GROWTH or HOMEOSTASIS |
| `mesh_refinement_ms`, `contact_detection_ms`, `polarization_internal_forces_ms`, `time_integration_ms` | mean time per call of each section over the window, ms |
| `total_iteration_ms` | sum of the four section times, ms; excludes output writing, division and cell removal |
| `thread_imbalance_pct` | `cov` x 100 (an estimate, not a measurement) |
| `unstable_cells_removed` | cells removed by the stability checks or after a failed refinement in the window |

With verbose output (the command-line default), `solver::run()` prints the monitor's summary
table at the end of the run.

## Profiling

### Whole runs

Use a Release build (CMake's default flags, `-O3 -DNDEBUG`) with frame pointers for usable call
graphs:

```console
$ cmake -B build-prof -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-fno-omit-frame-pointer"
$ cmake --build build-prof -j
$ cd build-prof
$ perf record -g ./simucell3d ../parameters/core/parameters_vesicle_quick.xml --threads=4 --output-dir=prof_run
$ perf report
```

The phase timings in the diagnostics CSV are often enough to locate a regression; `perf` then
narrows it down within the phase.

### Micro-benchmarks

`test/test_benchmarks/` holds three executables registered as CTest targets with the prefix
`benchmark_`:

| Executable | Measures |
|---|---|
| `test_contact_latency` | broad-phase contact detection, USPG and sweep-and-prune, on cubic grids of 64, 216 and 1000 cell bounding boxes, plus a scaling run; 3 warm-up and 10 measured trials, median reported |
| `test_atomic_contention` | cost of an uncontended and contended `std::mutex`, atomic versus mutex accumulation, false sharing, the per-node mutex pattern |
| `test_memory_bandwidth` | sequential read bandwidth, `vec3` array traversal, random access latency, structure-of-arrays versus array-of-structures, bandwidth versus thread count |

They report numbers and always return 0, so they never fail a build. Run one directly from the
build directory:

```console
$ ./test/test_benchmarks/test_contact_latency test_uspg_latency_large
```

`scripts/benchmarking/latency/perf_wrapper.sh` runs a benchmark under `perf stat` and writes the
hardware counters as JSON (requires `perf` and, for non-root users, a permissive
`kernel.perf_event_paranoid`); `roofline_analysis.py` plots the JSON results on a roofline
model. From the repository root, after building into `build/`:

```console
$ scripts/benchmarking/latency/perf_wrapper.sh --all --output-dir=results/
$ scripts/benchmarking/latency/perf_wrapper.sh --summary results/*.json
$ python3 scripts/benchmarking/latency/roofline_analysis.py --json results/ --output roofline.png
```

`roofline_analysis.py` detects peak FLOP/s and bandwidth unless `--peak-flops` and
`--peak-bandwidth` (GFLOP/s, GB/s) are given; `--summary-only` skips the plot.

### Memory

`scripts/benchmarking/stability/` holds a long-run memory monitor and a Valgrind wrapper; see
{ref}`dev-testing`. With `numactl` available, `numastat -p <pid>` shows whether a run's memory
is spread over NUMA nodes on multi-socket machines.

:::{seealso}
- {doc}`/user_guide/18_parallel_performance`
- {ref}`dev-architecture`
:::
