# SimuCell3D Physics Model: Equation-to-Code Mapping

This document maps the mathematical equations from the [Nature Computational Science paper](../SimuCell3D.pdf) to their implementations in the SimuCell3D codebase.

---

## Overview

SimuCell3D implements a **Deformable Cell Model (DCM)** where each cell is represented as a closed triangulated surface. The total potential energy of a cell is:

$$U = U_{volume} + U_{area} + U_{surface} + U_{bending}$$

Or from the paper (Equation 1):

$$U = KV\left(\ln\frac{V}{V_0} - 1\right) + \frac{k_a}{2}\left(\frac{A}{A_0} - 1\right)^2 + \int_{\partial\Omega}\left(\gamma + \frac{k_b}{2}(2H)^2\right)dS$$

---

## Energy Terms and Code Implementation

### 1. Volume Energy (Pressure)

**Paper Equation:**
$$U_{volume} = KV\left(\ln\frac{V}{V_0} - 1\right)$$

**Derived Pressure:**
$$p = \frac{dW}{dV} = -K\ln\frac{V}{V_0}$$

**Code Location:** `src/mesh/cell.cpp:1291-1303`

```cpp
// cell::update_pressure()
void cell::update_pressure() noexcept {
    // Equation: p = -K * ln(V/V0)
    pressure_ = -cell_type_->bulk_modulus_ * std::log(volume_ / target_volume_);

    // Cap pressure to maximum if specified
    if(pressure_ > cell_type_->max_pressure_)
        pressure_ = cell_type_->max_pressure_;

    // Energy: U = K * V * (ln(V/V0) - 1)
    pressure_energy_ = std::abs(cell_type_->bulk_modulus_ * volume_ *
                               (std::log(volume_ / target_volume_) - 1.));
}
```

**Parameters (from XML):**
| Parameter | Symbol | Code Variable | Typical Value | Unit |
|-----------|--------|---------------|---------------|------|
| Bulk modulus | K | `bulk_modulus_` | 2500 | Pa |
| Target volume | V₀ | `target_volume_` | 1.4×10⁻¹⁵ | m³ |
| Max pressure | p_max | `max_pressure_` | 2500 | Pa |

**Pressure Force on Nodes:** `src/mesh/cell.cpp:1310-1342`

```cpp
// cell::apply_pressure_on_surface()
// For each face: F_pressure = p * A * n̂ / 3  (distributed to 3 nodes)
const vec3 face_pressure = face_normal * pressure_ * face_area / 3.;
n1.add_force(face_pressure);
n2.add_force(face_pressure);
n3.add_force(face_pressure);
```

---

### 2. Area Elasticity Energy

**Paper Equation:**
$$U_{area} = \frac{k_a}{2}\left(\frac{A}{A_0} - 1\right)^2$$

**Code Location:** `src/mesh/cell.cpp:1350-1408`

```cpp
// cell::apply_surface_tension_and_membrane_elasticity()

// Target area from isoperimetric ratio: A0 = (Q0 * V^2)^(1/3)
target_area_ = std::cbrt(cell_type_->target_isoperimetric_ratio_ * volume_ * volume_);

// Membrane elasticity factor: -(ka/A0) * (A/A0 - 1)
const double membrane_elasticity_factor =
    -(cell_type_->area_elasticity_modulus_ / target_area_) *
    ((area_ / target_area_) - 1.);
```

**Force Calculation:**
The force on each node is derived from the area gradient:

$$\mathbf{f}_{s,i} = -\sum_{f \in \mathcal{F}_i} \gamma_f \nabla_i A_f$$

```cpp
// Area gradient for node n1: ∇A = -0.5 * n̂ × (r2 - r3)
const vec3 grad_pos_n1 = f.get_normal().cross(n2.pos_ - n3.pos_) * (-0.5);

// Combined force factor (surface tension + membrane elasticity)
const double force_factor = -face_type.surface_tension_ + membrane_elasticity_factor;

// Apply force: F = area_gradient * force_factor
const vec3 force_n1 = grad_pos_n1 * force_factor;
n1.add_force(force_n1);
```

**Parameters:**
| Parameter | Symbol | Code Variable | Typical Value | Unit |
|-----------|--------|---------------|---------------|------|
| Area elasticity | k_a | `area_elasticity_modulus_` | 10⁻¹⁵ | J |
| Isoperimetric ratio | Q₀ | `target_isoperimetric_ratio_` | 250 | - |

---

### 3. Surface Tension Energy

**Paper Equation:**
$$U_{surface} = \int_{\partial\Omega} \gamma \, dS = \sum_f \gamma_f A_f$$

**Code Location:** `src/mesh/cell.cpp:1350-1408` (same function as area elasticity)

```cpp
// Surface tension energy accumulation
surface_tension_energy_ += 0.5 * face_type.surface_tension_ * f.get_area();

// Force from surface tension (included in force_factor above)
// F_tension = -γ * ∇A
```

**Key Feature: Polarization-Dependent Surface Tension**

Different face types (apical, basal, lateral) can have different surface tensions:

```cpp
// Get face-specific surface tension from face type
const face_type_parameters& face_type = cell_type_->face_types_[f.type_id_];
const double gamma = face_type.surface_tension_;  // Can vary by face type
```

**Parameters:**
| Parameter | Symbol | Code Variable | Typical Value | Unit |
|-----------|--------|---------------|---------------|------|
| Surface tension | γ | `surface_tension_` | 0.001 | N/m |

---

### 4. Bending Energy

**Paper Equation (Discrete Bending Energy):**
$$U_b \approx \sum_{(i,j)} \bar{k}_b \frac{\|e_{ij}\|^2}{A_{ij}} \left(2\cos\frac{\theta_{ij}}{2}\right)^2$$

Where:
- $\bar{k}_b$ = average bending stiffness of adjacent faces
- $e_{ij}$ = edge vector from node i to j
- $A_{ij}$ = sum of areas of two adjacent faces
- $\theta_{ij}$ = dihedral angle between faces

**Code Location:** `src/mesh/cell.cpp:1413-1542`

```cpp
// cell::apply_bending_forces()
// Based on: Wardetzky et al., CAGD 24, 499 (2007)

// Average bending stiffness of adjacent faces
const double avg_bending_stiffness =
    (face_type_1.bending_modulus_ + face_type_2.bending_modulus_) / 2.;

// Sum of face areas
const double sum_face_areas = f1.get_area() + f2.get_area();

// Dihedral angle calculation using face normals
// θ = arccos(-n1 · n2) with sign from edge orientation
```

**Parameters:**
| Parameter | Symbol | Code Variable | Typical Value | Unit |
|-----------|--------|---------------|---------------|------|
| Bending rigidity | k_b | `bending_modulus_` | 2×10⁻¹⁸ | J |

---

## Contact Forces

### Spring-Based Contact Model (Contact Model Index = 0)

**Paper Equation (2):**
$$\sigma_{ab} = \begin{cases}
\xi d_{ab} & \text{if } d_{ab} \in [-c, 0) \text{ (repulsion)} \\
\omega d_{ab} & \text{if } d_{ab} \in [0, c/2) \text{ (adhesion)} \\
\omega(c - d_{ab}) & \text{if } d_{ab} \in [c/2, c] \text{ (detachment)}
\end{cases}$$

**Code Location:** `src/contact_models/contact_node_face_via_springs.cpp`

### Face-Face Coupling Model (Contact Model Index = 2)

**Tight Coupling:** Nodes from adjacent cells within distance `c` are coupled to move together.

**Code Location:** `src/contact_models/contact_face_face_via_coupling.cpp`

```cpp
// Node coupling: relocate to average position
// r_new = (r_a + r_b) / 2
// f_shared = (f_a + f_b) / 2
```

**Parameters:**
| Parameter | Symbol | Code Variable | Typical Value | Unit |
|-----------|--------|---------------|---------------|------|
| Repulsion strength | ξ | `repulsion_strength_` | 10⁹ | Pa/m |
| Adhesion strength | ω | `adherence_strength_` | 10⁹ | Pa/m |
| Contact cutoff | c | `contact_cutoff_` | 2×10⁻⁷ | m |
| Max coupling curvature | H_max | `max_coupling_curvature_` | 5×10⁶ | m⁻¹ |

---

## Time Integration

### Semi-Implicit Euler (Dynamic Model Index = 0)

**Equations of Motion:**
$$m\ddot{\mathbf{r}}_i + \zeta\dot{\mathbf{r}}_i = \mathbf{f}_i$$

**Integration Scheme:**
$$\mathbf{p}_i \leftarrow \mathbf{p}_i + \Delta t(\mathbf{f}_i - \zeta\mathbf{p}_i/m)$$
$$\mathbf{r}_i \leftarrow \mathbf{r}_i + \Delta t \mathbf{p}_i/m$$

**Code Location:** `src/time_integration/time_integration.cpp`

### Overdamped (Dynamic Model Index = 1)

**Simplified Equation (neglecting inertia):**
$$\mathbf{r}_i \leftarrow \mathbf{r}_i + \Delta t \mathbf{f}_i/\zeta$$

**Parameters:**
| Parameter | Symbol | Code Variable | Typical Value | Unit |
|-----------|--------|---------------|---------------|------|
| Time step | Δt | `time_step_` | 10⁻⁷ | s |
| Damping | ζ | `damping_` | 2.5×10⁻¹⁰ | kg/s |
| Mass density | ρ | `mass_density_` | 1000 | kg/m³ |

---

## Cell Polarization

SimuCell3D supports automatic detection of cell membrane regions:

| Face Type | Location | Biological Meaning |
|-----------|----------|-------------------|
| Apical | Facing lumen | Specialized for secretion/absorption |
| Basal | Facing ECM/exterior | Attachment, signaling |
| Lateral | Contacting other cells | Cell-cell adhesion |

**Code Location:** `src/automatic_polarization/`

Each face type can have independent parameters:
- Surface tension (γ)
- Bending modulus (k_b)
- Adhesion strength (ω)
- Repulsion strength (ξ)

---

## Cell Division

**Trigger Condition:**
$$V > V_{max}$$

**Code Location:** `src/triangulation_modules/cell_divider.cpp`

Division plane options:
1. Random orientation
2. Perpendicular to longest axis (Hertwig's rule)

---

## Volume and Area Calculation

**Volume (3D Shoelace Formula):**
$$V = \frac{1}{6}\left|\sum_{f \in \mathcal{M}} \det\begin{pmatrix} \mathbf{r}_i & \mathbf{r}_j & \mathbf{r}_k \end{pmatrix}\right|$$

**Code Location:** `src/mesh/cell.cpp` (volume calculation in geometry updates)

**Surface Area:**
$$A = \sum_{f \in \mathcal{M}} A_f = \sum_{f \in \mathcal{M}} \frac{1}{2}\|\mathbf{n}_f\|$$

---

## Compile-Time Configuration

Physics model selection is done in `include/global_configuration.hpp`:

```cpp
#define CONTACT_MODEL_INDEX 2      // 0=springs, 1=node-node, 2=face-face
#define DYNAMIC_MODEL_INDEX 0      // 0=semi-implicit Euler, 1=overdamped
#define POLARIZATION_MODE_INDEX 1  // 0=none, 1=contact-based, 2=spatial
```

**Changes require full rebuild:** `cd build && rm -rf * && cmake .. && make`

---

## Key Files Reference

| Component | Header | Implementation |
|-----------|--------|----------------|
| Cell mechanics | `include/mesh/cell.hpp` | `src/mesh/cell.cpp` |
| Contact (springs) | `include/contact_models/contact_node_face_via_springs.hpp` | `src/contact_models/contact_node_face_via_springs.cpp` |
| Contact (coupling) | `include/contact_models/contact_face_face_via_coupling.hpp` | `src/contact_models/contact_face_face_via_coupling.cpp` |
| Time integration | `include/time_integration/time_integration.hpp` | `src/time_integration/time_integration.cpp` |
| Polarization | `include/automatic_polarization/automatic_polarizer.hpp` | `src/automatic_polarization/automatic_polarizer.cpp` |
| Cell division | `include/triangulation_modules/cell_divider.hpp` | `src/triangulation_modules/cell_divider.cpp` |
| Solver loop | `include/solver.hpp` | `src/solver.cpp` |

---

## See Also

- [SimuCell3D Paper](../SimuCell3D.pdf) - Full mathematical derivations
- [Parameter Reference](../user-guide/parameter-reference.md) - XML parameter reference
- [parameter-guide.md](./parameter-guide.md) - Biological interpretation of parameters
