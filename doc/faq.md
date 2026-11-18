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
: These are the safeguards added in 2.1. Isolated removals during a long growth run are expected
  ({doc}`/examples/growth_from_one_cell`). Many removals in the first iterations mean the
  parameters or the time step do not suit the mesh; reduce `<time_step>` first. The safeguards
  and their log lines are described in {doc}`/user_guide/09_safeguards`.

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

Are two runs with the same input identical?
: Not in general. Multi-threaded runs are not bit-reproducible ({doc}`/whats_new/v2.1`). In
  addition, the random numbers used by the solver are seeded from the system clock: the growth
  rate and division volume of each cell (when their standard deviation is not zero) and the
  surface sampling of the initial triangulation and of cell division. A run with
  `--threads=1` is repeatable only when none of these is used.

## Results

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
