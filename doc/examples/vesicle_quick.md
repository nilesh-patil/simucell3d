(ex-vesicle-quick)=
# Short vesicle run

Run 200 iterations of the vesicle of {doc}`vesicle`, as a check that a build works and as the
first simulation of {doc}`/getting_started`.

**Files**: `parameters/core/parameters_vesicle_quick.xml`, `data/input_meshes/fig_3_vesicle.vtk`
(434 cells: 432 epithelial cells, one ECM surface, one lumen). \
**Compile-time configuration**: the defaults (`POLARIZATION_MODE_INDEX 1`,
`CONTACT_MODEL_INDEX 2`, `DYNAMIC_MODEL_INDEX 0`). \
**Length**: short. $2\times10^{-5}$ s of simulated time at a time step of `1e-7` s (200
iterations). \
**Measured run time**: 1 min 48 s with 2.1.0 on a 4-core cloud machine (GCP c2-standard-8, 4 threads), November 2026; not part of
the benchmark.

## The parameter file

The file is `parameters/core/parameters_vesicle.xml` with a shorter simulated time
($2\times10^{-5}$ s instead of $10^{-2}$ s) and a longer sampling period ($5\times10^{-6}$ s instead of
$10^{-6}$ s). All mechanical parameters are the same; they are listed in {doc}`vesicle`.

```{literalinclude} ../../parameters/core/parameters_vesicle_quick.xml
:language: xml
:caption: parameters/core/parameters_vesicle_quick.xml (numerical parameters)
:start-at: <numerical_parameters>
:end-at: </numerical_parameters>
```

## Run

From the `build/` directory:

```console
$ ./simucell3d ../parameters/core/parameters_vesicle_quick.xml
```

The output folder is `simulation_results/simulation_vesicle/` in the repository root, the same
as for {doc}`vesicle`; it is deleted at the start of the run. The warning that the mesh path
`data/input_meshes/fig_3_vesicle.vtk` was not found, followed by the path relative to the source
directory, is expected when running from `build/`.
The run takes about two minutes on 4 cores and writes about 290 MB of mesh files.

## What to look at

- `cell_data/result_1.vtk` to `result_4.vtk` and the matching `face_data/` files: the tissue every
  $5\times10^{-6}$ s, starting at $t = 0$. In ParaView, colour the cell meshes by
  `cell_type_id` or `cell_pressure` ({doc}`/getting_started`).
- `simulation_statistics.csv`: one row per cell at iterations 0, 50, 100, 150 and 200; the number of distinct
  `cell_id` values stays at 434.
- `performance_diagnostics.csv`: one row, at iteration 100. Its `unstable_cells_removed` column is
  0, and the standard error output contains no `Removing numerically unstable cell` lines.

:::{note}
The time step cannot be raised to shorten the run. With a time step of `5e-7` s this
vesicle becomes unstable at once. In a review run of 2.1, the safeguards removed all 432
epithelial cells within 8 iterations, and the run still exited with status 0
({ref}`whatsnew-v2-1-known-issues`).
A run whose log shows many removals in its first iterations usually needs a smaller time step.
:::

:::{seealso}
- {doc}`vesicle` for the full run and the mechanical parameters.
- {doc}`/getting_started`
:::
