(ex-growth)=
# Tissue growth from a single cell

Grow a tissue from one cell by repeated growth and division. This is the exponential-growth
scenario that Runser, Vetter & Iber (2024) use to measure computational efficiency (Fig. 1e of
the paper; {doc}`/citing`): they report growing a tissue to 125,000 cells in one day of
computation on an eight-core Intel Xeon W-2245 with 16 threads, for cells with 121 nodes and 238
faces on average. The same configuration is the long-run test case of release 2.1.

**Files**: `parameters/longrun_fast_growth.xml`, `data/input_meshes/paper_sphere.vtk` (one cell,
about 12 µm in diameter, volume about $10^{-15}$ m$^3$); launcher `scripts/runs/launch_longrun_20k.sh`.
**Compile-time configuration**: the defaults (`POLARIZATION_MODE_INDEX 1`,
`CONTACT_MODEL_INDEX 2`, `DYNAMIC_MODEL_INDEX 0`).
**Length**: very long. $2\times10^{-3}$ s of simulated time at a time step of $10^{-7}$ s
($2\times10^4$ iterations), with a cost per iteration that grows with the number of cells.

## The parameter file

```{literalinclude} ../../parameters/longrun_fast_growth.xml
:language: xml
:caption: parameters/longrun_fast_growth.xml (numerical parameters)
:start-at: <numerical_parameters>
:end-at: </numerical_parameters>
```

The file defines a single cell type, `epithelial`, with these properties:

| Quantity | XML tag | Value | Unit |
|---|---|---|---|
| Bulk modulus, pressure cap | `cell_bulk_modulus`, `max_inner_pressure` | 2.5e3 | Pa |
| Area elasticity modulus | `area_elasticity_modulus` | 1e-15 | J |
| Target isoperimetric ratio | `target_isoperimetric_ratio` | 250 | |
| Growth rate (mean, standard deviation) | `avg_growth_rate`, `std_growth_rate` | 1e-11, 1e-12 | m³/s |
| Division volume (mean, standard deviation) | `avg_division_volume`, `std_division_volume` | 1.4e-15, 1.4e-16 | m³ |
| Minimum volume | `min_vol` | 3.7e-16 | m³ |
| Surface tension (apical, lateral, basal) | `surface_tension` | 1e-3, 8e-4, 1e-3 | N/m |
| Bending modulus (all faces) | `bending_modulus` | 2e-18 | J |
| Adherence strength (apical, lateral, basal) | `adherence_strength` | 1e9, 1e9, 0 | N/m³ |

A daughter cell of about half the division volume reaches the division volume again after about
$0.7\times10^{-15} / 10^{-11} = 7\times10^{-5}$ s, that is 700 iterations. The minimum edge length
is $10^{-6}$ m, coarser than the value in Table 2 of the paper; the header comment of the file
explains that this reproduces the mesh resolution the paper reports for this benchmark. The
input cell is re-triangulated at the start of the run, and edge swaps are enabled.

## Run

The launcher starts the run in the background from `build/` with
`--output-dir=simulation_results/longrun_20k_fast_growth` and the diagnostics CSV in that folder,
and writes the log, which contains the safeguard messages, to
`simulation_results/longrun_20k_fast_growth_logs/run.log` (outside the output folder, which the
program deletes at start). It also deletes the previous output and log folders and stops a
running instance with the same output folder. From the repository root:

```console
$ scripts/runs/launch_longrun_20k.sh
$ tail -f simulation_results/longrun_20k_fast_growth_logs/run.log
```

`THREADS=N scripts/runs/launch_longrun_20k.sh` passes `--threads=N`. The launcher uses
`/usr/bin/time` and `stdbuf`, which are part of the GNU `time` and `coreutils` packages.
Without the launcher, the equivalent foreground run is, from `build/`:

```console
$ ./simucell3d ../parameters/longrun_fast_growth.xml --output-dir=../simulation_results/longrun_20k_fast_growth
```

## What to expect

The CHANGELOG reports these measurements for 2.1 with this configuration:

- On 8 cores, the run reaches 30,000 cells in about 4 hours.
- In an 8-core run from 1 to 150,498 cells, 14% of division attempts were refused (5% below
  30,000 cells, 21% between 125,000 and 150,000 cells). A refused mother tries again at the next
  division check; each refusal is logged with the cell id.
- Isolated cells are removed by the safeguards during long runs, sometimes in small groups after
  a single node is thrown far from its cell. Each removal is logged, and counted in the
  `unstable_cells_removed` column of `performance_diagnostics.csv`.
- Cells interpenetrate. A likely cause is the contact cutoff of $2\times10^{-7}$ m, five times
  smaller than the minimum edge length.

See {ref}`whatsnew-v2-1-known-issues` and {doc}`/user_guide/09_safeguards`.

## What to look at

- The `cells` column of `performance_diagnostics.csv` (every 100 iterations) gives the number of
  cells, and `total_iteration_ms` the wall time per iteration, for a plot of cost against cell
  count as in Fig. 1e of the paper.
- With `<sampling_period>` $10^{-4}$ s the run writes a pair of mesh files every 1000 iterations.
  Colour `cell_data/result_*.vtk` by `cell_volume` or `cell_pressure` in ParaView.

## Variant

`parameters/parameters_paper_exact_fast_growth.xml` uses the larger `big_sphere.vtk` cell, a
division volume of $1.4\times10^{-14}$ m$^3$, a growth rate of $5\times10^{-11}$ m$^3$/s, contact
cutoffs of $5\times10^{-7}$ m and no bending; its sampling period of 1 s is longer than any
practical run, so it writes almost no mesh files.

:::{seealso}
- {doc}`single_cell_dynamic_overdamped`: growth from one larger cell with the 1.0 default
  parameter files.
- {doc}`/user_guide/18_parallel_performance`: thread count, pinning and scheduling for long runs.
:::
