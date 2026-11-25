(ex-growth)=
# Tissue growth from a single cell

Grow a tissue from one cell by repeated growth and division. This is the exponential-growth
scenario that Runser, Vetter & Iber (2024) use to measure computational efficiency (Fig. 1e of
the paper; {doc}`/citing`): they report growing a tissue to 125,000 cells in one day of
computation on an eight-core Intel Xeon W-2245 with 16 threads, for cells with 121 nodes and 238
faces on average. The parameter file of this example is not the lab's configuration: it was
reconstructed for 2.x from Table 2 of the paper and is called the *reconstructed paper-like
configuration* in these pages ({ref}`ex-growth-reconstruction`). It is the long-run test case of
release 2.1. The lab's own benchmark file of release 1.0 uses another contact model; it is not
shipped with 2.1.0, and the November 2026 benchmark ran it instead of this file
({ref}`ex-growth-lab-file`). The measurements on this page therefore come from two sources, and
each is labelled with the file it belongs to.

**Files**: `parameters/longrun_fast_growth.xml`, `data/input_meshes/paper_sphere.vtk` (one cell,
about 12 µm in diameter, volume about $10^{-15}$ m$^3$); launcher `scripts/runs/launch_longrun_20k.sh`. \
**Compile-time configuration**: the defaults (`POLARIZATION_MODE_INDEX 1`,
`CONTACT_MODEL_INDEX 2`, `DYNAMIC_MODEL_INDEX 0`). \
**Length**: very long. $2\times10^{-3}$ s of simulated time at a time step of `1e-7` s (20,000
iterations), with a cost per iteration that grows with the number of cells. \
**Measured run time**: 30,000 cells in about 4 hours; stopped at 150,498 cells after 13,947
iterations and 52.9 hours, well before the end of the file. Release candidate of 2.1 on an
8-core cloud machine (GCP c2-standard-16, 8 threads), November 2026 ({ref}`ex-growth-expect`).

```{figure} /images/example_growth.png
:class: dark-light
:alt: A spherical cluster of about two thousand round cells in 3D, and its cross-section, in which many round cell outlines lie on top of each other.
:width: 100%

This page's file (the reconstructed paper-like configuration, contact model 2) at 1,934 cells, run with a pre-release build of 2.1, not 2.1.0: 3D view (left, bar 25 µm) and mid-plane cut (right, bar 10 µm). Fills are translucent; the darker areas of the cut are where cell profiles overlap, the interpenetration described under {ref}`ex-growth-expect`.
```

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

Release 1.0 shipped the lab's own benchmark file, `parameters_benchmarck.xml`, which is
closer to the paper's benchmark in its contact model but also differs from Table 2
({ref}`ex-growth-lab-file`).

## Run

The launcher starts the run in the background from `build/` with
`--output-dir=simulation_results/longrun_20k_fast_growth` and the diagnostics CSV in that folder,
and writes the log, which contains the safeguard messages, to
`simulation_results/longrun_20k_fast_growth_logs/run.log` (outside the output folder, which the
program deletes at start). It also deletes the previous output and log folders and stops a
running instance with the same output folder. From the repository root:

```console
$ bash scripts/runs/launch_longrun_20k.sh
$ tail -f simulation_results/longrun_20k_fast_growth_logs/run.log
```

`THREADS=N bash scripts/runs/launch_longrun_20k.sh` passes `--threads=N`. The launcher uses
`/usr/bin/time` and `stdbuf`, which are part of the GNU `time` and `coreutils` packages.
Without the launcher, the equivalent foreground run is, from `build/`:

```console
$ ./simucell3d ../parameters/longrun_fast_growth.xml --output-dir=../simulation_results/longrun_20k_fast_growth
```

(ex-growth-expect)=
## What to expect from this file

These measurements were made in October and November 2026 with release candidates of 2.1, on
8-core cloud machines (GCP c2-standard-16, 8 threads), one run per setting. They are reported in
the CHANGELOG of 2.1.0; they are single runs, not a validation.

- The run reaches 30,000 cells in about 4 hours. In one run from 1 cell, 150,498 cells were
  reached at iteration 13,945, and the run was stopped at iteration 13,947 after 52.9 hours.
- In that run, 14% of division attempts were refused (5% below 30,000 cells, 21% between
  125,000 and 150,000 cells). A refused mother tries again at the next division check; each
  refusal is logged with the cell id.
- The safeguards removed 50 cells in that run, sometimes in small groups after a single node was
  thrown far from its cell (8 cells in one iteration, 4 with the same extent of
  $1.5\times10^{9}$ m in another). Each removal is logged, and counted in the
  `unstable_cells_removed` column of `performance_diagnostics.csv`.
- Cells interpenetrate, with 2.1 and with 1.0. One possible cause is the contact cutoff of
  $2\times10^{-7}$ m, five times smaller than the minimum edge length of this file; Table 2 of
  the paper uses the same value for both.
- Count the removals in the log of every run: the unstable-cell removal has no limit, and a run
  that diverges can lose most of its cells and still exit with status 0
  ({doc}`/user_guide/09_safeguards`).

See {ref}`whatsnew-v2-1-known-issues` and {doc}`/user_guide/09_safeguards`.

(ex-growth-lab-file)=
## The lab's benchmark file

`parameters_benchmarck.xml` is the growth benchmark that the lab shipped with release 1.0. It
uses contact model 1 (node-node coupling, described in the paper) and polarization mode 0, a
smaller sphere (`data/input_meshes/sphere.vtk`), $l_{\min} = 6\times10^{-7}$ m,
$c = 5\times10^{-7}$ m, a bulk modulus of $6\times10^{3}$ Pa and a division volume of
$2\times10^{-16}$ m$^3$, among other differences from Table 2. Release 2.0 removed it without a
note, and 2.1.0 does not ship it: `parameters/benchmarks/` contains only the two Fig. 3e files
([#122](https://github.com/nilesh-patil/simucell3d/issues/122)). The November 2026 benchmark ran
this file, not `longrun_fast_growth.xml`, in every version ({ref}`benchmarks-method`).

To run it with 2.1.0, extract it from the tag and set the compile-time switches of its header
(`WRITE_NORMALS_IN_OUTPUT_MESH_FILE false`, `POLARIZATION_MODE_INDEX 0`,
`CONTACT_MODEL_INDEX 1`, `DYNAMIC_MODEL_INDEX 0`) in `include/global_configuration.hpp`, then
rebuild. Its shipped time step of `1e-7` s is not stable (see below). The recommended time step
is `2.5e-8` s: all three 48-hour runs of 2.1.0 at that step removed no cell. This is a measurement
on this file, not a guarantee for other files ({ref}`benchmarks-time-step`). From the repository
root:

```console
$ git show v1.0:parameters_benchmarck.xml > benchmark_lab.xml
$ sed -i 's|<time_step>1e-07</time_step>|<time_step>2.5e-08</time_step>|' benchmark_lab.xml
$ cd build && make -j8
$ ./simucell3d ../benchmark_lab.xml --output-dir=../simulation_results/benchmark_lab
```

The file sets `simulation_duration` to 1 s, far longer than any run; stop the run when it has
reached the cell count you need. Restore the default switches and rebuild afterwards.

Measured with 2.1.0 on 8-core cloud machines (GCP c2-standard-16, 8 threads), November 2026,
three runs per time step unless stated:

- **At `2.5e-8` s** no cell was removed in any run. The three runs reached 144,919,
  148,914 and 142,314 cells when they were stopped at 48 hours. 10,000 cells took
  $1.28 \pm 0.01$ h and 125,000 cells $37.7 \pm 0.8$ h; cumulative wall time grew as $N^{1.33}$,
  the exponent of the fit in Fig. 1e of the paper ({ref}`benchmarks-performance`). The paper
  reports one day for 125,000 cells on 16 threads at `1e-7` s; machine, thread count and time
  step all differ.
- **At `1e-7` s**, the shipped value and that of Table 2, the file is unstable in every
  version tested. One 2.1.0 run reached 150,000 cells in 21.7 hours, but the safeguards removed
  93,310 cells on the way: the first at 4,800 cells, then in large numbers from about 13,800
  cells. Only the log and the `unstable_cells_removed` column show this
  ({ref}`benchmarks-time-step`).
- **Beyond about 5,000 cells the aggregate is not a packed tissue**, in every version that grows
  past that size. It is packed up to about 1,000 cells in every run and up to about 4,000 cells
  in 2.1.0; in one run of 1.0 with two crash fixes the overlap began at about 2,000 cells. The core
  comes to hold more cell volume than space, because pairs of cells come to occupy the same place
  ({term}`cell superposition`); at 136,043 cells this was 39% of all cells. Treat cell counts above that
  size as a measure of throughput and cost, and per-cell statistics as valid only in the outer
  shell of the aggregate ({ref}`benchmarks-validity`).
- About a third of all cells ever created die by falling below the minimum volume, a
  {term}`minimum-volume death` (35.7% by
  144,000 living cells). These deaths are part of the model, but no output reports them; count
  them from the cell ids ({ref}`whatsnew-v2-1-known-issues`).
- About a quarter of division attempts are refused (21% below 30,000 cells, 27% between 125,000
  and 150,000), about 170,000 refusals in a 48-hour run.
- The runs used 8 threads, although contact model 1 adds contact forces without an atomic add
  ([#120](https://github.com/nilesh-patil/simucell3d/issues/120)). {term}`Replicates <replicate>` agreed to within 2% in
  final cell count; the size of the effect itself was not measured.
- Release 1.0 stops on this file with the error "The refinement of the mesh of cell N failed" at
  126 to 165 cells, at each of four time steps from `1e-7` s to `1.25e-8` s
  ({doc}`/whats_new/v1.0`).

## What to look at

- The `cells` column of `performance_diagnostics.csv` (every 100 iterations) gives the number of
  cells, and `total_iteration_ms` the wall time per iteration, for a plot of cost against cell
  count as in Fig. 1e of the paper.
- `longrun_fast_growth.xml` writes a pair of mesh files every 1,000 iterations
  (`<sampling_period>` $10^{-4}$ s at `1e-7` s). The lab's file writes one every 1,200 iterations
  (`3e-5` s at `2.5e-8` s). Colour `cell_data/result_*.vtk` by `cell_volume` or `cell_pressure` in ParaView.

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
