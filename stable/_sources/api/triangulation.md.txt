(api-triangulation)=
# Triangulation and division

These classes create and maintain the cell surface meshes. {cpp:class}`local_mesh_refiner`
keeps edge lengths between the minimum edge length and three times that value by splitting,
collapsing and swapping edges at every iteration ({doc}`/user_guide/05_mesh_refinement`).
{cpp:class}`cell_divider` splits a cell that has reached its division volume along the plane
normal to its longest axis ({doc}`/user_guide/06_growth_division_removal`). When `perform_initial_triangulation`
is enabled, {cpp:class}`initial_triangulation` re-triangulates the input meshes with
{cpp:class}`poisson_sampling` and {cpp:class}`ball_pivoting_algorithm`.

## Mesh refinement

```{doxygenclass} local_mesh_refiner
:project: simucell3d
:members:
```

## Cell division

```{doxygenclass} cell_divider
:project: simucell3d
:members:
```

## Initial triangulation

```{doxygenclass} initial_triangulation
:project: simucell3d
:members:
```

```{doxygenclass} poisson_sampling
:project: simucell3d
:members:
```

```{doxygenclass} ball_pivoting_algorithm
:project: simucell3d
:members:
```
