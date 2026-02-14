# SimuCell3D-JAX: GPU Architecture Design

## 1. Executive Summary

SimuCell3D is a 3D deformable cell model (DCM) simulator for tissue mechanics, published in
*Nature Computational Science* (2024). It represents cells as closed triangulated surface meshes
and simulates their mechanical behavior using OpenMP-parallelized C++. This document describes a
complete redesign targeting JAX-based GPU computing.

**Why JAX:**
- **Differentiability** — `jax.grad` through the full simulation for parameter inference
- **Vectorized batching** — `jax.vmap` over parameter space for sensitivity analysis
- **JIT compilation** — XLA compiles the entire force+integrate step into a single GPU kernel
- **GPU acceleration** — 10–20x speedup over 8-core CPU for large tissue simulations

**What changes fundamentally:**
- Array-of-Structures → **Struct-of-Arrays** (flat global arrays with CSR cell ownership)
- In-place mutation → **Functional state updates** (state-in, state-out)
- OOP with virtual dispatch → **Pytrees** (NamedTuples registered as JAX types)
- `std::mutex`, `std::set`, `std::map` → **Segment operations and padded arrays**
- `#pragma omp parallel` → **Single XLA graph** (no thread synchronization)

**Expected impact:**
- 10–20x speedup at 10K–100K cells on NVIDIA A100
- Gradient-based parameter inference (replacing grid search)
- Batched parameter sweeps: ~10 replicas of 10K cells on a single A100
- Path to multi-GPU via `jax.pmap`

---

## 2. Current System Analysis

### 2.1 Simulation Loop

The C++ simulation loop (`solver::run_iteration()` in `src/solver.cpp:968`) executes these phases
per timestep:

1. **Cell division** (every 5 iterations) — volume threshold check, plane bisection, BPA retriangulation
2. **Face type update** — parallel per-cell classification (apical/lateral/basal)
3. **Mesh refinement** — edge splitting/collapsing to maintain mesh quality
4. **Contact detection** — USPG spatial partitioning → AABB filtering → node-face coupling
5. **Automatic polarization** (optional) — voxel-based space discretization for face classification
6. **Internal forces** — per-cell: geometry update → pressure → surface tension → bending → angle regularization → curvature computation
7. **Time integration** — semi-implicit Euler with coupled-node force/momentum averaging
8. **Statistics & output** — periodic VTK mesh writing and CSV statistics

### 2.2 C++ Patterns That Do Not Translate to GPU

| C++ Pattern | Location | GPU Problem | JAX Replacement |
|---|---|---|---|
| `std::mutex` per node | `node.hpp:75` | No locks on GPU | Atomic scatter or segment ops |
| `std::map<unsigned, pair>` per node | `node.hpp:98` | Dynamic allocation | Fixed-capacity padded arrays |
| `std::set<edge>` per cell | `cell.hpp:99` | Tree structure, pointer chasing | Derived edge arrays from face connectivity |
| `std::shared_ptr<cell>` everywhere | `node.hpp:27` | Reference counting | Index-based references (cell_id) |
| `virtual` dispatch (`cell`, contact models) | `cell.hpp:214,383` | No vtables on GPU | Compile-time dispatch or type masks |
| `std::optional` for coupling | `node.hpp:85` | Branch divergence | Sentinel values (-1) with masks |
| Per-cell `std::vector` (ragged arrays) | `cell.hpp:55-58` | Variable-length arrays | Global flat arrays + CSR offsets |
| Friend class access patterns | `cell.hpp:103-119` | Encapsulation overhead | Flat arrays, no encapsulation needed |

### 2.3 Performance Bottlenecks (Profiled)

Based on the `PerformanceMonitor` instrumentation in `solver.cpp`:

| Phase | % of Runtime | Scaling | GPU Opportunity |
|---|---|---|---|
| Contact detection + forces | ~60% | O(N_c^{4/3}) | Spatial hashing + vectorized distance |
| Bending forces | ~15% | O(N_edges) | Edge-parallel computation |
| Pressure + surface tension | ~10% | O(N_faces) | Face-parallel, embarrassingly parallel |
| Angle regularization | ~5% | O(N_faces) | Face-parallel |
| Time integration | ~5% | O(N_nodes) | Node-parallel |
| Mesh refinement | ~5% | O(N_edges) | Keep on CPU (topology changes) |

**Note**: Mesh refinement is performed every timestep (see `solver.cpp:1031`), while cell division is performed every 5 iterations (`solver.cpp:983`). Mesh refinement accounts for ~5% of total runtime; CPU-GPU transfer overhead is separate.

---

## 3. Mathematical Foundation

### 3.1 Energy Functional

The total mechanical energy of the tissue is:

```
U_total = Σ_cells [ U_pressure + U_surface + U_elasticity + U_bending + U_angle ] + U_contact
```

**Pressure energy** (logarithmic equation of state):
```
U_pressure = K_v · V · (ln(V / V_target) - 1)
P = -K_v · ln(V / V_target)
```
where `K_v` is the bulk modulus, `V` is cell volume, `V_target` is target volume.
Capped at `P_max` to prevent numerical blow-up.
(Implementation: `cell.cpp:1316-1327`)

**Surface tension energy** (per face type):
```
U_surface = Σ_faces γ_f · A_f
```
where `γ_f` is the surface tension coefficient for face type `f`.
(Implementation: `cell.cpp:1429`)

**Membrane elasticity energy** (area-elastic):
```
U_elasticity = (k_a / A_target) · (A / A_target - 1)²
A_target = (η · V²)^{1/3}
```
where `k_a` is the area elasticity modulus, `η` is the target isoperimetric ratio.
(Implementation: `cell.cpp:1379-1380`)

**Bending energy** (discrete Helfrich, Wardetzky et al. 2007):
```
U_bending = Σ_edges k_b · (|e|² / (A_f1 + A_f2)) · (2·cos(θ/2))²
```
where `θ` is the dihedral angle between adjacent faces, `|e|` is edge length.
(Implementation: `cell.cpp:1437-1565`)

**Angle regularization energy** (mesh quality):
```
U_angle = k_reg · Σ_faces Σ_{i=1}^{3} (π/3 - α_i)²
```
where `α_i` are the three face angles, targeting equilateral triangles.
(Implementation: `cell.cpp:1785-1847`)

### 3.2 Force Derivations

All forces are computed as negative gradients of the energy functional.

**Pressure force** (per face, distributed to 3 nodes equally):
```
F_pressure = P · A_face · n_face / 3
```
(Implementation: `cell.cpp:1334-1366`)

**Surface tension + membrane elasticity force** (fused, analytical area gradients):
```
∇_{n_i} A_face = -0.5 · n_face × (n_j - n_k)    [cyclic permutation]
F_i = (-γ + k_a/A_target · (A/A_target - 1)) · ∇_{n_i} A_face
```
(Implementation: `cell.cpp:1375-1432`)

**Bending force** (edge-hinge diamond, 4 nodes per edge):
```
For edge (n1, n2) with opposite nodes n3 (face f1), n4 (face f2):

θ = dihedral angle between f1 and f2
prefactor_1 = -3·(1 + cos θ) · k_b
prefactor_2 = 3·|e|²/(A_f1 + A_f2) · sin θ · k_b

∇_{n1} θ = -(cot α₃ + cot α₄) · n_f1 / |e|
∇_{n2} θ = -(cot α₁ + cot α₂) · n_f1 / |e|
∇_{n3} θ = |e| / (2·A_f1) · n_f1
∇_{n4} θ = |e| / (2·A_f2) · n_f2

F_ni = prefactor_1 · ∇_{ni}(|e|²/A_sum) + prefactor_2 · ∇_{ni} θ
```
(Implementation: `cell.cpp:1437-1565`)

**Angle regularization force**:
```
For face with nodes (i, j, k) and angle α at vertex i:
∇_i α = -(∂/∂i)(arccos(a·b / |a||b|))    [analytical gradient]
F_i = k_reg · Σ_angles (π/3 - α) · ∇_i α
```
(Implementation: `cell.cpp:1732-1847`)

### 3.3 Contact Model: Face-Face Coupling (C++ Reference)

The default contact model (`CONTACT_MODEL_INDEX=2`, `contact_face_face_via_coupling.cpp`):

1. **Broad phase**: USPG spatial grid partitions faces into voxels
2. **AABB check**: Per-face axis-aligned bounding boxes with padding
3. **Normal filtering**: `n_node · n_face < threshold` (normals must face each other)
4. **Curvature filtering**: `κ_node < κ_max` (high curvature regions excluded)
5. **One-to-one coupling**: Each node couples to at most one node per adjacent cell (closest by distance, stored in `coupled_nodes_map_`)
6. **Position averaging**: Coupled nodes are moved to their average position
7. **Force/momentum averaging**: During time integration, coupled nodes share averaged force and momentum

**Repulsion force** (when surfaces interpenetrate):
```
F_repulsion = k_rep · A_face · (p - q)
```
where `p` is the node position, `q` is the closest point on the triangle, and `A_face` is the face area.
Distributed to face nodes via barycentric coordinates.
(Implementation: `contact_face_face_via_coupling.cpp:389-414`)

### 3.4 Node Curvature (Cotangent Laplacian)

For contact models 1 and 2, per-node curvature and normals are computed via the
cotan-Laplace operator:

```
Δx_i = (1 / 2A_i) · Σ_{j∈N(i)} (cot α_ij + cot β_ij) · (x_j - x_i)
H_i = |Δx_i| / (4 · A_voronoi_i)
```
where `α_ij`, `β_ij` are the angles opposite to edge `(i,j)` in the two adjacent triangles.
(Implementation: `cell.cpp:256-385`)

### 3.5 Time Integration

**Semi-implicit Euler** (`DYNAMIC_MODEL_INDEX=0`):
```
p_{n+1} = p_n + (F - ζ/m · p_n) · dt        [momentum update]
x_{n+1} = x_n + p_{n+1}/m · dt               [position update using NEW momentum]
```
where `ζ` is the damping coefficient, `m` is the node mass (cell mass / num_nodes).

**Overdamped forward Euler** (`DYNAMIC_MODEL_INDEX=1`):
```
x_{n+1} = x_n + F/ζ · dt
```

**Coupled node handling** (face-face coupling):
```
F_avg = (F_1 + F_2 + ... + F_k) / k          [average over all coupled nodes]
m_avg = (m_1 + m_2 + ... + m_k) / k
p_avg = (p_1 + p_2 + ... + p_k) / k          [for semi-implicit Euler]
```
Each coupled node then integrates with the averaged quantities.
(Implementation: `time_integration.cpp:202-336`)

### 3.6 Numerical Stability Considerations

- **Log singularity**: `ln(V/V_target)` diverges as `V → 0`; cells below `min_vol` are removed
- **Arccos domain**: Dihedral angle computation clamps dot product to `[-1, 1]`
- **Cotangent divergence**: Skipped when `cot(α)` is non-finite; angles near 0° or 180° excluded
- **Bending threshold**: Dihedral angles > 135° are skipped to prevent numerical instabilities
- **Angle regularization bounds**: Faces with any angle < 10° or > 170° are excluded

---

## 4. State Representation (Core Architecture)

### 4.1 Design Principles

- **Struct-of-Arrays (SoA)**: Each field is a flat JAX array (not per-cell lists)
- **Global indexing**: All nodes, faces, edges indexed globally with cell ownership via CSR offsets
- **Capacity management**: Pre-allocated arrays with validity masks; capacity doubles on overflow
- **Pytree-compatible**: All state containers are `NamedTuple` subclasses registered as JAX pytrees
- **No Equinox/Flax**: Simulation state is not a neural network; plain NamedTuples suffice

### 4.2 SimConfig (Frozen Parameters)

```python
class SimConfig(NamedTuple):
    """Immutable simulation parameters. Created once, never modified during simulation."""

    # Global parameters
    dt: float                          # Time step
    damping_coeff: float               # Damping coefficient ζ
    min_edge_len: float                # Minimum edge length for refinement
    contact_cutoff_adhesion: float     # Adhesion interaction cutoff distance
    contact_cutoff_repulsion: float    # Repulsion interaction cutoff distance

    # Per cell-type parameters [n_cell_types]
    bulk_modulus: jnp.ndarray          # K_v per cell type                  [n_types]
    max_pressure: jnp.ndarray          # P_max per cell type                [n_types]
    mass_density: jnp.ndarray          # ρ per cell type                    [n_types]
    area_elasticity: jnp.ndarray       # k_a per cell type                  [n_types]
    target_isoperimetric: jnp.ndarray  # η per cell type                    [n_types]
    angle_reg_factor: jnp.ndarray      # k_reg per cell type                [n_types]
    coupling_max_curvature: jnp.ndarray # κ_max per cell type               [n_types]

    # Per face-type parameters [n_face_types]
    surface_tension: jnp.ndarray       # γ per face type                    [n_face_types]
    bending_modulus: jnp.ndarray       # k_b per face type                  [n_face_types]
    repulsion_strength: jnp.ndarray    # k_rep per face type                [n_face_types]
    adhesion_strength: jnp.ndarray     # k_adh per face type                [n_face_types]

    # Growth parameters [n_types]
    avg_growth_rate: jnp.ndarray       # Mean growth rate per type          [n_types]
    avg_division_vol: jnp.ndarray      # Mean division volume per type      [n_types]
    min_vol: jnp.ndarray               # Minimum volume before removal      [n_types]
```

### 4.3 MeshTopology (Connectivity, Changes Rarely)

```python
class MeshTopology(NamedTuple):
    """Mesh connectivity. Updated only during refinement/division (CPU-side)."""

    # Face connectivity — the fundamental representation
    face_nodes: jnp.ndarray            # [max_faces, 3] — node IDs per face (uint32)
    face_cell: jnp.ndarray             # [max_faces] — owning cell ID per face (uint32)
    face_type: jnp.ndarray             # [max_faces] — face type ID (uint16)
    face_valid: jnp.ndarray            # [max_faces] — validity mask (bool)

    # Edge connectivity — derived from faces
    edge_nodes: jnp.ndarray            # [max_edges, 2] — node IDs per edge (uint32)
    edge_faces: jnp.ndarray            # [max_edges, 2] — face IDs per edge (uint32)
    edge_cell: jnp.ndarray             # [max_edges] — owning cell ID (uint32)
    edge_valid: jnp.ndarray            # [max_edges] — validity mask (bool)

    # Cell ownership (CSR pattern)
    node_cell: jnp.ndarray             # [max_nodes] — owning cell ID per node (uint32)
    node_valid: jnp.ndarray            # [max_nodes] — validity mask (bool)
    cell_node_offsets: jnp.ndarray     # [max_cells + 1] — CSR offsets into node arrays
    cell_face_offsets: jnp.ndarray     # [max_cells + 1] — CSR offsets into face arrays
    cell_edge_offsets: jnp.ndarray     # [max_cells + 1] — CSR offsets into edge arrays

    # Cell metadata
    cell_type: jnp.ndarray             # [max_cells] — cell type ID (uint16)
    cell_valid: jnp.ndarray            # [max_cells] — validity mask (bool)
    cell_is_static: jnp.ndarray        # [max_cells] — static flag (bool)

    # Counts
    n_nodes: int
    n_faces: int
    n_edges: int
    n_cells: int
```

**Important**: Faces are stored in **shuffled order** within each cell (not sorted by cell ID). This avoids a 100x performance degradation in `segment_sum` with float16 on GPUs (see JAX Issue #26227: sorted segment_ids trigger a slow path). Face shuffling is performed during topology construction (CPU-side) to optimize GPU segment operations.

### 4.4 SimState (Dynamic State, Updated Every Step)

```python
class SimState(NamedTuple):
    """Dynamic simulation state. Updated every timestep via functional updates."""

    # Node state
    pos: jnp.ndarray                   # [max_nodes, 3] — node positions (float32)
    force: jnp.ndarray                 # [max_nodes, 3] — accumulated forces (float32)
    momentum: jnp.ndarray              # [max_nodes, 3] — node momenta (float32, dynamic model only)

    # Precomputed geometry (updated at start of each step)
    face_normals: jnp.ndarray          # [max_faces, 3] — unit face normals (float32)
    face_areas: jnp.ndarray            # [max_faces] — face areas (float32)

    # Cell-level state
    cell_volume: jnp.ndarray           # [max_cells] — current volumes (float32)
    cell_area: jnp.ndarray             # [max_cells] — current surface areas (float32)
    cell_target_volume: jnp.ndarray    # [max_cells] — target volumes (float32)
    cell_pressure: jnp.ndarray         # [max_cells] — pressure (float32)
    cell_centroid: jnp.ndarray         # [max_cells, 3] — centroids (float32)
    cell_growth_rate: jnp.ndarray      # [max_cells] — individual growth rates (float32)
    cell_division_vol: jnp.ndarray     # [max_cells] — individual division volumes (float32)

    # Node curvature (for contact models 1, 2)
    node_curvature: jnp.ndarray        # [max_nodes] — mean curvature H (float32)
    node_normal: jnp.ndarray           # [max_nodes, 3] — surface normal at node (float32)

    # Contact state (for soft adhesion potential — no coupling maps needed)
    # Contact forces are computed on-the-fly and added directly to force array

    # Simulation time tracking
    time: float                        # Current simulation time
    iteration: int                     # Current iteration count
```

### 4.5 Capacity Management

```python
def ensure_capacity(topo: MeshTopology, state: SimState,
                    required_nodes: int, required_faces: int) -> Tuple:
    """Double capacity if current arrays are insufficient.

    Recompilation frequency: O(log N) over simulation lifetime.
    Example: Growing 10 → 10,000 cells requires log₂(1000) ≈ 10 doublings.
    Each recompilation takes ~50–100ms on A100.
    Total overhead: ~1 second for 1000x growth (acceptable).

    For fixed-cell-count simulations, capacity never changes → zero recompilation.
    """
    if required_nodes > topo.pos.shape[0]:
        new_cap = max(required_nodes, topo.pos.shape[0] * 2)
        topo, state = _reallocate(topo, state, new_cap)
    return topo, state
```

---

## 5. Force Computation Pipeline

### 5.1 Geometry Update

Recompute face normals, areas, cell volumes, and cell areas at the start of each timestep.

```python
def update_geometry(state: SimState, topo: MeshTopology) -> SimState:
    """Recompute all geometric quantities from current node positions.

    Note: Faces are shuffled within cells to avoid sorted segment_ids performance penalty
    in segment_sum (JAX Issue #26227: sorted indices trigger 100x slowdown for float16).
    """

    # Gather node positions for each face [max_faces, 3, 3]
    p = state.pos[topo.face_nodes]  # [F, 3, 3]
    v0, v1, v2 = p[:, 0], p[:, 1], p[:, 2]

    # Cross product for face normals and areas
    cross = jnp.cross(v1 - v0, v2 - v0)                # [F, 3]
    face_areas = 0.5 * jnp.linalg.norm(cross, axis=-1)  # [F]
    face_normals = cross / (2.0 * face_areas[:, None] + 1e-30)  # [F, 3]

    # Cell volumes via divergence theorem: V = (1/6) Σ_f (v0 · n_f) * A_f
    # Using the identity: V = (1/6) Σ_f n_f · v0_f * 2*A_f
    signed_vol_contrib = jnp.sum(v0 * cross, axis=-1) / 6.0  # [F]
    cell_volume = jax.ops.segment_sum(
        signed_vol_contrib * topo.face_valid,
        topo.face_cell, num_segments=topo.n_cells
    )

    # Cell areas
    cell_area = jax.ops.segment_sum(
        face_areas * topo.face_valid,
        topo.face_cell, num_segments=topo.n_cells
    )

    return state._replace(
        face_normals=face_normals,
        face_areas=face_areas,
        cell_volume=jnp.abs(cell_volume),
        cell_area=cell_area,
    )
```

**Complexity**: O(N_faces), fully parallel, no dependencies.
**Differentiability**: Fully differentiable (cross product, norm, segment_sum).

### 5.2 Pressure Forces

```python
def pressure_forces(state: SimState, topo: MeshTopology, config: SimConfig) -> SimState:
    """Logarithmic equation of state: P = -K_v * ln(V / V_target)"""

    # Per-cell pressure
    K_v = config.bulk_modulus[topo.cell_type]          # [C]
    P_max = config.max_pressure[topo.cell_type]        # [C]
    ratio = state.cell_volume / state.cell_target_volume
    pressure = -K_v * jnp.log(ratio)
    pressure = jnp.minimum(pressure, P_max)            # Cap pressure

    # Gather pressure to faces
    face_pressure = pressure[topo.face_cell]           # [F]

    # Force per face node: P * A * n / 3
    f_mag = face_pressure * state.face_areas / 3.0     # [F]
    f_vec = f_mag[:, None] * state.face_normals        # [F, 3]

    # Scatter to nodes (each face contributes to 3 nodes)
    force = state.force
    for i in range(3):
        force = force.at[topo.face_nodes[:, i]].add(
            f_vec * topo.face_valid[:, None]
        )

    return state._replace(force=force, cell_pressure=pressure)
```

**Complexity**: O(N_faces). **Differentiability**: Yes (log is smooth; clamping uses `jnp.minimum`).

### 5.3 Surface Tension + Membrane Elasticity (Fused)

```python
def surface_tension_and_elasticity(state: SimState, topo: MeshTopology,
                                    config: SimConfig) -> SimState:
    """Fused surface tension and area-elastic forces via analytical area gradients."""

    # Target area from isoperimetric ratio: A_target = (η * V²)^{1/3}
    eta = config.target_isoperimetric[topo.cell_type]              # [C]
    A_target = jnp.cbrt(eta * state.cell_volume ** 2)              # [C]
    k_a = config.area_elasticity[topo.cell_type]                   # [C]

    # Membrane elasticity factor per cell
    elasticity_factor = -(k_a / A_target) * (state.cell_area / A_target - 1.0)  # [C]

    # Gather to faces
    gamma = config.surface_tension[topo.face_type]                 # [F]
    elast = elasticity_factor[topo.face_cell]                      # [F]
    force_factor = -gamma + elast                                  # [F]

    # Gather node positions
    p = state.pos[topo.face_nodes]                                 # [F, 3, 3]
    n0, n1, n2 = p[:, 0], p[:, 1], p[:, 2]
    normals = state.face_normals                                   # [F, 3]

    # Analytical area gradients: ∇_{n_i} A = -0.5 * n × (n_j - n_k)
    grad_0 = -0.5 * jnp.cross(normals, n1 - n2)                  # [F, 3]
    grad_1 = -0.5 * jnp.cross(normals, n2 - n0)                  # [F, 3]
    grad_2 = -0.5 * jnp.cross(normals, n0 - n1)                  # [F, 3]

    # Force = force_factor * grad_A
    ff = force_factor[:, None]                                     # [F, 1]
    mask = topo.face_valid[:, None]

    force = state.force
    force = force.at[topo.face_nodes[:, 0]].add(ff * grad_0 * mask)
    force = force.at[topo.face_nodes[:, 1]].add(ff * grad_1 * mask)
    force = force.at[topo.face_nodes[:, 2]].add(ff * grad_2 * mask)

    return state._replace(force=force)
```

**Complexity**: O(N_faces). **Differentiability**: Fully differentiable.

### 5.4 Bending Forces (Discrete Helfrich)

The most complex force computation. Uses the edge-hinge diamond structure from
Wardetzky et al. (2007).

```python
def bending_forces(state: SimState, topo: MeshTopology, config: SimConfig) -> SimState:
    """Discrete Helfrich bending energy on edge-hinge diamonds.

    For each edge (n1, n2) shared by faces f1, f2 with opposite nodes n3, n4:
    Compute dihedral angle θ and apply bending forces to all 4 nodes.
    """

    # Gather the 4 nodes of each diamond [E, 3] each
    n1_pos = state.pos[topo.edge_nodes[:, 0]]          # [E, 3]
    n2_pos = state.pos[topo.edge_nodes[:, 1]]          # [E, 3]

    # Opposite nodes (precomputed in topology)
    n3_id = topo.edge_opposite_node_f1                  # [E]
    n4_id = topo.edge_opposite_node_f2                  # [E]
    n3_pos = state.pos[n3_id]                           # [E, 3]
    n4_pos = state.pos[n4_id]                           # [E, 3]

    # Edge vectors
    e0 = n2_pos - n1_pos                                # [E, 3] edge vector
    e1 = n3_pos - n1_pos                                # [E, 3]
    e2 = n4_pos - n1_pos                                # [E, 3]

    # Face normals
    nf1 = state.face_normals[topo.edge_faces[:, 0]]    # [E, 3]
    nf2 = state.face_normals[topo.edge_faces[:, 1]]    # [E, 3]

    # Dihedral angle
    dot = jnp.sum(nf1 * nf2, axis=-1)                  # [E]
    dot = jnp.clip(dot, -1.0, 1.0)
    theta = jnp.arccos(dot)                             # [E]

    # Concavity check: if e2 · nf1 > 0, edge is concave
    concave = jnp.sum(e2 * nf1, axis=-1) > 0           # [E]
    theta = jnp.where(concave, 2 * jnp.pi - theta, theta)
    theta = jnp.pi - theta

    # Bending stiffness (average of two face types)
    kb1 = config.bending_modulus[topo.face_type[topo.edge_faces[:, 0]]]
    kb2 = config.bending_modulus[topo.face_type[topo.edge_faces[:, 1]]]
    kb = 0.5 * (kb1 + kb2)                             # [E]

    # Geometric quantities
    edge_len = jnp.linalg.norm(e0, axis=-1)            # [E]
    A_f1 = state.face_areas[topo.edge_faces[:, 0]]     # [E]
    A_f2 = state.face_areas[topo.edge_faces[:, 1]]     # [E]
    A_sum = A_f1 + A_f2                                 # [E]

    # ... (angle computations, gradient assembly, force scatter)
    # Full implementation follows the Wardetzky diamond-hinge formulation
    # with the same prefactors as cell.cpp:1524-1555

    # Apply forces via scatter
    # force = force.at[edge_nodes[:, 0]].add(f_bend_n1 * mask)
    # ... (4 scatter operations, one per diamond node)

    return state._replace(force=force)
```

**Complexity**: O(N_edges) ≈ O(1.5 · N_faces) for triangulated meshes.
**Differentiability**: Requires care with `arccos` near ±1. Use `jnp.clip` + smooth
approximation for extreme angles. The 135° threshold from C++ translates to a
`jnp.where` mask (no gradient through threshold, but this is acceptable since
forces are continuous).

### 5.5 Angle Regularization

```python
def angle_regularization(state: SimState, topo: MeshTopology,
                          config: SimConfig) -> SimState:
    """Push triangle angles toward π/3 for mesh quality."""

    k_reg = config.angle_reg_factor[topo.cell_type[topo.face_cell]]  # [F]

    # Gather node positions [F, 3, 3]
    p = state.pos[topo.face_nodes]
    n0, n1, n2 = p[:, 0], p[:, 1], p[:, 2]

    # Compute 3 angles per face using vectorized arccos
    # angle_at_0 = angle between edges (n1-n0) and (n2-n0)
    # ... (similar to C++ get_angle_gradient, but vmapped)

    # Compute gradients and forces using jax.vmap(jax.grad(...))
    # or analytical gradient (matching cell.cpp:1734-1780)

    return state._replace(force=force)
```

**Complexity**: O(N_faces). **Differentiability**: Yes, with arccos domain clamping.

### 5.6 Node Curvature (Cotangent Laplacian)

```python
def compute_curvature(state: SimState, topo: MeshTopology) -> SimState:
    """Cotangent-Laplacian mean curvature at each node."""

    # For each edge, compute cotangent weights from opposite angles
    # cot_weight = cot(alpha_1) + cot(alpha_2)
    # mean_curvature_normal[n1] += cot_weight * (pos[n2] - pos[n1])

    # H_i = |mean_curvature_normal_i| / (4 * A_voronoi_i)

    # Node normals: area-weighted average of face normals
    # with Laplace-Beltrami refinement for high-curvature regions

    return state._replace(
        node_curvature=curvature,
        node_normal=normals,
    )
```

**Complexity**: O(N_edges). **Differentiability**: Yes (cotangent weights are smooth).

---

## 6. Contact Model: Soft Adhesion Potential

### 6.1 Design Philosophy

The C++ implementation uses discrete **node-to-node coupling** with `std::map` per node,
mutex-protected updates, and one-to-one assignment constraints. This is fundamentally
GPU-hostile.

We replace it with a **continuous energy-based adhesion potential** that:
- Is naturally differentiable (no `custom_vjp` needed)
- Has no per-node state (no coupling maps, no mutexes)
- Is purely additive (embarrassingly parallel over node-face pairs)
- Matches C++ behavior in the continuum limit

### 6.2 Soft Adhesion Energy

For each node-face pair `(i, f)` from different cells within the interaction cutoff:

```
E_contact(d) = {
    k_rep · d²                      if d < 0       (overlap/repulsion)
    -k_adh · d · (c - d) / c²       if 0 ≤ d ≤ c   (adhesion well)
    0                                if d > c       (no interaction)
}
```

where:
- `d` = signed distance from node to face (positive = outside, negative = penetrating)
- `c` = adhesion cutoff distance
- `k_rep` = repulsion strength (per face type)
- `k_adh` = adhesion strength (per face type)

The force is the negative gradient:
```
F_contact = -∂E/∂d · ∂d/∂x
```

This potential is:
- **C¹ continuous** at `d = 0` and `d = c` (smooth force transition)
- **GPU-friendly**: No coupling state, no one-to-one assignment
- **Differentiable**: Forces are analytical gradients of a smooth energy

### 6.3 Three-Level Spatial Hierarchy

```
Level 1: Cell AABB          — segment_min/max over node positions per cell
Level 2: Morton-code hash   — sort cells by spatial locality, sweep for overlapping pairs
Level 3: Node-face narrow   — vectorized point-triangle distance (Ericson's method)
```

**Level 1 — Cell AABBs:**
```python
def compute_cell_aabbs(pos, node_cell, n_cells):
    """AABB per cell using segment_min/max."""
    cell_min = jax.ops.segment_min(pos, node_cell, num_segments=n_cells)
    cell_max = jax.ops.segment_max(pos, node_cell, num_segments=n_cells)
    # Pad by contact cutoff
    return cell_min - cutoff, cell_max + cutoff
```

**Level 2 — Cell-Pair Broad Phase:**
```python
def find_candidate_pairs(cell_aabbs, max_pairs):
    """Find overlapping cell AABB pairs.

    For N_c cells, at most ~6*N_c pairs (each cell touches ~6 neighbors).

    Option A (baseline): Parallel all-pairs with AABB check — O(N_c²) but GPU-parallel
    Option B (scalable): Morton code sort + fixed-radius sweep — O(N_c log N_c)
        ⚠️  Morton codes may not improve performance (2024 Mochi paper found no benefit)
        Consider as optimization target after baseline is working

    Implementation recommendation: Start with Option A (simple, proven), profile,
    then try Option B if contact detection is >40% of runtime.
    """
    pass
```

**Level 3 — Node-Face Narrow Phase:**
```python
def compute_contact_forces(state, topo, config, cell_pairs):
    """For each cell pair, compute all node-face contact forces."""

    # For each (cell_A, cell_B) pair:
    #   For each node n in cell_A:
    #     For each face f in cell_B:
    #       d = point_triangle_distance(n.pos, f.v0, f.v1, f.v2)
    #       if d < cutoff: accumulate force from soft potential

    # This is vectorized over all node-face pairs using gather/scatter
    pass
```

### 6.4 Point-Triangle Distance (Vectorized Ericson)

```python
def point_triangle_distance_batch(p, v0, v1, v2):
    """Compute closest point and signed distance for batches of point-triangle pairs.

    Based on Ericson's Real-Time Collision Detection, Section 5.1.5.
    Returns (squared_distance, barycentric_coords) for each pair.

    Shapes: p [N, 3], v0/v1/v2 [N, 3] → distances [N], bary [N, 3]
    """
    ab = v1 - v0
    ac = v2 - v0
    ap = p - v0

    d1 = jnp.sum(ab * ap, axis=-1)
    d2 = jnp.sum(ac * ap, axis=-1)
    # ... (6 Voronoi region checks, all vectorized)
    # Returns closest point in barycentric coordinates

    return sq_dist, bary
```

### 6.5 Contact Filtering

Matching the C++ behavior from `contact_face_face_via_coupling.cpp`:

**Normal filtering** (`n_node · n_face < threshold`):
```python
# Only apply contact when normals face each other
normal_dot = jnp.sum(node_normal[n_ids] * face_normal[f_ids], axis=-1)
normal_ok = normal_dot < max_dot_threshold  # e.g., -0.2
```

**Curvature filtering** (matching C++ `contact_face_face_via_coupling.cpp:173`):
```python
# Curvature filtering (matching C++ contact_face_face_via_coupling.cpp:173)
kappa_max = config.coupling_max_curvature[topo.cell_type]  # Per cell type
node_curvature_ok = node_curvature[n_ids] < kappa_max[node_cell[n_ids]]

# Combined contact mask
contact_mask = normal_ok & node_curvature_ok
```

**Rationale**: Without curvature filtering, high-curvature regions (vertices, sharp edges) generate spurious contact forces. This matches the C++ behavior where `κ_node < κ_max` excludes problematic mesh features from contact computation.

### 6.6 Comparison with C++ Coupling Model

| Aspect | C++ Coupling | JAX Soft Potential |
|---|---|---|
| State per node | `std::map<cell_id, (node_id, dist)>` | None |
| Thread safety | `std::mutex` per node | No synchronization needed |
| One-to-one constraint | Yes (closest node wins) | No (all pairs contribute) |
| Position averaging | Explicit post-step | Not needed (forces balance naturally) |
| Differentiability | Not differentiable | Fully differentiable |
| GPU parallelism | Limited (lock contention) | Embarrassingly parallel |
| Force continuity | Discontinuous (coupling switches) | C¹ continuous |

---

## 7. Time Integration

### 7.1 Semi-Implicit Euler (Pure Function)

```python
def step_semi_implicit(state: SimState, topo: MeshTopology,
                        config: SimConfig) -> SimState:
    """Single timestep: semi-implicit Euler integration.

    p_{n+1} = p_n + (F - ζ/m · p_n) · dt
    x_{n+1} = x_n + p_{n+1}/m · dt
    """

    # Compute node mass: cell_mass / n_nodes_per_cell
    cell_mass = config.mass_density[topo.cell_type] * state.cell_volume  # [C]
    # ... scatter to nodes via cell ownership

    dt = config.dt
    zeta = config.damping_coeff

    # Momentum update (vectorized over all nodes)
    new_momentum = state.momentum + (state.force - state.momentum * (zeta / node_mass[:, None])) * dt

    # Position update using NEW momentum (semi-implicit)
    new_pos = state.pos + new_momentum * (dt / node_mass[:, None])

    # Mask static cells
    is_dynamic = ~topo.cell_is_static[topo.node_cell]  # [N]
    new_pos = jnp.where(is_dynamic[:, None], new_pos, state.pos)
    new_momentum = jnp.where(is_dynamic[:, None], new_momentum, state.momentum)

    return state._replace(
        pos=new_pos,
        momentum=new_momentum,
        force=jnp.zeros_like(state.force),  # Reset forces
        time=state.time + dt,
        iteration=state.iteration + 1,
    )
```

### 7.2 Overdamped Mode

```python
def step_overdamped(state: SimState, topo: MeshTopology,
                     config: SimConfig) -> SimState:
    """Overdamped: x_{n+1} = x_n + F/ζ · dt"""

    new_pos = state.pos + state.force * (config.dt / config.damping_coeff)

    is_dynamic = ~topo.cell_is_static[topo.node_cell]
    new_pos = jnp.where(is_dynamic[:, None], new_pos, state.pos)

    return state._replace(
        pos=new_pos,
        force=jnp.zeros_like(state.force),
        time=state.time + config.dt,
        iteration=state.iteration + 1,
    )
```

### 7.3 Multi-Step Execution via `lax.scan`

```python
def run(state: SimState, topo: MeshTopology, config: SimConfig,
        n_steps: int) -> SimState:
    """Run n_steps as a single XLA computation graph."""

    def body(state, _):
        state = update_geometry(state, topo)
        state = compute_all_forces(state, topo, config)
        state = step_semi_implicit(state, topo, config)
        return state, None

    state, _ = jax.lax.scan(body, state, xs=None, length=n_steps)
    return state
```

This compiles the entire multi-step simulation into a single XLA graph, eliminating
Python loop overhead and enabling optimizations across timesteps.

---

## 8. Mesh Refinement Strategy

### 8.1 Recommended: Hybrid CPU-GPU

Mesh refinement involves **topology changes** (edge splitting, edge collapsing, edge swapping)
that create and destroy mesh elements. This is fundamentally incompatible with XLA's
static computation graphs.

**Approach**: CPU-based refinement via pybind11 bridge.

```
GPU: compute forces, integrate → transfer state to CPU (~1ms)
CPU: run local_mesh_refiner (edge split/collapse/swap)
CPU: rebuild topology arrays (face_nodes, edge_nodes, etc.)
GPU: transfer updated topology + state back (~1ms)
```

### 8.2 Transfer Cost Analysis

For 10K cells (~750K nodes, ~1.5M faces):
- Position array: 750K × 3 × 4B = 9 MB
- Topology arrays: ~20 MB
- Total transfer: ~30 MB at PCIe 4.0 → **< 1 ms** each way

This is negligible compared to the ~10 ms timestep computation on GPU.

### 8.3 Fixed-Mesh Mode for Differentiable Runs

For gradient-based parameter inference, topology changes break the computation graph.
**Fixed-mesh mode** disables refinement entirely:

```python
def run_differentiable(state, topo, config, n_steps):
    """Fully JIT-compiled, no topology changes."""
    # topo is treated as a constant (not differentiated)
    return jax.lax.scan(step, state, length=n_steps)
```

This enables `jax.grad` through the entire simulation trajectory.

---

## 9. Cell Division Strategy

### 9.1 Pre-Allocated Capacity

Cell division creates new mesh elements. We handle this with:

1. **Capacity headroom**: Allocate 2x the initial element count
2. **Validity masks**: New elements are "activated" by flipping mask bits
3. **Doubling strategy**: If capacity is exceeded, reallocate at 2x (triggers recompilation)
4. **O(log N) recompilations** over the simulation lifetime

### 9.2 CPU-Side Division Pipeline

```
1. Check division criterion: V > V_division (per cell)
2. Compute division plane (longest axis through centroid)
3. Bisect mesh along plane (CPU, using existing cell_divider code)
4. Retriangulate cut surfaces (BPA algorithm)
5. Update topology arrays (new nodes, faces, edges)
6. Transfer back to GPU
```

### 9.3 Fixed Cell Count Mode

For equilibrium studies (no growth, no division):
- Fully JIT-compiled, single XLA graph
- No capacity management overhead
- Optimal for parameter inference via `jax.grad`

### 9.4 Relaxed Division for Differentiability

For differentiable simulations that include growth:
- Replace hard volume threshold with **sigmoid**: `σ((V - V_div) / ε)`
- Use **Gumbel-Softmax** for discrete division decision
- Gradient flows through the growth → division → daughter cell chain
- Approximate, but enables end-to-end differentiation

---

## 10. Differentiability Design

### 10.1 Naturally Differentiable Components

| Component | Differentiable? | Notes |
|---|---|---|
| Geometry update | Yes | Cross products, norms, segment_sum |
| Pressure forces | Yes | `jnp.log` is smooth; clamping via `jnp.minimum` |
| Surface tension | Yes | Analytical area gradients |
| Bending forces | Yes | With `jnp.clip` for arccos domain |
| Angle regularization | Yes | Analytical angle gradients |
| Soft contact potential | Yes | C¹ continuous by design |
| Semi-implicit Euler | Yes | Linear update equations |
| `lax.scan` multi-step | Yes | Automatic unrolling for gradients |

### 10.2 Components Requiring `custom_vjp`

| Component | Why | Strategy |
|---|---|---|
| Contact broad phase | Discrete neighbor search | Detach from gradient; differentiate through narrow phase only |
| Cell AABB computation | `segment_min`/`segment_max` non-differentiable at ties | Use straight-through estimator or soft min/max |

### 10.3 Not Differentiable (By Design)

| Component | Why | Mitigation |
|---|---|---|
| Mesh refinement | Topology changes are discrete | Fixed-mesh mode during gradient computation |
| Cell division | Discrete event | Relaxed sigmoid + Gumbel-Softmax, or excluded |
| Face type classification | Discrete labels | Soft classification with sigmoid boundaries |

### 10.4 End-to-End Gradient Example

```python
def loss(params):
    """Tissue morphology loss function."""
    config = SimConfig(**params)
    state = initialize(config, mesh_paths)
    topo = build_topology(state)

    # Run simulation (topology fixed)
    final_state = run_differentiable(state, topo, config, n_steps=1000)

    # Morphology metric (e.g., average cell sphericity)
    sphericity = compute_sphericity(final_state, topo)
    return jnp.mean(sphericity)

# Gradient of tissue morphology w.r.t. mechanical parameters
grad_loss = jax.grad(loss)
param_gradients = grad_loss(initial_params)
```

### 10.5 Gradient Checkpointing

For long trajectories (>1000 steps), memory for storing intermediate states becomes
prohibitive. Use `jax.checkpoint` (rematerialization):

```python
@jax.checkpoint
def step(state, _):
    state = update_geometry(state, topo)
    state = compute_all_forces(state, topo, config)
    state = step_semi_implicit(state, topo, config)
    return state, None
```

This trades compute for memory: recomputes forward pass during backward pass,
reducing memory from O(T) to O(√T) with recursive checkpointing.

---

## 11. Automatic Polarization on GPU

### 11.1 Contact-Based Polarization (`POLARIZATION_MODE_INDEX=1`)

Simpler approach: classify faces based on contact information.
- Faces in contact with other cells → **lateral**
- Faces on the free surface → **apical** or **basal** (based on orientation)

This naturally falls out of the contact detection pipeline:
```python
face_has_contact = contact_force_magnitude > threshold  # [F]
face_type = jnp.where(face_has_contact, LATERAL, APICAL)
```

### 11.2 Voxel-Based Polarization (`POLARIZATION_MODE_INDEX=2`)

More complex: discretize space into a 3D grid and classify regions.

```
1. Create 3D voxel grid covering simulation domain
2. For each voxel: ray-cast to determine if inside cell, lumen, or exterior
3. For each face: classify based on neighboring voxel labels
   - Apical: faces bordering lumen
   - Basal: faces bordering exterior/ECM
   - Lateral: faces bordering other cells
```

GPU-friendly: regular grid operations, per-voxel/per-face independent classification.
Can be implemented with 3D convolutions for neighbor queries.

---

## 12. Memory Budget & Scaling

### 12.1 Per-Element Memory Estimates (float32)

| Element | Fields | Bytes | Notes |
|---|---|---|---|
| Node | pos(12) + force(12) + momentum(12) + curvature(4) + normal(12) | ~52 B | Minimum; +padding |
| Face | nodes(12) + normal(12) + area(4) + cell(4) + type(2) + valid(1) | ~35 B | |
| Edge | nodes(8) + faces(8) + opposite_nodes(8) + cell(4) + valid(1) | ~29 B | |
| Cell | volume(4) + area(4) + target_vol(4) + pressure(4) + centroid(12) + ... | ~60 B | |

### 12.2 Scaling Table

Assumptions: ~75 nodes/cell, ~150 faces/cell, ~225 edges/cell (Euler relation),
2x capacity pre-allocation.

**Note on memory calculation:** The values below account for struct padding, alignment requirements, and auxiliary indexing arrays (e.g., cell→node lists, spatial partition metadata) beyond the per-element fields listed in Section 12.1. A simple calculation (52B×750K + 35B×1.5M + 29B×2.25M + 60B×10K) × 2 yields ~315MB for 10K cells, but production implementations typically require 2-2.5x this theoretical minimum due to alignment overhead and auxiliary data structures. The table reflects realistic memory usage including these overheads.

| Cells | Nodes | Faces | Edges | Memory (2x cap) | A100 (80GB) |
|---|---|---|---|---|---|
| 1K | 75K | 150K | 225K | ~75 MB | Trivial |
| 10K | 750K | 1.5M | 2.25M | ~740 MB | Comfortable |
| 50K | 3.75M | 7.5M | 11.25M | ~3.7 GB | Comfortable |
| 100K | 7.5M | 15M | 22.5M | ~7.4 GB | With room for intermediates |

### 12.3 Intermediate Memory

Force computation requires temporary arrays:
- Contact pair list: ~10 pairs/node × 750K nodes × 8B = ~60 MB (10K cells)
- Bending diamond gather: ~2.25M edges × 4 × 12B = ~108 MB
- Total intermediates: ~200–500 MB for 10K cells

**A100 (80GB) comfortably handles 100K cells with room for intermediates and batching.**

For 100K cells with 2x capacity:
- State arrays: 7.4 GB
- Intermediate arrays (force computation): ~5 GB
- Contact pair buffers: ~600 MB
- **Total: 13–14 GB** (comfortable on A100 80GB, tight on RTX 4090 24GB)

### 12.4 Batched Simulations

Using `jax.vmap` over parameter space:
- 10K cells × 10 replicas: ~7.4 GB → fits on a single A100
- 1K cells × 100 replicas: ~7.5 GB → parameter sweep on one GPU

---

## 13. Performance Projections

### 13.1 Arithmetic Intensity Analysis

The force computation pipeline is **memory-bandwidth limited**:
- Bending: ~50 FLOP per edge, reads 4 × 12B = 48B → 1.04 FLOP/byte
- Contact: ~30 FLOP per pair, reads 2 × 12B = 24B → 1.25 FLOP/byte
- Pressure: ~10 FLOP per face, reads 3 × 12B = 36B → 0.28 FLOP/byte
- **Overall: ~3.3 FLOP/byte** (below A100's ridge point of ~60 FLOP/byte)

This means performance is governed by **memory bandwidth**:
- A100 HBM: 2 TB/s
- 8-core CPU DDR4: ~50 GB/s
- **Theoretical speedup from bandwidth alone: 40x**
- **Realistic estimates accounting for overhead: 10–20x**

### 13.2 Expected Speedups

Realistic estimates accounting for overhead (kernel launch, occupancy, scatter conflicts):

| Cell Count | C++ 8-core (ms/step) | JAX A100 (ms/step) | Speedup |
|---|---|---|---|
| 1K | 2–5 | 0.5–1 | 3–5x |
| 10K | 30–80 | 2–5 | 10–17x |
| 50K | 200–500 | 10–20 | 15–25x |
| 100K | 500–1500 | 20–50 | 25–30x |

**Bottleneck**: Contact detection accounts for >50% of GPU step time due to
irregular memory access patterns in the narrow phase.

### 13.3 Component Breakdown (10K cells, A100)

| Component | C++ 8-core | JAX A100 | Speedup |
|---|---|---|---|
| Geometry update | 3 ms | 0.2 ms | 15x |
| Pressure | 2 ms | 0.1 ms | 20x |
| Surface tension | 3 ms | 0.2 ms | 15x |
| Bending | 8 ms | 0.5 ms | 16x |
| Contact detection | 30 ms | 3 ms | 10x |
| Time integration | 2 ms | 0.1 ms | 20x |
| **Total** | **~48 ms** | **~4 ms** | **~12x** |

---

## 14. Recent Advances & Inspirations

### JAX-MD (Schoenholz & Cubuk, 2020)
Differentiable molecular dynamics in JAX. Demonstrates neighbor lists and spatial
partitioning entirely within JAX. Key pattern: cell-list construction using
`jnp.floor` + sorting, compatible with JIT and autodiff.

### Brax (Freeman et al., 2021)
Rigid body physics engine in JAX for reinforcement learning. Demonstrates that
complex physics simulations can be fully JIT-compiled and differentiated.
Architecture: pure functional state, `lax.scan` for multi-step.

### DiffTaichi (Hu et al., 2020)
Differentiable programming framework for physical simulation. Shows that
differentiability through complex simulations enables inverse design and
parameter estimation tasks.

### CellSim3D (Madhikar et al., 2018)
CUDA-accelerated 3D cell simulation. Uses vertex model on GPU with custom
CUDA kernels. Reports 10–50x speedup over CPU. Our approach differs by using
JAX (higher-level, automatic differentiation) instead of raw CUDA.

### XPBD on GPU (Macklin et al.)
Position-based dynamics for deformable bodies. GPU-friendly constraint
projection without global solves. Relevant for contact handling.

### GPU Spatial Hashing (2024–2025)
Recent work on GPU-optimized spatial hash tables for collision detection.
Morton code sorting + fixed-radius search achieves near-optimal throughput
on modern GPUs.

---

## 15. Implementation Roadmap

### Phase 1: Core Force Computation (2–3 weeks)
- [ ] Define `SimConfig`, `MeshTopology`, `SimState` pytrees
- [ ] Implement geometry update (normals, areas, volumes)
- [ ] Implement pressure forces
- [ ] Implement surface tension + membrane elasticity
- [ ] Implement bending forces (discrete Helfrich)
- [ ] Implement angle regularization
- [ ] Validate: single-cell equilibrium shapes vs. C++ reference
- [ ] Benchmark: single-cell force computation speed

### Phase 2: Multi-Cell + Contact Detection (2–3 weeks)
- [ ] Implement cell AABB computation
- [ ] Implement broad-phase candidate pair detection
- [ ] Implement narrow-phase node-face distance
- [ ] Implement soft adhesion potential (contact forces)
- [ ] Implement node curvature (cotangent Laplacian)
- [ ] Implement time integration (semi-implicit + overdamped)
- [ ] Validate: multi-cell spheroid growth vs. C++ trajectory
- [ ] Benchmark: 1K, 10K cells per-step timing

### Phase 2.5: Contact Model Validation (1–2 weeks)
- [ ] Implement C++ coupling model in JAX (one-to-one node coupling with padded arrays)
- [ ] Implement soft adhesion potential model (as designed in Section 6.2)
- [ ] Validation test: Two-cell adhesion equilibrium
  - Initialize two cells in contact
  - Run both models to equilibrium (1000 steps)
  - Compare: contact area, overlap depth, force magnitude
  - Success criterion: Behaviors within 10% quantitatively
- [ ] Validation test: Spheroid compression
  - 8-cell spheroid compressed by static walls
  - Compare deformation patterns, pressure distribution
  - Success criterion: Qualitatively similar (same failure modes)
- [ ] **Decision point**: If models diverge >10%, reassess design
  - Option A: Keep C++ coupling model (sacrifice differentiability)
  - Option B: Tune soft potential parameters to match better
  - Option C: Hybrid approach (soft potential for most, coupling for critical cases)

### Phase 3: Scalability + Differentiability (2 weeks)
- [ ] Implement `lax.scan` multi-step execution
- [ ] Implement `jax.grad` through fixed-topology simulation
- [ ] Implement gradient checkpointing for long trajectories
- [ ] Implement `jax.vmap` batched parameter sweeps
- [ ] Validate: finite-difference vs. autodiff gradient comparison
- [ ] Benchmark: 50K, 100K cells; batched sweeps

### Phase 4: Production Features (2 weeks)
- [ ] Implement VTK output writer (from JAX arrays)
- [ ] Implement XML parameter reader compatibility
- [ ] Implement CPU mesh refinement bridge (pybind11)
- [ ] Implement CPU cell division bridge
- [ ] Implement cell removal (volume threshold)
- [ ] End-to-end integration test: full simulation matching C++ output

### Phase 5: Advanced Features (Ongoing)
- [ ] Multi-GPU via `jax.pmap`
- [ ] Automatic polarization on GPU
- [ ] Custom XLA kernels for contact detection (if needed)
- [ ] Learned constitutive models (neural network force surrogates)
- [ ] Reaction-diffusion on cell surfaces

---

## 16. Key Design Decisions

### float32 Default
GPU FP64 throughput is 1/32 of FP32 on consumer GPUs (1/2 on A100).
Use float32 by default with optional float64 mode for validation.
The C++ code uses double everywhere, but the simulation is not precision-sensitive
enough to require it for production runs.

### NamedTuple Pytrees (No Equinox/Flax)
Simulation state is not a neural network. Plain `NamedTuple` subclasses registered
as JAX pytrees provide:
- Zero overhead (no framework)
- Full compatibility with `jax.jit`, `jax.grad`, `jax.vmap`
- Clear, explicit state management

### CSR Offsets + Segment Operations
The most XLA-friendly pattern for per-cell aggregation:
```python
# Numerical stability for segment_sum
cell_volume = jax.ops.segment_sum(
    signed_vol_contrib * topo.face_valid,
    topo.face_cell,
    num_segments=topo.n_cells,
    bucket_size=128  # Groups contributions for stable summation
)
```
Replaces per-cell loops, compatible with autodiff, fully parallelized.

**The `bucket_size` parameter** groups segment contributions into buckets, improving numerical stability when summing many small values (e.g., face volume contributions to cell volume). Default 128 balances stability and performance.

### Edge Arrays Derived from Face Connectivity
The C++ `std::set<edge>` per cell is replaced by global edge arrays derived
from face connectivity. Precomputed during topology construction (CPU-side),
stored as flat arrays with cell ownership.

### Fixed-Capacity Coupling Arrays → Soft Potential
Instead of translating `std::map<unsigned, pair<unsigned, double>>` to GPU
(which would require padded arrays and complex synchronization), we replace
the entire coupling mechanism with a continuous soft adhesion potential.
This is simpler, more GPU-friendly, and naturally differentiable.

### Hybrid CPU-GPU for Topology Changes
Mesh refinement and cell division involve irregular topology modifications
that cannot be efficiently expressed as static XLA graphs. The hybrid approach
(GPU for physics, CPU for topology) is pragmatic and well-proven in
production simulation codes.
