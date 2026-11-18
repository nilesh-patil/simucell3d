(ug-contact)=
# Contact between cells

Cells interact only where their surfaces come close: they adhere, and they repel each other
where they overlap. SimuCell3D offers three {term}`contact models <contact model>`, chosen at
compile time with `CONTACT_MODEL_INDEX`. This chapter describes how contacts are found, the
force law or coupling rule of each model, the parameters involved, and the limitations that
matter in practice.

## Concept

The three models differ in how adhesion is transmitted:

| Index | Name | Adhesion | Repulsion | Described in the paper |
|---|---|---|---|---|
| 0 | node-face springs | bilinear spring stress between a node and a face, strength $\omega$ | linear spring stress, strength $\xi$ | yes ("spring-based model") |
| 1 | node-node coupling | a node is tied to one node of another epithelial cell | linear spring stress, strength $\xi$ | yes ("coupling-based model") |
| 2 | {term}`face-face coupling` (default) | a node is tied to one node on each neighbouring epithelial cell | linear spring stress, strength $\xi$ | no; extension of model 1 |

Models 1 and 2 replace a finite adhesion stiffness by a rigid tie between nodes, so contact
mechanics do not depend on an adhesion strength. Runser, Vetter & Iber (2024) validate the
coupling model against the Young-Dupré relation for cell doublets and triplets (Supplementary
Fig. 5) and report that the spring model depends somewhat on mesh resolution (Supplementary
Fig. 4). Model 2 is the default of 2.x and of the tag `v1.0`; the Zenodo archive of 1.0 cited by
the paper has model 1 as its default ({doc}`/whats_new/v1.0`). The parameter files for the
vesicle, sheet, tube and growth examples are written for model 2.

## Finding contacts

All three models test nodes of one cell against faces of other cells. Every face gets an
axis-aligned bounding box enlarged by the contact range $c = \max(c_a, c_r)$, where $c_a$ is
`contact_cutoff_adhesion` and $c_r$ is `contact_cutoff_repulsion`. A node is a contact
candidate for a face of another cell if it lies in that box. For each candidate the solver
computes the closest point $\mathbf r_\mathrm{cp}$ of the triangle to the node, its barycentric
coordinates $(\lambda_1, \lambda_2, \lambda_3)$, and the vector
$\mathbf z = \mathbf r_\mathrm{node} - \mathbf r_\mathrm{cp}$, of length $d = \lVert\mathbf z\rVert$.

The side of the face decides the type of contact. A node in front of the face
($\mathbf z \cdot \hat{\mathbf n}_f > 0$) is outside the other cell and can adhere; a node
behind it is inside the other cell and is repelled. Two pairs of {term}`cell classes <cell class>`
swap the two sides, because one surface encloses the other: a node of an epithelial cell
against a face of the ECM (the ECM encloses the tissue), and a node of a nucleus against a face
of an epithelial cell (the nucleus lies inside the cell).

Candidates are found with a uniform grid or with Sweep-and-Prune (see
[Contact detection algorithms](#contact-detection-algorithms) below).

## Model 0: node-face springs

The contact stress follows the bilinear traction-separation law of the paper's Eq. (2). With
$d$ the distance between node and face,

$$
\sigma(d) =
\begin{cases}
\xi\, d & \text{node behind the face, } d < c_r \quad \text{(repulsion)}\\
\omega\, d & \text{node in front of the face, } d < c_a/2 \quad \text{(adhesion, rising)}\\
\omega\, (c_a - d) & \text{node in front of the face, } c_a/2 \le d < c_a \quad \text{(adhesion, falling)}\\
0 & \text{otherwise.}
\end{cases}
$$ (eq-contact-stress)

The stress acts over the area $A_f$ of the face, so the force has magnitude $\sigma A_f$ and
points along $\mathbf z$: towards the face for adhesion, away from it, out of the other cell,
for repulsion. The force on the node is $-\mathbf F$ and the opposite force is distributed
over the three nodes of the face with weights $\lambda_1, \lambda_2, \lambda_3$, so momentum is
conserved. $\omega$ and $\xi$ are taken from the {term}`face type` of the face that is touched.
Every node of every cell is tested against the faces of all other cells, so each contact is
seen from both sides.

:::{note}
The paper's Methods state the law for pairs of faces $\{f_a, f_b\}$ and the contact area
$A_{ab}$. The code applies it to node-face pairs with the area of the face.
:::

With polarization mode 1, model 0 also labels the touched face as apical, lateral or basal
from the class of the touching cell ({doc}`07_polarization`).

## Model 1: node-node coupling

Model 1 couples nodes of adjacent epithelial cells. For a node $n_1$ of an epithelial cell and
a candidate face of another epithelial cell, the closest node $n_2$ of that face is coupled to
$n_1$ if all of the following hold:

- the distance between $n_1$ and $n_2$ is below $c_a$ and below the distance of any partner
  $n_1$ already has;
- $n_2$ has no closer partner;
- the node normals point in sufficiently opposite directions
  ($\hat{\mathbf n}_1 \cdot \hat{\mathbf n}_2 \le \cos 45^\circ$), and the node normal of $n_1$
  points against the face normal;
- the mean curvature at both nodes is below `surface_coupling_max_curvature` $H_{\max}$ of
  the cell type of $n_1$.

A node has at most one partner. Coupled nodes are moved to their common midpoint
$(\mathbf r_1 + \mathbf r_2)/2$, and during time integration they share their averaged force
and momentum, so they move together ({doc}`04_time_integration`). This transmits tension
directly from one cell surface to the next, as in the paper's coupling-based model. Couplings
are recomputed at every iteration, which lets cells detach.

Pairs that are not coupled (contacts with non-epithelial cells, or epithelial nodes without a
partner) only repel: a node behind a face, closer than $\max(c_a, c_r)$, receives the
repulsive stress $\xi d$ of {eq}`eq-contact-stress`. The adherence strength $\omega$ is not
used by model 1.

## Model 2: face-face coupling

Model 2, the default, generalises model 1 so that a node can be coupled to one node on *each*
neighbouring epithelial cell. This matters at junctions where three or more cells meet. For a
node $n_1$ and a candidate face of a neighbouring epithelial cell, the candidate partners are
the three nodes of the face; a node is excluded if

- the normal of $n_1$ does not point against the normal of the face;
- its normal does not point against the normal of $n_1$
  ($\hat{\mathbf n}_1 \cdot \hat{\mathbf n}_2 > 0$);
- its mean curvature exceeds $H_{\max}$ of its cell type;
- $n_1$ is already coupled to a closer node of that cell, or the candidate is already coupled
  to a closer node of the cell of $n_1$.

The closest remaining node within $c_a$ becomes the partner of $n_1$ for that neighbouring cell,
in both directions. Each node keeps at most one partner per neighbouring cell. A node and all
its partners are moved to their mean position, and the time integration moves the group as one
node with averaged force, momentum and mass. As in model 1, uncoupled pairs only repel, with
range $\max(c_a, c_r)$, and $\omega$ is not used.

:::{versionchanged} 2.1
The current partner of a node is now looked up by the local id of the neighbouring cell, the
key under which it is stored. In 1.0 it was looked up by the global cell id, which differs from
the local id once cells have divided, so the partner was whichever candidate a thread visited
last rather than the nearest node
([#113](https://github.com/nilesh-patil/simucell3d/issues/113)). The closest-partner rule is now
applied under the locks of both nodes, both directions of a coupling are stored together, and
repulsion forces on shared nodes use an atomic add. These fixes change results compared with
1.0.
:::

:::{versionchanged} 2.0
For a node that projects onto the interior of a triangle, 1.0 measured the distance to a wrong
point (`a + ab*v + a + ac*w`, one vertex `a` too many), so the cutoff tests missed contacts in
face interiors. The distance is now measured to the closest point. This affects all three
models.
:::

## Mean curvature at the nodes

Models 1 and 2 need a normal and a mean curvature at every node. After the internal forces of a
cell are computed, each node $i$ receives the cotangent-Laplacian mean curvature

$$
H_i = \frac{1}{4 A_i} \left\lVert \sum_{j} (\cot\alpha_{ij} + \cot\beta_{ij})
      (\mathbf r_j - \mathbf r_i) \right\rVert ,
$$ (eq-node-curvature)

where the sum runs over the neighbours $j$ of $i$, $\alpha_{ij}$ and $\beta_{ij}$ are the angles
opposite the edge $ij$, and $A_i$ is an area of about one third of the faces around $i$. $H_i$
is an unsigned value in 1/m. The node normal is the area-weighted mean of the face normals;
where $H_i$ exceeds the curvature of a sphere of the cell's volume, $1/R$ with
$R = (3V/4\pi)^{1/3}$, the direction of the mean-curvature vector is used instead. The values
are those of the previous iteration when contacts are detected.

$H_{\max}$ lets strongly curved parts of the surface, such as the rim of a contact, detach: no
node with $H_i > H_{\max}$ is coupled.

## Contact detection algorithms

The search for candidate node-face pairs (the broad phase) is chosen in the parameter file with
the optional tag `contact_detection_algorithm`. It is used only by contact model 2; models 0
and 1 always use the uniform grid.

| Value | Method |
|---|---|
| `uspg` (default) | Uniform grid. Model 2 bins the face boxes in a compact grid whose voxel edge is the mean box size, at most $3\,l_{\min} + 2c$, and splits the nodes of very large cells into several work units. Models 0 and 1 use the {term}`USPG` with voxel edge $3\,l_{\min} + 2c$. |
| `sweep_and_prune` | Sweep-and-Prune over the face boxes. |
| `adaptive` | The uniform grid below 500 cells, Sweep-and-Prune from 500 cells on. |

The contact rules are the same with all three; the choice is about run time
({doc}`18_parallel_performance`). If the bounding box of the tissue is no longer finite, or the
grid would need more voxels than it can index, the run stops with an error instead of hanging
({doc}`09_safeguards`).

## Parameters

| XML tag | Symbol | Section | Unit | Meaning | Models |
|---|---|---|---|---|---|
| `contact_cutoff_adhesion` | $c_a$ | `<numerical_parameters>` | m | Adhesion range; coupling distance | 0, 1, 2 |
| `contact_cutoff_repulsion` | $c_r$ | `<numerical_parameters>` | m | Repulsion range (models 1 and 2 use $\max(c_a, c_r)$) | 0, 1, 2 |
| `adherence_strength` | $\omega$ | `<face_type>` | N/m³ | Slope of the adhesion law | 0 |
| `repulsion_strength` | $\xi$ | `<face_type>` | N/m³ | Slope of the repulsion law | 0, 1, 2 |
| `surface_coupling_max_curvature` | $H_{\max}$ | `<cell_type>` | 1/m | Nodes with larger mean curvature are not coupled | 1, 2 |
| `contact_detection_algorithm` | | `<numerical_parameters>` | | Broad phase, optional | 2 |

The unit N/m³ of $\omega$ and $\xi$ (stress per length) equals the Pa/m of the paper's Table 2.

## Practical guidance

- **Repulsion has a maximum stress.** The repulsive stress grows as $\xi d$ only up to the
  range of the law, so the largest repulsive stress is about $\xi\,c$. With the Table 2 values
  ($\xi = 10^{9}$ N/m³, $c = 2\times10^{-7}$ m) this is 200 Pa. Where cells press on each other
  with a larger pressure (see {doc}`02_cell_mechanics`), repulsion cannot hold them apart and
  they can pass through each other. Penetration deeper than the range produces no force.
- **Cells interpenetrate** in long growth runs of the reconstructed paper-like configuration
  `parameters/longrun_fast_growth.xml`, with 2.1 and with 1.0. The CHANGELOG lists this as a known
  issue. In that file the contact cutoff ($2\times10^{-7}$ m) is five times smaller than the
  minimum edge length ($10^{-6}$ m), which may be one cause; the paper's Table 2 uses
  $2\times10^{-7}$ m for both, and the file is a reconstruction for 2.x, not the lab's
  configuration ({doc}`/examples/growth_from_one_cell`). Check the overlap of cells in the output
  when the geometry of contacts matters.
- **Adhesion in models 1 and 2 exists only between epithelial cells.** Contacts with lumen, ECM,
  nucleus and static cells are purely repulsive. $\omega$ has an effect only with model 0.
- **Shipped values.** Most epithelial face types use $\omega = \xi = 10^{9}$ N/m³ on apical and lateral
  faces and $\omega = 0$, $\xi = 5\times10^{8}$ N/m³ on basal faces; ECM, lumen and nucleus faces
  use $\xi$ between $10^{9}$ and $5\times10^{9}$ N/m³. Cutoffs are $2\times10^{-7}$ to
  $5\times10^{-7}$ m and $H_{\max}$ is $2.5\times10^{6}$ or $5\times10^{6}$ 1/m for epithelial
  cells.
- **Thread safety.** Models 0 and 1 add contact forces to the nodes of other cells without an
  atomic add, inside parallel loops; with several threads, some contact forces can be lost, and no
  warning is printed. These are the two models described in the paper. Run them with
  `--threads=1`. Model 2 is not affected. Multi-threaded runs of any model are not
  bit-reproducible.
- **Stale links.** When a node switches to a closer partner, its old partner keeps a link back to
  it until the couplings are rebuilt at the next iteration. This behaviour is present since 1.0.
- The contact model is a compile-time choice; a parameter file written for one model gives a
  different simulation with another ({doc}`10_compile_time_configuration`).

:::{seealso}
- {doc}`07_polarization` for how contacts set face types.
- {doc}`04_time_integration` for the motion of coupled nodes.
- {doc}`/api/contact_models` for the contact model classes.
:::
