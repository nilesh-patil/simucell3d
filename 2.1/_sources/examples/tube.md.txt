(ex-tube)=
# Epithelial tube

Relax an epithelial tube of 192 cells. A tube is one of the tissue topologies shown in Fig. 1f of
Runser, Vetter & Iber (2024) ({doc}`/citing`).

**Files**: `parameters/core/parameters_tube.xml`, `data/input_meshes/fig_1_tube.vtk`
(192 epithelial cells).
**Compile-time configuration**: the defaults (`POLARIZATION_MODE_INDEX 1`,
`CONTACT_MODEL_INDEX 2`, `DYNAMIC_MODEL_INDEX 0`).
**Length**: long. $10^{-2}$ s of simulated time at a time step of $10^{-7}$ s ($10^5$
iterations).

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

## What to look at

Open `cell_data/result_*.vtk` in ParaView and colour by `cell_pressure`; open
`face_data/result_*.vtk` and colour by `face_type_id` to see the apical and lateral labels
assigned from cell contacts. As for the sheet, the basal face type and `adherence_strength` of
the file have no effect in the default build ({doc}`/user_guide/10_compile_time_configuration`).

:::{seealso}
- {doc}`sheet`
- {doc}`/user_guide/07_polarization`
:::
