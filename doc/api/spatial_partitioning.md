(api-spatial-partitioning)=
# Spatial partitioning

A uniform spatial partitioning grid (USPG) sorts objects into cubic voxels so that a
neighbour search only visits nearby voxels. {cpp:class}`uspg_4d` stores any number of objects
per voxel and is used by contact models 0 and 1, by the Poisson sampling and ball pivoting of
the initial triangulation, by the point sampling of a division interface, and by
{cpp:class}`contact_detection_uspg_strategy`;
{cpp:class}`uspg_3d` stores at most one object per voxel and is used by the automatic
polarizer. {cpp:class}`morton_code` computes Morton (Z-order) codes; it is used by
{cpp:class}`contact_detection_uspg_strategy` to sort faces by position before inserting them.
Contact model 2 does not use these classes during a run: it bins faces in its own compact grid
({cpp:class}`contact_face_face_via_coupling`).

The grid is rebuilt from the current face bounding boxes at every contact detection, so its
size follows the extent of the tissue. A cell whose nodes diverge would make that extent, and
the number of voxels, grow without bound; the grids refuse such a domain and the solver removes
diverged cells before they reach contact detection ({doc}`/user_guide/09_safeguards`).

| Class | Header | Role |
|---|---|---|
| {cpp:class}`uspg_abstract` | `uspg/uspg_abstract.hpp` | Common grid geometry: bounds, voxel size and voxel counts |
| {cpp:class}`uspg_4d` | `uspg/uspg_4d.hpp` | Any number of objects per voxel (contact models 0 and 1, initial triangulation, cell division) |
| {cpp:class}`uspg_3d` | `uspg/uspg_3d.hpp` | At most one object per voxel (automatic polarizer) |
| {cpp:class}`morton_code` | `uspg/morton_code.hpp` | Morton (Z-order) codes |

## Grids

```{doxygenclass} uspg_abstract
:project: simucell3d
:members:
```

```{doxygenclass} uspg_4d
:project: simucell3d
:members:
```

```{doxygenclass} uspg_3d
:project: simucell3d
:members:
```

```{doxygenclass} morton_code
:project: simucell3d
:members:
```
