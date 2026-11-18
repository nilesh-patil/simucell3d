(api-cell-classes)=
# Cell classes

Each `<cell_type>` block of the parameter file selects one of five classes through its
`global_cell_id`: 0 {cpp:class}`epithelial_cell`, 1 {cpp:class}`ecm_cell`,
2 {cpp:class}`lumen_cell`, 3 {cpp:class}`nucleus_cell`, 4 {cpp:class}`static_cell`
(`src/io/simulation_initializer.cpp`). Each class derives from {cpp:class}`cell` and overrides
the parts of the mechanics or the polarization that differ for that kind of cell. The
narrative description is in {doc}`/user_guide/08_cell_classes`.

## Classes

```{doxygenclass} epithelial_cell
:project: simucell3d
:members:
```

```{doxygenclass} ecm_cell
:project: simucell3d
:members:
```

```{doxygenclass} lumen_cell
:project: simucell3d
:members:
```

```{doxygenclass} nucleus_cell
:project: simucell3d
:members:
```

```{doxygenclass} static_cell
:project: simucell3d
:members:
```
