(gs)=
# Getting started

This tutorial runs a first simulation: a short run of an epithelial vesicle of 432 cells around
a lumen, inside a layer of extracellular matrix (ECM). It shows what the program prints, where
the results are written, how to view them in ParaView and plot the statistics, and how to change
a parameter. It assumes that SimuCell3D has been built from source as described in
{doc}`install`, in the `build/` directory of the repository, with the default compile-time
configuration.

## How a simulation is defined

A simulation needs two input files:

- A **parameter file** in XML, which sets the numerical parameters (time step, duration,
  output period, mesh resolution, contact cutoffs) and the mechanical properties of each
  {term}`cell type` and {term}`face type`.
- An **input mesh** in the legacy VTK format, which gives the initial shape of every cell. The
  parameter file names it in `<input_mesh_file_path>`.

The parameter file also names the folder where the results are written,
`<output_mesh_folder_path>`. Some choices, such as the contact model, are made at compile time
instead ({doc}`/user_guide/10_compile_time_configuration`).

This tutorial uses `parameters/core/parameters_vesicle_quick.xml`. Its numerical parameters are:

```{literalinclude} ../parameters/core/parameters_vesicle_quick.xml
:language: xml
:caption: parameters/core/parameters_vesicle_quick.xml (numerical parameters)
:start-at: <numerical_parameters>
:end-at: </numerical_parameters>
```

The run covers $2\times10^{-5}$ s of simulated time with a time step of $10^{-7}$ s, that is 200
{term}`iterations <iteration>`, and writes the tissue every $5\times10^{-6}$ s. The input mesh,
`data/input_meshes/fig_3_vesicle.vtk`, contains 434 cells: 432 epithelial cells, one ECM surface
and one lumen. It is the initial tissue of Fig. 3a in Runser, Vetter & Iber (2024).

## Run the simulation

From the `build/` directory:

```console
$ ./simucell3d ../parameters/core/parameters_vesicle_quick.xml
```

:::{warning}
The output folder named in the parameter file is **deleted** at the start of every run, with all
its content. Here it is `../simulation_results/simulation_vesicle`, which the
{doc}`vesicle example </examples/vesicle>` uses too. To keep the results of a run, copy them
elsewhere or give each run its own folder with `--output-dir=PATH`.
:::

The program first reports how it found the input mesh and how it will run in parallel:

```text
WARNING: the absolute mesh file path: data/input_meshes/fig_3_vesicle.vtk was not found
The program will try instead the relative path: /path/to/simucell3d/data/input_meshes/fig_3_vesicle.vtk

Triangulating cell 1 of type: epithelial
...
Thread count: Using 4 physical cores (out of 8 allowed logical CPUs, 2 threads/core, 1 socket(s))
Thread pinning: enabled, thread i -> CPU 0,1,2,3
...
Using OpenMP adaptive (base: static, per-loop: ON) scheduling
  Cells: 434, Threads: 4, Chunk size: 0
...
The output folder is: "/path/to/simucell3d/build/../simulation_results/simulation_vesicle"
```

- The mesh path in the parameter file is relative to the repository root. Run from `build/`, it
  is not found as given, so the program tries it relative to the source directory recorded at
  build time. This warning is expected.
- One `Triangulating cell` line is printed for each of the 434 cells as it is created from the
  input mesh, also when the initial triangulation is disabled in the parameter file.
- By default the program uses one thread per physical core and pins each thread to its own core.
  The numbers depend on the machine; `--threads=N` sets another thread count.
- The scheduling lines describe how the OpenMP loops are scheduled
  ({doc}`/user_guide/18_parallel_performance`).

The program then prints one progress line per iteration:

```text
Progression 0%, iteration: 0, file number 1, nb cells 434
Progression 0%, iteration: 1, file number 1, nb cells 434
...
```

`file number` is the number of the last mesh files written, and `nb cells` the current number of
cells. The run ends when the simulated time reaches `<simulation_duration>`.
Each iteration of this 434-cell tissue takes about a second or more, so the run takes a few
minutes. The last progress line is
`Progression 100%, iteration: 199, file number 4, nb cells 434`. At the end the program prints a table of the
average time spent in each phase of an iteration (polarization and internal forces, contact
detection, mesh refinement, time integration).

## Look at the output

The output folder, `simulation_results/simulation_vesicle/` in the repository root, contains:

```text
simulation_vesicle/
├── cell_data/
│   ├── result_1.vtk
│   ├── ...
│   └── result_4.vtk
├── face_data/
│   ├── result_1.vtk
│   ├── ...
│   └── result_4.vtk
├── performance_diagnostics.csv
└── simulation_statistics.csv
```

`cell_data/result_<n>.vtk`
: The tissue at simulated time $t$, with $n = \lfloor t / \text{sampling period} \rfloor + 1$, so
  `result_1.vtk` is the initial state. The file holds one polyhedral VTK cell per simulated cell,
  with the arrays `cell_id`, `cell_type_id`, `cell_area`, `cell_volume`, `cell_pressure`,
  `cell_growth_rate`, `cell_division_vol` and `cell_is_static`.

`face_data/result_<n>.vtk`
: The same tissue as individual triangles, with face arrays such as `face_type_id`, `face_area`
  and `face_surface_tension`, and node arrays such as `mean_curvature` and `node_is_coupled`.

`simulation_statistics.csv`
: One row per cell, every 50 iterations: the iteration, the wall time, the simulated time, and the
  cell's geometry (`area`, `volume`, their targets), `pressure` and energies.

`performance_diagnostics.csv`
: Timing and solver diagnostics every 100 iterations (here one row, at iteration 100), including
  the number of cells removed by the safeguards (`unstable_cells_removed`).

The mesh files are large: this run writes about 290 MB in total, most of it in `face_data/`.

All files are described in {doc}`/user_guide/14_output_files`.

### View the tissue in ParaView

[ParaView](https://www.paraview.org/download/) reads the legacy VTK files directly.

1. *File > Open*, go to `simulation_results/simulation_vesicle/cell_data/`, select the group
   `result_..vtk` and click *OK*. ParaView loads the numbered files as a time series.
2. Click *Apply* in the *Properties* panel.
3. In the colouring menu of the toolbar, choose `cell_type_id` to tell the epithelial cells, the
   lumen and the ECM apart, or `cell_pressure` to see the pressure of each cell.
4. Apply *Filters > Common > Clip* to cut the vesicle open and see the lumen inside.
5. Use the time controls of the toolbar to step through the output files.

Open the `face_data/` series in the same way and colour by `face_type_id` to see the apical and
lateral faces of the epithelial cells. The parameter file also defines a basal face type, but the
default build never uses it: with the default contact model, faces that face the ECM or the
lumen are labelled apical ({doc}`/user_guide/07_polarization`).
{doc}`/user_guide/15_visualization` describes more filters and arrays.

### Plot the statistics

`scripts/data_plotting/plot_data.py` plots the geometry and energies over time from
`simulation_statistics.csv`. It needs the Python packages `pandas`, `matplotlib`, `numpy` and
`tqdm`. From the repository root:

```console
$ python3 scripts/data_plotting/plot_data.py simulation_results/simulation_vesicle
```

The script writes two figures into the output folder. `sim_geometry_data.png` shows, against
simulated time, the number of cells and the mean and standard deviation over all cells of the
volume, area and pressure and of the relative deviation of volume and area from their targets;
`sim_energy_data.png` shows the energies. The averages include the lumen and the ECM, whose
volumes are much larger than those of the epithelial cells.

## Change a parameter

Parameters are changed by editing a copy of the parameter file. For example, to see the effect
of the surface tension of the apical faces, copy the file and change the apical
`<surface_tension>` of the epithelial cell type from `1e-3` to `1e-5` (N/m), and give the copy its
own output folder:

```console
$ cp ../parameters/core/parameters_vesicle_quick.xml ../parameters/core/my_vesicle.xml
$ ./simucell3d ../parameters/core/my_vesicle.xml --output-dir=../simulation_results/my_vesicle
```

```{code-block} xml
:caption: the apical face type in my_vesicle.xml
<face_type>
    <global_face_id>0</global_face_id>
    <face_type_name>apical</face_type_name>
    <adherence_strength>1e9</adherence_strength>
    <repulsion_strength>1e9</repulsion_strength>
    <surface_tension>1e-5</surface_tension>
    <bending_modulus>0</bending_modulus>
</face_type>
```

A run of 200 iterations shows only the start of the tissue's response. The
{doc}`/examples/monolayer_multilayer` example runs the same comparison for $10^5$ iterations.
Every XML tag is described in {doc}`/user_guide/11_parameter_file`.

## Next steps

- {doc}`/examples/index`: the other shipped parameter files.
- {doc}`/user_guide/index`: the model, the parameter file, input meshes and output files.
- {doc}`/user_guide/13_running_simulations`: command-line options, threads and long runs.
- {doc}`/faq`: common problems.
