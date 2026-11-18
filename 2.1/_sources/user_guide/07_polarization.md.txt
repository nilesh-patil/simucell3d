(ug-polarization)=
# Polarization

Epithelial cells have membrane regions with different mechanical properties: an apical side
facing a lumen or the outside, lateral sides in contact with neighbouring cells, and a basal side
facing the extracellular matrix. SimuCell3D represents these regions by {term}`face types <face type>`,
each with its own surface tension, bending modulus, adherence strength and repulsion strength.
This chapter explains how faces receive their types and how the {term}`polarization mode`
(`POLARIZATION_MODE_INDEX`, a compile-time choice) relabels them during a run.

## Face types

Each `<cell_type>` of the parameter file lists one or more `<face_type>` entries. Inside a cell,
a face refers to its face type by **position** in that list: the first entry is local face type 0,
the second 1, the third 2. The tag `global_face_id` is the number written to the output meshes as
`face_type_id`, so that face types of different cell types can be told apart in ParaView; it does
not select the face type.

The polarization code assigns local types to the faces of epithelial cells with this meaning:

| Local face type | Region |
|---|---|
| 0 | apical |
| 1 | lateral |
| 2 | basal |

The face types of an epithelial cell type must therefore be listed in the order apical, lateral,
basal, as in every shipped parameter file:

```xml
<face_types>
    <face_type>
        <global_face_id>0</global_face_id>
        <face_type_name>apical</face_type_name>
        <adherence_strength>1e9</adherence_strength>
        <repulsion_strength>1e9</repulsion_strength>
        <surface_tension>1e-3</surface_tension>
        <bending_modulus>0</bending_modulus>
    </face_type>
    <face_type>
        <global_face_id>1</global_face_id>
        <face_type_name>lateral</face_type_name>
        <adherence_strength>1e9</adherence_strength>
        <repulsion_strength>1e9</repulsion_strength>
        <surface_tension>4e-4</surface_tension>
        <bending_modulus>0</bending_modulus>
    </face_type>
    <face_type>
        <global_face_id>2</global_face_id>
        <face_type_name>basal</face_type_name>
        <adherence_strength>0</adherence_strength>
        <repulsion_strength>5e8</repulsion_strength>
        <surface_tension>1e-3</surface_tension>
        <bending_modulus>0</bending_modulus>
    </face_type>
</face_types>
```

(from `parameters/core/parameters_vesicle.xml`). The input mesh carries no face types: every face
starts with local type 0. Faces created by an edge split copy the type of the face they replace. Only epithelial cells are relabelled; the faces of lumen, ECM, nucleus and static cells
keep local type 0, so these cell types need only one face type.

## Polarization modes

At the start of every iteration, all faces of every epithelial cell are reset to local type 0
(apical). The polarization mode then decides which faces become lateral or basal in the same
iteration, before the internal forces are computed.

### Mode 0: no polarization

No face is relabelled. All faces of epithelial cells use the first face type for the whole run.

### Mode 1: from contacts (default)

Faces are labelled from the contacts found by the {term}`contact model`. The rule depends on the
contact model:

- **Contact model 0.** When a node of another cell is in adhesive or repulsive contact with a
  face of an epithelial cell, the face is labelled from the {term}`cell class` of the touching
  cell: ECM gives basal (2), lumen gives apical (0), any other class gives lateral (1).
- **Contact model 2.** A face is lateral (1) if its three nodes are all coupled to nodes of the
  same neighbouring cell and the three partner nodes form a face of that cell. All other faces are
  apical (0).
- **Contact model 1.** A face whose three nodes are all coupled, and at least one of whose node
  normals points to the same side as the face normal, is lateral (1) if the partners lie on
  different cells or are joined by edges on the same cell, and apical (0) otherwise. Faces with an
  uncoupled node stay apical.

No face becomes basal with contact models 1 and 2, because couplings exist only between
epithelial cells.

With contact models 1 and 2, mode 1 therefore distinguishes only contact with another epithelial
cell (lateral) from everything else (apical); the basal face type is not used.

### Mode 2: from a voxel map of the tissue

This mode follows the second algorithm of Runser, Vetter & Iber (2024), Fig. 2, and works without
contact information. At every iteration:

1. The bounding box of the tissue, enlarged by two voxels on each side, is divided into cubic
   voxels of edge $3\,l_{\min}$.
2. Every voxel that contains a mesh node is a boundary voxel.
3. Starting from the voxel of each cell's centroid, a flood fill over face-adjacent voxels marks
   the cytoplasm of every cell, stopping at boundary voxels.
4. A flood fill from a corner of the box marks the exterior. (The paper names the
   Hoshen-Kopelman algorithm for this clustering; the code uses flood fills over the six
   face neighbours of each voxel.)
5. All remaining voxels are luminal.
6. For every face of an epithelial cell, a ray is cast from its first node along the outward face
   normal. The first non-boundary voxel it reaches gives the region the face looks at.

In 2.1 a face that looks at exterior space becomes local type 0, and a face that looks at
cytoplasm or at a lumen becomes local type 1. The voxel map is written at every sampling period
to `region_data/result_<n>.vtk`, from the second iteration on ({doc}`14_output_files`).

:::{note}
The paper describes the outcome as lateral (facing another cell), apical (facing an enclosed
volume such as a lumen) and basal (facing the surrounding medium or ECM). The 2.1 code, unchanged
from 1.0, maps lumen and cytoplasm to the same local type 1 and the exterior to local type 0, and
never assigns local type 2. Check the `face_type_id` array of the face meshes before relying on
mode 2 for apical-basal differences.
:::

The cytoplasm flood fill starts at the centroid of the cell surface. For a strongly non-convex cell
whose centroid lies outside the cell, the fill marks the wrong region. A ray that runs along a row
of boundary voxels can also report the exterior for a face that is in contact with another cell.

## Effect on the mechanics

The face type of a face selects, at every iteration:

- its surface tension $\gamma$ and the bending modulus used for the hinges at its edges
  ({doc}`02_cell_mechanics`);
- the adherence strength $\omega$ and repulsion strength $\xi$ applied when a node of another cell
  touches it ({doc}`03_contact`).

A lower lateral than apical surface tension, as in the shipped files, favours large contacts
between cells: for two coupled cells the contact angle $\theta$ follows the Young-Dupré relation
$\cos\theta = \gamma_l/\gamma_a$ (paper, Supplementary Fig. 5). The paper explores the balance
between surface tension and adhesion in its Figs. 3 and 4.

## Contact area fraction

With polarization mode 1, the statistics CSV has a column `cell_contact_area_fraction`: for an
epithelial cell, the area of its lateral faces divided by its total area; for other cells, 0. The
column is absent in the other modes ({doc}`14_output_files`).

## Compile-time options

| `POLARIZATION_MODE_INDEX` | Behaviour |
|---|---|
| 0 | No relabelling; epithelial faces stay apical |
| 1 (default) | From contacts, as described above |
| 2 | From the voxel map; writes `region_data/` |

Every shipped parameter file that states its configuration was written for mode 1
(`fig3e_*.xml` state none). Changing the mode requires a rebuild ({doc}`10_compile_time_configuration`).

:::{seealso}
- {doc}`08_cell_classes` for which classes are polarized.
- {doc}`/api/polarization` for the automatic polarizer.
:::
