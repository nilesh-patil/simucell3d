(api-mesh)=
# Cell meshes

A {cpp:class}`cell` is a closed, oriented triangulated surface made of {cpp:class}`node`,
{cpp:class}`edge` and {cpp:class}`face` objects. The cell computes its geometry (volume, area,
curvature) and the mechanical forces of its own membrane: pressure, surface tension, area
elasticity and bending ({doc}`/user_guide/02_cell_mechanics`). Nodes carry position, force and, with the
full equations of motion, momentum. Faces carry their {term}`face type` and, with contact
models 1 and 2, nodes store their coupling partners on neighbouring cells. The five cell classes
derived from {cpp:class}`cell` are documented in {ref}`api-cell-classes`.

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
