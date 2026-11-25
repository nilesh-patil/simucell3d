(api-triangulation)=
# Triangulation and division

These classes create and maintain the cell surface meshes. {cpp:class}`local_mesh_refiner`
keeps edge lengths between the minimum edge length and three times that value by splitting,
collapsing and swapping edges at every iteration ({doc}`/user_guide/05_mesh_refinement`).
{cpp:class}`cell_divider` splits a cell that has reached its division volume along the plane
normal to its longest axis ({doc}`/user_guide/06_growth_division_removal`). When `perform_initial_triangulation`
is enabled, {cpp:class}`initial_triangulation` re-triangulates the input meshes with
{cpp:class}`poisson_sampling` and {cpp:class}`ball_pivoting_algorithm`.

In an iteration, the divider runs every 5 iterations, before the refiner, and the refiner runs
on every cell before contact detection. A cell whose refinement fails is reported in a
{cpp:struct}`local_mesh_refiner::refinement_failure` and removed by the solver; a division that
fails or is refused leaves the mother unchanged, to be tried again at the next division check.

| Class | Header | Role |
|---|---|---|
| {cpp:class}`local_mesh_refiner` | `triangulation_modules/local_mesh_refiner.hpp` | Edge splits, collapses and swaps at every iteration |
| {cpp:class}`cell_divider` | `triangulation_modules/cell_divider.hpp` | Cuts a mother cell into two daughters |
| {cpp:class}`initial_triangulation` | `triangulation_modules/initial_triangulation.hpp` | Re-triangulates an input surface before the run |
| {cpp:class}`poisson_sampling` | `triangulation_modules/poisson_sampling.hpp` | Poisson-disk point sampling of a surface; also places the points of a division interface |
| {cpp:class}`ball_pivoting_algorithm` | `triangulation_modules/ball_pivoting_algorithm.hpp` | Surface reconstruction from the sampled points |

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
