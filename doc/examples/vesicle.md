(ex-vesicle)=
# Epithelial vesicle with lumen and ECM

Relax a hollow spherical vesicle of 432 columnar epithelial cells around a lumen, encased in
extracellular matrix (ECM). This is the initial tissue of Fig. 3a in Runser, Vetter & Iber (2024)
({doc}`/citing`), and the starting point of {doc}`monolayer_multilayer`.

**Files**: `parameters/core/parameters_vesicle.xml`, `data/input_meshes/fig_3_vesicle.vtk`
(434 cells: 432 epithelial cells, one ECM surface, one lumen). \
**Compile-time configuration**: the defaults (`POLARIZATION_MODE_INDEX 1`,
`CONTACT_MODEL_INDEX 2`, `DYNAMIC_MODEL_INDEX 0`). \
**Length**: very long. $10^{-2}$ s of simulated time at a time step of `1e-7` s (100,000
iterations), with cells that grow throughout the run. \
**Measured run time**: more than two days as shipped (87,084 iterations in 48 hours with the
same parameters); 6.20 ± 0.95 hours with growth switched off. 2.1.0 on an 8-core cloud machine (GCP c2-standard-16, 8 threads),
November 2026 ({ref}`ex-vesicle-expect`).

```{figure} /images/example_vesicle.png
:class: dark-light
:alt: Two cross-sections of the vesicle at one scale: a small ring of columnar cells around a lumen inside a green ECM outline, and the same tissue at the end of the run, many times larger, with large polygonal cells extending far beyond the ECM outline.
:width: 100%

The vesicle with the shipped parameters, run with 2.1.0: mid-plane cross-sections of the input mesh (left) and of the last snapshot, at iteration 87,000 (right), at one scale. The cells grow without dividing and end almost entirely outside the static ECM surface (green circle), as described under {ref}`ex-vesicle-expect`. Outlines by face type: apical orange, lateral blue, ECM face green, lumen face amber. Bar 20 µm.
```

## The parameter file

The numerical parameters of the run:

```{literalinclude} ../../parameters/core/parameters_vesicle.xml
:language: xml
:caption: parameters/core/parameters_vesicle.xml (numerical parameters)
:start-at: <numerical_parameters>
:end-at: </numerical_parameters>
```

The file defines five cell types, in this order: `epithelial`, `ecm`, `lumen`, `nucleus` and
`static_cell`. The `cell_type_id` of each cell in the input mesh is the position of its cell type
in this list, so the mesh uses the first three. Epithelial cells have three face types (apical,
lateral, basal) with surface tensions of $10^{-3}$, $4\times10^{-4}$ and $10^{-3}$ N/m; the file
gives apical and lateral faces an adherence strength of $10^{9}$ N/m$^3$ and basal faces none.
The epithelial cells grow ($10^{-12}$ m$^3$/s) but never divide (`avg_division_volume` is `inf`).
The full parameter reference is {doc}`/user_guide/11_parameter_file`.

:::{important}
In the default build (contact model 2) the basal face type and `adherence_strength` have no
effect. Polarization never labels a face basal, so the faces that face the ECM use the apical
parameters, and adhesion is a rigid coupling between nodes of neighbouring epithelial cells, not a
force of strength $\omega$. Only contact model 0 uses basal faces and $\omega$; this file has not
been tested with it ({doc}`/user_guide/10_compile_time_configuration`).
:::

## Run

From the `build/` directory:

```console
$ ./simucell3d ../parameters/core/parameters_vesicle.xml
```

:::{warning}
`<sampling_period>` is $10^{-6}$ s, so the run writes 10,000 pairs of mesh files (`cell_data` and
`face_data`). The pairs grow with the cells. With the same parameters, the first pair took 112 MB
and the pair at iteration 87,000 took 651 MB. The complete run would need several terabytes.
Set `<sampling_period>` to $10^{-4}$ s in a copy of the file before a full run. That gives about
100 pairs; the 88 pairs written up to iteration 87,000 took 33 GB. The output folder, `simulation_results/simulation_vesicle/` in the
repository root, is deleted at the start of the run.
:::

The input mesh is given as `data/input_meshes/fig_3_vesicle.vtk`, a path relative to the
repository root. Run from `build/`, that path does not exist, so the program prints a warning and
loads the mesh relative to the source directory instead; this is expected.

## What to look at

- In ParaView, open the series `cell_data/result_*.vtk` and colour by `cell_type_id` to separate
  the epithelium, the lumen and the ECM, or by `cell_pressure` and `cell_volume`.
- Open `face_data/result_*.vtk` and colour by `face_type_id` to see the apical and lateral faces
  (no face is basal in the default build), or by `face_surface_tension`.
- `simulation_statistics.csv` has one row per cell every 50 iterations; plot it with
  `python3 scripts/data_plotting/plot_data.py simulation_results/simulation_vesicle` from the
  repository root ({doc}`/getting_started`).

(ex-vesicle-expect)=
## What to expect

The November 2026 benchmark measured this tissue with 2.1.0 on 8-core cloud machines (GCP
c2-standard-16, 8 threads). It did not run this file as shipped, but two files close to it:

- **The shipped parameters.** `parameters/benchmarks/fig3e_multilayer.xml` has the parameters of
  this file and a longer sampling period. With it, 2.1.0 completed 87,084 of the 100,000
  iterations in 48 hours, when the run was stopped. The epithelial cells grow at $10^{-12}$
  m$^3$/s for the whole run, so their target volume rises by $10^{-14}$ m$^3$, about 58 times the
  mean initial cell volume, and each iteration costs more as their surfaces grow. The ECM
  surface does not hold them: late in the run, 98-100% of the epithelial cells, in every version
  tested, had more than half of their nodes outside it ({doc}`monolayer_multilayer`).
- **A relaxation sweep.** The same tissue with growth switched off and the apical surface tension
  set to $\varepsilon \times 4\times10^{-4}$ N/m, for five ratios
  $\varepsilon = \gamma_\text{apical} / \gamma_\text{lateral}$ from 1 to 4 (this file corresponds to $\varepsilon = 2.5$ with growth),
  three runs each. Every one of the 15 runs reached iteration 100,000 with no cell removed, in
  $6.20 \pm 0.95$ hours.

What the sweep showed, for 2.1.0:

- Readouts that respond to $\varepsilon$ well beyond the noise between repeats: the median
  sphericity of the epithelial cells falls from 0.983 at $\varepsilon = 1$ to 0.877 at
  $\varepsilon = 4$, the mean pressure rises from 291 to 430 Pa, and the contact-area fraction
  changes by more than ten times its repeat spread between neighbouring values, but not
  monotonically (it peaks at $\varepsilon = 3$).
- {term}`Replicates <replicate>` with the same input are not identical: mean pressure varies by 0.1-1% between repeats,
  median sphericity by less than 0.1% ({ref}`benchmarks-reproducibility`).
- The static ECM shell does not confine the tissue into a monolayer. The median aspect ratio of
  the cells (from the oriented bounding box of each cell) stays between 1.09 and 1.21 over a
  fourfold change of $\varepsilon$; at $\varepsilon = 1$ the epithelium splits into two layers, one on the lumen and one on the ECM,
  and above $\varepsilon = 1$, 12-27% of the cells touch neither surface. The shape of the
  epithelium in this setup is therefore not a test of how cell aspect ratio depends on tension
  ({ref}`benchmarks-validity`).

To run the sweep, copy this file and set, for the `epithelial` cell type, `avg_growth_rate` to 0
and the `surface_tension` of the apical face type to the value for the chosen $\varepsilon$; a
`sampling_period` of $10^{-4}$ s gives 100 pairs of mesh files.

## Variations

- {doc}`vesicle_quick` runs a short version of this setup.
- {doc}`monolayer_multilayer` changes the apical surface tension of the same tissue.
- `parameters/core/parameters_vesicle_realistic_biology.xml` is a further variant (longer
  duration, division enabled); it is not described here.
