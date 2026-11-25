(dev-architecture)=
# Architecture

This page describes how the source tree is organised, how one {term}`iteration` of the solver
moves data between the modules, who owns which data, and where to add a new cell class, contact
model or output column. It is written for developers who change the C++ code; the model itself
is described in the {doc}`/user_guide/index`.

## Source layout

The C++ code is split into a header tree, `include/`, and an implementation tree, `src/`, with
the same module subdirectories. Each module in `src/` is a CMake library target of the same
name (`src/CMakeLists.txt`). The exceptions: the top-level files form the target `src`, the
Python module is the target `simucell3d_python_wrapper`, and the executable `simucell3d` is
built from `src/app/`.

| Module | Content |
|---|---|
| top level | `solver` (the main loop); `global_configuration.hpp` (compile-time switches); `custom_structures.hpp` (parameter structures); `custom_exception.hpp`; `cpu_topology.hpp` (thread placement); `performance_monitor.hpp`; `utils.hpp` |
| `mesh/` | `cell`, `node`, `edge`, `face`; the five {term}`cell classes <cell class>` in `mesh/cell_types/` |
| `io/` | `simulation_initializer`, `parameter_reader`, `mesh_reader`, `mesh_writer`, `statistics_writer`; `mesh_data.hpp` and `mesh_data_mappers.hpp` (output columns) |
| `contact_models/` | `contact_model_abstract`, the three contact models, the contact detection strategies |
| `time_integration/` | `time_integration_scheme` |
| `triangulation_modules/` | `local_mesh_refiner`, `cell_divider`, `initial_triangulation`, `ball_pivoting_algorithm`, `poisson_sampling` |
| `automatic_polarization/` | `automatic_polarizer` and its writer (polarization mode 2 only) |
| `uspg/` | uniform spatial partitioning grids: `uspg_3d`, `uspg_4d`, `morton_code` (header-only in practice) |
| `math_modules/` | `vec3`, `mat33`, `quaternion`, `eigen_solver` |
| `python_bindings/` | pybind11 module, built only with `-DENABLE_PYTHON_BINDINGS=TRUE` |
| `src/app/` | `main.cpp`: command-line parsing, then `simulation_initializer` and `solver` |

Other top-level directories:

| Directory | Content |
|---|---|
| `lib/` | Vendored third-party code: TinyXML-2 (XML parsing), Delaunator (2D Delaunay triangulation, used by the cell divider) and pybind11. |
| `test/` | C++ tests, one subdirectory per module, registered with CTest ({ref}`dev-testing`). |
| `tests/` | Python tests for the scripts in `scripts/ci/`. |
| `parameters/` | Shipped {term}`parameter files <parameter file>`. |
| `data/input_meshes/` | Shipped input meshes (legacy VTK). |
| `scripts/` | Python and shell tools: plotting, mesh conversion, parameter screening, benchmarking, run launchers. |
| `doc/` | This documentation (Sphinx). |

Project classes live in the global namespace; the exceptions are the thread-placement types
and functions in `cpu_topology` and the vendored eigen solver in `gte`. Header
guards are named `DEF_<NAME>` ({ref}`dev-coding-style`).

The top-level `CMakeLists.txt` sets C++17, reads the version from
`project(SimuCell3D VERSION X.Y.Z)`, writes `generated/version.hpp` into the build directory
(version and git commit, used by `simucell3d --version`), and adds `lib`, `src` (as `bin`),
`test` and `src/app`. Because `src` is added with the binary directory `bin`, the module
libraries appear under `build/bin/` and the executable under `build/src/app/`; a post-build step
creates the symbolic link `build/simucell3d`.

## Compile-time configuration

`include/global_configuration.hpp` holds five switches that select code paths with the
preprocessor. Changing one requires a rebuild, and each shipped parameter file assumes particular
values ({doc}`/user_guide/10_compile_time_configuration`).

| Macro | Default | Selects |
|---|---|---|
| `CONTACT_MODEL_INDEX` | `2` (also 0, 1) | contact model class constructed by the solver, and the matching branch of `time_integration_scheme::update_nodes_positions()` |
| `DYNAMIC_MODEL_INDEX` | `0` (also 1) | full equations of motion (semi-implicit Euler) or overdamped (forward Euler) |
| `POLARIZATION_MODE_INDEX` | `1` (also 0, 2) | no polarization, contact-based face types, or the `automatic_polarizer` with `region_data/` output |
| `FACE_STORE_CONTACT_ENERGY` | `false` | per-face adhesion and repulsion energies |
| `WRITE_NORMALS_IN_OUTPUT_MESH_FILE` | `true` | normals in the VTK output |

Code that depends on these macros must compile for every documented
combination; `#if CONTACT_MODEL_INDEX == ...` blocks appear in thirteen files, among them
`solver.cpp`, `time_integration.cpp`, `cell.cpp`, `node.hpp`, the three contact models and
`mesh_data.hpp`.

## Program flow

`src/app/main.cpp` does three things:

1. Parse the command line: the parameter file (the only non-flag argument) and the flags
   `--schedule=`, `--threads=`, `--output-dir=`, `--diagnostics-csv=` and `--version`
   ({doc}`/user_guide/13_running_simulations`). The removed flags `--schedule=auto` and
   `--enable-per-loop-scheduling` stop the program with an error; any other argument that
   starts with `--` and matches none of these is ignored.
2. Build a `simulation_initializer` from the parameter file. It reads the XML with
   `parameter_reader`, reads the input mesh with `mesh_reader`, triangulates each cell surface in
   parallel unless `perform_initial_triangulation` is off, and creates one object per cell. The
   class of each object is chosen by the `global_cell_id` of its cell type
   (`src/io/simulation_initializer.cpp`, `switch(cell_type->global_type_id_)`).
3. Construct a `solver` with the parameters and the cell list, then call `solver::run()`.
   Any exception that reaches `main()` is printed to standard error and the program exits with
   status 1.

### Solver construction

`solver::solver()` (`src/solver.cpp`) prepares everything the loop needs:

- Thread count and placement: one thread per physical core unless `--threads=N` is given, and
  {term}`thread pinning` unless `SIMUCELL3D_PIN_THREADS=0` or `OMP_PLACES` is set
  ({ref}`dev-performance-internals`).
- OpenMP schedule: the {term}`scheduling mode` given by `--schedule`; for `adaptive`, a base
  schedule and four per-loop schedules.
- Output folder: **deleted** and recreated with `cell_data/` and `face_data/` (and
  `region_data/` in polarization mode 2).
- Cell ids: `set_id()` gives each cell a unique id from `max_cell_id_`; `set_local_id()` sets
  the cell's position in `cell_lst_`.
- Modules, owned through `std::unique_ptr`: `local_mesh_refiner` (edge lengths between
  $\ell_{min}$ = `min_edge_length` and $3\,\ell_{min}$), `time_integration_scheme`, the contact
  model selected by `CONTACT_MODEL_INDEX`, the statistics writer (CSV file, or string for the
  Python module) and, in polarization mode 2, the `automatic_polarizer`.
- Initial target volumes: $V_0 = V\,\exp(p_0/K)$ from the initial pressure $p_0$ and bulk
  modulus $K$ of the cell type ($V_0 = V$ when $K = 0$).

(dev-architecture-main-loop)=
### Main loop

`solver::run()` calls `run_iteration()` while the simulation time is below
`simulation_duration` and at least one cell remains, then writes a final statistics row,
compacts every cell mesh (`cell::rebase()`) and prints the performance report.

One call of `solver::run_iteration()` performs these steps, in this order. Where a step is
timed, the timer name (the phase name in the {term}`diagnostics CSV`) is given in brackets.

1. Apply a schedule change deferred from the previous iteration's divisions
   (`handle_division_event()`).
2. Write `cell_data/result_<n>.vtk` and `face_data/result_<n>.vtk` when
   $n = \lfloor t / \text{sampling period} \rfloor + 1$ changes (`save_mesh()`,
   `mesh_writer::write()`).
3. Every 5 iterations, divide the cells that are ready (`cell_divider::run()`, parallel over
   cells).
4. Update face types (`cell::update_face_types()`, parallel over cells).
5. Refine every cell mesh (`local_mesh_refiner::refine_meshes()`, parallel over cells)
   [`mesh_refinement`]; remove the cells whose refinement failed, or stop the run if too many
   failed (`handle_refinement_failures()`).
6. Detect contacts and apply contact forces (`contact_model_abstract::run()`, parallel)
   [`contact_detection`].
7. In polarization mode 2 only, polarize faces by space discretisation
   (`automatic_polarizer::polarize_faces()`).
8. Contact-based polarization, then internal forces: pressure, tension, area elasticity, bending
   (`cell::special_polarization_update()`, `cell::apply_internal_forces()`, parallel over cells)
   [`polarization_and_internal_forces`].
9. Integrate node positions and advance the time by $\Delta t$
   (`time_integration_scheme::update_nodes_positions()`; parallel over cells with contact models
   0 and 1, on one thread with contact model 2)
   [`time_integration`].
10. Every 50 iterations, write a statistics row (`statistics_writer::write_data()`) and update
    the workload estimate and the phase-based schedule (`update_workload_heterogeneity()`,
    `adaptive_schedule_update()`).
11. Every 100 iterations, append a row to the diagnostics CSV and reset the timers
    (`export_diagnostics()`).
12. With contact models 1 and 2, pass an empty parallel region that acts as a barrier.
13. Remove cells below `min_vol` and {term}`unstable cells <unstable cell>`, then
    resynchronise the local ids (`remove_dead_cells()`).
14. Print the progress line.

Forces accumulate on the nodes during steps 6 and 8 and are consumed in step 9. The order
matters: contact forces are computed on meshes refined in the same iteration, and cells that
cannot be refined are removed (step 5) before contact detection sees them. Cells that diverge
during integration are removed in step 13, so a non-finite or exploded cell never reaches the
next contact detection, where it would make the spatial grid unbounded. The
{term}`safeguards <safeguard>` themselves are described in {doc}`/user_guide/09_safeguards`.

Errors are reported in two ways. Problems confined to one cell remove that cell and print one
line to standard error (`Removing numerically unstable cell ...`, `Removing cell ... mesh
refinement failed: ...`). Problems that invalidate the run throw an exception from
`custom_exception.hpp` (for example `unstable_simulation_exception` when more than
max(10, 1 % of the cells) refinements fail in one iteration or every cell's refinement fails,
or when the contact grid cannot be built); the exception propagates out of `run()` to
`main()`.

### Data flow

```text
parameter file ─► parameter_reader ─► global_simulation_parameters, cell_type_parameters
input mesh     ─► mesh_reader      ─► mesh (per cell) ─► initial_triangulation ─► cell objects
                                                                                 │
                    ┌────────────────────────── solver::cell_lst_ ◄──────────────┘
                    │
   each iteration:  ├─► cell_divider          adds daughters, removes mothers
                    ├─► local_mesh_refiner    splits, collapses, swaps edges
                    ├─► contact model         node forces (and couplings, models 1 and 2)
                    ├─► cell::apply_internal_forces   node forces
                    ├─► time_integration_scheme       node positions, momenta, time
                    └─► remove_dead_cells     removes cells
   outputs:  mesh_writer ─► cell_data/, face_data/       statistics_writer ─► simulation_statistics.csv
             export_diagnostics ─► performance_diagnostics.csv (or --diagnostics-csv path)
```

## Data ownership

| Object | Owner | Notes |
|---|---|---|
| `cell` | `solver::cell_lst_` (`std::vector<cell_ptr>`, `cell_ptr` = `std::shared_ptr<cell>`) | The order of `cell_lst_` is significant, see local ids below. |
| `node`, `face` | the cell: `std::vector<node> node_lst_`, `std::vector<face> face_lst_` | Faces refer to their three nodes by index in `node_lst_`. Removed nodes and faces are marked unused and their slots are reused (`free_node_queue_`); `cell::rebase()` compacts the vectors. Loops must skip entries with `is_used() == false`. |
| `edge` | the cell: `edge_set_` (`std::set<edge>`) | Rebuilt by `rebase()` when slots were freed. |
| cell type, face type parameters | shared: `cell_type_param_ptr` (`std::shared_ptr`) | One object per `<cell_type>` block, shared by all cells of that type. |
| simulation parameters | copied: `global_simulation_parameters` | The solver, the contact model and the time integrator each hold a copy. |

Identifiers:

- `cell::get_id()`: unique for the whole run, never reused; daughters get new ids from
  `max_cell_id_`. Use it in output and log lines.
- `cell::get_local_id()`: the cell's index in `cell_lst_`. Contact models 1 and 2 store
  couplings as (local cell id, local node id) pairs and index `cell_lst_` with them, and the time
  integrator of model 2 uses the local ids to decide which cell of a coupled group moves the
  group. **Any code that inserts into or erases from `cell_lst_` must reassign local ids
  afterwards**, as `cell_divider::run()`, `remove_dead_cells()` and
  `handle_refinement_failures()` do.
- `face::global_face_id_`: index of the face in the contact model's flat face list; reassigned at
  the start of every contact detection.

Concurrency: parallel loops run over cells. Writes into a node of another cell happen only in
the contact models; model 2 adds such forces with an atomic add and updates the coupling maps
under the per-node `std::mutex` (`node.hpp`). Contact models 0 and 1 do not make these
cross-cell writes atomic (see the known issues in `CHANGELOG.md`).

## Extension points

### A new cell class

1. Derive from `cell` in `include/mesh/cell_types/` and `src/mesh/` (see `lumen_cell.hpp` for a
   minimal class). Override the virtual members that differ:
   `apply_internal_forces()`, `update_face_types()`, `special_polarization_update()`,
   `is_ready_to_divide()`, `get_cell_division_axis()`, `face_is_in_contact()`, and
   `get_cell_same_type()`, which the divider uses to create daughters of the same class.
2. Give it a new `global_cell_id` value in the `switch` of
   `simulation_initializer::triangulate_surface()` (`src/io/simulation_initializer.cpp`).
3. Document the value in the parameter-file reference and add tests in
   `test/test_mesh/test_cell_types/`.

### A new contact model

1. Derive from `contact_model_abstract` and implement `run(const std::vector<cell_ptr>&)`. It
   is called outside any parallel region and may throw `unstable_simulation_exception`.
2. Construct it in the `#if CONTACT_MODEL_INDEX == ...` chain of the solver constructor and add
   the matching branch to `time_integration_scheme::update_nodes_positions()`.
3. Document the new index in `global_configuration.hpp` and the user guide.

### A new output column

- Per-cell arrays in the VTK output: add a `cell_data_mapper` to `cell_data_mapper_lst` in
  `include/io/mesh_data.hpp` (faces: `face_data_mapper`, nodes: `node_data_mapper`).
- Per-cell columns in `simulation_statistics.csv`: add a `cell_data_mapper` to
  `file_data_mapper_lst` in the same file. The writer emits the header from the mapper names.
- Columns of the diagnostics CSV are written by `solver::export_diagnostics()`; downstream
  scripts (`scripts/preprocess_benchmark_metrics.py`) read them by name.

:::{seealso}
- {ref}`dev-performance-internals` for the scheduling and pinning code in the solver.
- {doc}`/api/index` for the class reference.
:::
