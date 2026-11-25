(api-polarization)=
# Polarization

With `POLARIZATION_MODE_INDEX 2`, the solver uses {cpp:class}`automatic_polarizer` to assign
face types from a discretisation of the simulation space, and
{cpp:class}`automatic_polarization_writer` writes the resulting regions to `region_data/`.
With `POLARIZATION_MODE_INDEX 1` (default), polarization is based on cell contacts and is
implemented in the cell classes, mainly {cpp:class}`epithelial_cell`. The narrative description
is in {doc}`/user_guide/07_polarization`.

In an iteration, `automatic_polarizer::polarize_faces()` runs after contact detection and
before the internal forces. The writer is called with every mesh output after iteration 1, so
the meshes written at the start of the run have no `region_data/` file.

| Class or constant | Header | Role |
|---|---|---|
| {cpp:class}`automatic_polarizer` | `automatic_polarization/automatic_polarizer.hpp` | Labels voxels and assigns face types from the labels |
| {cpp:class}`automatic_polarization_writer` | `automatic_polarization/automatic_polarization_writer.hpp` | Writes the voxel labels to `region_data/` |
| region labels | `automatic_polarization/automatic_polarizer.hpp` | Values of the voxel labels, listed at the end of this page |

## Classes

```{doxygenclass} automatic_polarizer
:project: simucell3d
:members:
```

```{doxygenclass} automatic_polarization_writer
:project: simucell3d
:members:
```

## Region labels

```{doxygenvariable} exterior_region_id
:project: simucell3d
```

```{doxygenvariable} boundary_region_id
:project: simucell3d
```

```{doxygenvariable} cytoplasm_region_id
:project: simucell3d
```

```{doxygenvariable} lumen_region_id
:project: simucell3d
```
