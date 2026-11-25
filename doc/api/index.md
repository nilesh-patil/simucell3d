(api-index)=
# API reference

SimuCell3D is used mainly as the command-line program `simucell3d`, configured by a
{term}`parameter file` and by the compile-time macros in `include/global_configuration.hpp`.
This reference is for readers who extend the solver in C++ (a new cell class, contact model or
output array) or who drive it from Python. Running simulations does not require it; see the
{doc}`/user_guide/index`.

The C++ API is the set of headers in `include/`. All classes are in the global namespace, with
two exceptions: the thread-placement types and functions in the namespace `cpu_topology` and the
eigensolver in the namespace `gte`. Class names are lower case with underscores (`solver`,
`cell`, `local_mesh_refiner`), and data members end with an underscore.

## One iteration, class by class

The {cpp:class}`solver` runs the simulation. Each call of {cpp:func}`solver::run_iteration`
passes the list of cells through the modules below, in this order. The step-by-step
description, with the timers and the conditions of each step, is in
{ref}`dev-architecture-main-loop`.

| Step | Code | Page |
|---|---|---|
| Write the output meshes when a sampling period has elapsed | {cpp:func}`mesh_writer::write` | {doc}`io` |
| Divide the cells that reached their division volume (every 5 iterations) | {cpp:func}`cell_divider::run` | {doc}`triangulation` |
| Update the face types | {cpp:func}`cell::update_face_types` | {doc}`mesh`, {doc}`cell_classes` |
| Keep edge lengths between $\ell_{min}$ and $3\,\ell_{min}$ | {cpp:func}`local_mesh_refiner::refine_meshes` | {doc}`triangulation` |
| Find contacts, apply adhesion and repulsion | {cpp:func}`contact_model_abstract::run` | {doc}`contact_models` |
| Polarize faces from a voxel grid (polarization mode 2 only) | {cpp:func}`automatic_polarizer::polarize_faces` | {doc}`polarization` |
| Contact-based polarization, then pressure, tension, area elasticity and bending | {cpp:func}`cell::special_polarization_update`, {cpp:func}`cell::apply_internal_forces` | {doc}`mesh`, {doc}`cell_classes` |
| Move the nodes, advance the time | {cpp:func}`time_integration_scheme::update_nodes_positions` | {doc}`time_integration` |
| Write a statistics row (every 50 iterations) | {cpp:func}`abstract_statistics_writer::write_data` | {doc}`io` |
| Remove cells below their minimum volume or numerically unstable | {cpp:func}`cell::check_numerical_stability` | {doc}`solver`, {doc}`mesh` |

Before the first iteration, {cpp:class}`simulation_initializer` reads the parameter file
({cpp:class}`parameter_reader`) and the input mesh ({cpp:class}`mesh_reader`), re-triangulates
each cell surface ({cpp:class}`initial_triangulation`) unless `perform_initial_triangulation`
is off, and creates one object per cell.

## Pages

| Page | Headers | Contents |
|---|---|---|
| {doc}`solver` | `solver.hpp`, `global_configuration.hpp`, `custom_structures.hpp`, `custom_exception.hpp` | The solver and its main loop, compile-time macros, parameter structures, exceptions |
| {doc}`io` | `io/` | Parameter file and input mesh readers, simulation initializer, mesh and statistics writers, output arrays |
| {doc}`mesh` | `mesh/cell.hpp`, `node.hpp`, `edge.hpp`, `face.hpp` | Cell surfaces and their mechanics |
| {doc}`cell_classes` | `mesh/cell_types/` | Epithelial, ECM, lumen, nucleus and static cells |
| {doc}`contact_models` | `contact_models/` | Contact models 0, 1, 2 and contact detection |
| {doc}`time_integration` | `time_integration/` | Integration of the equations of motion |
| {doc}`triangulation` | `triangulation_modules/` | Mesh refinement, cell division, initial triangulation |
| {doc}`polarization` | `automatic_polarization/` | Polarization by space discretisation (mode 2) |
| {doc}`spatial_partitioning` | `uspg/` | Uniform spatial partitioning grids, Morton codes |
| {doc}`math` | `math_modules/` | Vectors, matrices, quaternions, eigensolver |
| {doc}`runtime` | `cpu_topology.hpp`, `performance_monitor.hpp`, `utils.hpp` | Thread placement, timing, utilities |
| {doc}`python` | `python_bindings/` | The Python module `simucell3d_python_wrapper` |

Each page starts with the role of its classes in the simulation, a table of the classes, and a
link to the user-guide chapter that explains the model. The class entries below list the public
members with the comments from the headers; protected members are not listed (see the header
when deriving a class). Members that exist only for some values of the compile-time macros (for
example only with `CONTACT_MODEL_INDEX` 1 or 2) are listed whatever the configuration; their
comment states the condition.

## Where to start

| Task | Start with | Guide |
|---|---|---|
| Add a cell class | {cpp:class}`cell`, {cpp:class}`lumen_cell` (a minimal derived class), `simulation_initializer` | {ref}`dev-architecture` |
| Add a contact model | {cpp:class}`contact_model_abstract`, {cpp:class}`time_integration_scheme` | {ref}`dev-architecture` |
| Add an output array or a statistics column | {cpp:struct}`cell_data_mapper`, `mesh_data.hpp` | {ref}`dev-architecture` |
| Change thread placement or scheduling | `cpu_topology`, {cpp:class}`solver` | {ref}`dev-performance-internals` |
| Run a simulation from Python | {doc}`python` | {doc}`/user_guide/16_python_interface` |

:::{note}
The class entries are built from the Doxygen comments in `include/` when the documentation
is published. A local build without Doxygen shows the module descriptions only; see
{doc}`/developers/writing_documentation` to build the full reference.
:::

```{toctree}
:maxdepth: 1
:hidden:

solver
io
mesh
cell_classes
contact_models
time_integration
triangulation
polarization
spatial_partitioning
math
runtime
python
```
