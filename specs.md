# SimuCell3D-JAX: System Specification

## 1. Project Overview

**SimuCell3D-JAX** is a GPU-accelerated, differentiable 3D tissue mechanics simulator.
It is a ground-up reimplementation of SimuCell3D (*Nature Computational Science*, 2024)
in JAX, targeting:

- **10–20x speedup** over the 8-core C++ implementation on NVIDIA A100
- **Gradient-based parameter inference** via `jax.grad` through the full simulation
- **Batched parameter sweeps** via `jax.vmap` for sensitivity analysis
- **100K+ cell scale** with single-GPU memory budget ~13–15 GB (state + intermediates + contact pairs)

SimuCell3D models 3D tissues as collections of deformable cells, each represented by a
closed triangulated surface mesh. Cells interact mechanically through pressure, surface
tension, membrane elasticity, bending resistance, and contact adhesion/repulsion.
The simulator supports cell growth, division, and multiple cell types (epithelial,
lumen, nucleus, ECM).

---

## 2. Biophysical Model Specification

### 2.1 Cell Representation

Each cell is a **closed triangulated surface mesh** satisfying:
- Euler characteristic χ = 2 (V - E + F = 2, genus-0 topology)
- All edges manifold (shared by exactly 2 faces)
- Outward-facing normals (consistent winding order)
- Typical mesh: ~75 nodes, ~150 faces, ~225 edges per cell

### 2.2 Energy Functional

The total energy of the tissue is:

```
U = Σ_cells [ U_pressure + U_surface + U_elasticity + U_bending + U_angle ] + U_contact
```

| Term | Formula | Parameters |
|---|---|---|
| Pressure | U_p = K_v · V · (ln(V/V₀) − 1) | K_v: bulk modulus, V₀: target volume |
| Surface tension | U_γ = Σ_f γ_f · A_f | γ_f: surface tension per face type |
| Membrane elasticity | U_a = (k_a/A₀) · (A/A₀ − 1)² | k_a: area elasticity, A₀ = (η·V²)^{1/3} |
| Bending | U_b = Σ_e k_b · \|e\|²/(A₁+A₂) · (2cos(θ/2))² | k_b: bending modulus, θ: dihedral angle |
| Angle regularization | U_r = k_r · Σ_f Σᵢ (π/3 − αᵢ)² | k_r: regularization factor |
| Contact (v1) | F_rep = k_rep · A_f · (p−q) | Spring-based repulsion |
| Contact (v2) | E = ω·f(d), smooth adhesion potential | Continuous, differentiable |

### 2.3 Force Contributions

All forces are computed as negative gradients of the energy functional:

1. **Pressure** — Logarithmic equation of state, distributed equally to face nodes
2. **Surface tension + membrane elasticity** — Fused computation via analytical area gradients
3. **Bending** — Discrete Helfrich energy on edge-hinge diamonds (Wardetzky et al. 2007)
4. **Angle regularization** — Pushes triangle angles toward π/3 for mesh quality
5. **Node curvature** — Cotangent Laplacian for mean curvature and surface normals
6. **Contact repulsion** — Prevents surface interpenetration
7. **Contact adhesion** — Mechanical coupling of adjacent cell surfaces

### 2.4 Contact Models

**Spring-based (v1, reference):**
- Node-face distance → linear spring force
- Simple, GPU-friendly, no coupling state
- Matches `CONTACT_MODEL_INDEX=0` in C++

**Soft adhesion potential (v2, primary):**
- Continuous energy E(d) with repulsive (d<0) and adhesive (0<d<c) regions
- Naturally differentiable — no custom_vjp needed
- Replaces C++ face-face coupling (CONTACT_MODEL_INDEX=2)
- No per-node coupling state, no mutexes, purely additive forces
- Includes normal filtering (face-to-face orientation) and curvature filtering (κ < κ_max) matching C++ behavior

### 2.5 Time Integration

| Scheme | Equations | Use Case |
|---|---|---|
| Semi-implicit Euler | p' = p + (F − ζ/m·p)·dt; x' = x + p'/m·dt | Full dynamics (inertia + damping) |
| Overdamped Euler | x' = x + F/ζ·dt | Quasi-static equilibrium |

**Coupled node handling**: For contact coupling, forces and momenta are averaged across
all coupled nodes before integration (matching C++ behavior).

### 2.6 Cell Growth and Division

- **Growth**: Linear target volume increase: V₀(t+dt) = V₀(t) + g·dt
- **Division criterion**: V > V_division (drawn from normal distribution per cell)
- **Division plane**: Perpendicular to cell longest axis (PCA), through centroid
- **Triangulation**: Ball-Pivoting Algorithm (BPA) for cut surface reconstruction
- **Daughter cells**: Inherit half the parent volume, new growth rate from distribution

### 2.7 Automatic Polarization

| Mode | Method | Complexity |
|---|---|---|
| None (mode 0) | No face classification | — |
| Contact-based (mode 1) | Faces in contact → lateral; free surface → apical/basal | O(N_faces) |
| Space-discretized (mode 2) | 3D voxel grid + ray casting for region classification | O(N_voxels) |

---

## 3. System Architecture

### 3.1 Design Principles

1. **Pure functional**: State in → state out, no mutation
2. **Pytree state**: All data as `NamedTuple` subclasses, JAX-compatible
3. **Struct-of-Arrays**: Flat global arrays with CSR cell ownership offsets
4. **Single XLA graph**: Entire force+integrate step compiles to one GPU kernel
5. **Hybrid CPU-GPU**: Physics on GPU, topology changes on CPU

### 3.2 Three-Layer State

```
SimConfig    — Immutable parameters (created once from XML)
MeshTopology — Connectivity arrays (updated only during refinement/division)
SimState     — Dynamic state (positions, forces, momenta — updated every step)
```

### 3.3 Data Layout

**Global flat arrays with CSR cell ownership:**
```
node_positions: [max_nodes, 3]     — all nodes across all cells
face_nodes:     [max_faces, 3]     — global node IDs per face
face_cell:      [max_faces]        — owning cell ID per face
node_cell:      [max_nodes]        — owning cell ID per node
cell_offsets:   [max_cells + 1]    — CSR offsets for per-cell aggregation
```

Per-cell aggregation uses `jax.ops.segment_sum/min/max`:
```python
cell_volume = segment_sum(face_volume_contrib, face_cell, num_segments=n_cells)
```

**Note**: Face arrays are shuffled within cells to optimize GPU segment operations (avoids sorted-indices performance penalty in `segment_sum`, see JAX Issue #26227).

### 3.4 Computation Flow

```
Per timestep:
  1. update_geometry()              — normals, areas, volumes [GPU, O(F)]
  2. pressure_forces()              — logarithmic EOS [GPU, O(F)]
  3. surface_tension_elasticity()   — fused area forces [GPU, O(F)]
  4. bending_forces()               — discrete Helfrich [GPU, O(E)]
  5. angle_regularization()         — mesh quality [GPU, O(F)]
  6. compute_curvature()            — cotangent Laplacian [GPU, O(E)]
  7. contact_detection()            — spatial hash + narrow phase [GPU, O(N^{4/3})]
  8. contact_forces()               — soft adhesion potential [GPU, O(pairs)]
  9. time_integrate()               — semi-implicit Euler [GPU, O(N)]

Every 5 timesteps (CPU bridge for topology changes):
  10. mesh_refinement()             — edge split/collapse/swap [CPU]
  11. cell_division()               — bisection + BPA [CPU]
  12. output_writing()              — VTK mesh files [CPU]
```

---

## 4. Computational Requirements

### 4.1 Target Hardware

| Tier | GPU | HBM | Use Case |
|---|---|---|---|
| Primary | NVIDIA A100 | 80 GB | Research, large-scale runs |
| Secondary | NVIDIA H100 | 80 GB | Maximum performance |
| Development | NVIDIA RTX 4090 | 24 GB | Development, small-scale testing |

### 4.2 Scale Targets

| Scale | Cells | Nodes | Memory | Target Performance |
|---|---|---|---|---|
| Small | 1K | 75K | ~75 MB | Real-time (< 1 ms/step) |
| Medium | 10K | 750K | ~740 MB | Interactive (< 5 ms/step) |
| Large | 50K | 3.75M | ~3.7 GB | Research (< 20 ms/step) |
| Very large | 100K | 7.5M | ~7.4 GB | Research (< 50 ms/step) |

**Note:** Memory values include 2x capacity pre-allocation for dynamic topology changes, plus overhead for struct padding, alignment, and auxiliary indexing arrays (cell→node lists, spatial partition metadata). Values represent realistic production memory usage, not theoretical minimums. Intermediate arrays for force computation add 200-500 MB for 10K cells (see gpu.md Section 12.3).

### 4.3 Precision

- **Default**: float32 (GPU-optimized, sufficient for tissue mechanics)
- **Optional**: float64 for validation against C++ reference
- **Integer IDs**: uint32 (supports up to 4 billion elements)

---

## 5. Key Capabilities

### 5.1 GPU Acceleration
- 10–20x speedup over 8-core C++ for 10K–100K cells
- Single XLA graph per timestep eliminates kernel launch overhead
- Memory-bandwidth limited (3.3 FLOP/byte) → GPU bandwidth advantage is decisive

### 5.2 Differentiability
- `jax.grad` through the full simulation for parameter inference
- Gradient checkpointing for long trajectories (>1000 steps)
- Soft contact potential enables differentiation through contact forces
- Fixed-mesh mode for topology-stable gradient computation

### 5.3 Batched Simulation
- `jax.vmap` over parameter space for sensitivity analysis
- ~10 replicas of 10K cells on a single A100 (80 GB)
- ~100 replicas of 1K cells for rapid parameter sweeps

### 5.4 Multi-GPU (Future)
- `jax.pmap` for data-parallel simulation across GPUs
- Spatial domain decomposition for model-parallel scaling
- Requires contact force communication at domain boundaries

### 5.5 Interoperability
- Python API (direct JAX arrays)
- VTK output for visualization (ParaView compatible)
- XML parameter file compatibility with C++ version
- pybind11 bridge for CPU-side mesh operations

---

## 6. API Surface

### 6.1 Configuration

```python
# Load parameters from XML (C++ compatible format)
config = SimConfig.from_xml("parameters/core/parameters_default_dynamic.xml")

# Or construct programmatically
config = SimConfig(
    dt=1e-9,
    damping_coeff=5e-10,
    bulk_modulus=jnp.array([1e4]),
    surface_tension=jnp.array([1e-3]),
    # ...
)
```

### 6.2 Initialization

```python
# Initialize from VTK mesh files
state, topo = initialize_from_vtk(config, mesh_paths=["cell_0.vtk", "cell_1.vtk", ...])

# Or from programmatic mesh specification
state, topo = initialize_from_meshes(config, node_positions, face_indices, cell_types)
```

### 6.3 Simulation

```python
# Single timestep
new_state = step(state, topo, config)

# Multi-step via lax.scan (single XLA graph)
final_state = run(state, topo, config, n_steps=10000)

# With periodic output callback
final_state = run_with_output(state, topo, config, n_steps=10000,
                               output_every=100, output_dir="results/")
```

### 6.4 Differentiation

```python
# Gradient of morphology metric w.r.t. parameters
def loss(params):
    config = SimConfig(**params)
    state, topo = initialize(config)
    final = run(state, topo, config, n_steps=1000)
    return morphology_metric(final, topo)

gradients = jax.grad(loss)(initial_params)
```

### 6.5 Batched Parameter Sweeps

```python
# Vectorize over parameter space
def simulate_one(bulk_modulus):
    config = config_template._replace(bulk_modulus=bulk_modulus)
    state, topo = initialize(config)
    return run(state, topo, config, n_steps=1000)

# Run 100 simulations in parallel
bulk_moduli = jnp.linspace(1e3, 1e5, 100)
results = jax.vmap(simulate_one)(bulk_moduli)
```

### 6.6 Mesh Refinement Bridge

```python
# CPU-side refinement (called every K steps)
state, topo = refine_meshes_cpu(state, topo, config)

# CPU-side cell division (called every 5 steps)
state, topo = divide_cells_cpu(state, topo, config)
```

---

## 7. Validation Strategy

### 7.1 Unit Tests

Each force computation is validated against the C++ reference implementation:

| Test | Method | Tolerance |
|---|---|---|
| Face normal / area | Compare per-face values | < 1e-6 (float64) |
| Cell volume | Divergence theorem vs. C++ | < 1e-6 |
| Pressure force | Per-node force comparison | < 1e-5 |
| Surface tension force | Per-node force comparison | < 1e-5 |
| Bending force | Per-node force on standard hinges | < 1e-4 |
| Angle regularization | Per-node force comparison | < 1e-5 |
| Cotangent curvature | Per-node H values | < 1e-5 |
| Contact distance | Point-triangle Ericson | < 1e-8 |

### 7.2 Integration Tests

| Test | Description | Success Criterion |
|---|---|---|
| Single-cell equilibrium | Sphere relaxation under pressure | Volume converges within 1% |
| Pressure-area balance | Cell with known analytical solution | Forces sum to zero at equilibrium |
| Two-cell adhesion | Two cells in contact | Equilibrium separation matches C++ |
| Bending relaxation | High-curvature → sphere | Converges to minimum energy shape |

### 7.3 Multi-Cell Tests

| Test | Description | Success Criterion |
|---|---|---|
| Spheroid growth | 8 cells → ~100 cells | Volume doubling time within 15% of C++ |
| Contact forces | 27-cell cube | Contact area fraction within 10% of C++ |
| Cell division | Growing tissue with divisions | Division timing within 10% of C++ |
| Polarization | Epithelial monolayer | Face types match C++ classification |

**Note**: Validation targets allow ±10–15% tolerance due to different contact model physics (soft potential vs discrete coupling). Exact matching is not expected, but behaviors should be qualitatively similar.

### 7.4 Gradient Validation

| Test | Method | Tolerance |
|---|---|---|
| Force gradients | Finite difference vs. autodiff | Relative error < 1e-3 |
| Multi-step gradients | Finite difference vs. scan grad | Relative error < 1e-2 |
| Parameter sensitivity | Known analytical derivatives | Matches within 5% |

### 7.5 Performance Benchmarks

| Scale | Metric | Target |
|---|---|---|
| 1K cells | ms/step | < 1 ms |
| 10K cells | ms/step | < 5 ms |
| 50K cells | ms/step | < 20 ms |
| 100K cells | ms/step | < 50 ms |
| 10K × 10 batch | ms/step | < 50 ms |

---

## 8. Future Biological Extensions (Enabled by GPU)

The JAX architecture enables biological extensions that would be impractical in the
C++ implementation due to computational cost or differentiability requirements.

### 8.1 Reaction-Diffusion on Cell Surfaces
Solve diffusion equations on the triangulated mesh using FEM/FVM:
```
∂c/∂t = D · ΔS c + R(c)
```
where `ΔS` is the Laplace-Beltrami operator on the cell surface.
The cotangent Laplacian already computed for curvature serves as the discrete operator.

### 8.2 Active Cell Motility
Add persistent random forces for cell crawling and migration:
```
F_motility = F₀ · p(t)    where dp/dt = -p/τ + σ·ξ(t)
```
where `p` is the polarity vector with persistence time `τ`.

### 8.3 Morphogen Gradient Signaling
Solve diffusion in the extracellular space:
```
∂c/∂t = D · ∇²c - λ·c + S(x)
```
Discretize on a regular 3D grid (GPU-native), couple to cell surface via interpolation.

### 8.4 Cell-Cell Communication
Gap junctions and paracrine signaling:
- Gap junctions: direct transfer between coupled faces
- Paracrine: secretion → diffusion → receptor binding
Both leverage the contact detection infrastructure.

### 8.5 Nutrient-Limited Growth
Couple cell growth rate to local nutrient concentration:
```
g(c) = g_max · c / (c + K_m)    [Michaelis-Menten kinetics]
```
Requires extracellular diffusion solver (see 8.3).

### 8.6 Stochastic Langevin Dynamics
Add thermal fluctuation forces:
```
F_thermal = √(2·k_B·T·ζ/dt) · ξ(t)
```
Natural in JAX: `jax.random.normal` + force accumulation.

### 8.7 Learned Constitutive Models
Replace analytical force laws with neural network surrogates:
```python
force = neural_net(local_geometry, params)  # Differentiable by construction
```
Train on high-resolution simulations, deploy for fast large-scale runs.

### 8.8 Continuum Coupling (Multiscale)
Bridge cell-level DCM with continuum tissue mechanics:
- Cells provide local stress tensors → continuum solver
- Continuum provides boundary conditions → cell simulation
- Enables whole-organ simulation with cellular resolution in regions of interest

---

## 9. Non-Goals (v1.0)

The following are explicitly **out of scope** for the initial release:

| Non-Goal | Rationale |
|---|---|
| GPU-based mesh refinement | CPU bridge is sufficient; topology changes are infrequent |
| GPU-based cell division | CPU bridge is sufficient; division is rare |
| Multi-GPU with topology changes across devices | Requires complex distributed topology management |
| Fluid dynamics coupling | Requires a full CFD solver; future work |
| Real-time visualization | Separate rendering pipeline; VTK output is sufficient |
| GUI / interactive parameter tuning | Command-line + Jupyter notebook interface is sufficient |
| Backward compatibility with C++ binary format | VTK and XML formats provide interoperability |
| Support for non-NVIDIA GPUs (AMD/Intel) | JAX XLA backend supports CUDA primarily; ROCm support is experimental |
