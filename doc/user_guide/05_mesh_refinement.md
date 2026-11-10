(ug-mesh-refinement)=
# Mesh refinement

Cells change shape, grow and divide, so a mesh that was well shaped at the start would soon have
edges that are much too long or too short. SimuCell3D keeps the resolution of every cell surface
in a fixed range with a {term}`local mesh refiner` that runs at every {term}`iteration`. This
chapter describes the edge-length bounds, the three local operations, the initial
triangulation of the input mesh, and the checks that 2.1 adds to the refiner.

## Edge-length bounds

All edges of all cells are kept in the range

$$
l_{\min} \le l \le l_{\max}, \qquad l_{\max} = 3\, l_{\min},
$$

where $l_{\min}$ is `min_edge_length`. The maximum is not a parameter: as in the paper's
Methods, it is always three times the minimum. The bounds set the resolution of the model. A cell
of area $A$ has of the order of $A / l_{\min}^2$ faces, and the cost of a run grows with the
total number of faces.

## Local operations

At every iteration, before contact detection, the refiner processes each cell independently (in
parallel over cells):

1. **Edge swap** (only if `enable_edge_swap_operation` is 1). Every face with a quality score
   $S_f = \dfrac{36}{\sqrt 3}\, \dfrac{A_f}{P_f^2}$ below 0.2, where $P_f$ is the perimeter, has its
   longest edge swapped: the two triangles that share the edge are reconnected along the other
   diagonal. The swap is skipped if that diagonal is already an edge of the mesh. $S_f = 1$ for an equilateral triangle and tends to 0 for a needle or a sliver. The
   nodes do not move.
2. **Edge split.** An edge longer than $l_{\max}$ is split at its midpoint, which adds one node
   and replaces two faces by four. The new faces copy the face type of the faces they replace.
3. **Edge collapse.** An edge shorter than $l_{\min}$ is collapsed into one node at its midpoint,
   which removes one node and two faces. The collapse is allowed only if the two end nodes share
   exactly two neighbours; otherwise it would create an edge shared by more than two faces, and
   the edge is left as it is.

Splits and collapses are repeated until no edge is out of range. In the dynamic model the
momentum is carried over: on a split each end node gives one third of its momentum to the new
node; on a collapse the new node gets the sum of the two momenta.

:::{note}
The paper's Methods give a swap threshold of $S_f < 0.3$; the code uses 0.2. The edge swap is
disabled (`enable_edge_swap_operation` 0) in every shipped parameter file except
`parameters/longrun_fast_growth.xml`.
:::

:::{versionchanged} 2.1
When the refiner split an edge, it oriented the new triangles with the normals stored on the old
faces, which can be out of date within one refinement pass. Some new triangles came out inside
out, and the cell kept an inverted patch: its volume was mis-measured, its pressure stopped
resisting compression, and it swelled to several times its target volume or stretched into a
long thin cell. The new triangles now copy the winding of the face they replace. 1.0 has the
same bug, so this change alters results.
:::

## Limits and failure handling

Two limits stop the refinement of a cell that does not converge:

- **Oscillation bail-out.** Each split or collapse is counted against a cubic bucket of edge
  $10^{-7}$ m that contains the midpoint of the edge. If more than 50 operations fall in the same
  bucket within one call, the refiner stops working on that cell for the current iteration, logs
  it, and continues with the other cells. Some edges of that cell can stay outside
  $[l_{\min}, l_{\max}]$ until the next iteration. The bucket size is fixed; it does not scale with
  `min_edge_length`.
- **Operation cap.** More than 500,000 operations on one cell in one call raise an error
  ("The refinement of the mesh of cell X failed").

:::{versionchanged} 2.1
In 1.0 a cell that did not converge, or any error raised while refining one cell, ended the whole
run. In 2.1 the bail-out lets the run continue (introduced in 2.0, logged since 2.1), and a cell
whose refinement raises an error is removed from the tissue before contact detection, unless so
many cells fail in one iteration that a bug is more likely. The rules and log lines are in
{doc}`09_safeguards`.
:::

## Orientation self-check

All operations are meant to preserve the orientation of the surface. 2.1 verifies it after every
refinement: each cell checks that the two faces of every edge traverse it in opposite directions.
The check first computes a cheap checksum over all faces and counts edges only if the checksum is
non-zero. A cell that fails is logged and all its faces are re-oriented from one seed face, as for
a new cell; a cell whose surface is no longer manifold is logged but cannot be repaired. The check
costs about 0.25% of a time step.

## Initial triangulation

The input mesh describes each cell by polygons that need not be triangles or respect the edge
bounds ({doc}`12_input_meshes`). With `perform_initial_triangulation` set to 1, each cell surface
is rebuilt before the first iteration, following the procedure of the paper's Methods and
Supplementary Fig. 1:

1. Each polygon is split into triangles around its centre.
2. The surface is sampled with Poisson disk sampling, with a minimum point separation of
   $l_{\min}$.
3. The ball pivoting algorithm connects the sample points into a closed triangle mesh.

The resulting edges are at least $l_{\min}$ long; edges longer than $l_{\max}$ are split by the
refiner during the first iteration. The faces are oriented outwards, and the surface must be
closed and manifold. A cell whose triangulation fails is retried up to 10 times (the sampling is
random), with a log line for each attempt; after the tenth failure the run stops.

With `perform_initial_triangulation` set to 0, the input mesh is used as given and must already
consist of triangles. Its edges are brought into range by the refiner during the first iteration.
The vesicle files, `fig3e_*.xml` and `parameters_paper_exact_fast_growth.xml` use this
setting.

## Parameters

| XML tag | Symbol | Section | Unit | Meaning |
|---|---|---|---|---|
| `min_edge_length` | $l_{\min}$ | `<numerical_parameters>` | m | Minimum edge length; $l_{\max} = 3 l_{\min}$ |
| `enable_edge_swap_operation` | | `<numerical_parameters>` | | 1 enables the edge swap, 0 disables it |
| `perform_initial_triangulation` | | `<numerical_parameters>` | | 1 re-triangulates the input mesh; 0 uses it as given |

## Practical guidance

- **Resolution and contact range.** The contact cutoffs ({doc}`03_contact`) should be compared
  with $l_{\min}$: a node can only couple with or be repelled by a nearby surface within the cutoff,
  and the faces of a coarse mesh are large compared with a short cutoff. The paper's Table 2 sets
  $l_{\min} = c = 2\times10^{-7}$ m; `parameters/longrun_fast_growth.xml` uses
  $l_{\min} = 10^{-6}$ m with $c = 2\times10^{-7}$ m, to reproduce the mesh size reported for the
  paper's growth benchmark (121 nodes and 238 faces per cell on average, Runser, Vetter & Iber
  (2024)). The CHANGELOG lists cell interpenetration as a known issue and names this ratio as a
  likely cause.
- **Resolution and time step.** A finer mesh has lighter nodes and smaller faces, and usually needs
  a smaller time step ({doc}`04_time_integration`).
- **Log lines.** Occasional oscillation bail-outs are expected with fine meshes and small cells.
  Frequent ones, or many re-oriented cells, point to a time step that is too large or to strongly
  deformed cells. The first 20 occurrences of each message are logged, then every 1000th.

:::{seealso}
- {doc}`06_growth_division_removal`: the daughters of a division are refined at once.
- {doc}`09_safeguards`: log lines and failure limits.
- {doc}`/api/triangulation` for `local_mesh_refiner` and the initial triangulation.
:::
