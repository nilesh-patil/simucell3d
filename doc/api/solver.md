(api-solver)=
# Solver and configuration

The {cpp:class}`solver` owns the cells of a simulation and runs the main loop. Its constructor
receives the numerical parameters ({cpp:struct}`global_simulation_parameters`) and the list of
cells built by {cpp:class}`simulation_initializer`; {cpp:func}`solver::run` then advances the
simulation until the simulation duration is reached, writing output every sampling period.
One iteration ({cpp:func}`solver::run_iteration`) writes the output meshes when a sampling
period has elapsed, divides the cells that have reached their division volume (checked every
5 iterations), updates the face types, refines the cell meshes, computes the contact forces,
updates the polarization of the faces, computes the internal forces of each cell (which also advances its target volume), integrates
the equations of motion, and removes cells below their minimum volume or found numerically
unstable. The narrative description is in {doc}`/user_guide/01_model_overview`.

The macros in `global_configuration.hpp` select the contact model, the dynamic model and the
polarization mode at compile time ({doc}`/user_guide/10_compile_time_configuration`). The parameter structures mirror the
sections of the parameter file ({doc}`/user_guide/11_parameter_file`). Errors are reported with the
exception classes listed at the end of this page.

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
