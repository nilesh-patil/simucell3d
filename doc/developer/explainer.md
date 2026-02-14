# SimuCell3D: Deep Dive & Explainer

This document explains the **SimuCell3D** codebase in the context of the scientific simulations described in the paper *"SimuCell3D: three-dimensional simulation of tissue mechanics with cell polarization"* (Runser et al., Nature Computational Science 2024).

## 1. Introduction to SimuCell3D

**Scientific Goal**: To simulate the mechanics of biological tissues at **subcellular resolution**. Unlike vertex models (which approximate cells as polygons), SimuCell3D uses a **Deformable Cell Model (DCM)** where each cell surface is a triangulated mesh. This allows for realistic shapes, bending, and complex cell-cell interactions.

**Codebase Core**: The project is a high-performance C++ engine. The entry point is `main.cpp`, which initializes a `solver`. The core logic resides in `src/solver.cpp` and `src/contact_models/`, using OpenMP for parallelization.

---

## 2. The Deformable Cell Model (DCM)

### Science
Real cells are 3D objects with fluid interiors, elastic membranes, and internal pressure. They can squeeze, flatten, and bend.

### Codebase Implementation
*   **Mesh**: Each cell is an instance of the `cell` class (implied from `cell_ptr`), composed of `node`s (vertices) and `face`s (triangles).
*   **Refinement**: To maintain numerical stability during deformation, the mesh is constantly refined.
    *   **File**: `src/mesh/local_mesh_refiner.cpp` (managed by `lmr_ptr_` in `solver.cpp`).
    *   **Logic**: In `solver::run_iteration()`, `lmr_ptr_->refine_meshes(cell_lst_)` splits long edges and collapses short ones to keep triangles regular.

---

## 3. Simulation: Epithelial Sheet Mechanics

This is a fundamental simulation often shown in Figure 1 of such papers (referencing `data/input_meshes/fig_1_sheet_geometry.vtk`).

### Step 1: Configuration (`parameters/core/parameters_sheet.xml`)
The simulation is defined by an XML file setting the physics:
*   **Cell Types**:
    *   `epithelial` (ID 0): The main building block.
    *   `ecm` (Extracellular Matrix, ID 1) or `static_cell`: Provides a substrate.
*   **Mechanical Properties**:
    *   `<cell_bulk_modulus>2.5e3</cell_bulk_modulus>`: Controls how hard it is to change cell volume (compressibility).
    *   `<surface_tension>1e-3</surface_tension>`: Controls the contractility of the membrane.

### Step 2: Physics Engine (Contact Model)
For a sheet to stay together, cells must adhere.
*   **Code**: `src/contact_models/contact_face_face_via_coupling.cpp` (selected by `CONTACT_MODEL_INDEX 2` in `include/global_configuration.hpp`).
*   **Algorithm**:
    1.  **Broad Phase**: The space is divided into a grid (USPG - Uniform Space Partitioning Grid) to find nearby faces.
    2.  **Narrow Phase**: The code calculates the distance between faces.
    3.  **Forces**: If distance < `contact_cutoff_adhesion`, an attractive force is applied (simulating Cadherins). If distance is very small, a repulsive force prevents overlap.

### Step 3: Time Evolution
The `solver::run_iteration()` loop moves the simulation forward:
1.  **Forces**: Calculates Pressure + Surface Tension + Contact Forces.
2.  **Integration**: Updates node positions using `time_integrator_ptr_->update_nodes_positions`.

---

## 4. Simulation: Cell Polarization

### Science
Epithelial cells are polarized: they have an **Apical** side (top, facing lumen), **Basal** side (bottom, facing substrate), and **Lateral** sides (touching neighbors). This polarity dictates mechanical properties (e.g., higher tension apically causes folding).

### Codebase Implementation
*   **Polarization Mode**: Configured via `POLARIZATION_MODE_INDEX` in `include/global_configuration.hpp`.
    *   **Mode 1 (Contact-based)**: Cells detect who they touch. If touching another cell -> Lateral. If touching ECM -> Basal. If touching nothing/lumen -> Apical.
*   **Face Types**: defined in `parameters_sheet.xml`:
    *   `<face_type_name>apical</face_type_name>`: High surface tension (`1e-3`) to simulate an actin ring.
    *   `<face_type_name>lateral</face_type_name>`: Lower tension (`5e-4`) but high `adherence_strength` (`1e9`) to keep cells glued.
    *   `<face_type_name>basal</face_type_name>`: Interacts with the substrate.

In `solver::run_iteration()`:
```cpp
// Polarize the faces based on their contacts
cell_lst_[i]->special_polarization_update(cell_lst_);
```
This function dynamically updates the face type (and thus the forces) as cells move and rearrange.

---

## 5. Simulation: Pressurized Vesicle / Cyst

### Science
A cyst is a hollow sphere of cells surrounding a fluid-filled lumen. The lumen pressure drives expansion.

### Codebase Implementation
*   **Config**: `parameters/core/parameters_vesicle.xml`
*   **Lumen Representation**: The lumen is modeled as a "Ghost Cell" (Cell Type 2).
    *   `<cell_type_name>lumen</cell_type_name>`
    *   `<max_inner_pressure>INF</max_inner_pressure>`: It exerts pressure outward on the epithelial cells.
*   **Growth**: The simulation typically increases the target volume of the lumen cell or epithelial cells (via `<avg_growth_rate>`) to drive expansion, testing the tissue's viscoelasticity.

---

## 6. Performance & Optimization

The paper claims high performance (125k cells/day). This is achieved via **Adaptive OpenMP Scheduling** in `src/solver.cpp`.

*   **Problem**: Biology is heterogeneous. Some cells divide (high workload), some are static (low workload). Standard parallel loops (`#pragma omp parallel for`) can be inefficient if one thread gets all the hard work.
*   **Solution**: The `solver` measures "workload heterogeneity" (`calculate_workload_heterogeneity`) and dynamically switches scheduling strategies:
    *   **Static**: Good for uniform meshes (low overhead).
    *   **Dynamic/Guided**: Good when some cells are dividing or interacting heavily (better load balancing).

```cpp
// Example from solver.cpp
if (heterogeneity_cov > 0.6) {
    // High heterogeneity: smaller chunks for better load balancing
    divisor = 20;
}
```
This allows the code to simulate massive tissues efficiently.

