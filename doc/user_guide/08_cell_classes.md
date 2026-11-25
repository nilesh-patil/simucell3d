(ug-cell-classes)=
# Cell classes

Besides epithelial cells, a simulated tissue can contain lumens, an extracellular matrix (ECM),
nuclei and fixed obstacles. All of them are closed triangulated surfaces with the same mechanics
({doc}`02_cell_mechanics`); they differ in a few behaviours, implemented by five C++
{term}`cell classes <cell class>`. This chapter explains how a cell of the input mesh is
assigned a cell type and a class, and what each class does differently.

## From the input mesh to a cell class

Two different numbers are involved:

1. Each cell of the {term}`input mesh` has an integer `cell_type_id` ({doc}`12_input_meshes`).
   It selects a `<cell_type>` block of the parameter file **by position**: 0 is the first block
   in the file, 1 the second, and so on. A value that is not smaller than the number of blocks
   stops the run with "The cell type id of cell <i> (<k>) is not valid".
2. That block's `global_cell_id` selects the cell class:

| `global_cell_id` | Cell class | C++ class |
|---|---|---|
| 0 | epithelial | `epithelial_cell` |
| 1 | extracellular matrix (ECM) | `ecm_cell` |
| 2 | lumen | `lumen_cell` |
| 3 | nucleus | `nucleus_cell` |
| 4 | static | `static_cell` |

Any other value stops the run. Several cell types can share a class, for example two epithelial
cell types with different surface tensions. In the shipped parameter files the blocks are listed
in the order of their `global_cell_id` (epithelial, ECM, lumen, nucleus, static), so the two
numbers coincide; in a file with a different order they do not.

The output meshes write the `global_cell_id` of each cell (array `cell_type_id` in the cell
meshes, column `type_id` in the statistics CSV), not the position of its block. Cell types that
share a class cannot be told apart in the output, and an output mesh can serve as input only if
the blocks of the parameter file are in class order.

## Behaviour of the classes

| Behaviour | Epithelial | ECM | Lumen | Nucleus | Static |
|---|---|---|---|---|---|
| Nodes move | yes | no | yes | yes | no |
| Internal forces (pressure, tension, area, bending) | yes | no | yes | yes | computed, no effect |
| Target volume grows | yes | no | yes | yes | yes, but the cell does not move |
| Divides | yes | no | no | no | no |
| Faces relabelled by polarization | yes | no | no | no | no |
| Coupled to other cells (contact models 1 and 2) | with epithelial cells | no | no | no | no |
| Repels and is repelled | yes | yes | yes | yes | yes |
| Dies below `min_vol` ({term}`minimum-volume death`) | yes | yes | yes | yes | yes |

Notes on each class:

**Epithelial (0).** The only class that divides ({doc}`06_growth_division_removal`), has its faces
labelled apical, lateral or basal ({doc}`07_polarization`), and couples to neighbours in contact
models 1 and 2 ({doc}`03_contact`). Its cell types need at least as many face types as the
polarization uses: three (apical, lateral, basal) with polarization mode 1 and contact model 0,
two with mode 1 and contact models 1 or 2 or with mode 2, one with mode 0. The shipped files
always define three.

**ECM (1).** A fixed surface: its nodes never move and no internal forces are computed, so its
bulk modulus, tension and growth parameters have no effect. Its faces still interact with other
cells through repulsion and, in contact model 0, adhesion. The ECM is expected to enclose the
tissue, so for a node of an epithelial cell the two sides of an ECM face are swapped: the inside
of the ECM surface counts as the free side ({doc}`03_contact`). The shipped files give the ECM a
bulk modulus of 0.

**Lumen (2).** A fluid-filled cavity represented as a cell. It has a pressure and a surface
tension and moves like any cell, but it does not divide or couple. In the vesicle files its
growth rate is 0, so its target volume stays at its initial value while the epithelial cells
around it grow.

**Nucleus (3).** A surface inside an epithelial cell. For a node of a nucleus, the sides of the
faces of an epithelial cell are swapped, so the nucleus is repelled from the inside of the cell
membrane and stays inside its cell. Nothing else ties a nucleus to its cell. Runser, Vetter &
Iber (2024) use nuclei in their pseudostratified epithelium simulations (Fig. 4).

**Static (4).** A rigid obstacle: its nodes never move. Its internal forces are computed but
have no effect. Runser, Vetter & Iber (2024) use such cells as rigid boundaries around a patch of
simulated cells (Fig. 4).

:::{note}
Parameter files of release 1.0 contain a tag `<INMForce>` in the nucleus cell type. The files
shipped with 2.1 no longer have it, and no code in 2.1 reads it, so it has no effect if present.
:::

## Safeguards and static cells

The 2.1 safeguards ({doc}`09_safeguards`) check every cell for non-finite coordinates and an
excessive extent. The checks for a non-positive signed volume and for a volume larger than the
bounding box are not applied to the ECM and to static cells, because these never move and an
ECM can be an open sheet.

:::{seealso}
- {doc}`11_parameter_file` for the tags of a cell type.
- {doc}`12_input_meshes` for the `cell_type_id` field of the input mesh.
- {doc}`/api/cell_classes` for the C++ classes.
:::
