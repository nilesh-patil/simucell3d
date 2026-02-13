# Reproducing Nature Paper Results

This guide maps figures from **Runser et al. (Nature Computational Science, 2024)** to SimuCell3D parameter files and commands.

**Paper reference:** [SimuCell3D.pdf](../SimuCell3D.pdf) in this repository

---

## Prerequisites

1. Build SimuCell3D in Release mode:
   ```bash
   mkdir -p build && cd build
   cmake -DCMAKE_BUILD_TYPE=Release .. && make -j$(nproc)
   ```

2. Verify default configuration (`include/global_configuration.hpp`):
   ```cpp
   #define CONTACT_MODEL_INDEX 2       // Face-face coupling
   #define DYNAMIC_MODEL_INDEX 0       // Semi-implicit Euler
   #define POLARIZATION_MODE_INDEX 1   // Contact-based polarization
   ```

---

## Figure 1: Tissue Representations and Scaling

### Figure 1e: Computational Efficiency (Exponential Growth)

**Paper claim:** "125,000 cells in 1 day" with O(N^{4/3}) computational scaling

**Reproduce with:**
```bash
cd build
./simucell3d ../parameters/scalability/parameters_growth_32768cells.xml --schedule=adaptive
```

**Expected output:**
- Cell count approximately doubles at regular intervals
- Runtime scales as O(N^{4/3}) where N is cell count

**Validation:**
1. Plot `cell_count` vs `wall_time` from `simulation_statistics.csv`
2. Fit power law: expect exponent ~1.33

**Available scale files:**

| Parameter File | Initial Cells | Expected Runtime |
|---------------|---------------|------------------|
| `parameters_growth_64cells.xml` | 64 | ~1 min |
| `parameters_growth_256cells.xml` | 256 | ~5 min |
| `parameters_growth_1024cells.xml` | 1,024 | ~30 min |
| `parameters_growth_4096cells.xml` | 4,096 | ~2 hours |
| `parameters_growth_32768cells.xml` | 32,768 | ~12 hours |

### Figure 1f: Tissue Topologies (Vesicle, Spheroid, Sheet, Tube)

| Topology | Parameter File | Input Mesh | Expected Runtime |
|----------|---------------|------------|------------------|
| Vesicle  | `parameters/core/parameters_vesicle.xml` | `fig_1_vesicle.vtk` | ~30s (test), ~3h (full) |
| Sheet    | `parameters/core/parameters_sheet.xml` | `fig_1_sheet_geometry.vtk` | ~1h |
| Tube     | `parameters/core/parameters_tube.xml` | `fig_1_tube.vtk` | ~1h |

**Run vesicle (quick test):**
```bash
./simucell3d ../parameters/core/parameters_vesicle.xml
```

**Expected output:** Vesicle maintains spherical shape, lumen volume stable

---

## Figure 3: Monolayer vs Multilayer Transition

**Paper claim:** Surface tension γ̃ = 0.02-0.10 controls epithelial stratification

### Key Parameters

| Parameter | XML Path | Paper Range | Units |
|-----------|----------|-------------|-------|
| Surface tension (γ) | `<surface_tension>` | 0.001-0.01 | N/m |
| Adhesion strength | `<adherence_strength>` | 10^9 | Pa/m |
| Dimensionless γ̃ | γ / (adhesion × contact_cutoff) | 0.02-0.10 | - |

### Setup

1. Start with vesicle configuration:
   ```bash
   cp parameters/core/parameters_vesicle.xml parameters/experiments/fig3_surface_tension.xml
   ```

2. Modify surface tension values in the copied file:
   - Monolayer regime: `<surface_tension>1e-3</surface_tension>` (low γ)
   - Multilayer regime: `<surface_tension>5e-3</surface_tension>` (high γ)

3. Run parameter sweep:
   ```bash
   for gamma in 0.001 0.002 0.003 0.005 0.01; do
       sed "s/<surface_tension>.*<\/surface_tension>/<surface_tension>$gamma<\/surface_tension>/g" \
           parameters/experiments/fig3_surface_tension.xml > /tmp/fig3_gamma_$gamma.xml
       ./simucell3d /tmp/fig3_gamma_$gamma.xml --output-dir=results/fig3_gamma_$gamma
   done
   ```

**Expected output:**
- Low γ (γ̃ < 0.05): Monolayer epithelium maintained
- High γ (γ̃ > 0.08): Multilayer (stratified) tissue forms

---

## Figure 4: Pseudostratified Epithelia

**Paper context:** Cell nuclei position affects apparent stratification

### Required Setup

This figure requires custom mesh data from the paper's supplementary materials.

**Key parameters for pseudostratification:**
- `<INMForce>`: Nuclear movement force (interkinetic nuclear migration)
- `<target_isoperimetric_ratio>`: Cell elongation control

### Validation Metrics

| Metric | Paper Value | How to Measure |
|--------|-------------|----------------|
| Coordination number (z) | 9-11 | Count cell-cell contacts |
| Cell aspect ratio | 3-5 | Height / width from VTK |
| Nuclear position variance | high | Z-coordinate distribution |

---

## Table 2: Parameter Validation

The paper provides experimentally validated parameter ranges:

| Parameter | Default Value | Measured Range | Literature Source |
|-----------|--------------|----------------|-------------------|
| Bulk modulus (K) | 2500 Pa | 2250 Pa | Ref 78 |
| Surface tension (γ) | 0.001 N/m | 0.0005-0.0025 N/m | Refs 81, 85, 86 |
| Bending stiffness (kb) | 2×10⁻¹⁸ J | 1-2×10⁻¹⁸ J | Ref 87 |
| Cell density (ρ) | 1000 kg/m³ | ~1000 kg/m³ | Water-like |

**To use validated parameters:**
The default `parameters_vesicle.xml` already uses these validated values.

---

## Verification Checklist

After running a simulation, verify:

- [ ] **Cell count growth:** Matches expected doubling rate
- [ ] **Pressure values:** Within 300-2200 Pa range (Table 2)
- [ ] **Volume conservation:** Total volume error < 1%
- [ ] **Morphology:** Tissue shape matches figure qualitatively
- [ ] **Energy minimization:** Total energy decreasing over time

### Quick Sanity Check

```bash
# Run short vesicle simulation
./simucell3d ../parameters/core/parameters_vesicle.xml

# Check output statistics
tail -5 simulation_results/simulation_vesicle/simulation_statistics.csv
```

Expected: `total_kinetic_energy` decreasing, `avg_cell_pressure` ~500-2000 Pa

---

## Troubleshooting Reproduction

### Simulation Crashes (SIGSEGV)

Usually indicates physics instability, not code bugs:
1. Reduce `<time_step>` by 10x
2. Check `<damping_coefficient>` is reasonable (5e-10 typical)
3. Verify mesh quality in input VTK file

### Results Don't Match Paper

1. **Check compile-time config:** Rebuild after changing `global_configuration.hpp`
2. **Verify parameter values:** Units matter! (Pa vs kPa, N/m vs mN/m)
3. **Check mesh scale:** Paper uses ~μm scale meshes

### Performance Issues

1. Use `--schedule=adaptive` (default) for best automatic tuning
2. Set `OMP_NUM_THREADS` to physical core count
3. Verify Release build (not Debug)

---

## Data Analysis Tips

### Extracting Statistics

```bash
# Cell count over time
awk -F',' 'NR>1 {print $1, $2}' simulation_statistics.csv

# Average pressure
awk -F',' 'NR>1 {print $1, $8}' simulation_statistics.csv
```

### Visualizing in ParaView

1. Load all `mesh_*.vtk` files as a time series
2. Color by `cell_type` to distinguish epithelial/lumen/ECM
3. Use `Clip` filter to see internal structure

### Comparing to Paper Figures

1. **Figure 1f:** Compare tissue morphology visually
2. **Figure 3:** Measure layer count in ParaView
3. **Table 2:** Extract parameter values from CSV, compare to measured ranges

---

## Additional Resources

- [Physics Model](physics-model.md): Mathematical formulation details
- [Parameter Guide](parameter-guide.md): Scientific meaning of parameters
- [Nature Paper PDF](../SimuCell3D.pdf): Original publication with all supplementary data
