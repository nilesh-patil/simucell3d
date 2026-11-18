(ug-python)=
# Python interface

SimuCell3D includes an optional Python module, `simucell3d_python_wrapper`, built with
[pybind11](https://pybind11.readthedocs.io/). It runs the same solver as the `simucell3d`
executable, but takes its parameters from Python objects instead of an XML file and returns
the per-cell statistics and the final cells to Python. It is useful for scripted parameter
studies and for analysing results without going through files.

The module is a thin layer over the C++ structures. Its names end in an underscore, its
constructor runs the whole simulation, and it does not validate parameters. This chapter
describes how to build and import it, every class and attribute it exports, and its
limitations.

## Building the module

The module is not built by default. Configure with `ENABLE_PYTHON_BINDINGS` set to `TRUE`; the
pybind11 sources are included in `lib/pybind11`, so only the Python development headers
(`Python.h`) are needed in addition to the usual build requirements ({doc}`/install`). From the
repository root:

```console
$ cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_PYTHON_BINDINGS=TRUE \
      -DPYTHON_EXECUTABLE=$(which python3)
$ cmake --build build --target simucell3d_python_wrapper -j 4
```

`-DPYTHON_EXECUTABLE` selects the interpreter the module is built for; the module can only be
imported by that Python version. CMake prints `-- Python bindings enabled` when the option is
active. Omitting `--target` builds the module together with everything else.

The module is written to `build/bin/python_bindings/`, with a name such as
`simucell3d_python_wrapper.cpython-310-x86_64-linux-gnu.so` (the middle part depends on the
Python version and platform).

The Docker image ({doc}`/user_guide/13_running_simulations`) builds the module, in
`/app/container_build/bin/python_bindings/`.

:::{note}
The continuous-integration builds do not build the Python module. For the 2.1 release it
was built and run on Linux with Python 3.10 and GCC. It uses the vendored pybind11 2.11.
:::

## Importing the module

Python finds the module once its directory is on the module search path, either through
`PYTHONPATH`:

```console
$ export PYTHONPATH=$PWD/build/bin/python_bindings:$PYTHONPATH
$ python3 -c "import simucell3d_python_wrapper as sc; print(dir(sc))"
```

or from inside a script, as `scripts/python_bindings/launch_simulation_example.py` does:

```python
import sys
sys.path.insert(0, "/path/to/simucell3d/build/bin/python_bindings")
import simucell3d_python_wrapper as sc
```

The module exports six classes: `global_simulation_parameters`, `cell_type_parameters`,
`face_type_parameters`, `simulation_outputs`, `cell` and `simucell3d_wrapper`.

## A complete run

The following script runs one epithelial cell (`data/input_meshes/big_sphere.vtk`) for 100
time steps and reads the results. It follows `launch_simulation_example.py`, with a shorter
duration.

```python
import sys
from io import StringIO

import pandas as pd

sys.path.insert(0, "build/bin/python_bindings")
import simucell3d_python_wrapper as sc

# Numerical parameters (the <numerical_parameters> section of a parameter file)
sim = sc.global_simulation_parameters()
sim.input_mesh_path_ = "data/input_meshes/big_sphere.vtk"
sim.output_folder_path_ = "python_run"  # deleted and re-created by the run
sim.damping_coefficient_ = 2e-9         # kg/s
sim.simulation_duration_ = 1e-5         # s
sim.sampling_period_ = 5e-6             # s
sim.time_step_ = 1e-7                   # s
sim.min_edge_len_ = 1e-6                # m
sim.contact_cutoff_adhesion_ = 7.5e-7   # m
sim.contact_cutoff_repulsion_ = 7.5e-7  # m
sim.enable_edge_swap_operation_ = False
sim.perform_initial_triangulation_ = False

# One face type (a <face_type> block)
apical = sc.face_type_parameters()
apical.name_ = "apical"
apical.face_type_global_id_ = 0
apical.surface_tension_ = 1e-3          # N/m
apical.bending_modulus_ = 0.0           # J
apical.adherence_strength_ = 0.0        # N/m^3
apical.repulsion_strength_ = 1e9        # N/m^3

# One cell type (a <cell_type> block)
epithelial = sc.cell_type_parameters()
epithelial.name_ = "epithelial"
epithelial.global_type_id_ = 0          # cell class: epithelial
epithelial.mass_density_ = 1e3          # kg/m^3
epithelial.bulk_modulus_ = 1e4          # Pa
epithelial.max_pressure_ = 1e20         # Pa
epithelial.initial_pressure_ = 0.0      # Pa
epithelial.area_elasticity_modulus_ = 0.0       # J
epithelial.target_isoperimetric_ratio_ = 150
epithelial.angle_regularization_factor_ = 0.0   # J
epithelial.avg_growth_rate_ = 2e-11     # m^3/s
epithelial.std_growth_rate_ = 0.0
epithelial.avg_division_vol_ = 1e-14    # m^3
epithelial.std_division_vol_ = 0.0
epithelial.min_vol_ = 3.7e-17           # m^3
epithelial.surface_coupling_max_curvature_ = 1e7  # 1/m
epithelial.add_face_type(apical)

# Run: positional arguments (parameters, cell types, verbose, number of threads)
run = sc.simucell3d_wrapper(sim, [epithelial], False, 2)
out = run.get_simulation_outputs()

if out.RETURN_CODE_:
    print("Simulation failed:", out.error_message_)
else:
    stats = pd.read_csv(StringIO(out.cell_statistics_))
    print(stats[["iteration", "cell_id", "volume", "pressure"]])
    for c in out.cell_lst_:
        print(c.get_id(), c.get_volume(), len(c.get_node_coord_lst()))
```

Run it from the repository root. The relative mesh path is also found from other
directories, because, as in the command-line program, a mesh path that does not exist is tried
again relative to the source directory the module was built from
([Paths](11_parameter_file.md#paths)).

In this example the sphere is larger than `avg_division_vol_`, so the cell divides at the
first division check and the run ends with two cells.

## Reference

### `global_simulation_parameters`

Corresponds to the `<numerical_parameters>` section ({doc}`/user_guide/11_parameter_file`).
Create it with `global_simulation_parameters()` and set the attributes.

| Attribute | Type | Unit | XML tag |
|---|---|---|---|
| `input_mesh_path_` | `str` | | `input_mesh_file_path` |
| `output_folder_path_` | `str` | | `output_mesh_folder_path` |
| `damping_coefficient_` | `float` | kg/s | `damping_coefficient` |
| `simulation_duration_` | `float` | s | `simulation_duration` |
| `sampling_period_` | `float` | s | `sampling_period` |
| `time_step_` | `float` | s | `time_step` |
| `min_edge_len_` | `float` | m | `min_edge_length` |
| `contact_cutoff_adhesion_` | `float` | m | `contact_cutoff_adhesion` |
| `contact_cutoff_repulsion_` | `float` | m | `contact_cutoff_repulsion` |
| `perform_initial_triangulation_` | `bool` | | `perform_initial_triangulation` (default `True`) |
| `enable_edge_swap_operation_` | `bool` | | `enable_edge_swap_operation` (default `True`) |

The real-valued attributes have no default value: set every one of them. The contact detection
algorithm is not exported; Python runs always use the uniform spatial partitioning grid.

### `cell_type_parameters`

Corresponds to one `<cell_type>` block. Create it with `cell_type_parameters()`.

| Attribute | Type | Unit | XML tag |
|---|---|---|---|
| `name_` | `str` | | `cell_type_name` |
| `global_type_id_` | `int` | | `global_cell_id` (cell class, 0 to 4) |
| `mass_density_` | `float` | kg/m³ | `cell_mass_density` |
| `bulk_modulus_` | `float` | Pa | `cell_bulk_modulus` (default 1) |
| `max_pressure_` | `float` | Pa | `max_inner_pressure`; use `float("inf")` for no bound |
| `initial_pressure_` | `float` | Pa | not in the XML file (always 0 there); see below |
| `area_elasticity_modulus_` | `float` | J | `area_elasticity_modulus` |
| `target_isoperimetric_ratio_` | `float` | dimensionless | `target_isoperimetric_ratio` |
| `angle_regularization_factor_` | `float` | J | `angle_regularization_factor` |
| `avg_growth_rate_` | `float` | m³/s | `avg_growth_rate` |
| `std_growth_rate_` | `float` | m³/s | `std_growth_rate` |
| `avg_division_vol_` | `float` | m³ | `avg_division_volume`; `float("inf")` disables division |
| `std_division_vol_` | `float` | m³ | `std_division_volume` |
| `min_vol_` | `float` | m³ | `min_vol` |
| `surface_coupling_max_curvature_` | `float` | 1/m | `surface_coupling_max_curvature` |

Unset attributes are 0 (except `bulk_modulus_`).

`initial_pressure_` sets the initial target volume of each cell of this type to
$V_0 = V \exp(p_0 / K)$, where $V$ is the volume of the cell in the input mesh, $p_0$ the
initial pressure and $K$ the bulk modulus, so that the cell starts with pressure $p_0$. With
$K = 0$ it has no effect.

Methods:

`add_face_type(face_type)`
: Append a `face_type_parameters` object to the cell type. The order of the calls is the
  order of the face types; for epithelial cells it must be apical, lateral, basal
  ([Face types](11_parameter_file.md#face-types)). The face types cannot be read back from
  Python.

`add_additional_parameter(name, value)`
: Store a named number in the cell type. No part of SimuCell3D 2.1 reads these values.

### `face_type_parameters`

Corresponds to one `<face_type>` entry. Create it with `face_type_parameters()`.

| Attribute | Type | Unit | XML tag |
|---|---|---|---|
| `name_` | `str` | | `face_type_name` |
| `face_type_global_id_` | `int` | | `global_face_id` |
| `surface_tension_` | `float` | N/m | `surface_tension` |
| `bending_modulus_` | `float` | J | `bending_modulus` |
| `adherence_strength_` | `float` | N/m³ | `adherence_strength` |
| `repulsion_strength_` | `float` | N/m³ | `repulsion_strength` |

### `simucell3d_wrapper`

```python
simucell3d_wrapper(simulation_parameters, cell_type_list, verbose, nb_threads)
```

Constructing the object runs the complete simulation and returns when it has finished. The
four arguments are required and positional; keyword arguments raise a `TypeError`
("incompatible constructor arguments").

`simulation_parameters`
: A `global_simulation_parameters` object.

`cell_type_list`
: A list of `cell_type_parameters` objects. The input-mesh `cell_type_id` indexes this list.

`verbose`
: `bool`. If `True`, the run prints the same progress information as the command-line
  program.

`nb_threads`
: `int`. Number of OpenMP threads; 0 or a negative value means one thread per physical core.
  The scheduling mode is always `adaptive`.

Method:

`get_simulation_outputs()`
: Return a `simulation_outputs` object with the result of the run.

An error during the run (an invalid mesh, a diverged simulation, `Ctrl-C` in the terminal)
does not raise a Python exception. It ends the run and is reported in the outputs.

### `simulation_outputs`

| Attribute | Type | Content |
|---|---|---|
| `RETURN_CODE_` | `bool` | `False` if the run completed, `True` if it stopped with an error. |
| `error_message_` | `str` | The error message, empty on success. |
| `cell_statistics_` | `str` | The statistics table in CSV format, with the columns of `simulation_statistics.csv` ({doc}`/user_guide/14_output_files`). Empty if the run failed. |
| `cell_lst_` | `list[cell]` | The cells at the end of the run. Empty if the run failed. |

Read `cell_statistics_` with `pandas.read_csv(io.StringIO(out.cell_statistics_))`.

### `cell`

The objects in `cell_lst_`. They cannot be created from Python.

| Method | Returns | Unit |
|---|---|---|
| `get_id()` | cell identifier (`cell_id` in the output) | |
| `get_cell_type_id()` | `global_cell_id` of the cell's type | |
| `get_volume()` | volume | m³ |
| `get_target_volume()` | target volume | m³ |
| `get_area()` | surface area | m² |
| `get_pressure()` | pressure | Pa |
| `get_growth_rate()` | growth rate | m³/s |
| `get_node_coord_lst()` | node coordinates, a list of `[x, y, z]` lists | m |
| `get_face_point_ids()` | triangles, a list of `[i, j, k]` lists of indices into `get_node_coord_lst()` | |

## Differences from the command-line program

- **Files.** The run writes `cell_data/` and `face_data/` meshes and, every 100 iterations,
  `performance_diagnostics.csv` to `output_folder_path_`, which is deleted at the start as for
  the command-line program. It does not write `simulation_statistics.csv`; the table is
  returned in `cell_statistics_` instead.
- **No validation.** The range checks of the parameter-file reader
  ([Numerical parameters](11_parameter_file.md#numerical-parameters)) are not applied. A zero
  time step, a sampling period smaller than the time step or a negative modulus is accepted
  and gives meaningless results or a failed run. Use values that a parameter file would
  accept.
- **No contact detection choice.** `contact_detection_algorithm` cannot be set.
- **Compile-time options.** The contact model, dynamic model and polarization mode are those of
  the build ({doc}`/user_guide/10_compile_time_configuration`).
- **The interpreter is blocked.** The constructor holds the Python interpreter until the run
  ends; other Python threads do not run in the meantime. `Ctrl-C` is checked once per
  iteration and stops the run with `RETURN_CODE_` set to `True`.
- **Thread pinning affects the calling thread.** With {term}`thread pinning` on (the default),
  the thread that calls the constructor is pinned to one CPU and stays pinned after the run,
  which slows down other multi-threaded work in the same Python process (for example NumPy).
  Set the environment variable `SIMUCELL3D_PIN_THREADS=0` before the run if that matters.
- **Memory.** All statistics rows are kept in memory until the run ends.

## The example script

`scripts/python_bindings/launch_simulation_example.py` runs a single cell from
`data/input_meshes/big_sphere.vtk` for $10^{-3}$ s ($10^4$ iterations), saves the statistics as
`build/python_sim_example/cell_statistics_data.csv` and prints them. It expects the module in
`build/bin/python_bindings/` of the same checkout and needs `pandas`:

```console
$ pip install pandas
$ python3 scripts/python_bindings/launch_simulation_example.py
```

To run many simulations, call `simucell3d_wrapper` in a loop, or in separate processes with
`multiprocessing` and `nb_threads` set so that the processes do not share cores. Give every
run its own `output_folder_path_`. The example page {doc}`/examples/python_batch_runs` shows a
batch of runs.

:::{seealso}
- {doc}`/api/python`: the module reference in the API section.
- {doc}`/user_guide/11_parameter_file`: meaning and units of every parameter.
- {doc}`/user_guide/17_parameter_screening`: parameter screens with the command-line program on
  a Slurm cluster.
:::
