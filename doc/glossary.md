(glossary)=
# Glossary

Terms used throughout the documentation, in alphabetical order. Each entry links to the page that
treats the term in detail.

```{glossary}
adherence strength
  The slope $\omega$ of the adhesive part of the contact law, set per face type with the XML tag
  `adherence_strength` (unit N/m³, equal to Pa/m). Used only by contact model 0; contact models 1
  and 2 replace adhesion by a coupling of nodes. See {doc}`/user_guide/03_contact`.

area elasticity modulus
  The modulus $k_a$ of the energy $\tfrac{k_a}{2}(A/A_0 - 1)^2$ that keeps the area $A$ of a cell
  near its target area $A_0$. XML tag `area_elasticity_modulus`, unit J. See
  {doc}`/user_guide/02_cell_mechanics`.

bending modulus
  The modulus $k_b$ of the discrete bending energy, set per face type with the XML tag
  `bending_modulus` (unit J). See {doc}`/user_guide/02_cell_mechanics`.

bulk modulus
  The modulus $K$ in the cell pressure $p = -K \ln(V/V_0)$; it sets how strongly a cell resists
  changes of its volume. XML tag `cell_bulk_modulus`, unit Pa. See
  {doc}`/user_guide/02_cell_mechanics`.

cell
  A simulated biological cell, or a lumen, nucleus, ECM or obstacle treated like one: a closed,
  triangulated surface whose nodes move under internal and contact forces. In the context of the
  VTK file format, "cell" can also mean a VTK polyhedron; the documentation says so where it does.
  See {doc}`/user_guide/01_model_overview`.

cell class
  One of the five C++ classes that implement the behaviour of a cell: epithelial, extracellular
  matrix (ECM), lumen, nucleus and static. Selected by the `global_cell_id` (0 to 4) of a cell
  type. See {doc}`/user_guide/08_cell_classes`.

cell superposition
  Two cells whose separate meshes occupy almost the same place, with nearly equal volumes. A node
  pushed deeper into a neighbour than the contact range feels no contact force, so such a pair
  does not separate again. Observed in the core of large growth aggregates with contact model 1;
  nothing logs it. See {doc}`/user_guide/03_contact` and {ref}`benchmarks-validity`.

cell type
  An entry `<cell_type>` of the parameter file, with the volume, area, growth and division
  parameters of a group of cells and its list of face types. Cells of the input mesh name their
  cell type by its position in the file. See {doc}`/user_guide/08_cell_classes`.

contact cutoff
  The distance range of a contact interaction: `contact_cutoff_adhesion` ($c_a$) for adhesion
  and coupling, `contact_cutoff_repulsion` ($c_r$) for repulsion. Unit m. See
  {doc}`/user_guide/03_contact`.

contact detection algorithm
  The method that finds candidate node-face pairs for contact (the broad phase), chosen with the
  optional XML tag `contact_detection_algorithm`: `uspg` (default), `sweep_and_prune` or
  `adaptive`. Used only by contact model 2. See {doc}`/user_guide/03_contact`.

contact model
  The rule by which cells exert forces on each other, chosen at compile time with
  `CONTACT_MODEL_INDEX`: 0 node-face springs, 1 node-node coupling, 2 face-face coupling
  (default). See {doc}`/user_guide/03_contact`.

coupling
coupled node
  In contact models 1 and 2, a node of an epithelial cell tied to a node of an adjacent
  epithelial cell. Coupled nodes are moved to their mean position and advanced together, which
  transmits forces directly between the two surfaces. See {doc}`/user_guide/03_contact`.

deformable cell model (DCM)
  A class of cell-based models in which each cell is a deformable closed surface (a polygon in
  two dimensions, a triangulated surface in three) rather than a point, a sphere or a polyhedron
  with shared faces. SimuCell3D is a three-dimensional DCM. See
  {doc}`/user_guide/01_model_overview`.

diagnostics CSV
  The file `performance_diagnostics.csv` in the output folder, or the path given with
  `--diagnostics-csv`. One row per 100 iterations with timings, cell and division counts and the
  number of unstable cells removed. See {doc}`/user_guide/14_output_files`.

division axis
  The longest axis of a cell, the eigenvector with the largest eigenvalue of the covariance
  matrix of its node positions. The plane that divides the cell is normal to it. See
  {doc}`/user_guide/06_growth_division_removal`.

dynamic model
  The equations of motion of the nodes, chosen at compile time with `DYNAMIC_MODEL_INDEX`: 0 the
  full equations with semi-implicit Euler integration (default), 1 the overdamped equations with
  forward Euler integration. See {doc}`/user_guide/04_time_integration`.

edge
  A segment between two nodes of a cell mesh, shared by exactly two faces. See
  {doc}`/user_guide/05_mesh_refinement`.

face
  A triangle of a cell mesh, wound so that its normal points out of the cell. Each face carries a
  face type. See {doc}`/user_guide/01_model_overview`.

face type
  An entry `<face_type>` of a cell type, with the surface tension, bending modulus, adherence
  strength and repulsion strength of the faces that carry it; for epithelial cells, the apical,
  lateral and basal membrane. A face refers to its face type by position in the list. See
  {doc}`/user_guide/07_polarization`.

face-face coupling
  Contact model 2, the default: a node of an epithelial cell can be coupled to one node on each
  neighbouring epithelial cell. See {doc}`/user_guide/03_contact`.

input mesh
  The legacy VTK file named in `input_mesh_file_path` that gives the initial geometry of every
  cell and its cell type. See {doc}`/user_guide/12_input_meshes`.

isoperimetric ratio
  The dimensionless ratio $Q = A^3/V^2$ of a closed surface; $36\pi \approx 113$ for a sphere and
  larger for any other shape. The target isoperimetric ratio $Q_0$ sets the target area. See
  {doc}`/user_guide/02_cell_mechanics`.

iteration
  One time step of the solver loop. In order: mesh output (when due), cell division (every
  5 iterations), face-type update, mesh refinement, contact detection, polarization and
  internal forces, time integration, statistics (every 50 iterations) and diagnostics (every
  100), and removal of dead and unstable cells. See
  {doc}`/user_guide/01_model_overview`.

local mesh refiner
  The component that keeps every edge length within $[l_{\min}, 3\,l_{\min}]$ at every
  iteration, by splitting, collapsing and swapping edges. See
  {doc}`/user_guide/05_mesh_refinement`.

maximum edge length
  Three times the minimum edge length; not a parameter. See {doc}`/user_guide/05_mesh_refinement`.

minimum edge length
  The lower bound $l_{\min}$ of the edge lengths of the cell meshes, XML tag `min_edge_length`
  (unit m). It sets the spatial resolution. See {doc}`/user_guide/05_mesh_refinement`.

minimum-volume death
  The deletion of a cell whose volume falls below its `min_vol`, at the end of an iteration. It
  models cell death and is part of the model, not a safeguard. It is not logged or counted. See
  {doc}`/user_guide/06_growth_division_removal`.

node
  A vertex of a cell mesh. The nodes carry the degrees of freedom of the model: position and, in
  the dynamic model, momentum. See {doc}`/user_guide/01_model_overview`.

parameter file
  The XML file passed to `simucell3d` on the command line. It sets the numerical parameters, the
  cell types and their face types. See {doc}`/user_guide/11_parameter_file`.

polarization mode
  The way the faces of epithelial cells are labelled apical, lateral or basal, chosen at compile
  time with `POLARIZATION_MODE_INDEX`: 0 none, 1 from contacts (default), 2 from a voxel map of
  the tissue. See {doc}`/user_guide/07_polarization`.

refused division
  A division that the cell divider declines because the triangulation of the cut does not cover
  its whole outline (2.1). The mother is kept and divides at a later division check. Logged on
  standard error as `Cell division failed for cell N: ... division refused`; not counted in the
  diagnostics CSV. See {doc}`/user_guide/06_growth_division_removal`.

removal
  In these pages, the deletion of a cell by a {term}`safeguard`: a numerically
  {term}`unstable cell`, or a cell whose mesh refinement failed. Each removal is logged on
  standard error with a line starting `Removing` and counted in the `unstable_cells_removed`
  column of the diagnostics CSV. Distinct from a {term}`minimum-volume death`. See
  {doc}`/user_guide/09_safeguards`.

replicate
  One of several runs of the same parameter file with the same build and settings. Replicates
  differ because of the clock-seeded random generator and threading, so a result is compared
  against the spread of its replicates. See {ref}`benchmarks-reproducibility`.

repulsion strength
  The slope $\xi$ of the repulsive contact law, set per face type with the XML tag
  `repulsion_strength` (unit N/m³). Used by all three contact models. See
  {doc}`/user_guide/03_contact`.

safeguard
  A mechanism added in 2.0 or 2.1, not part of the published model, that detects a damaged or
  diverged cell, removes or repairs it, and logs the event. See {doc}`/user_guide/09_safeguards`.

sampling period
  The simulated time between two writes of the output meshes, XML tag `sampling_period` (unit s).
  See {doc}`/user_guide/04_time_integration`.

scheduling mode
  The OpenMP loop scheduling of a run, chosen with `--schedule=`: `adaptive` (default), `static`,
  `dynamic` or `guided`. It affects run time, not the model; multi-threaded runs differ slightly
  between {term}`replicates <replicate>` in any case. See
  {doc}`/user_guide/18_parallel_performance`.

SimuCell3D
  The simulation software described in this documentation, created by Steve Runser in the Iber
  Lab (ETH Zurich) and published by Runser, Vetter & Iber (2024) ({doc}`/citing`). Its
  command-line executable is `simucell3d`, built as `build/src/app/simucell3d` with a link at
  `build/simucell3d` ({doc}`/user_guide/13_running_simulations`).

simulation duration
  The simulated time at which a run ends, XML tag `simulation_duration` (unit s). See
  {doc}`/user_guide/04_time_integration`.

statistics CSV
  The file `simulation_statistics.csv` in the output folder, with one row per cell every
  50 iterations (volume, area, pressure, energies, ...). See {doc}`/user_guide/14_output_files`.

surface tension
  The tension $\gamma$ of the cell cortex, set per face type with the XML tag `surface_tension`
  (unit N/m). See {doc}`/user_guide/02_cell_mechanics`.

target area
  The area $A_0 = (Q_0 V^2)^{1/3}$ towards which the area elasticity drives a cell. See
  {doc}`/user_guide/02_cell_mechanics`.

target volume
  The volume $V_0$ at which a cell has zero pressure. It increases with growth and is halved for
  each daughter at division. See {doc}`/user_guide/06_growth_division_removal`.

thread pinning
  Binding each OpenMP thread to its own physical core, the default in 2.1. Disabled by setting
  `SIMUCELL3D_PIN_THREADS=0` or `OMP_PLACES`. See {doc}`/user_guide/18_parallel_performance`.

time step
  The fixed increment $\Delta t$ of simulated time per iteration, XML tag `time_step` (unit s).
  See {doc}`/user_guide/04_time_integration`.

unstable cell
  A cell whose mesh has diverged (non-finite coordinates or an excessive extent) or whose surface
  is inside out or self-intersecting. 2.1 removes such cells. See
  {doc}`/user_guide/09_safeguards`.

USPG
  Uniform spatial partitioning grid: a regular grid of cubic voxels used to find contact
  candidates. See {doc}`/user_guide/03_contact`.
```
