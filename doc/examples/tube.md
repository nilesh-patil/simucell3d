(ex-tube)=
# Epithelial tube

Relax an epithelial tube of 192 cells. A tube is one of the tissue topologies shown in Fig. 1f of
Runser, Vetter & Iber (2024) ({doc}`/citing`).

**Files**: `parameters/core/parameters_tube.xml`, `data/input_meshes/fig_1_tube.vtk`
(192 epithelial cells). \
**Compile-time configuration**: the defaults (`POLARIZATION_MODE_INDEX 1`,
`CONTACT_MODEL_INDEX 2`, `DYNAMIC_MODEL_INDEX 0`). \
**Length**: long. $10^{-2}$ s of simulated time at a time step of `1e-7` s (100,000
iterations). \
**Measured run time**: 3.69 hours with 2.1.0 on an 8-core cloud machine (GCP c2-standard-16, 8 threads), November 2026.

```{figure} /images/example_tube.png
:class: dark-light
:alt: An elongated tissue of rounded cells in 3D, and its cross-section, which is filled with polygonal cells and has no central lumen.
:width: 100%

The tube after 100,000 iterations with 2.1.0 (192 cells): 3D view (left) and transverse mid-plane cut (right). The tube has shortened and its lumen has closed, so cells fill the cross-section. Free surfaces apical (orange), contacts lateral (blue). Bars 10 µm.
```

## The parameter file

```{literalinclude} ../../parameters/core/parameters_tube.xml
:language: xml
:caption: parameters/core/parameters_tube.xml (numerical parameters)
:start-at: <numerical_parameters>
:end-at: </numerical_parameters>
```

The file differs from {doc}`sheet` only in the input mesh, the output folder, the damping
coefficient ($5\times10^{-10}$ kg/s instead of $2\times10^{-9}$ kg/s) and the surface tension of
the lateral faces ($4\times10^{-4}$ N/m instead of $5\times10^{-4}$ N/m). All cells of the mesh use the
`epithelial` cell type, which neither grows nor divides. The input cells are triangulated at the
start of the run.

## Run

From the `build/` directory:

```console
$ ./simucell3d ../parameters/core/parameters_tube.xml
```

The output folder is `simulation_results/simulation_tube/` in the repository root; it is deleted
at the start of the run. As for the sheet, the run writes $10^4$ pairs of mesh files, and the
mesh path starting with `/data/` is resolved relative to the source directory after a warning.

## What to expect

The November 2026 benchmark ran this file with 2.1.0 on an 8-core cloud machine (GCP
c2-standard-16, 8 threads), one run, with a sampling period of $10^{-4}$ s instead of
$10^{-6}$ s (output only):

- The 100,000 iterations took 3.69 hours (1.0 with two crash fixes: 6.29 hours), and no cell was
  removed.
- The tube relaxes without growing: it shortens by 19-24%, its lumen closes so that cells fill
  the cross-section, and the cells end at a pressure of 284-286 Pa with about 10.5 contact
  neighbours each. This is the same in every version tested, 1.0 included, to within the
  run-to-run spread.

## What to look at

Open `cell_data/result_*.vtk` in ParaView and colour by `cell_pressure`; open
`face_data/result_*.vtk` and colour by `face_type_id` to see the apical and lateral labels
assigned from cell contacts. As for the sheet, the basal face type and `adherence_strength` of
the file have no effect in the default build ({doc}`/user_guide/10_compile_time_configuration`).

:::{seealso}
- {doc}`sheet`
- {doc}`/user_guide/07_polarization`
:::
