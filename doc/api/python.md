(api-python)=
# Python module

Reference for the pybind11 module `simucell3d_python_wrapper`, defined in
`src/python_bindings/simucell3d_wrapper.cpp`. The module exposes the parameter structures, a
class that runs a complete simulation, and read-only access to the cells at the end of the
run. How to build and use it, with the mapping of each attribute to its XML tag, is described in
{doc}`/user_guide/16_python_interface`; a complete script is
`scripts/python_bindings/launch_simulation_example.py`.

## Building and importing

The module is built only when CMake is configured with `-DENABLE_PYTHON_BINDINGS=TRUE`
(default `FALSE`). From the repository root:

```console
$ cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_PYTHON_BINDINGS=TRUE
$ cmake --build build -j
```

The shared library is written to `build/bin/python_bindings/` as
`simucell3d_python_wrapper.cpython-<version>-<platform>.so`. It is not installed; add that
directory to `sys.path` (or `PYTHONPATH`) before importing:

```python
import sys
sys.path.insert(0, "build/bin/python_bindings")
import simucell3d_python_wrapper as sc
```

:::{note}
The `bindings` job of the CI workflow builds the module and imports it on every pull request
({ref}`dev-ci`). No test runs a simulation through it.
:::

## Running a simulation

```python
outputs = sc.simucell3d_wrapper(sim_params, [cell_type], True, -1).get_simulation_outputs()
if outputs.RETURN_CODE_ == 0:
    print(outputs.cell_statistics_.splitlines()[0])
else:
    print(outputs.error_message_)
```

The constructor of {py:class}`~simucell3d_python_wrapper.simucell3d_wrapper` performs the whole simulation: it creates the
cells from the input mesh and the cell types, runs the solver until `simulation_duration_`, and
stores the result. Errors do not raise Python exceptions; they set `RETURN_CODE_` and
`error_message_`. Pressing Ctrl-C stops the run at the start of the next iteration and is
reported the same way.

Differences from a run of the `simucell3d` executable:

- The contact detection algorithm cannot be chosen: Python runs always use the uniform spatial
  partitioning grid (USPG).
- Per-cell statistics are returned in {py:attr}`~simucell3d_python_wrapper.simulation_outputs.cell_statistics_` instead of
  being written to `simulation_statistics.csv`. The output meshes (`cell_data/`,
  `face_data/`) and `performance_diagnostics.csv` are still written to
  `output_folder_path_`, and that folder is deleted at the start of the run.
- The OpenMP scheduling mode is the solver default (`adaptive`).
- Thread pinning applies to the calling Python thread as well: after the run it stays pinned to
  one CPU (check with `os.sched_getaffinity(0)`). Set `SIMUCELL3D_PIN_THREADS=0` before the run
  to avoid this.

## Classes

Attribute names end with an underscore, as the C++ members do. All parameter classes have a
constructor without arguments; set attributes one by one after construction. Units are SI.

| Class | Role |
|---|---|
| `global_simulation_parameters` | Numerical parameters (the numerical section of the parameter file) |
| `cell_type_parameters` | One cell type, with its face types |
| `face_type_parameters` | One face type |
| `simucell3d_wrapper` | Runs the simulation in its constructor |
| `simulation_outputs` | Return code, error message, statistics and final cells |
| `cell` | Read-only view of a cell at the end of the run |

```{py:module} simucell3d_python_wrapper
```

````{py:class} global_simulation_parameters()

Numerical parameters of a simulation. Equivalent to the numerical section of the parameter
file ({doc}`/user_guide/11_parameter_file`); C++ type {cpp:struct}`global_simulation_parameters`.

```{py:attribute} output_folder_path_
:type: str

Folder for the output files. Deleted and re-created at the start of the run. A relative path
is relative to the working directory of the Python process.
```

```{py:attribute} input_mesh_path_
:type: str

Input mesh (legacy VTK). Tried as given, then relative to the source directory recorded at
configure time.
```

```{py:attribute} damping_coefficient_
:type: float

Damping coefficient, kg/s.
```

```{py:attribute} simulation_duration_
:type: float

Simulated time, s.
```

```{py:attribute} sampling_period_
:type: float

Simulated time between two output meshes, s.
```

```{py:attribute} time_step_
:type: float

Time step, s.
```

```{py:attribute} min_edge_len_
:type: float

Minimum edge length of the cell meshes, m. The maximum edge length is three times this value.
```

```{py:attribute} contact_cutoff_adhesion_
:type: float

Distance beyond which adhesion forces are zero, m.
```

```{py:attribute} contact_cutoff_repulsion_
:type: float

Distance beyond which repulsion forces are zero, m.
```

```{py:attribute} perform_initial_triangulation_
:type: bool

Re-triangulate the input meshes before the run. Default `True`.
```

```{py:attribute} enable_edge_swap_operation_
:type: bool

Allow the local mesh refiner to swap the longest edge of triangles with a high aspect ratio.
Default `True`.
```
````

````{py:class} cell_type_parameters()

Parameters of one cell type, equivalent to a `<cell_type>` block of the parameter file; C++
type {cpp:struct}`cell_type_parameters`. The position of a cell type in the list passed to
{py:class}`simucell3d_wrapper` is the `cell_type_id` used in the input mesh.

```{py:attribute} name_
:type: str
```

```{py:attribute} global_type_id_
:type: int

Cell class: 0 epithelial, 1 ECM, 2 lumen, 3 nucleus, 4 static. Default 0.
```

```{py:attribute} mass_density_
:type: float

Mass density, kg/m^3. Default 0.
```

```{py:attribute} bulk_modulus_
:type: float

Bulk modulus $K$, Pa. Default 1.
```

```{py:attribute} max_pressure_
:type: float

Upper bound of the pressure difference between the cell and its surroundings, Pa. Default 0.
```

```{py:attribute} initial_pressure_
:type: float

Pressure at the start of the run, Pa. The initial target volume is set to
$V \exp(p_0/K)$, where $V$ is the volume of the input mesh (or $V$ when $K = 0$). Not
available in the XML parameter file, where it is always 0. Default 0.
```

```{py:attribute} area_elasticity_modulus_
:type: float

Area elasticity modulus $k_a$, J. Default 0.
```

```{py:attribute} avg_division_vol_
:type: float

Mean of the normal distribution from which each cell draws its division volume, m^3.
Default 0.
```

```{py:attribute} std_division_vol_
:type: float

Standard deviation of the division volume, m^3. Default 0.
```

```{py:attribute} avg_growth_rate_
:type: float

Mean of the normal distribution from which each cell draws its growth rate, m^3/s.
Default 0.
```

```{py:attribute} std_growth_rate_
:type: float

Standard deviation of the growth rate, m^3/s. Default 0.
```

```{py:attribute} angle_regularization_factor_
:type: float

Strength of the force that keeps triangle angles close to 60 degrees. Default 0.
```

```{py:attribute} target_isoperimetric_ratio_
:type: float

Target isoperimetric ratio $A^3/V^2$, dimensionless. Default 0.
```

```{py:attribute} min_vol_
:type: float

Volume below which the cell is removed, m^3. Default 0.
```

```{py:attribute} surface_coupling_max_curvature_
:type: float

Curvature above which a node is not coupled to a neighbouring cell, 1/m. Default 0.
```

```{py:method} add_face_type(face_type)

Append a {py:class}`face_type_parameters` to this cell type. Face types are stored in the
order they are added and cannot be read back from Python.
```

```{py:method} add_additional_parameter(name, value)

Store a named floating-point parameter in the cell type (`str`, `float`). The value is stored
but no part of the 2.1 solver reads it.
```
````

````{py:class} face_type_parameters()

Parameters of one face type, equivalent to a `<face_type>` block; C++ type
{cpp:struct}`face_type_parameters`.

```{py:attribute} name_
:type: str
```

```{py:attribute} face_type_global_id_
:type: int

Identifier of the face type written to the output meshes. Default 0.
```

```{py:attribute} surface_tension_
:type: float

Surface tension $\gamma$, N/m. Default 0.
```

```{py:attribute} adherence_strength_
:type: float

Adherence strength $\omega$, N/m^3. Default 0.
```

```{py:attribute} repulsion_strength_
:type: float

Repulsion strength $\xi$, N/m^3. Has no default; always set it.
```

```{py:attribute} bending_modulus_
:type: float

Bending modulus $k_b$, J. Default 0.
```
````

````{py:class} simucell3d_wrapper(sim_parameters, cell_types, verbose, nb_threads)

Run a complete simulation in the constructor.

The four arguments are positional and required; keyword arguments are not accepted.

:param sim_parameters: Numerical parameters.
:type sim_parameters: global_simulation_parameters
:param cell_types: Cell types, indexed by the `cell_type_id` of the input mesh.
:type cell_types: list[cell_type_parameters]
:param verbose: Print the progress of the simulation.
:type verbose: bool
:param nb_threads: Number of OpenMP threads. A value of 0 or less uses one thread per physical
  core.
:type nb_threads: int

```{py:method} get_simulation_outputs()

Return a copy of the result of the run.

:rtype: simulation_outputs
```
````

````{py:class} simulation_outputs()

Result of a run; C++ type {cpp:struct}`simulation_outputs`.

```{py:attribute} RETURN_CODE_
:type: bool

`False` (equal to 0) if the run completed, `True` (equal to 1) if an exception stopped it.
```

```{py:attribute} error_message_
:type: str

Message of the exception that stopped the run; empty on success.
```

```{py:attribute} cell_statistics_
:type: str

Per-cell statistics in CSV format, with the same columns as `simulation_statistics.csv`
({doc}`/user_guide/14_output_files`): one row per cell every 50 iterations and once at the end of
the run. Empty if the run failed.
```

```{py:attribute} cell_lst_
:type: list[cell]

The cells at the end of the run. Empty if the run failed.
```
````

````{py:class} cell

A cell at the end of a run, from {py:attr}`simulation_outputs.cell_lst_`. It cannot be
constructed from Python. C++ type {cpp:class}`cell`.

```{py:method} get_id()

Identifier of the cell (`int`).
```

```{py:method} get_cell_type_id()

`global_type_id_` of the cell type (`int`).
```

```{py:method} get_growth_rate()

Growth rate of the target volume, m^3/s.
```

```{py:method} get_area()

Surface area, m^2.
```

```{py:method} get_volume()

Volume, m^3.
```

```{py:method} get_pressure()

Pressure, Pa.
```

```{py:method} get_target_volume()

Target volume $V_0$, m^3.
```

```{py:method} get_node_coord_lst()

Node coordinates as a list of `[x, y, z]` lists, m.
```

```{py:method} get_face_point_ids()

Faces as a list of `[i, j, k]` lists of indices into {py:meth}`get_node_coord_lst`.
```
````

## C++ side

The module is a thin layer over the C++ classes. {cpp:class}`simucell3d_wrapper` builds the
cells with {cpp:class}`simulation_initializer` and runs a {cpp:class}`solver_python_wrapper`,
a {cpp:class}`solver` that checks for a pending Python signal before each iteration and
collects the statistics in a {cpp:class}`string_statistics_writer`. Both headers are in
`include/python_bindings/`.

```{doxygenclass} simucell3d_wrapper
:project: simucell3d
:members:
```

```{doxygenclass} solver_python_wrapper
:project: simucell3d
:members:
```
