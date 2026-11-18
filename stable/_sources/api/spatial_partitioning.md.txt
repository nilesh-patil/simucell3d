(api-spatial-partitioning)=
# Spatial partitioning

A uniform spatial partitioning grid (USPG) sorts objects into cubic voxels so that a
neighbour search only visits nearby voxels. {cpp:class}`uspg_4d` stores any number of objects
per voxel and is used by contact models 0 and 1 and by the initial triangulation;
{cpp:class}`uspg_3d` stores at most one object per voxel and is used by the automatic
polarizer. {cpp:class}`morton_code` computes Morton (Z-order) codes; it is used by
{cpp:class}`contact_detection_uspg_strategy` to sort faces by position before inserting them.

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
