(api-solver)=
# Solver and configuration

The {cpp:class}`solver` owns the cells of a simulation and runs the main loop. Its constructor
receives the numerical parameters ({cpp:struct}`global_simulation_parameters`) and the list of
cells built by {cpp:class}`simulation_initializer`, chooses the thread count and placement and
the OpenMP schedule, and creates the modules (mesh refiner, contact model, time integrator,
statistics writer). {cpp:func}`solver::run` then calls {cpp:func}`solver::run_iteration` until
the simulation duration is reached or no cell is left. The order of the steps inside one
iteration is given in {ref}`api-index` and, in detail, in {ref}`dev-architecture-main-loop`;
the model is described in {doc}`/user_guide/01_model_overview`.

The macros in `global_configuration.hpp` select the contact model, the dynamic model and the
polarization mode at compile time ({doc}`/user_guide/10_compile_time_configuration`). The
parameter structures mirror the sections of the parameter file
({doc}`/user_guide/11_parameter_file`). Errors that end a run are reported with the exception
classes listed at the end of this page.

| Class or type | Header | Role |
|---|---|---|
| {cpp:class}`solver` | `solver.hpp` | Owns the cells and the modules; runs the main loop |
| {cpp:struct}`global_simulation_parameters` | `custom_structures.hpp` | Numerical section of the parameter file |
| {cpp:struct}`cell_type_parameters`, {cpp:struct}`face_type_parameters` | `custom_structures.hpp` | One `<cell_type>` block and its `<face_type>` blocks |
| {cpp:enum}`ContactDetectionAlgorithm` | `custom_structures.hpp` | Value of `contact_detection_algorithm` |
| {cpp:struct}`simulation_outputs` | `custom_structures.hpp` | Result of a run of the Python module |
| {cpp:struct}`mesh`, {cpp:struct}`oriented_point` | `custom_structures.hpp` | Light mesh and point types used while reading and triangulating |
| exception classes | `custom_exception.hpp` | One class per kind of error that ends the run |

## Solver

```{doxygenclass} solver
:project: simucell3d
:members:
```

## Compile-time configuration

The switches are macros in `include/global_configuration.hpp`; changing one requires a rebuild
({doc}`/user_guide/10_compile_time_configuration`).

| Macro | Values (default in bold) |
|---|---|
| `CONTACT_MODEL_INDEX` | 0: springs between nodes and faces of adjacent cells; 1: coupling of pairs of adjacent nodes; **2**: coupling of pairs of adjacent faces |
| `DYNAMIC_MODEL_INDEX` | **0**: full equations of motion, semi-implicit Euler; 1: overdamped, forward Euler |
| `POLARIZATION_MODE_INDEX` | 0: none; **1**: from the contacts of the cell; 2: from a discretization of the simulation space |
| `FACE_STORE_CONTACT_ENERGY` | **false**; true stores the adhesion and repulsion energies on each face |
| `WRITE_NORMALS_IN_OUTPUT_MESH_FILE` | false; **true** writes face and node normals in the output meshes |

## Parameters and results

```{doxygenenum} ContactDetectionAlgorithm
:project: simucell3d
```

```{doxygenvariable} ADAPTIVE_SAP_CELL_THRESHOLD
:project: simucell3d
```

```{doxygenstruct} global_simulation_parameters
:project: simucell3d
:members:
```

```{doxygenstruct} cell_type_parameters
:project: simucell3d
:members:
```

```{doxygenstruct} face_type_parameters
:project: simucell3d
:members:
```

```{doxygenstruct} simulation_outputs
:project: simucell3d
:members:
```

```{doxygenstruct} mesh
:project: simucell3d
:members:
```

```{doxygenstruct} oriented_point
:project: simucell3d
:members:
```

## Exceptions

```{doxygenclass} parameter_reader_exception
:project: simucell3d
:members:
```

```{doxygenclass} mesh_reader_exception
:project: simucell3d
:members:
```

```{doxygenclass} mesh_writer_exception
:project: simucell3d
:members:
```

```{doxygenclass} intialization_exception
:project: simucell3d
:members:
```

```{doxygenclass} initial_triangulation_exception
:project: simucell3d
:members:
```

```{doxygenclass} bpa_exception
:project: simucell3d
:members:
```

```{doxygenclass} mesh_integrity_exception
:project: simucell3d
:members:
```

```{doxygenclass} division_exception
:project: simucell3d
:members:
```

```{doxygenclass} unstable_simulation_exception
:project: simucell3d
:members:
```

```{doxygenclass} automatic_polarization_exception
:project: simucell3d
:members:
```

```{doxygenclass} uspg_exception
:project: simucell3d
:members:
```
