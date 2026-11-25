(ug-running)=
# Running simulations

This chapter is the reference for the `simucell3d` command: its arguments and options, the
environment variables it reads, how it resolves paths, what it prints, and how a run ends. It
assumes a build as described in {doc}`/install`; the executable is `build/simucell3d` (a
symbolic link to `build/src/app/simucell3d`).

## Synopsis

```text
simucell3d PARAMETER_FILE [--threads=N] [--schedule=MODE] [--output-dir=PATH]
                          [--diagnostics-csv=PATH]
simucell3d --version
```

A typical run, from the `build/` directory:

```console
$ cd build
$ ./simucell3d ../parameters/core/parameters_vesicle_quick.xml
```

The {term}`parameter file` is the only argument that does not start with `--`. Options may
come before or after it, in any order.

## Options

| Option | Default | Effect |
|---|---|---|
| `--threads=N` | one thread per physical core | Number of OpenMP threads, a positive integer. The default counts the physical cores of the CPUs the process may run on (its affinity mask, as set by `taskset`, a container or a batch scheduler), not the hyperthreads. See {doc}`/user_guide/18_parallel_performance`. |
| `--schedule=MODE` | `adaptive` | OpenMP {term}`scheduling mode`: `adaptive`, `static`, `dynamic` or `guided`. See {doc}`/user_guide/18_parallel_performance`. |
| `--output-dir=PATH` | `output_mesh_folder_path` from the parameter file | Write all output to `PATH` instead. A relative path is relative to the working directory, and the program prints a warning saying so. `PATH` must not be an existing file. Like the folder from the parameter file, it is deleted at start. |
| `--diagnostics-csv=PATH` | `<output folder>/performance_diagnostics.csv` | Write the {term}`diagnostics CSV` to `PATH`. Rows are **appended** if the file exists, and the header is written only to a new file. See {doc}`/user_guide/14_output_files`. |
| `--version` | | Print `SimuCell3D <version> (<git commit>)` and exit with status 0, whatever the other arguments. |

Running `simucell3d` without arguments prints a usage summary and exits with status 1.

:::{important}
An option that `simucell3d` does not recognise is ignored without a message, as long as it
starts with `--`. Check the spelling of every option: `--thread=4` (without the s) runs with
the default thread count.
:::

### Removed options

Two options of 2.0 were removed in 2.1 and now stop the program with an error:

| Option | Message | Replacement |
|---|---|---|
| `--schedule=auto` | `Error: --schedule=auto has been removed.` | `--schedule=adaptive`, the default. |
| `--enable-per-loop-scheduling` | `Error: --enable-per-loop-scheduling has been removed.` | None needed: per-loop scheduling is part of `--schedule=adaptive`. |

### Option errors

| Message | Cause |
|---|---|
| `Error: --threads requires a positive integer, got '<value>'` | `--threads=0`, a negative number, or a value that is not an integer. |
| `Error: --schedule must be 'static', 'dynamic', 'guided', or 'adaptive'` | Unknown scheduling mode. |
| `Error: --output-dir points to existing file, not directory: <path>` | `--output-dir` names a file. |
| `Error: --output-dir requires a directory path` | `--output-dir=` without a value. |
| `Error: --diagnostics-csv requires a file path` | `--diagnostics-csv=` without a value. |
| `Error: Multiple parameter files specified` | More than one argument without `--`. |
| `No input parameter file given in the command line` | Options only, no parameter file. |

## Environment variables

| Variable | Effect |
|---|---|
| `SIMUCELL3D_PIN_THREADS` | Set to `0` to switch off {term}`thread pinning`. Any other value, or no value, leaves it on. |
| `OMP_PLACES` | If set (to any value), SimuCell3D does not pin its threads and leaves placement to the OpenMP runtime. |
| `OMP_NUM_THREADS` | Has no effect. The thread count comes from `--threads` or from the number of physical cores. |
| `OMP_WAIT_POLICY` | Read by the OpenMP runtime, not by SimuCell3D. `scripts/runs/launch_longrun_20k.sh` sets it to `passive`, so that idle threads sleep instead of spinning. |

## Working directory and paths

`simucell3d` can be run from any directory. Paths are resolved as follows (details in
[Paths](11_parameter_file.md#paths)):

- The parameter file: as given, absolute or relative to the working directory.
- The input mesh: as given; if that file does not exist, relative to the source directory
  the executable was built from. The shipped parameter files rely on the second rule, and the
  run prints a `WARNING: the absolute mesh file path ... was not found` line followed by the
  path it uses instead.
- The output folder: as given, absolute or relative to the working directory. The shipped
  files write to `../simulation_results/<name>`, which is `simulation_results/<name>` in the
  repository when the run starts in `build/`.

:::{warning}
At start, `simucell3d` deletes the output folder (the one in the parameter file, or
`--output-dir`) with all its contents, then re-creates it. Running the same parameter file
twice overwrites the first run. Do not use a folder that contains other data, such as your
home directory or the repository root.
:::

Keep anything that must survive a restart outside the output folder, in particular the log
of the run and, if you collect diagnostics across restarts, the `--diagnostics-csv` file.

## What a run prints

The program reports the set-up and then one progress line per iteration. Abridged output of
`parameters_vesicle_quick.xml` with `--threads=4`, both streams together (the two `WARNING`
lines about the mesh path are on standard error, the rest on standard output):

```text
WARNING: the absolute mesh file path: data/input_meshes/fig_3_vesicle.vtk was not found
The program will try instead the relative path: <source directory>/data/input_meshes/fig_3_vesicle.vtk

Triangulating cell 0 of type: lumen
Triangulating cell 1 of type: epithelial
...
Thread count: User-specified 4 threads (4 physical cores, 8 allowed logical CPUs)
Thread pinning: enabled, thread i -> CPU 0,1,2,3
Using OpenMP adaptive (base: static, per-loop: ON) scheduling
  Cells: 434, Threads: 4, Chunk size: 0
  Workload heterogeneity (CoV): 1.743
The output folder is: "<output folder>"
Progression 0%, iteration: 0, file number 1, nb cells 434
Progression 0%, iteration: 1, file number 1, nb cells 434
Progression 1%, iteration: 2, file number 1, nb cells 434
...
```

- `Triangulating cell N of type: <name>` is printed for every cell at start, also when
  `perform_initial_triangulation` is 0. The lines are not in order because the cells are
  prepared in parallel.
- `Progression P%, iteration: I, file number F, nb cells C` gives the percentage of
  `simulation_duration` reached, the iteration, the number of the latest output mesh file and
  the number of cells.
- Lines starting with `[Iteration N]` report the scheduling decisions of the adaptive mode
  ({doc}`/user_guide/18_parallel_performance`).
- At the end of a run that reaches `simulation_duration`, a performance report lists the time
  spent in each phase of the iteration.

Standard error carries warnings and the messages of the {term}`safeguards <safeguard>`, for
example:

```text
local_mesh_refiner: oscillation bail-out on cell 221 after 293 operations, 754 edges left unchecked (occurrence 6)
Removing numerically unstable cell 4 at iteration 3: extent 5.43873e-05 m (max 4.86471e-05 m), volume 8.57603e-17 m^3, target volume 1.15126e-16 m^3, 0 non-finite nodes
```

What each of these means, and how often it is expected, is described in
{doc}`/user_guide/09_safeguards`. To keep both streams, redirect them to a file outside the
output folder:

```console
$ ./simucell3d ../parameters/core/parameters_vesicle_quick.xml > ../vesicle_quick.log 2>&1
```

## How a run ends

A run ends in one of these ways:

| End | Exit status | What is written last |
|---|---|---|
| The simulation time reaches `simulation_duration` | 0 | A final block of rows in the {term}`statistics CSV`; the performance report on standard output. |
| No cell is left (all were removed) | 0 | As above. A run can also end normally after the safeguards removed most of its cells: the exit status does not reflect removals ({doc}`09_safeguards`). |
| An error: invalid input, a diverged domain (for example "The contact grid would need more than 1e10 voxels: at least one cell has diverged." or "The mesh is too large, the simulation is probably unstable. Reducing the time step might help."), mesh refinement failing for too many cells in one iteration, a non-finite coordinate when a mesh file is written, a full disk | 1 | The error message on standard error. Output written up to that point is kept. |
| Termination by a signal (`Ctrl-C`, `kill`) | signal status (130 for `Ctrl-C`) | Output written up to that point is kept. |
| An internal check fails in a `noexcept` function (a known issue in 2.1, see {doc}`/whats_new/v2.1`) | 134 (`SIGABRT`) | Output written up to that point is kept. |

The last mesh files written are those of the last completed sampling period; the run does not
write an extra mesh file when it ends.

A run cannot be resumed: there is no checkpoint file. To continue from the state of an
earlier run, use its last `cell_data/result_<n>.vtk` as the input mesh of a new run
([Reusing simulation output](12_input_meshes.md#reusing-simulation-output)). The new run
starts at simulation time 0 and with new random growth rates.

:::{important}
Exit status 0 means that the program reached `simulation_duration` (or ran out of cells). It does
not mean that the tissue survived. In the November 2026 benchmark, the shipped
`parameters/benchmarks/fig3e_monolayer.xml`, run to its full duration with 2.1.0, ended with
status 0 and 18 of its 434 cells. Check every run as described in
[Checking a run](#checking-a-run) and {ref}`benchmarks-validity`.
:::

(ug-running-time-step)=
## Time step

`time_step` is the setting that most often decides whether a run is usable. A step that is too
large does not stop the run: cells blow up and are removed one by one, and the run continues.
The November 2026 benchmark measured, with 2.1.0 ({ref}`benchmarks-time-step`):

- The vesicle (at five apical surface tensions), the multilayer vesicle and the tube, built
  from the shipped files, and the single-cell test cases run without removals at
  `1e-7` s. Two cases complete in no version: the full-length
  monolayer file `fig3e_monolayer.xml`, which loses most of its cells in 2.1.0, and one two-cell
  contact case, which loses a cell ({ref}`benchmarks-completion`).
- The lab's growth benchmark (`parameters_benchmarck.xml` of 1.0, contact model 1) is **not**
  stable at its published `1e-7` s in any version. With 2.1.0 the first cell is removed at
  about 4,800 cells and removals become massive from about 13,800 cells. The recommended step
  for this file is `2.5e-8` s: with 2.1.0 it was clean in three of three 48 h runs, which grew
  to 142,314-148,914 cells with no removal. This is a measurement, not a guarantee for other
  machines or longer runs.

For a new parameter file, run a few thousand iterations, count the removals
([Checking a run](#checking-a-run)) and halve the time step until there are none. The cost of a
run is close to proportional to $1/\Delta t$ ({doc}`18_parallel_performance`). The stability
limits themselves are explained in [Choosing the time step](04_time_integration.md#choosing-the-time-step).

(ug-running-checking)=
## Checking a run

Five things can happen to cells during a run. Only the first two are logged; count them in the
standard error of every run.

| Event | Logged as | Counted in |
|---|---|---|
| {term}`Removal <removal>` of a numerically unstable cell | `Removing numerically unstable cell N at iteration I: ...` | `unstable_cells_removed` of the {term}`diagnostics CSV` |
| Removal of a cell whose mesh refinement failed | `Removing cell N at iteration I: mesh refinement failed: ...` | `unstable_cells_removed` |
| {term}`Refused division <refused division>`: the mother is kept and tries again | `Cell division failed for cell N: ... division refused` | not counted |
| {term}`Minimum-volume death <minimum-volume death>`: a cell below `min_vol` is deleted | nothing | not counted |
| {term}`Cell superposition`: two cells end up in nearly the same place | nothing | not counted |

```console
$ ./simucell3d ../parameters/core/parameters_vesicle_quick.xml > run.log 2> run.err
$ grep -c "Removing" run.err          # removals, both kinds
$ grep -c "division refused" run.err  # refused divisions
```

A clean run has no removals. Any removal means that the run is no longer a faithful solution
of the model at that time step, usually because the time step is too large or a parameter is
out of range; when removals continue or rise with the number of cells, the tissue that is left
is no longer the model's answer, even if the run ends with status 0
({ref}`benchmarks-time-step`). Some refusals are normal in growing tissues;
in the benchmark they rose from about 21% of division attempts below 30,000 cells to about 27%
above 125,000.

Minimum-volume deaths are part of the model, but they are silent. Estimate them from the cell
identifiers in the {term}`statistics CSV`: each division gives two new identifiers, so with
$n_0$ cells in the input mesh, $N$ cells now and $M$ the largest `cell_id` seen so far, about
$D = (M + 1 - n_0)/2$ divisions have happened and $n_0 + D - N$ cells have been removed or have
died. Subtract the logged removals to get the deaths. In the growth benchmark, 35.7% of all
cells ever born had died at the minimum volume by 144,000 live cells, with a similar rate in
1.0.

Cell superposition limits large aggregates: in the growth benchmark (contact model 1) the core
of the tissue holds more cell volume than space beyond about 5,000 cells, in every version.
Per-cell statistics of such a run are valid only for the outer cells. The vesicle and tube
(contact model 2) showed none. See {ref}`benchmarks-validity`.

Two runs of the same file are not identical; differences of a few percent between
{term}`replicates <replicate>` are expected ([Reproducibility](18_parallel_performance.md#reproducibility),
{ref}`benchmarks-reproducibility`).

## Long runs

Runs of many hours should be started detached from the terminal, with the log outside the
output folder. `scripts/runs/launch_longrun_20k.sh` does this for the reconstructed paper-like
growth run of `parameters/longrun_fast_growth.xml`; it is a template for other long runs:

```bash
cd "$PROJECT/build"
export OMP_WAIT_POLICY=passive
nohup /usr/bin/time -v stdbuf -oL -eL ./simucell3d \
    --schedule=adaptive \
    --output-dir="$OUT" \
    --diagnostics-csv="$OUT/performance_diagnostics.csv" \
    "$PARAM" \
    > "$LOGDIR/run.log" 2>&1 &
```

- `stdbuf -oL -eL` makes the log line-buffered. Without it, standard output redirected to a
  file is written in blocks of a few kilobytes, which on a large tissue can mean many minutes
  between updates.
- `/usr/bin/time -v` adds the peak memory use and the CPU time to the end of the log.
- The script first kills a run of the same launcher that is still writing to `$OUT`, then
  deletes and re-creates `$OUT` and its log folder. Set
  `THREADS=N` in the environment to pass `--threads=N`.

Monitor such a run with `tail -f` on the log, and with the diagnostics CSV, which gains one row
every 100 iterations ({doc}`/user_guide/14_output_files`).

## Running in Docker

The `Dockerfile` in the repository root builds SimuCell3D, with the Python bindings, in an
image based on `python:3.10`. Build the image from the repository root:

```console
$ docker build -t simucell3d .
```

The image copies the whole repository to `/app`, builds in `/app/container_build`, and uses
the executable as its entry point, with `/app/container_build` as the working directory.
Arguments after the image name are passed to `simucell3d`, so parameter file paths are
relative to `/app/container_build`:

```console
$ docker run --rm -v "$PWD/simulation_results:/app/simulation_results" \
      simucell3d ../parameters/core/parameters_vesicle_quick.xml
```

The shipped parameter files write to `../simulation_results/<name>`, that is
`/app/simulation_results/<name>` in the container; the `-v` option makes that folder the
`simulation_results/` folder of the current directory on the host, so that the results remain
after the container exits. Results written elsewhere in the container are lost with it.

The image contains the repository as it was when the image was built. To run a parameter file
or mesh that is not in the image, mount it as well (for example
`-v "$PWD/my_params:/app/my_params"` and the argument `../my_params/run.xml`), or rebuild the
image. Thread count and pinning follow the CPUs that Docker gives the container
(`--cpus` limits CPU time but not the set of CPUs; use `--cpuset-cpus` to restrict the CPUs).

:::{seealso}
- {doc}`/getting_started`: a first run step by step.
- {doc}`/user_guide/14_output_files`: what the run writes.
- {doc}`/user_guide/18_parallel_performance`: threads and scheduling modes.
:::
