(faq)=
# Frequently asked questions

Short answers with links to the part of the documentation that covers each topic.

## Building

CMake cannot find OpenMP.
: The compiler has no OpenMP support. Use GCC (on Debian and Ubuntu, `build-essential`) and
  configure again in an empty build directory. See {ref}`install`.

The build runs out of memory.
: Compile with fewer parallel jobs, for example `make -j2`.

Do Debug builds run without OpenMP?
: No. The root `CMakeLists.txt` prints "Building in debug mode", but every module links OpenMP
  in all build types. Debug builds are slower because they are not optimised; use
  `-DCMAKE_BUILD_TYPE=Release` for simulations.

How is the contact model or the dynamic model changed?
: They are compile-time options. Edit `include/global_configuration.hpp` and rebuild; see
  {doc}`/user_guide/10_compile_time_configuration`.

## Running

The program prints "The parameter file could not be found at the path: ...".
: The path is resolved relative to the current working directory, or is absolute. Check it from
  the directory where `simucell3d` is started. A file that exists but is not valid XML is
  reported as "The parameter file is not valid XML (line N)", followed by the parser's message.

The program prints "ERROR: input mesh file ... not found".
: The input mesh path in `<input_mesh_file_path>` is tried as given (relative to the working
  directory, or absolute), then relative to the source directory recorded at build time. See
  {doc}`/user_guide/11_parameter_file`. A warning "the absolute mesh file path ... was not found"
  followed by a successful run means the second attempt succeeded.

My previous results disappeared.
: The output folder is deleted at the start of every run. Use `--output-dir=PATH` or a different
  `<output_mesh_folder_path>` for each run you want to keep
  ({doc}`/user_guide/13_running_simulations`).

The log shows "Removing numerically unstable cell ..." or "oscillation bail-out".
: These are safeguards of 2.x (the removal is new in 2.1). A removal means the run is no longer a clean solution
  of the model at that time step, even if the run continues and ends with status 0. Reduce
  `<time_step>` until no cell is removed ({ref}`benchmarks-time-step`); if removals remain, a
  parameter is out of range for the mesh. A bail-out is not a removal: the refiner stops
  working on that cell for one iteration. The safeguards and their log lines are described in {doc}`/user_guide/09_safeguards`.

Which time step should I use?
: The shipped vesicle, multilayer and tube files run without removals at their `1e-7` s. The
  lab's growth benchmark does not: with 2.1.0 it removes cells in large numbers at its published
  `1e-7` s. The recommended step for it is `2.5e-8` s, which was clean in three of three 48 h
  runs of 2.1.0 (to about 145,000 cells). For a new file, run a few
  thousand iterations and halve the step until no cell is removed; see
  [Time step](user_guide/13_running_simulations.md#time-step) and {ref}`benchmarks-time-step`.

My run exited with status 0 but has few cells left.
: The exit status does not report lost cells. Safeguard removals and deaths at the minimum volume
  both leave the run running, and only the removals are logged. The shipped
  `fig3e_monolayer.xml`, run to its full duration, ends with status 0 and 18 of its 434 cells in
  2.1.0. Count the `Removing` lines of every run and estimate the deaths from the cell
  identifiers as described in [Checking a run](user_guide/13_running_simulations.md#checking-a-run);
  see also {ref}`benchmarks-validity`.

An unknown command-line option had no effect.
: Options that `simucell3d` does not recognise are ignored without a message. Check the spelling
  against {doc}`/user_guide/13_running_simulations`. The removed options `--schedule=auto` and
  `--enable-per-loop-scheduling` stop the program with an error.

## Performance

How many threads does a run use?
: One per physical core that the process may use, each pinned to its own core. `--threads=N`
  sets another number. `OMP_NUM_THREADS` has no effect, because the program sets the thread
  count itself. See {doc}`/user_guide/18_parallel_performance`.

How is thread pinning turned off?
: Set `SIMUCELL3D_PIN_THREADS=0`. Pinning is also turned off when `OMP_PLACES` is set, so that a
  placement chosen with OpenMP environment variables is respected.

Why do repeated runs of the same file differ?
: Two runs with the same input are not identical. The random numbers of the solver are seeded
  from the system clock: the growth rate and division volume of each cell (when their standard
  deviation is not zero) and the surface sampling of the initial triangulation and of cell
  division. With more than one thread, runs also differ when nothing random is drawn. A run with
  `--threads=1` is repeatable only when none of these is used. In the benchmark, scalar
  readouts such as mean pressure varied by 0.1-1.0% between replicates and wall times by up to
  about 2%; run three replicates ([Reproducibility](user_guide/18_parallel_performance.md#reproducibility),
  {ref}`benchmarks-reproducibility`).

How much memory does a run need?
: In the growth benchmark, 2.1.0 needs about 0.13 GiB per 1,000 cells; a run at 145,000 cells
  held 18.9 GiB. Memory grows with the number of faces, so a finer mesh needs more per cell.
  Sizing rules for time and memory are in
  [Sizing a run](user_guide/18_parallel_performance.md#sizing-a-run).

How long will a growth run take?
: Measured on 8-core cloud machines with 8 threads, 2.1.0 grows the lab's benchmark tissue to
  10,000 cells in 1.28 h and to 125,000 cells in 37.7 h at a time step of
  `2.5e-8` s. The cumulative wall time grows as $N^{1.33}$; see
  [Sizing a run](user_guide/18_parallel_performance.md#sizing-a-run) and
  {ref}`benchmarks-performance`.

## Results

Why does my growth run not match the timing of Fig. 1e of the paper?
: The paper reports 125,000 cells in one day on a Xeon W-2245 with 16 threads, at the time step of
  `1e-7` s of its Table 2. On the lab's benchmark file that step is unstable in every
  version, including 1.0, so 2.1.0 has to run at a quarter of it or less, with four times as
  many iterations; it then needs 37.7 h on 8 threads. The scaling, $N^{1.33}$, is the same as
  in the paper's fit. The benchmark file `parameters_benchmarck.xml` is not shipped with 2.1;
  it is in the `v1.0` tag (`git show v1.0:parameters_benchmarck.xml`) and needs the
  compile-time switches in its header. `parameters/longrun_fast_growth.xml` is a different,
  reconstructed configuration ({doc}`/examples/growth_from_one_cell`,
  {ref}`benchmarks-performance`).

Can I trust per-cell statistics of a large aggregate?
: Not in its core. In the growth benchmark (contact model 1), beyond about 5,000 cells the
  centre of the aggregate holds more cell volume than space: cells are superposed, two meshes in
  almost the same place, in every version. At 136,043 cells 39% of all cells were superposed,
  most of them in the centre. Use statistics of the outer cells only, and check the
  packing of your own runs ({ref}`benchmarks-validity`).

Why do my results differ from the paper?
: Release 2.1 fixes bugs of the published 1.0 code that changed the physics, and adds safeguards
  that remove unstable cells. Results of 2.1 therefore differ from those of 1.0. To reproduce a
  published figure, use release 1.0 ({doc}`/whats_new/v1.0`). Also check that the program was
  compiled with the configuration stated in the header of the parameter file.

How are the results viewed?
: Open the `cell_data/result_*.vtk` series in ParaView; see {doc}`/getting_started` and
  {doc}`/user_guide/15_visualization`.

How is SimuCell3D cited?
: See {doc}`/citing`.
