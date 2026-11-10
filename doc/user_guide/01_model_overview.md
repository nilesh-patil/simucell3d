(ug-model-overview)=
# Model overview

SimuCell3D is a {term}`deformable cell model (DCM)` in three dimensions. Every cell of a
tissue is a closed, triangulated surface that deforms under its own mechanical energy and
under contact with other cells. This chapter describes what a simulated tissue consists of,
what the solver does in one {term}`iteration`, which choices are made at compile time and
which in the {term}`parameter file`, and how the 2.1 code relates to the model published by
Runser, Vetter & Iber (2024). The following chapters treat each part in detail.

## A tissue as a set of closed surfaces

A {term}`cell` is represented only by its surface: a closed, orientable triangle mesh made of
{term}`nodes <node>`, edges and {term}`faces <face>`. The faces are wound so that their normals
point out of the cell. The interior is not discretised; the cell volume $V$ and area $A$ are
computed from the surface (chapter {doc}`02_cell_mechanics`). The forces of the model act on
the nodes, and the nodes carry the degrees of freedom: a position $\mathbf r_i$ and, with the
full equations of motion, a momentum $\mathbf p_i$.

Three levels of description give each part of a cell its parameters:

{term}`Cell type`
: An entry `<cell_type>` of the parameter file. It holds the volume and area parameters
  (bulk modulus, target isoperimetric ratio, growth and division parameters, ...) and a list
  of face types. Each cell of the {term}`input mesh` names its cell type by position in the
  file (chapter {doc}`08_cell_classes`).

{term}`Cell class`
: The C++ class that implements a cell type, selected by `global_cell_id`: 0 epithelial,
  1 extracellular matrix (ECM), 2 lumen, 3 nucleus, 4 static. The class decides whether a cell
  moves, divides, couples to neighbours and has its faces polarized.

{term}`Face type`
: An entry `<face_type>` inside a cell type. It holds the surface parameters: surface tension,
  bending modulus, adherence strength and repulsion strength. Every face of a cell carries one
  face type of its cell type. For epithelial cells the face types stand for apical, lateral and
  basal membrane, and the solver can relabel faces as contacts change
  (chapter {doc}`07_polarization`).

Lumens, nuclei and the ECM are represented the same way as cells: by closed triangulated
surfaces with their own cell type. They differ from epithelial cells only in the behaviour of
their class.

## Forces and motion

The mechanical state of a cell is described by the potential energy of Runser, Vetter & Iber
(2024), Eq. (1): a pressure term that keeps the volume near a {term}`target volume`, an area
elasticity term, surface tension, and bending. Its negative gradient gives the internal nodal
forces (chapter {doc}`02_cell_mechanics`). Contact forces between cells come from one of three
{term}`contact models <contact model>`: springs between nodes and faces, or a mechanical
coupling of nodes on adjacent cells (chapter {doc}`03_contact`). The nodes then move according
to either the full equations of motion with viscous damping or their overdamped limit
(chapter {doc}`04_time_integration`).

Between force evaluations the {term}`local mesh refiner` keeps every edge length within
$[l_{\min}, 3\,l_{\min}]$ (chapter {doc}`05_mesh_refinement`). Cells grow by increasing their
target volume, divide when their volume reaches a division volume, and are removed when their
volume falls below a minimum (chapter {doc}`06_growth_division_removal`).

## One iteration of the solver

The solver advances the tissue by one {term}`time step` $\Delta t$ per iteration. In 2.1 one
iteration performs these steps, in this order:

```text
 1. write the cell and face meshes if a new sampling period has started
 2. every 5 iterations: divide the cells that are ready to divide
 3. reset the face types of epithelial cells (all faces apical)
 4. refine the mesh of every cell (split, collapse, swap); remove cells whose refinement fails
 5. detect contacts and compute contact forces (or node couplings)
 6. polarization mode 2 only: label faces from a voxel map of the tissue
 7. polarization mode 1 with contact models 1 and 2: relabel faces from their couplings;
    compute the internal forces of every cell (pressure, tension, area elasticity, bending)
 8. integrate the equations of motion; advance the simulation time by the time step
 9. every 50 iterations: append to the statistics CSV
    every 100 iterations: append to the diagnostics CSV
10. remove cells below their minimum volume and numerically unstable cells
```

Most steps run in parallel over cells with OpenMP. The time integration with contact model 2
and the voxel map of polarization mode 2 are serial. The run stops when the simulation time reaches
`simulation_duration` or when no cell is left.

:::{versionchanged} 2.1
Step 4 removes a cell whose mesh refinement throws an error instead of ending the run, and
step 10 also removes numerically unstable cells. Both are {term}`safeguards <safeguard>`; see
{doc}`09_safeguards`.
:::

## Compile-time and run-time choices

Some model choices are fixed when SimuCell3D is compiled; the others are read from the
parameter file at start-up.

| Choice | Where | Values |
|---|---|---|
| Contact model | `CONTACT_MODEL_INDEX` in `include/global_configuration.hpp` | 0 node-face springs, 1 node-node coupling, 2 face-face coupling (default) |
| Dynamic model | `DYNAMIC_MODEL_INDEX` | 0 full equations of motion, semi-implicit Euler (default); 1 overdamped, forward Euler |
| Polarization mode | `POLARIZATION_MODE_INDEX` | 0 none, 1 from contacts (default), 2 from a voxel map |
| Material parameters, time step, mesh resolution, contact cutoffs | parameter file | see {doc}`11_parameter_file` |
| Contact detection algorithm (contact model 2 only) | parameter file, `contact_detection_algorithm` | `uspg` (default), `sweep_and_prune`, `adaptive` |

Every shipped parameter file states in its header comment which compile-time configuration it
was written for. Chapter {doc}`10_compile_time_configuration` explains how to change the
configuration and rebuild.

## Units

All quantities are in SI units: lengths in m, times in s, pressures in Pa, surface tensions in
N/m, energies in J. The parameter files store plain numbers; the unit of each tag is given in
{doc}`11_parameter_file`. Simulation time is the physical time of the equations of motion; it
is not rescaled.

## Relationship to the published model

The model and its numerical methods are described in the Methods section of Runser, Vetter &
Iber (2024) and in its Supplementary Information. SimuCell3D 2.1 implements that model with the
differences listed in the notes of the following chapters. The main ones are:

- The default contact model in the code, face-face coupling (contact model 2), is not one of
  the two models described in the paper's Methods; it extends the node-node coupling of the
  paper so that a node can be coupled to one node on each neighbouring cell. It has been the
  default since release 1.0.
- The target area is computed from the current volume, not from the target volume
  (chapter {doc}`02_cell_mechanics`).
- The bending force has a prefactor of 3 where the paper's Methods give 2
  (chapter {doc}`02_cell_mechanics`).
- The division plane is always normal to the longest cell axis; there is no random orientation
  (chapter {doc}`06_growth_division_removal`).

:::{important}
Results differ from 1.0. A simulation run with SimuCell3D 2.1 does not reproduce the numbers
produced by 1.0, the code published with the paper. Several 1.0 bugs changed the physics (the
contact distance to a face interior, the time step of coupled nodes, the choice of coupling
partner, the face orientation after an edge split), and fixing them changes trajectories. 2.1
also adds safeguards that remove or repair cells that 1.0 kept or crashed on. To compare with
the published figures, run release 1.0. The full list is in {doc}`/whats_new/v2.1`.
:::

:::{seealso}
- {doc}`/getting_started` for a first run.
- {doc}`11_parameter_file` for every tag of the parameter file.
- {doc}`/glossary` for the terms used in this guide.
:::
