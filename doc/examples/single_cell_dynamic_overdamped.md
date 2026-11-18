(ex-single-cell)=
# Growing cell: full and overdamped dynamics

Grow and divide a single spherical cell under the two dynamic models of SimuCell3D: the full
equations of motion integrated with semi-implicit Euler, and the overdamped equations integrated
with forward Euler. The two parameter files are the default files of release 1.0, and each was
written for its own compile-time configuration.

**Files**: `parameters/core/parameters_default_dynamic.xml`,
`parameters/core/parameters_default_overdamped.xml`, `data/input_meshes/big_sphere.vtk` (one
cell, about 30 µm in diameter).
**Length**: long. $10^{-2}$ s of simulated time at a time step of $10^{-7}$ s ($10^5$
iterations), with a growing number of cells.

## Compile-time configuration

Neither file matches the default build. Before each run, set the macros in
`include/global_configuration.hpp` and rebuild:

| Parameter file | `POLARIZATION_MODE_INDEX` | `CONTACT_MODEL_INDEX` | `DYNAMIC_MODEL_INDEX` |
|---|---|---|---|
| `parameters_default_dynamic.xml` | 1 | 1 (node-node coupling) | 0 (full equations, semi-implicit Euler) |
| `parameters_default_overdamped.xml` | 1 | 0 (node-face springs) | 1 (overdamped, forward Euler) |
| default build | 1 | 2 (face-face coupling) | 0 |

For example, for the dynamic file, change `#define CONTACT_MODEL_INDEX 2` to
`#define CONTACT_MODEL_INDEX 1`, then from the `build/` directory:

```console
$ make -j4
$ ./simucell3d ../parameters/core/parameters_default_dynamic.xml --threads=1
```

Contact models 0 and 1 add contact forces to the nodes of neighbouring cells without an atomic
add, so with more than one thread some contact forces can be lost, without a warning (a known
issue of 2.1, {doc}`/user_guide/03_contact`). Run both files with `--threads=1`, as above.

Restore the defaults and rebuild afterwards. The compile-time options are described in
{doc}`/user_guide/10_compile_time_configuration`, the dynamic models in
{doc}`/user_guide/04_time_integration`, and the contact models in {doc}`/user_guide/03_contact`.

:::{important}
Running a parameter file with a build for another configuration does not produce an error: the
simulation runs with a contact or dynamic model the parameters were not chosen for. Check the
header comment of each parameter file.
:::

## The parameter files

Both files use the same mesh, time step, minimum edge length ($7.5\times10^{-7}$ m), contact
cutoffs ($5\times10^{-7}$ m) and the five cell types of the other core files, with growth and
division enabled for the `epithelial` type. The input mesh is re-triangulated at the start of the
run (`perform_initial_triangulation` 1). They differ in:

| XML tag | `..._dynamic.xml` | `..._overdamped.xml` | Unit |
|---|---|---|---|
| `damping_coefficient` | 5e-10 | 3e-9 | kg/s |
| `sampling_period` | 1e-6 | 5e-6 | s |
| `max_inner_pressure` (epithelial) | 2.5e3 | INF | Pa |
| `avg_growth_rate` / `std_growth_rate` (epithelial) | 2e-11 / 2e-12 | 1e-11 / 1e-12 | m³/s |
| `area_elasticity_modulus` (epithelial) | 1e-15 | 0 | J |
| `surface_coupling_max_curvature` (epithelial) | 2.5e6 | 5e6 | 1/m |
| `adherence_strength` (apical, lateral) | 1e9 | 0 | N/m³ |
| `repulsion_strength` (apical, lateral / basal) | 1e9 / 5e8 | 1e8 / 1e8 | N/m³ |

The epithelial division volume is $1.4\times10^{-14}$ m$^3$ in both files, about the volume of the
initial cell, so the first division happens early in the run. Growth rates are drawn from a
normal distribution for each cell, so two runs are not identical.

The output folders are `simulation_results/dynamic_simulation/` and
`simulation_results/overdamped_simulation/` in the repository root. The input mesh path starts
with `/data/`; it is found relative to the source directory after a warning.

## What to look at

Open `cell_data/result_*.vtk` in ParaView and colour by `cell_id` to follow the divisions, or by
`cell_volume`. The number of cells at each output time is the number of distinct `cell_id` values
per `iteration` in `simulation_statistics.csv`.

## Variant

`parameters/parameters_paper_exact.xml` is `parameters_default_dynamic.xml` with a longer
duration ($5\times10^{-2}$ s) and a sampling period of $10^{-4}$ s; it needs the same build
(`CONTACT_MODEL_INDEX 1`).

:::{seealso}
- {doc}`growth_from_one_cell`: growth from one cell in the default configuration.
:::
