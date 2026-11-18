(api-index)=
# API reference

SimuCell3D is used mainly as the command-line program `simucell3d`, configured by a
{term}`parameter file` and by the compile-time macros in `include/global_configuration.hpp`.
This reference is for readers who extend the solver in C++ (a new cell class, contact model or
output array) or who drive it from Python. Running simulations does not require it; see the {doc}`/user_guide/index`.

## C++ classes

The C++ API is the set of headers in `include/`. All classes are in the global namespace, with
two exceptions: the thread-placement functions in the namespace `cpu_topology` and the
eigensolver in the namespace `gte`. Class names are lower case with underscores (`solver`,
`cell`, `local_mesh_refiner`), and data members end with an underscore.

The reference is organised by module, following the subdirectories of `include/` and `src/`:

| Page | Headers | Contents |
|---|---|---|
| {doc}`solver` | `solver.hpp`, `global_configuration.hpp`, `custom_structures.hpp`, `custom_exception.hpp` | The solver and its main loop, compile-time macros, parameter structures, exceptions |
| {doc}`io` | `io/` | Parameter file and input mesh readers, simulation initializer, mesh and statistics writers |
| {doc}`mesh` | `mesh/cell.hpp`, `node.hpp`, `edge.hpp`, `face.hpp` | Cell surfaces and their mechanics |
| {doc}`cell_classes` | `mesh/cell_types/` | Epithelial, ECM, lumen, nucleus and static cells |
| {doc}`contact_models` | `contact_models/` | Contact models 0, 1, 2 and contact detection |
| {doc}`time_integration` | `time_integration/` | Integration of the equations of motion |
| {doc}`triangulation` | `triangulation_modules/` | Mesh refinement, cell division, initial triangulation |
| {doc}`polarization` | `automatic_polarization/` | Space-discretisation polarization (mode 2) |
| {doc}`spatial_partitioning` | `uspg/` | Uniform spatial partitioning grids |
| {doc}`math` | `math_modules/` | Vectors, matrices, quaternions, eigensolver |
| {doc}`runtime` | `cpu_topology.hpp`, `performance_monitor.hpp`, `utils.hpp` | Thread placement, timing, utilities |

Each page starts with a short description of the module's role in the simulation and links to
the user-guide chapter that explains the model. The class entries below it list the public
members with the comments from the headers. Members that exist only for some values of the
compile-time macros (for example only with `CONTACT_MODEL_INDEX` 1 or 2) are listed whatever
the configuration; their comment states the condition.

:::{note}
The class entries are built from the Doxygen comments in `include/` when the documentation
is published. A local build without Doxygen shows the module descriptions only; see
{doc}`/developers/writing_documentation` to build the full reference.
:::

## Python module

The optional pybind11 module `simucell3d_python_wrapper` is documented on its own page,
{doc}`python`. It runs a complete simulation from Python with parameters set as attributes
and returns per-cell statistics and the final cells.

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
