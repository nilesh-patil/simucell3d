# SimuCell3D Parameter Guide: Biological Interpretation

This guide explains what each simulation parameter means biologically and how to choose appropriate values for your system.

---

## Overview

SimuCell3D parameters can be grouped into four categories:

1. **Cell Volume Parameters** - Control cell compressibility, growth, and division
2. **Cell Surface Parameters** - Control membrane mechanics and shape
3. **Contact Parameters** - Control cell-cell interactions
4. **Numerical Parameters** - Control simulation accuracy and stability

---

## Cell Volume Parameters

### Bulk Modulus (K)

**Symbol:** K | **Code:** `bulk_modulus_` | **Unit:** Pa

**Biological Meaning:**
The bulk modulus controls how resistant a cell is to volume changes. It represents the combined effect of:
- Cytoplasmic incompressibility
- Internal osmotic pressure
- Cytoskeletal resistance to compression

**Biological Context:**
- Cells are mostly water (~70%) and thus nearly incompressible
- The cytoskeleton provides additional resistance
- Measured values: 2000-3000 Pa for most epithelial cells

| Cell Type | Typical K (Pa) | Notes |
|-----------|---------------|-------|
| Soft cells (embryonic) | 1000-2000 | Less cytoskeletal structure |
| Epithelial cells | 2000-3000 | Default for most simulations |
| Stiff cells (chondrocytes) | 3000-5000 | More cytoskeletal crosslinking |

**Effect of Changing K:**
- **Increase K** → Cells resist compression more, maintain volume better
- **Decrease K** → Cells are more compressible, can be squeezed more easily

---

### Maximum Pressure (p_max)

**Symbol:** p_max | **Code:** `max_pressure_` | **Unit:** Pa

**Biological Meaning:**
Caps the internal pressure a cell can generate. This represents:
- Maximum osmotic pressure difference cells can sustain
- Limit before membrane damage occurs

**Measured Values:**
- Interstitial pressure: 300-2200 Pa
- Typical simulation value: 2500 Pa

**When to Adjust:**
- If cells "explode" numerically → increase p_max or reduce growth rate
- For modeling high-pressure environments (e.g., tumors) → adjust accordingly

---

### Division Volume (V_max)

**Symbol:** V_max | **Code:** `avg_division_volume_` | **Unit:** m³

**Biological Meaning:**
The volume threshold that triggers cell division. Represents:
- Completion of cell cycle checkpoints
- Sufficient resource accumulation for division

**Measured Values:**
| Cell Type | Division Volume (m³) | Notes |
|-----------|---------------------|-------|
| Small epithelial | 0.9×10⁻¹⁵ | ~900 μm³ |
| Typical epithelial | 1.4×10⁻¹⁵ | ~1400 μm³ (default) |
| Large cells | 2.0×10⁻¹⁵ | ~2000 μm³ |

**Effect of Changing:**
- **Increase V_max** → Larger cells, fewer divisions
- **Decrease V_max** → Smaller cells, more frequent divisions

---

### Growth Rate (g)

**Symbol:** g | **Code:** `avg_growth_rate_` | **Unit:** m³/s

**Biological Meaning:**
Rate of cell volume increase per unit time. Represents:
- Protein synthesis rate
- Nutrient uptake and metabolism
- Cell cycle duration

**Biological Context:**
- Typical cell cycle: 12-24 hours
- Default growth rate: 10⁻¹¹ m³/s corresponds to ~12h doubling time

**Choosing Growth Rate:**
$$g = \frac{V_{division} - V_{initial}}{t_{cycle}}$$

| Scenario | g (m³/s) | Doubling Time |
|----------|----------|---------------|
| Slow growth | 10⁻¹² | ~5-7 days |
| Normal growth | 10⁻¹¹ | ~12-24 hours |
| Fast growth | 10⁻²⁰ | ~6 hours |
| Exponential benchmark | 10⁻¹¹ | ~12 hours |

---

## Cell Surface Parameters

### Surface Tension (γ)

**Symbol:** γ | **Code:** `surface_tension_` | **Unit:** N/m

**Biological Meaning:**
Surface tension represents the contractile force of the actomyosin cortex - a meshwork of actin and myosin II beneath the plasma membrane.

**Biological Context:**
- The cortex constantly contracts, minimizing cell surface area
- This is why isolated cells are roughly spherical
- Higher cortical tension → more spherical cells
- Measured values: 0.0005-0.0025 N/m

**Effect on Cell Shape:**
| γ Value (N/m) | Cell Shape | Biological Meaning |
|---------------|------------|-------------------|
| 0.0005 | Spread/flat | Low cortical activity |
| 0.001 | Moderate rounding | Normal epithelial |
| 0.002 | Very round | High contractility |

**Polarization Effects:**
Different membrane regions can have different tensions:
- **Apical** (γ_apical): Often higher, specialized surface
- **Basal** (γ_basal): Intermediate, anchored to ECM
- **Lateral** (γ_lateral): Usually lower, contact with neighbors

---

### Bending Rigidity (k_b)

**Symbol:** k_b | **Code:** `bending_modulus_` | **Unit:** J

**Biological Meaning:**
Resistance of the cell membrane to bending. Represents:
- Lipid bilayer bending stiffness
- Cortex rigidity
- Membrane-cytoskeleton coupling

**Measured Values:**
- Pure lipid bilayer: ~10⁻¹⁹ J
- Cell membrane + cortex: 1-3×10⁻¹⁸ J

**Effect on Morphology:**
- **Higher k_b** → Smoother cell surfaces, resists local curvature
- **Lower k_b** → More irregular surfaces, allows membrane wrinkles

---

### Area Elasticity (k_a)

**Symbol:** k_a | **Code:** `area_elasticity_modulus_` | **Unit:** J

**Biological Meaning:**
Penalizes deviations of cell surface area from a target value. Represents:
- Membrane tension regulation
- Cytoskeletal remodeling capability
- Endocytosis/exocytosis capacity

**Default Value:** 10⁻¹⁵ J

**Effect:**
- **Higher k_a** → Cell maintains constant surface area, resists stretching
- **Lower k_a** → Surface area can vary more freely

---

### Target Isoperimetric Ratio (Q₀)

**Symbol:** Q₀ | **Code:** `target_isoperimetric_ratio_` | **Dimensionless**

**Biological Meaning:**
Defines the target relationship between cell volume and surface area:
$$A_0 = \sqrt[3]{Q_0 \cdot V^2}$$

For a sphere: Q₀ = 36π ≈ 113

**Typical Values:**
- Q₀ = 250: Default, allows some deviation from spherical
- Q₀ = 300: Measured in some epithelia
- Q₀ = 113: Perfectly spherical target

**Effect:**
- **Higher Q₀** → Cells can have more surface area relative to volume (elongated shapes)
- **Lower Q₀** → Cells tend toward more compact/spherical shapes

---

## Contact Parameters

### Adhesion Strength (ω)

**Symbol:** ω | **Code:** `adherence_strength_` | **Unit:** Pa/m

**Biological Meaning:**
Strength of cell-cell adhesion. Represents:
- Cadherin-mediated adhesion (E-cadherin in epithelia)
- Tight junctions and desmosomes
- Overall cell-cell cohesion

**Biological Context:**
- E-cadherin: Primary adhesion molecule in epithelia
- N-cadherin: Neural tissues
- Loss of cadherins → metastasis in cancer

**Effect on Tissue:**
| ω Value (Pa/m) | Tissue Behavior |
|---------------|-----------------|
| 10⁷ | Weak adhesion, cells easily separate |
| 10⁸ | Moderate adhesion, tissue cohesion |
| 10⁹ | Strong adhesion (default), tight epithelium |
| 10¹⁰ | Very strong, minimal cell rearrangement |

**Key Insight from Paper:**
The competition between surface tension (γ) and adhesion strength (ω) determines:
- Whether tissue stays monolayer or becomes multilayer
- Cell-cell contact areas
- Tissue fluidity vs. solidity

---

### Repulsion Strength (ξ)

**Symbol:** ξ | **Code:** `repulsion_strength_` | **Unit:** Pa/m

**Biological Meaning:**
Prevents cells from overlapping (volume exclusion). Represents:
- Physical incompatibility of occupying same space
- Glycocalyx and steric repulsion
- Contact inhibition (partial)

**Default Value:** 10⁹ Pa/m

**Typically:** Keep ξ = ω (equal repulsion and adhesion strength)

---

### Contact Cutoff Distance (c)

**Symbol:** c | **Code:** `contact_cutoff_distance_` | **Unit:** m

**Biological Meaning:**
Maximum distance at which cells can sense and interact. Represents:
- Cadherin extracellular domain length
- Glycocalyx thickness
- Signaling range

**Default Value:** 2×10⁻⁷ m (200 nm)

**Biological Context:**
- E-cadherin ectodomain: ~20-25 nm
- Cell-cell gap in epithelia: 10-30 nm
- Default is larger to account for mesh discretization

---

## Polarization Parameters

### Face Types

SimuCell3D automatically classifies membrane faces into:

| Face Type | XML ID | Faces... | Biological Features |
|-----------|--------|----------|-------------------|
| Apical | 0 | Lumen | Microvilli, secretory |
| Basal | 1 | ECM/exterior | Integrins, focal adhesions |
| Lateral | 2 | Other cells | Cadherins, tight junctions |

Each face type can have independent mechanical parameters, enabling realistic epithelial modeling.

---

## Numerical Parameters

### Time Step (Δt)

**Symbol:** Δt | **Code:** `time_step_` | **Unit:** s

**Choosing Time Step:**
- Must be small enough for numerical stability
- Too small → simulation takes forever
- Too large → simulation crashes (SIGSEGV)

**Guidelines:**
| Scenario | Recommended Δt |
|----------|---------------|
| Initial testing | 10⁻⁷ s |
| Stable simulation | 10⁻⁷ to 5×10⁻⁷ s |
| Unstable/crashing | Try 5×10⁻⁸ s |

---

### Damping Coefficient (ζ)

**Symbol:** ζ | **Code:** `damping_` | **Unit:** kg/s

**Biological Meaning:**
Represents viscous dissipation from:
- Cytoplasmic viscosity
- Friction with surrounding medium
- Energy dissipation in cytoskeleton

**Effect:**
- **Higher ζ** → Slower dynamics, more stable
- **Lower ζ** → Faster dynamics, may become unstable

---

### Minimum Edge Length

**Code:** `min_edge_len_` | **Unit:** m

**Effect on Resolution:**
- Smaller → Higher resolution, more triangles, slower
- Larger → Lower resolution, fewer triangles, faster

**Rule of Thumb:**
$$l_{min} \approx \frac{1}{10} \times \text{cell diameter}$$

---

## Parameter Tuning Workflow

### 1. Start with Defaults

Use the vesicle parameter file as a starting point:
```bash
./simucell3d ../parameters/core/parameters_vesicle.xml
```

### 2. Match Your Biology

Adjust parameters to match your specific cell type:

1. **Cell size**: Adjust `avg_division_volume_`
2. **Growth rate**: Match to observed doubling time
3. **Stiffness**: Adjust `bulk_modulus_` based on cell type
4. **Adhesion**: Set `adherence_strength_` based on cadherin expression

### 3. Test Stability

Run short simulations to check:
- No SIGSEGV crashes (reduce time_step if needed)
- Cells don't explode (reduce growth_rate or increase damping)
- Tissue stays cohesive (increase adhesion if cells separate)

### 4. Validate Against Experiments

Compare simulation outputs to:
- Cell shape measurements (sphericity, aspect ratio)
- Tissue organization (layer number, contact areas)
- Dynamics (rearrangement rates, response to perturbation)

---

## Common Parameter Combinations

### Vesicle Formation (Default)

```xml
<bulk_modulus>2500</bulk_modulus>
<surface_tension>0.001</surface_tension>
<adherence_strength>1e9</adherence_strength>
<avg_growth_rate>1e-11</avg_growth_rate>
```

### Stiff Epithelium

```xml
<bulk_modulus>4000</bulk_modulus>
<surface_tension>0.002</surface_tension>
<adherence_strength>2e9</adherence_strength>
```

### Soft/Embryonic Tissue

```xml
<bulk_modulus>1500</bulk_modulus>
<surface_tension>0.0005</surface_tension>
<adherence_strength>5e8</adherence_strength>
```

---

## See Also

- [physics-model.md](./physics-model.md) - Equation-to-code mapping
- [Parameter Reference](../user-guide/parameter-reference.md) - Complete XML reference
- [SimuCell3D.pdf](../SimuCell3D.pdf) - Paper with measured values (Table 2)
