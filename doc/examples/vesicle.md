(ex-vesicle)=
# Epithelial vesicle with lumen and ECM

Relax a hollow spherical vesicle of 432 columnar epithelial cells around a lumen, encased in
extracellular matrix (ECM). This is the initial tissue of Fig. 3a in Runser, Vetter & Iber (2024)
({doc}`/citing`), and the starting point of {doc}`monolayer_multilayer`.

**Files**: `parameters/core/parameters_vesicle.xml`, `data/input_meshes/fig_3_vesicle.vtk`
(434 cells: 432 epithelial cells, one ECM surface, one lumen).
**Compile-time configuration**: the defaults (`POLARIZATION_MODE_INDEX 1`,
`CONTACT_MODEL_INDEX 2`, `DYNAMIC_MODEL_INDEX 0`).
**Length**: long. $10^{-2}$ s of simulated time at a time step of $10^{-7}$ s, that is $10^5$
iterations.

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
`<sampling_period>` is $10^{-6}$ s, so the run writes $10^4$ pairs of mesh files. For this tissue
one pair (`cell_data` and `face_data`) takes about 60 MB, so the complete run needs several
hundred gigabytes. Increase `<sampling_period>` in a copy of the file (for example to $10^{-4}$ s,
100 pairs) before a full run. The output folder, `simulation_results/simulation_vesicle/` in the
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

## Variations

- {doc}`vesicle_quick` runs a short version of this setup.
- {doc}`monolayer_multilayer` changes the apical surface tension of the same tissue.
- `parameters/core/parameters_vesicle_realistic_biology.xml` is a further variant (longer
  duration, division enabled); it is not described here.
