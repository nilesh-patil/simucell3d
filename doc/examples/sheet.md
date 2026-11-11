(ex-sheet)=
# Epithelial sheet

Relax a flat epithelial sheet of 118 cells. A sheet is one of the tissue topologies shown in
Fig. 1f of Runser, Vetter & Iber (2024) ({doc}`/citing`).

**Files**: `parameters/core/parameters_sheet.xml`, `data/input_meshes/fig_1_sheet_geometry.vtk`
(118 epithelial cells).
**Compile-time configuration**: the defaults (`POLARIZATION_MODE_INDEX 1`,
`CONTACT_MODEL_INDEX 2`, `DYNAMIC_MODEL_INDEX 0`).
**Length**: long. $10^{-2}$ s of simulated time at a time step of $10^{-7}$ s ($10^5$
iterations).

## The parameter file

```{literalinclude} ../../parameters/core/parameters_sheet.xml
:language: xml
:caption: parameters/core/parameters_sheet.xml (numerical parameters)
:start-at: <numerical_parameters>
:end-at: </numerical_parameters>
```

All cells of the input mesh have `cell_type_id` 0, the `epithelial` cell type, whose cells neither
grow (`avg_growth_rate` 0) nor divide (`avg_division_volume` `inf`). The input cells are
polyhedra and are triangulated at the start of the run (`perform_initial_triangulation` 1), with
a minimum edge length of $4\times10^{-7}$ m. The damping coefficient,
$2\times10^{-9}$ kg/s, is four times that of the vesicle example.

## Run

From the `build/` directory:

```console
$ ./simucell3d ../parameters/core/parameters_sheet.xml
```

The output folder is `simulation_results/simulation_sheet/` in the repository root; it is deleted
at the start of the run. With `<sampling_period>` $10^{-6}$ s the run writes $10^4$ pairs of mesh
files; increase it in a copy of the file to limit the disk space used.

The input mesh path `/data/input_meshes/fig_1_sheet_geometry.vtk` starts with `/` but is not an
absolute path on most systems. The program does not find it, prints a warning, and loads it
relative to the source directory; this is expected.

## What to look at

Open `cell_data/result_*.vtk` in ParaView and colour by `cell_pressure` or `cell_volume`; open
`face_data/result_*.vtk` and colour by `face_type_id` to see how the faces of each cell are
labelled apical, lateral or basal from their contacts (contact-based polarization,
{doc}`/user_guide/07_polarization`).

:::{seealso}
- {doc}`tube`: the same parameters on a tubular tissue.
- {doc}`/user_guide/12_input_meshes`: preparing input meshes.
:::
