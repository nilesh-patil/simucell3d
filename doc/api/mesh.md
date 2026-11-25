(api-mesh)=
# Cell meshes

A {cpp:class}`cell` is a closed, oriented triangulated surface made of {cpp:class}`node`,
{cpp:class}`edge` and {cpp:class}`face` objects. The cell computes its geometry (volume, area,
curvature) and the mechanical forces of its own membrane: pressure, surface tension, area
elasticity and bending ({doc}`/user_guide/02_cell_mechanics`). Nodes carry position, force and, with the
full equations of motion, momentum. Faces carry their {term}`face type` and, with contact
models 1 and 2, nodes store their coupling partners on neighbouring cells. The five cell classes
derived from {cpp:class}`cell` are documented in {ref}`api-cell-classes`.

In an iteration, the cell's own methods update the face types, compute the internal forces
(`cell::apply_internal_forces()`, after the contact forces) and, in
`solver::remove_dead_cells()`, report whether the mesh has diverged
({cpp:func}`cell::check_numerical_stability`). The mesh itself is changed only by the
{doc}`triangulation` classes, and node positions only by the {doc}`time_integration` scheme.

| Class | Header | Role |
|---|---|---|
| {cpp:class}`cell` | `mesh/cell.hpp` | Owns its nodes, faces and edges; geometry, internal forces, stability check |
| {cpp:struct}`cell::stability_report` | `mesh/cell.hpp` | Result of the numerical stability check |
| {cpp:class}`node` | `mesh/node.hpp` | Position, force, momentum; coupling partners with contact models 1 and 2 |
| {cpp:class}`edge` | `mesh/edge.hpp` | Two node indices and up to two face indices; used by the mesh refiner |
| {cpp:class}`face` | `mesh/face.hpp` | Triangle of three node indices, face type, normal and area |
| {cpp:struct}`edge_hasher`, {cpp:struct}`edge_comparator` | `mesh/edge.hpp` | Hash and equality functors for edges in unordered containers |

Cells are held through `cell_ptr` (`std::shared_ptr<cell>`), and a cell refers to its cell type
through `cell_type_param_ptr` (`std::shared_ptr<cell_type_parameters>`). Nodes and faces are
values stored in the cell's vectors; faces refer to their nodes by index. Removed nodes and
faces leave unused slots that are reused, so loops over them skip entries for which
`is_used()` is false ({ref}`dev-architecture`).

## Cell

```{doxygenclass} cell
:project: simucell3d
:members:
```

## Nodes, edges and faces

```{doxygenclass} node
:project: simucell3d
:members:
```

```{doxygenclass} edge
:project: simucell3d
:members:
```

```{doxygenclass} face
:project: simucell3d
:members:
```

## Helper types

```{doxygenstruct} edge_hasher
:project: simucell3d
:members:
```

```{doxygenstruct} edge_comparator
:project: simucell3d
:members:
```
