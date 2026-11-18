(ex-growth)=
# Tissue growth from a single cell

Grow a tissue from one cell by repeated growth and division. This is the exponential-growth
scenario that Runser, Vetter & Iber (2024) use to measure computational efficiency (Fig. 1e of
the paper; {doc}`/citing`): they report growing a tissue to 125,000 cells in one day of
computation on an eight-core Intel Xeon W-2245 with 16 threads, for cells with 121 nodes and 238
faces on average. The parameter file of this example is not the lab's configuration: it was
reconstructed for 2.x from Table 2 of the paper and is called the *reconstructed paper-like
configuration* in these pages ({ref}`ex-growth-reconstruction`). It is the long-run test case of
release 2.1.

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
gives the reason (the mesh resolution the paper reports for this benchmark). The input cell is
re-triangulated at the start of the run, and edge swaps are enabled. With the default build the
basal face type and `adherence_strength` have no effect
({doc}`/user_guide/10_compile_time_configuration`).

(ex-growth-reconstruction)=
### Differences from the paper

The file differs from Table 2 of the paper (dynamic column) in:

| Quantity | Table 2 | `longrun_fast_growth.xml` |
|---|---|---|
| Minimum edge length $l_{\min}$ | $2\times10^{-7}$ m | $10^{-6}$ m |
| Contact cutoff $c$ | $2\times10^{-7}$ m | $2\times10^{-7}$ m (so $c = l_{\min}/5$) |
| Lateral surface tension | $10^{-3}$ N/m | $8\times10^{-4}$ N/m |
| Basal adhesion $\omega$, repulsion $\xi$ | $10^{9}$, $10^{9}$ Pa/m | 0, $5\times10^{8}$ Pa/m (no effect with contact model 2) |
| Contact model | not stated per figure | 2 (face-face coupling, not described in the paper) |
| Standard deviations of growth rate and division volume | none listed | 10% of the means |

Release 1.0 shipped the lab's own benchmark file, `parameters_benchmarck.xml` (contact model 1,
polarization mode 0, $l_{\min} = 6\times10^{-7}$ m and $c = 5\times10^{-7}$ m, among other
differences from Table 2). It was removed in 2.0 without a note and can be obtained with
`git show v1.0:parameters_benchmarck.xml`. It was run once per setting on 8 cores in November
2026, with its compile-time switches. At its shipped time step of $10^{-7}$ s, 2.1 removed
blown-up cells in large numbers from about 9,000 cells on (4,456 of 157,716 cells created) and was
stopped at 28,600 cells; at $2.5\times10^{-8}$ s it grew to 18,300 cells without removals before
it was stopped. 1.0 stopped with a mesh refinement error at about 170 cells at $10^{-7}$ s. These
are single runs, not a validation.

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
- Cells interpenetrate, with 2.1 and with 1.0. One possible cause is the contact cutoff of
  $2\times10^{-7}$ m, five times smaller than the minimum edge length of this file; Table 2 of
  the paper uses the same value for both.
- Count the removals in the log of every run: the unstable-cell removal has no limit, and a run
  that diverges can lose most of its cells and still exit with status 0
  ({doc}`/user_guide/09_safeguards`).

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
