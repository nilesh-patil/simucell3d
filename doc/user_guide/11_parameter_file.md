(ug-parameter-file)=
# Parameter file reference

A simulation is configured by one XML {term}`parameter file`, passed to `simucell3d` on the
command line. It names the {term}`input mesh` and the output folder, sets the time step and
the other numerical parameters, and defines the mechanical properties of every
{term}`cell type` and {term}`face type`. This chapter lists every tag that SimuCell3D 2.1
reads, with its type, unit, validation rule and meaning, and the parameter files shipped in
`parameters/`.

Three choices are not in the parameter file: the contact model, the dynamic model and the
polarization mode are compile-time options in `include/global_configuration.hpp`
({doc}`/user_guide/10_compile_time_configuration`). Every shipped parameter file states in its
header comment which compile-time configuration it was written for.

## File structure

A parameter file has two top-level sections, `<numerical_parameters>` and `<cell_types>`.
`<cell_types>` holds one or more `<cell_type>` blocks, and each cell type holds a
`<face_types>` block with one or more `<face_type>` entries:

```xml
<?xml version="1.0"?>

<numerical_parameters>
    <input_mesh_file_path>...</input_mesh_file_path>
    <!-- ... the other numerical parameters ... -->
</numerical_parameters>

<cell_types>
    <cell_type>
        <cell_type_name>epithelial</cell_type_name>
        <global_cell_id>0</global_cell_id>
        <!-- ... the other cell-type parameters ... -->
        <face_types>
            <face_type>
                <global_face_id>0</global_face_id>
                <face_type_name>apical</face_type_name>
                <!-- ... the other face-type parameters ... -->
            </face_type>
        </face_types>
    </cell_type>
</cell_types>
```

The reader (`src/io/parameter_reader.cpp`, based on tinyxml2) applies these rules:

- Within a section the order of the tags does not matter. If a tag appears twice, the first
  occurrence is used.
- Tags that the reader does not know are ignored without a warning. Check the spelling of
  every tag against the tables below: a misspelled required tag stops the run, but a
  misspelled optional tag (`contact_detection_algorithm`) is silently replaced by its default.
- Every tag in the tables is required, except `contact_detection_algorithm`. There are no
  defaults for required tags.
- The order of the `<cell_type>` blocks matters: the input mesh refers to cell types by their
  position in the file (see [Cell types and the input mesh](#cell-types-and-the-input-mesh)).
- The order of the `<face_type>` entries inside a cell type also matters for epithelial cells:
  see [Face types](#face-types).
- XML comments (`<!-- ... -->`) are allowed anywhere.

:::{note}
Parameter files of release 1.0 contain an `<INMForce>` tag in the nucleus cell type. No code
reads it, so it has no effect; the files shipped with 2.1 no longer contain it.
:::

### Values

Numbers are read with the C++ functions `std::stod` (real numbers) and `std::stoi`
(integers). Plain decimal and scientific notation are accepted (`1000`, `2.5e3`, `5e-7`).
Characters after a valid number are ignored, so `1e-7 s` is read as $10^{-7}$; do not rely on
this. A value that does not start with a number stops the program before the simulation
starts, with the one-word message `stod` or `stoi`.

Where a table says that `inf` is accepted, the value is compared case-insensitively, so `inf`,
`INF` and `Inf` all mean $+\infty$.

Switches (`perform_initial_triangulation`, `enable_edge_swap_operation`) are integers: `0`
means off and any other integer means on. Words such as `true` are not accepted.

### Units

All values are in SI units. The comments in the shipped files give units as dimensions
(M for mass, L for length, T for time); the `Unit` columns below translate them. One of
those comments is wrong:

:::{important}
The comments in the shipped files give `area_elasticity_modulus` the dimension
`[M / (T^2 L^2)]`. The code uses it as an energy: the area elasticity energy of a cell is
$\frac{k_a}{2}(A/A_0 - 1)^2$, so $k_a$ is in joules (J). The header of
`parameters/longrun_fast_growth.xml` gives the correct unit.
:::

(ug-parameter-file-numerical)=
## Numerical parameters

Section `<numerical_parameters>`. These apply to the whole simulation.

| XML tag | Type | Unit | Valid values | Meaning |
|---|---|---|---|---|
| `input_mesh_file_path` | path |  | an existing legacy VTK file | Input mesh with the initial cells. See [Paths](#paths) and {doc}`/user_guide/12_input_meshes`. |
| `output_mesh_folder_path` | path |  | a folder that may be deleted | Folder for all output. **Deleted and re-created at start.** Overridden by `--output-dir`. See [Paths](#paths) and {doc}`/user_guide/14_output_files`. |
| `perform_initial_triangulation` | switch |  | `0` or any other integer | If on, every cell surface is re-triangulated before the first iteration, with edge lengths between `min_edge_length` and 3 x `min_edge_length`. If off, the input mesh must already consist of triangles. See {doc}`/user_guide/05_mesh_refinement`. |
| `enable_edge_swap_operation` | switch |  | `0` or any other integer | If on, the local mesh refiner also swaps the longest edge of badly shaped triangles. |
| `damping_coefficient` | real | kg/s | $\ge 0$ | Damping coefficient $\zeta$ of the equations of motion. See {doc}`/user_guide/04_time_integration`. |
| `simulation_duration` | real | s | $> 0$ | Simulated time. The run stops when the simulation time reaches this value, or earlier if no cell is left. |
| `time_step` | real | s | $> 0$ | Time step $\Delta t$ of the integrator. The number of iterations is `simulation_duration / time_step`. |
| `sampling_period` | real | s | $> 0$ and $\ge$ `time_step` | Simulated time between two output mesh files. |
| `min_edge_length` | real | m | $> 0$ | Minimum edge length of the cell meshes. The maximum edge length is not a parameter: it is 3 x `min_edge_length`. See {doc}`/user_guide/05_mesh_refinement`. |
| `contact_cutoff_adhesion` | real | m | $> 0$ | Distance between two cell surfaces above which there is no adhesion. See {doc}`/user_guide/03_contact`. |
| `contact_cutoff_repulsion` | real | m | $> 0$ | Distance between two cell surfaces above which there is no repulsion. |
| `contact_detection_algorithm` | word |  | Optional, default `uspg`; `uspg`, `sweep_and_prune`, `adaptive` (case-insensitive) | Broad-phase contact detection. Used only with contact model 2 (face-face coupling, the default build); contact models 0 and 1 always use the uniform spatial partitioning grid (USPG). `adaptive` uses the grid below 500 cells and Sweep-and-Prune from 500 cells on. See {doc}`/user_guide/18_parallel_performance`. |

:::{versionadded} 2.0
`contact_detection_algorithm`. No shipped parameter file sets it, so all of them use the
spatial grid.
:::

The reader rejects a `damping_coefficient` below zero with the message
"The damping_coefficient_ must be strictly positive", although zero itself is accepted.

### Values in the shipped files

| File | Input mesh | `perform_initial_triangulation` | `enable_edge_swap_operation` |
|---|---|---|---|
| `parameters_vesicle_quick.xml` | `fig_3_vesicle.vtk` | 0 | 0 |
| `parameters_vesicle.xml` | `fig_3_vesicle.vtk` | 0 | 0 |
| `parameters_vesicle_realistic_biology.xml` | `fig_3_vesicle.vtk` | 0 | 0 |
| `parameters_sheet.xml` | `fig_1_sheet_geometry.vtk` | 1 | 0 |
| `parameters_tube.xml` | `fig_1_tube.vtk` | 1 | 0 |
| `parameters_default_dynamic.xml` | `big_sphere.vtk` | 1 | 0 |
| `parameters_default_overdamped.xml` | `big_sphere.vtk` | 1 | 0 |
| `fig3e_monolayer.xml` | `fig_3_vesicle.vtk` | 0 | 0 |
| `fig3e_multilayer.xml` | `fig_3_vesicle.vtk` | 0 | 0 |
| `longrun_fast_growth.xml` | `paper_sphere.vtk` | 1 | 1 |
| `parameters_paper_exact.xml` | `big_sphere.vtk` | 1 | 0 |
| `parameters_paper_exact_fast_growth.xml` | `big_sphere.vtk` | 0 | 0 |

| File | $\Delta t$ (s) | Duration (s) | Sampling period (s) | $l_{\min}$ (m) | Cutoffs (m) | $\zeta$ (kg/s) |
|---|---:|---:|---:|---:|---:|---:|
| `parameters_vesicle_quick.xml` | 1e-7 | 2e-5 | 5e-6 | 4e-7 | 2e-7 | 5e-10 |
| `parameters_vesicle.xml` | 1e-7 | 1e-2 | 1e-6 | 4e-7 | 2e-7 | 5e-10 |
| `parameters_vesicle_realistic_biology.xml` | 1e-7 | 0.5 | 0.01 | 4e-7 | 2e-7 | 5e-10 |
| `parameters_sheet.xml` | 1e-7 | 1e-2 | 1e-6 | 4e-7 | 2.5e-7 | 2e-9 |
| `parameters_tube.xml` | 1e-7 | 1e-2 | 1e-6 | 4e-7 | 2.5e-7 | 5e-10 |
| `parameters_default_dynamic.xml` | 1e-7 | 1e-2 | 1e-6 | 7.5e-7 | 5e-7 | 5e-10 |
| `parameters_default_overdamped.xml` | 1e-7 | 1e-2 | 5e-6 | 7.5e-7 | 5e-7 | 3e-9 |
| `fig3e_monolayer.xml` | 1e-7 | 1e-2 | 1e-4 | 4e-7 | 2e-7 | 5e-10 |
| `fig3e_multilayer.xml` | 1e-7 | 1e-2 | 1e-4 | 4e-7 | 2e-7 | 5e-10 |
| `longrun_fast_growth.xml` | 1e-7 | 2e-3 | 1e-4 | 1e-6 | 2e-7 | 2.5e-10 |
| `parameters_paper_exact.xml` | 1e-7 | 5e-2 | 1e-4 | 7.5e-7 | 5e-7 | 5e-10 |
| `parameters_paper_exact_fast_growth.xml` | 1e-7 | 100.0 | 1.0 | 1e-6 | 5e-7 | 2.5e-10 |

The files are in `parameters/` and its sub-folders (see [Shipped parameter files](#shipped-parameter-files));
the meshes are in `data/input_meshes/`. In the second table, $l_{\min}$ is `min_edge_length`,
$\zeta$ is `damping_coefficient`, and the cutoffs are `contact_cutoff_adhesion` and
`contact_cutoff_repulsion`, which are equal in every shipped file.

(ug-parameter-file-cell-types)=
## Cell types

Section `<cell_types>`, one `<cell_type>` block per cell type. At least one cell type must be
defined, and each must have at least one face type.

| XML tag | Type | Unit | Valid values | Meaning |
|---|---|---|---|---|
| `cell_type_name` | text |  | any | Name used in log and error messages. |
| `global_cell_id` | integer |  | `0` to `4` | Selects the {term}`cell class`: 0 epithelial, 1 extracellular matrix (ECM), 2 lumen, 3 nucleus, 4 static. Written to the output as `cell_type_id` (cell meshes) and `type_id` (statistics CSV). See {doc}`/user_guide/08_cell_classes`. |
| `cell_mass_density` | real | kg/m³ | not checked | Mass density $\rho$; node masses follow from it and the cell volume. Enters the full equations of motion (dynamic model 0) and, in both dynamic models, the kinetic energy written to the output. |
| `cell_bulk_modulus` | real | Pa | not checked | Bulk modulus $K$ in the pressure $p = -K \ln(V/V_0)$. With 0 the cell has no pressure. See {doc}`/user_guide/02_cell_mechanics`. |
| `max_inner_pressure` | real or `inf` | Pa | not checked | Upper bound on the pressure $p$. `inf` means no bound. |
| `area_elasticity_modulus` | real | J | not checked | Area elasticity modulus $k_a$. See the unit note above. |
| `target_isoperimetric_ratio` | real | 1 | $> 0$ | Target isoperimetric ratio $A^3/V^2$; with the cell volume it sets the target area $A_0$. A sphere has $36\pi \approx 113$. |
| `angle_regularization_factor` | real | J | not checked | Strength of a force that drives the angles of each triangle towards 60 degrees. 0 disables it; all shipped files use 0. |
| `avg_growth_rate` | real | m³/s | not checked | Mean of the normal distribution from which each cell draws its growth rate. The target volume grows by `growth_rate x time_step` every iteration. See {doc}`/user_guide/06_growth_division_removal`. |
| `std_growth_rate` | real | m³/s | not checked | Standard deviation of that distribution. Drawn values are limited to the mean $\pm$ 3 standard deviations; 0 gives every cell the mean. |
| `avg_division_volume` | real or `inf` | m³ | not checked | Mean of the normal distribution from which each cell draws the volume at which it divides. `inf` disables division. |
| `std_division_volume` | real | m³ | not checked | Standard deviation of that distribution, limited as for the growth rate. Ignored when `avg_division_volume` is `inf`. |
| `min_vol` | real | m³ | not checked | A cell whose volume falls below this value is removed. The target volume is never set below it. |
| `surface_coupling_max_curvature` | real | 1/m | not checked (see note) | A node whose absolute mean curvature exceeds this value is not coupled to another cell. Used by contact models 1 and 2. See {doc}`/user_guide/03_contact`. |

:::{note}
The reader intends to require a positive `surface_coupling_max_curvature`, but the check
tests `target_isoperimetric_ratio` instead, so any value is accepted.
:::

Each cell draws its growth rate and division volume when it is created (at the start, and for each daughter at division). The random generator is seeded from the system clock, so two runs with a non-zero standard deviation differ even on one thread.

Not read from the parameter file: the initial pressure of the cells is 0 Pa, so each cell
starts at its target volume. The Python interface can set it (`initial_pressure_`,
{doc}`/user_guide/16_python_interface`).

### Cell types and the input mesh

Each cell in the input mesh carries an integer `cell_type_id`. SimuCell3D uses it as an index
into the list of `<cell_type>` blocks **in the order in which they appear in the parameter
file**: `cell_type_id 0` is the first block, `1` the second, and so on. The `global_cell_id`
of that block then selects the cell class. A `cell_type_id` equal to or larger than the number
of cell types stops the run with "The cell type id of cell N (X) is not valid".

All shipped parameter files define their cell types in the order of their `global_cell_id`:
either all five (epithelial 0, ECM 1, lumen 2, nucleus 3, static 4) or, in
`longrun_fast_growth.xml` and `parameters_paper_exact_fast_growth.xml`, the epithelial type
alone. In these files the input-mesh
`cell_type_id` and the `global_cell_id` coincide. If you reorder the blocks, or define
several cell types of the same class, they no longer coincide.

`global_cell_id` itself is checked only when a cell of that type is created: a value outside
0 to 4 makes the triangulation of that cell fail ten times and then stops the run with
"The cell N could not be triangulated after 10 attempts". A cell type that no cell in the
input mesh uses is never checked.

(ug-parameter-file-face-types)=
## Face types

Inside each `<cell_type>`, the `<face_types>` block holds one or more `<face_type>` entries.

| XML tag | Type | Unit | Valid values | Meaning |
|---|---|---|---|---|
| `face_type_name` | text |  | any | Name used in error messages. |
| `global_face_id` | integer |  | $\ge 0$ | Identifier written to the face meshes as `face_type_id`. It is not used by the model; give each face type in the file a distinct value so that the output can be told apart. |
| `surface_tension` | real | N/m | $\ge 0$ | Surface tension $\gamma$ of faces of this type. See {doc}`/user_guide/02_cell_mechanics`. |
| `bending_modulus` | real | J | $\ge 0$ | Bending modulus $k_b$. |
| `adherence_strength` | real | N/m³ | $\ge 0$ | Adhesion strength $\omega$ between this face and the surface of another cell. See {doc}`/user_guide/03_contact`. |
| `repulsion_strength` | real | N/m³ | $\ge 0$ | Repulsion strength $\xi$. |

Inside the solver a face type is identified by its position in the `<face_types>` block (0, 1,
2, ...), not by `global_face_id`. For epithelial cells this position has a meaning: with
contact-based polarization (`POLARIZATION_MODE_INDEX 1`, the default build) the cell labels
its faces with position 0 for apical, 1 for lateral and 2 for basal faces, depending on what
each face touches. An epithelial cell type therefore needs its face types in the order
apical, lateral, basal, as in every shipped file. See {doc}`/user_guide/07_polarization`.

## Paths

Paths in the parameter file and on the command line are resolved as follows.

The parameter file
: The path on the command line is opened as given: an absolute path, or a path relative to
  the current working directory. It is not tried relative to the source directory. A file that
  does not exist, cannot be read or is not well-formed XML stops the program with one of the
  messages in [Error messages](#error-messages).

`input_mesh_file_path`
: First opened as given (absolute, or relative to the working directory). If that fails, it
  is opened relative to the source directory of the SimuCell3D checkout that the executable
  was built from (the CMake `PROJECT_SOURCE_DIR`, fixed at configure time). The shipped files
  use paths such as `data/input_meshes/fig_3_vesicle.vtk` and `/data/input_meshes/big_sphere.vtk`,
  which work only through this second rule. When it is used, the run prints
  "WARNING: the absolute mesh file path: ... was not found" followed by the path it tries
  instead. This is expected.

`output_mesh_folder_path`
: Used as given: absolute, or relative to the working directory. The shipped files use
  `../simulation_results/<name>`, which from the `build/` directory is
  `simulation_results/<name>` in the repository root. The command-line option
  `--output-dir=PATH` replaces it ({doc}`/user_guide/13_running_simulations`).

:::{warning}
`simucell3d` deletes the output folder, with everything in it, before it starts writing. Do
not point `output_mesh_folder_path` or `--output-dir` at a folder that contains anything else,
and copy the results of a run elsewhere before running the same parameter file again.
:::

(ug-parameter-file-errors)=
## Error messages

The parameter file is read before any output is written. A problem stops the program with
exit status 1 and a message on the standard error stream. The most common messages are:

| Message | Cause |
|---|---|
| `The parameter file could not be found at the path: <path>` | The file does not exist. The path may be absolute or relative to the working directory. |
| `The parameter file could not be read: <path>` | The file exists but cannot be opened or read; the next line gives the tinyxml2 error. |
| `The parameter file is not valid XML (line N): <path>` | The file is not well-formed XML (for example a closing tag that does not match); the next line gives the tinyxml2 error. |
| `The section "numerical_parameters" was not found in the parameter file.` | Missing or misspelled section (`cell_types` likewise). |
| `The xml markup "<tag>" was not found in the parameter file.` | A required numerical parameter is missing or misspelled. The cell-type and face-type messages add the name of the cell type or face type. A missing `std_growth_rate` is reported under the name `avg_growth_rate`. |
| `stod` or `stoi` | A value is not a number. |
| `The <name>_ must be strictly positive` | A value fails its range check (for example `time_step`). |
| `The sampling_period_ must be greater than the time step` | `sampling_period` < `time_step`. |
| `Invalid contact_detection_algorithm: '<value>'. Valid values: 'uspg', 'sweep_and_prune', 'adaptive'` | Unknown algorithm name. |
| `No cell type has been defined in the parameter file.` | `<cell_types>` contains no `<cell_type>`. |
| `No face type has been defined for the cell type <name> in the parameter file.` | Empty `<face_types>`. |

(ug-parameter-file-shipped)=
## Shipped parameter files

All files are in `parameters/`. Each file's header comment states the compile-time
configuration it assumes; running it with a different build gives a different model.

| File | Compile-time configuration | Purpose | Example page |
|---|---|---|---|
| `core/parameters_vesicle_quick.xml` | default (polarization 1, contact 2, dynamic 0) | Short version of the vesicle run, 200 iterations. Smoke test. | {doc}`/examples/vesicle_quick` |
| `core/parameters_vesicle.xml` | default | Vesicle: 432 epithelial cells, one lumen cell and one ECM cell; $10^5$ iterations. | {doc}`/examples/vesicle` |
| `core/parameters_vesicle_realistic_biology.xml` | default | Variant of the vesicle file with a 0.5 s duration and division enabled. | |
| `core/parameters_sheet.xml` | default | Epithelial sheet (118 cells). | {doc}`/examples/sheet` |
| `core/parameters_tube.xml` | default | Epithelial tube (192 cells). | {doc}`/examples/tube` |
| `core/parameters_default_dynamic.xml` | contact model 1 | Single growing and dividing cell, full equations of motion. | {doc}`/examples/single_cell_dynamic_overdamped` |
| `core/parameters_default_overdamped.xml` | contact model 0, dynamic model 1 | Single growing and dividing cell, overdamped equations of motion. | {doc}`/examples/single_cell_dynamic_overdamped` |
| `benchmarks/fig3e_monolayer.xml` | default | Vesicle with surface tensions that give a monolayer (paper Fig. 3e). | {doc}`/examples/monolayer_multilayer` |
| `benchmarks/fig3e_multilayer.xml` | default | As above, with surface tensions that give a multilayer. | {doc}`/examples/monolayer_multilayer` |
| `longrun_fast_growth.xml` | default | Growth from one cell, a 2.x reconstruction of the paper's Fig. 1e setup, not the lab's file. Differs from paper Table 2 in `min_edge_length` (1e-6 m instead of 2e-7 m), lateral `surface_tension` (8e-4 N/m instead of 1e-3), basal `adherence_strength` 0 and `repulsion_strength` 5e8 (instead of 1e9; inert with contact model 2), the contact model (2) and 10% standard deviations of growth rate and division volume. Used by `scripts/runs/launch_longrun_20k.sh`. | {doc}`/examples/growth_from_one_cell` |
| `parameters_paper_exact.xml` | contact model 1 | The 1.0 default dynamic configuration (coarser mesh and larger cells than paper Table 2; the header lists the differences). Single starting cell. | {doc}`/examples/growth_from_one_cell` |
| `parameters_paper_exact_fast_growth.xml` | default | Growth benchmark with a coarser mesh, larger cells and a faster growth rate than paper Table 2. | {doc}`/examples/growth_from_one_cell` |

:::{note}
The lab's own growth benchmark, `parameters_benchmarck.xml` of release 1.0, is not shipped with
2.x; it is the file of the November 2026 benchmark ({ref}`benchmarks-method`). To run it,
extract it from the `v1.0` tag and build with the switches in its header comment
(`POLARIZATION_MODE_INDEX 0`, `CONTACT_MODEL_INDEX 1`, `WRITE_NORMALS_IN_OUTPUT_MESH_FILE false`):

```console
$ git show v1.0:parameters_benchmarck.xml > parameters/parameters_benchmarck.xml
```

Its `time_step` of `1e-7` s is not stable with this file in any version. The recommended step
is `2.5e-8` s, which was clean in three of three 48 h runs of 2.1.0
({ref}`benchmarks-time-step`). Its `simulation_duration` of 1 s is far
longer than any measured run, so stop the run at the cell count you need. Contact model 1 can
lose contact forces with more than one thread
([Compile-time configuration](10_compile_time_configuration.md#threads-and-contact-models-0-and-1)).
:::

The safest way to write a new parameter file is to copy the shipped file closest to the
intended simulation, check its header comment against your build, and change one value at a
time.

:::{seealso}
- {doc}`/user_guide/12_input_meshes`: the format of the file named in `input_mesh_file_path`.
- {doc}`/user_guide/13_running_simulations`: command-line options and working directory.
- {doc}`/user_guide/16_python_interface`: the same parameters set from Python.
:::
