(api-polarization)=
# Polarization

With `POLARIZATION_MODE_INDEX 2`, the solver uses {cpp:class}`automatic_polarizer` to assign
face types from a discretisation of the simulation space, and
{cpp:class}`automatic_polarization_writer` writes the resulting regions to `region_data/`.
With `POLARIZATION_MODE_INDEX 1` (default), polarization is based on cell contacts and is
implemented in the cell classes, mainly {cpp:class}`epithelial_cell`. The narrative description
is in {doc}`/user_guide/07_polarization`.

## Classes

```{doxygenclass} automatic_polarizer
:project: simucell3d
:members:
```

```{doxygenclass} automatic_polarization_writer
:project: simucell3d
:members:
```
