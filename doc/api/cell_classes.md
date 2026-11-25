(api-cell-classes)=
# Cell classes

Each `<cell_type>` block of the parameter file selects one of five classes through its
`global_cell_id`: 0 {cpp:class}`epithelial_cell`, 1 {cpp:class}`ecm_cell`,
2 {cpp:class}`lumen_cell`, 3 {cpp:class}`nucleus_cell`, 4 {cpp:class}`static_cell`
(`src/io/simulation_initializer.cpp`). Each class derives from {cpp:class}`cell` and overrides
the parts of the mechanics or the polarization that differ for that kind of cell. The
narrative description is in {doc}`/user_guide/08_cell_classes`.

The class of a cell decides what it does in the steps of an iteration that call virtual members
of {cpp:class}`cell`: the face-type update, the contact-based polarization, the internal forces,
the division check, and whether the time integration moves it.

| Class | `global_cell_id` | Moves | Internal forces | Divides | Notes |
|---|---|---|---|---|---|
| {cpp:class}`epithelial_cell` | 0 | yes | yes | yes | Faces polarized by contacts (apical, lateral, basal) |
| {cpp:class}`ecm_cell` | 1 | no | no | no | Takes part in contact detection; epithelial faces touching it become basal (contact model 0, polarization mode 1); not checked for inside-out or self-intersecting surfaces |
| {cpp:class}`lumen_cell` | 2 | yes | yes | no | Base-class mechanics; epithelial faces touching it become apical (contact model 0, polarization mode 1) |
| {cpp:class}`nucleus_cell` | 3 | yes | yes | no | Base-class mechanics |
| {cpp:class}`static_cell` | 4 | no | yes | no | Not checked for inside-out or self-intersecting surfaces |

{cpp:struct}`face_area_pair_hasher` and {cpp:struct}`face_area_pair_comparator` are helper
functors of {cpp:class}`epithelial_cell`. ECM and static cells are static cells in the sense of
`is_static_`: the time integration skips them, and {cpp:func}`cell::check_numerical_stability`
does not test them for a negative volume or a volume larger than their bounding box. To add a
class, see {ref}`dev-architecture`.

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

## Helper types

```{doxygenstruct} face_area_pair_hasher
:project: simucell3d
:members:
```

```{doxygenstruct} face_area_pair_comparator
:project: simucell3d
:members:
```
