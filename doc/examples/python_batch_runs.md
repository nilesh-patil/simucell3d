(ex-python)=
# Batch runs from Python

Drive several simulations from a Python script with the optional Python module, and collect
their statistics in a pandas data frame. The module runs the same solver as the `simucell3d`
program, but takes its parameters from Python objects instead of an XML file.

**Files**: `scripts/python_bindings/launch_simulation_example.py`,
`data/input_meshes/big_sphere.vtk` (one cell).
**Requirements**: a build with the Python module (`-DENABLE_PYTHON_BINDINGS=TRUE`,
{doc}`/install`) in the `build/` directory of the repository, and the Python package `pandas`.
**Compile-time configuration**: the module is compiled with the same
`include/global_configuration.hpp` as the program; these scripts use the defaults.

## The example script

`launch_simulation_example.py` finds the module in `build/bin/python_bindings/` relative to its
own location, defines one epithelial cell type with apical and lateral faces, runs $10^{-3}$ s of
simulated time ($10^4$ iterations) on `big_sphere.vtk`, and saves the per-cell statistics. From
the repository root:

```console
$ python3 scripts/python_bindings/launch_simulation_example.py
```

The mesh files are written to `build/python_sim_example/`, which is deleted at the start of the
run like any output folder, and the statistics to `build/python_sim_example/cell_statistics_data.csv`.

The module differs from the command-line program in a few points (details in
{doc}`/user_guide/16_python_interface`):

- `simucell3d_wrapper(params, cell_types, verbose, nb_threads)` takes four positional arguments;
  keyword arguments are not accepted. The whole simulation runs inside this constructor.
- The statistics are returned as a CSV string in `simulation_outputs.cell_statistics_`, with the
  same columns as `simulation_statistics.csv`; no statistics file is written.
- Errors do not raise a Python exception: `RETURN_CODE_` is 1 and `error_message_` holds the
  message.
- The contact detection algorithm cannot be selected; Python runs use the default spatial grid.

## A parameter sweep

The script below runs the cell of the example once for each of three lateral surface tensions,
writes each run to its own folder under `simulation_results/`, and summarises the last statistics
sample of each run. Save it as `batch_surface_tension.py` in the repository root and run it from
there:

```console
$ python3 batch_surface_tension.py
```

```{code-block} python
:caption: batch_surface_tension.py
"""Run one SimuCell3D simulation per lateral surface tension and collect the statistics."""
import sys
from io import StringIO
from pathlib import Path

import pandas as pd

REPO = Path.cwd()                  # run from the repository root
BUILD = REPO / "build"             # configured with -DENABLE_PYTHON_BINDINGS=TRUE
DURATION = 1e-3                    # simulated time per run [s]
sys.path.insert(0, str(BUILD / "bin" / "python_bindings"))
import simucell3d_python_wrapper as sc


def make_cell_type(lateral_tension):
    apical = sc.face_type_parameters()
    apical.name_ = "apical"
    apical.face_type_global_id_ = 0
    apical.adherence_strength_ = 0
    apical.repulsion_strength_ = 1e9
    apical.surface_tension_ = 1e-3
    apical.bending_modulus_ = 0

    lateral = sc.face_type_parameters()
    lateral.name_ = "lateral"
    lateral.face_type_global_id_ = 1
    lateral.adherence_strength_ = 0
    lateral.repulsion_strength_ = 1e9
    lateral.surface_tension_ = lateral_tension
    lateral.bending_modulus_ = 0

    cell_type = sc.cell_type_parameters()
    cell_type.name_ = "epithelial"
    cell_type.global_type_id_ = 0
    cell_type.mass_density_ = 1e3
    cell_type.bulk_modulus_ = 1e4
    cell_type.initial_pressure_ = 0
    cell_type.max_pressure_ = 1e20
    cell_type.avg_growth_rate_ = 2e-11
    cell_type.std_growth_rate_ = 0
    cell_type.target_isoperimetric_ratio_ = 150
    cell_type.angle_regularization_factor_ = 0
    cell_type.area_elasticity_modulus_ = 0
    cell_type.surface_coupling_max_curvature_ = 1e7
    cell_type.avg_division_vol_ = 1e-14
    cell_type.std_division_vol_ = 0
    cell_type.min_vol_ = 3.7e-17
    cell_type.add_face_type(apical)
    cell_type.add_face_type(lateral)
    return cell_type


results = []
for lateral_tension in [2.5e-4, 5e-4, 1e-3]:
    params = sc.global_simulation_parameters()
    params.output_folder_path_ = str(REPO / "simulation_results" / f"lateral_{lateral_tension:g}")
    params.input_mesh_path_ = str(REPO / "data" / "input_meshes" / "big_sphere.vtk")
    params.damping_coefficient_ = 2e-9
    params.simulation_duration_ = DURATION
    params.sampling_period_ = 1e-5
    params.time_step_ = 1e-7
    params.min_edge_len_ = 1e-6
    params.contact_cutoff_adhesion_ = 7.5e-7
    params.contact_cutoff_repulsion_ = 7.5e-7
    params.enable_edge_swap_operation_ = 0
    params.perform_initial_triangulation_ = 0

    # Positional arguments: parameters, list of cell types, verbose, number of threads (-1: one
    # per physical core). The simulation runs inside the constructor.
    run = sc.simucell3d_wrapper(params, [make_cell_type(lateral_tension)], False, -1)
    outputs = run.get_simulation_outputs()
    if outputs.RETURN_CODE_ != 0:
        print(f"lateral tension {lateral_tension:g}: failed: {outputs.error_message_}")
        continue

    stats = pd.read_csv(StringIO(outputs.cell_statistics_))
    last = stats[stats["iteration"] == stats["iteration"].max()]
    results.append({"lateral_tension": lateral_tension,
                    "cells": last["cell_id"].nunique(),
                    "mean_volume": last["volume"].mean()})

print(pd.DataFrame(results))
```

Each run reports the number of cells and their mean volume at the end of the run. The cell of
`big_sphere.vtk` is already larger than the division volume of $10^{-14}$ m$^3$, so it divides
early in every run.

For larger sweeps, run on a cluster with the screening scripts described in
{doc}`/user_guide/17_parameter_screening`, which write one XML parameter file per row of a table and
submit one `simucell3d` job per file.

:::{seealso}
- {doc}`/user_guide/16_python_interface`
- {doc}`/api/python`
:::
