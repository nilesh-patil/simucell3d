# Scientific Validation Suite Implementation Plan

**Goal:** Implement 15 scientific correctness tests and Nature paper reproduction scripts for the SimuCell3D benchmarking system (Phase 2).

**Architecture:** Tests use the existing custom cassert-based framework (return 0=pass, 1=fail). Each test creates a solver with controlled parameters, runs a short simulation, and validates physics invariants. Parameter files are XML, test data uses existing VTK meshes. Nature reproduction scripts are bash wrappers around `simucell3d` with custom parameter files.

**Tech Stack:** C++17, OpenMP, CMake 3.0+, custom test framework, XML parameters, VTK mesh files, bash scripts, Git LFS for baselines.

---

### Task 1: Create test directory structure and CMake scaffolding

**Files:**
- Create: `test/test_scientific_benchmarks/CMakeLists.txt`
- Modify: `test/CMakeLists.txt:16` (add subdirectory)

**Step 1: Create the test CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.0)

# Use C++17 standard
set(CMAKE_CXX_STANDARD 17)

# Indicate to the test source the path to project source directory
add_definitions(-DPROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

# Energy conservation benchmark
add_executable(test_energy_conservation energy_conservation_benchmark.cpp)
target_link_libraries(test_energy_conservation PUBLIC src mesh io contact_models time_integration triangulation_modules)
target_include_directories(test_energy_conservation PUBLIC
    "${PROJECT_SOURCE_DIR}/include/"
    "${PROJECT_SOURCE_DIR}/include/mesh"
    "${PROJECT_SOURCE_DIR}/include/io"
    "${PROJECT_SOURCE_DIR}/include/contact_models"
    "${PROJECT_SOURCE_DIR}/include/time_integration"
    "${PROJECT_SOURCE_DIR}/include/triangulation_modules"
    "${PROJECT_SOURCE_DIR}/include/math_modules"
)

add_test(NAME scientific_energy_conservation_single_cell      COMMAND test_energy_conservation test_energy_conservation_single_cell)
add_test(NAME scientific_energy_conservation_two_cells         COMMAND test_energy_conservation test_energy_conservation_two_cells)
add_test(NAME scientific_energy_monotonic_dissipation          COMMAND test_energy_conservation test_energy_monotonic_dissipation)

# Pressure validation benchmark
add_executable(test_pressure_validation pressure_validation_benchmark.cpp)
target_link_libraries(test_pressure_validation PUBLIC src mesh io contact_models time_integration triangulation_modules)
target_include_directories(test_pressure_validation PUBLIC
    "${PROJECT_SOURCE_DIR}/include/"
    "${PROJECT_SOURCE_DIR}/include/mesh"
    "${PROJECT_SOURCE_DIR}/include/io"
    "${PROJECT_SOURCE_DIR}/include/contact_models"
    "${PROJECT_SOURCE_DIR}/include/time_integration"
    "${PROJECT_SOURCE_DIR}/include/triangulation_modules"
    "${PROJECT_SOURCE_DIR}/include/math_modules"
)

add_test(NAME scientific_pressure_physiological_range          COMMAND test_pressure_validation test_pressure_physiological_range)
add_test(NAME scientific_pressure_bulk_modulus_response         COMMAND test_pressure_validation test_pressure_bulk_modulus_response)
add_test(NAME scientific_laplace_pressure                      COMMAND test_pressure_validation test_laplace_pressure)

# Volume and geometry validation benchmark
add_executable(test_geometry_validation geometry_validation_benchmark.cpp)
target_link_libraries(test_geometry_validation PUBLIC src mesh io contact_models time_integration triangulation_modules)
target_include_directories(test_geometry_validation PUBLIC
    "${PROJECT_SOURCE_DIR}/include/"
    "${PROJECT_SOURCE_DIR}/include/mesh"
    "${PROJECT_SOURCE_DIR}/include/io"
    "${PROJECT_SOURCE_DIR}/include/contact_models"
    "${PROJECT_SOURCE_DIR}/include/time_integration"
    "${PROJECT_SOURCE_DIR}/include/triangulation_modules"
    "${PROJECT_SOURCE_DIR}/include/math_modules"
)

add_test(NAME scientific_volume_preservation                   COMMAND test_geometry_validation test_volume_preservation)
add_test(NAME scientific_mesh_quality                          COMMAND test_geometry_validation test_mesh_quality)
add_test(NAME scientific_face_normal_consistency               COMMAND test_geometry_validation test_face_normal_consistency)
add_test(NAME scientific_manifoldness                          COMMAND test_geometry_validation test_manifoldness)
add_test(NAME scientific_isoperimetric_ratio                   COMMAND test_geometry_validation test_isoperimetric_ratio)

# Contact mechanics validation benchmark
add_executable(test_contact_validation contact_validation_benchmark.cpp)
target_link_libraries(test_contact_validation PUBLIC src mesh io contact_models time_integration triangulation_modules)
target_include_directories(test_contact_validation PUBLIC
    "${PROJECT_SOURCE_DIR}/include/"
    "${PROJECT_SOURCE_DIR}/include/mesh"
    "${PROJECT_SOURCE_DIR}/include/io"
    "${PROJECT_SOURCE_DIR}/include/contact_models"
    "${PROJECT_SOURCE_DIR}/include/time_integration"
    "${PROJECT_SOURCE_DIR}/include/triangulation_modules"
    "${PROJECT_SOURCE_DIR}/include/math_modules"
)

add_test(NAME scientific_contact_reciprocity                   COMMAND test_contact_validation test_contact_reciprocity)
add_test(NAME scientific_force_balance_equilibrium             COMMAND test_contact_validation test_force_balance_equilibrium)
add_test(NAME scientific_momentum_conservation                 COMMAND test_contact_validation test_momentum_conservation)
add_test(NAME scientific_contact_area_fraction                 COMMAND test_contact_validation test_contact_area_fraction)
```

**Step 2: Add subdirectory to parent CMakeLists.txt**

Add after line 15 in `test/CMakeLists.txt`:
```cmake
add_subdirectory(test_scientific_benchmarks)
```

**Step 3: Verify build**

Run: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc) 2>&1 | tail -5`
Expected: Build errors about missing .cpp files (expected at this stage)

**Step 4: Commit**

```bash
git add test/test_scientific_benchmarks/CMakeLists.txt test/CMakeLists.txt
git commit -m "feat(benchmarks): add CMake scaffolding for scientific validation tests"
```

---

### Task 2: Create test parameter files for scientific benchmarks

**Files:**
- Create: `test/test_scientific_benchmarks/params_single_sphere.xml` (single cell, no growth, no division)
- Create: `test/test_scientific_benchmarks/params_two_spheres_contact.xml` (two cells in contact)
- Create: `test/test_scientific_benchmarks/params_laplace_pressure.xml` (single cell for Laplace test)

**Step 1: Create single sphere parameter file**

This parameter file creates a minimal simulation with one epithelial cell (no growth, no division, short duration) to test energy conservation and volume preservation. Uses `sphere.vtk` as input mesh.

Key settings:
- `simulation_duration`: 1e-4 (100 microseconds - very short for fast testing)
- `time_step`: 1e-7 (same as production)
- `avg_growth_rate`: 0 (no growth to test energy conservation)
- `avg_division_volume`: inf (no division)
- `surface_tension`: 1e-3 (standard value)
- `bulk_modulus`: 2500 (standard value)
- Uses relative path `data/input_meshes/sphere.vtk`

**Step 2: Create two spheres contact parameter file**

Uses `test/test_io/test_simulation_initializer/2_spheres.vtk` for contact reciprocity tests. Same short duration but with adhesion/repulsion active.

**Step 3: Create Laplace pressure parameter file**

Uses `data/input_meshes/sphere.vtk`. Key: set known surface tension γ, let cell equilibrate, check P = 2γ/R.

**Step 4: Commit**

```bash
git add test/test_scientific_benchmarks/params_*.xml
git commit -m "feat(benchmarks): add parameter files for scientific validation tests"
```

---

### Task 3: Implement energy conservation benchmark

**Files:**
- Create: `test/test_scientific_benchmarks/energy_conservation_benchmark.cpp`

**Step 1: Write the energy conservation test**

The test:
1. Loads a single cell from `sphere.vtk` with no growth/division
2. Creates a solver and runs N iterations
3. At each statistics sampling point, records total energy = kinetic + pressure + surface_tension + bending
4. Validates: |E(t) - E(0)| / E(0) < 1% (relative energy drift)

Physics rationale: With no external forces and damping, total mechanical energy should be approximately conserved or monotonically dissipated (damped system). The 1% threshold accounts for numerical dissipation in the semi-implicit Euler scheme.

Implementation approach:
- Use `test_solver` derived class (same pattern as existing test_solver.cpp) to expose `run_iteration()`
- After each iteration, sum energy components from all cells
- Track max relative drift from initial energy

Tests:
- `test_energy_conservation_single_cell`: Single cell, |ΔE/E₀| < 1%
- `test_energy_conservation_two_cells`: Two cells in contact, |ΔE/E₀| < 5% (contact adds noise)
- `test_energy_monotonic_dissipation`: For damped system, total energy should not increase between consecutive samplings (allow 0.1% tolerance for numerical noise)

**Step 2: Run tests to verify they fail (no implementation yet - actually this IS the implementation)**

Run: `cd build && ctest -R scientific_energy -V`
Expected: All 3 tests PASS

**Step 3: Commit**

```bash
git add test/test_scientific_benchmarks/energy_conservation_benchmark.cpp
git commit -m "feat(benchmarks): implement energy conservation scientific tests"
```

---

### Task 4: Implement pressure validation benchmark

**Files:**
- Create: `test/test_scientific_benchmarks/pressure_validation_benchmark.cpp`

**Step 1: Write the pressure validation tests**

The tests validate:

1. **Physiological range** (300-2200 Pa):
   - Load cells, run short sim, check all pressures are in [0, 10000] Pa range
   - For epithelial cells with standard bulk_modulus=2500 Pa, pressure = -K * ln(V/V_target)
   - At equilibrium V ≈ V_target, so P ≈ 0 initially, grows as cell volume changes

2. **Bulk modulus response**:
   - Pressure should respond to volume changes: if V < V_target, P > 0 (expansion)
   - if V > V_target, P < 0 (compression)
   - Validate sign convention: pressure_ = -K * ln(V/V_target)

3. **Laplace pressure** (ΔP = 2γ/R):
   - For a single spherical cell at equilibrium:
     - Surface tension γ is set in parameters
     - Radius R can be computed from cell volume: R = (3V/(4π))^(1/3)
     - Expected Laplace pressure: ΔP = 2γ/R
     - Actual pressure comes from bulk modulus: P = -K * ln(V/V_target)
     - At equilibrium, these should approximately balance
   - Relative error < 5% (relaxed from 2% due to mesh discretization effects)

**Step 2: Build and run**

Run: `cd build && ctest -R scientific_pressure -V`
Expected: All 3 tests PASS

**Step 3: Commit**

```bash
git add test/test_scientific_benchmarks/pressure_validation_benchmark.cpp
git commit -m "feat(benchmarks): implement pressure validation scientific tests"
```

---

### Task 5: Implement geometry validation benchmark

**Files:**
- Create: `test/test_scientific_benchmarks/geometry_validation_benchmark.cpp`

**Step 1: Write the geometry validation tests**

5 tests:

1. **Volume preservation**:
   - Run short sim (no growth), track volume of each cell
   - |V(t) - V(0)| / V(0) < 5% after 1000 iterations
   - Volume drift indicates instability in time integration

2. **Mesh quality**:
   - After simulation, check triangle aspect ratios
   - No degenerate triangles (aspect ratio > 100 means degenerate)
   - At least 90% of faces have aspect ratio < 5 (reasonable quality)
   - Aspect ratio = longest_edge / shortest_altitude

3. **Face normal consistency**:
   - All face normals should point outward (away from centroid)
   - dot(face_normal, face_center - cell_centroid) > 0 for all faces
   - Threshold: > 95% of faces pass (mesh refinement may create transient inversions)

4. **Manifoldness**:
   - All cells should be manifold at all times (cell::is_manifold() returns true)
   - Run sim, check manifoldness after initialization and after N iterations

5. **Isoperimetric ratio bounds**:
   - IR = Area^3 / (36π * Volume^2) should be >= 1 (sphere = 1)
   - For biological cells, typically IR < 50 (elongated cells < 100)
   - Validate IR stays in [0.8, 500] throughout simulation

**Step 2: Build and run**

Run: `cd build && ctest -R scientific_geometry -V`
Expected: All 5 tests PASS

**Step 3: Commit**

```bash
git add test/test_scientific_benchmarks/geometry_validation_benchmark.cpp
git commit -m "feat(benchmarks): implement geometry validation scientific tests"
```

---

### Task 6: Implement contact mechanics validation benchmark

**Files:**
- Create: `test/test_scientific_benchmarks/contact_validation_benchmark.cpp`

**Step 1: Write the contact mechanics tests**

4 tests:

1. **Contact reciprocity** (Newton's 3rd law):
   - Run two cells in contact
   - Sum all forces on cell 1 nodes = F1
   - Sum all forces on cell 2 nodes = F2
   - At equilibrium-ish (after N iterations): |F1 + F2| should be small relative to |F1|
   - Threshold: |F1 + F2| / max(|F1|, |F2|) < 10% (contact model has numerical asymmetry)

2. **Force balance at equilibrium**:
   - Single cell at equilibrium: net force on all nodes → 0
   - Run sim until "converged" (many iterations with damping)
   - Sum all node forces: |ΣF| < threshold * N_nodes * typical_force
   - Threshold: average force per node < 1% of initial average force

3. **Momentum conservation**:
   - Total momentum P = Σ(m_i * v_i) for all nodes across all cells
   - For system with no external forces: dP/dt ≈ 0 (modulo damping)
   - Track total momentum vector, verify it stays bounded
   - |P(t)| / |P(0)| < 10 (momentum shouldn't blow up)

4. **Contact area fraction**:
   - Two cells in contact should have reasonable contact area fraction
   - For epithelial cells: contact fraction in [0.01, 0.8]
   - Not 0 (cells are touching) and not 1 (not fully engulfing)

**Step 2: Build and run**

Run: `cd build && ctest -R scientific_contact -V`
Expected: All 4 tests PASS

**Step 3: Commit**

```bash
git add test/test_scientific_benchmarks/contact_validation_benchmark.cpp
git commit -m "feat(benchmarks): implement contact mechanics validation tests"
```

---

### Task 7: Build, run all 15 tests, fix any failures

**Step 1: Full rebuild**

Run: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)`
Expected: Clean build

**Step 2: Run all scientific tests**

Run: `cd build && ctest -R scientific_ --output-on-failure`
Expected: All 15 tests PASS

**Step 3: Fix any failures**

Debug and fix any test that fails. Common issues:
- Threshold too tight (relax if physics justifies it)
- Parameter file path wrong (check PROJECT_SOURCE_DIR)
- Cell initialization issues (verify parameter XML matches compile-time config)

**Step 4: Commit fixes if any**

```bash
git add -A test/test_scientific_benchmarks/
git commit -m "fix(benchmarks): resolve scientific test failures"
```

---

### Task 8: Create Nature paper reproduction parameter files

**Files:**
- Create: `parameters/benchmarks/fig3e_monolayer.xml` (Fig 3e - low γ̃, monolayer)
- Create: `parameters/benchmarks/fig3e_multilayer.xml` (Fig 3e - high γ̃, multilayer)
- Create: `parameters/benchmarks/fig4c_pseudostratified.xml` (Fig 4c-d)
- Create: `parameters/benchmarks/fig1e_scaling.xml` (Fig 1e - 125K cell scaling)

**Step 1: Create Fig 3e parameter files**

Fig 3e from the Nature paper shows the monolayer-to-multilayer transition as a function of the dimensionless surface tension ratio γ̃ = γ_apical/γ_lateral.

- `fig3e_monolayer.xml`: γ̃ < 0.05 (monolayer regime). Uses `fig_3_vesicle.vtk` input mesh.
  Key: γ_apical = 4e-4, γ_lateral = 4e-4 (γ̃ ≈ 1.0, but low enough adhesion to stay monolayer)
  Actually, the transition is controlled by the ratio of surface tension to adhesion.
  Use existing vesicle parameters as base, adjust surface tensions.

- `fig3e_multilayer.xml`: γ̃ > 0.05 (multilayer regime). Higher apical/basal surface tension.

**Step 2: Create Fig 4c-d parameter files**

Pseudostratified epithelium validation. Uses vesicle mesh with nucleus cells enabled.

**Step 3: Create Fig 1e parameter file**

Large-scale simulation (125K cells target). Uses `big_sphere.vtk` with aggressive growth rates.
Note: This is a long-running benchmark (24hr), not a CI test.

**Step 4: Commit**

```bash
git add parameters/benchmarks/
git commit -m "feat(benchmarks): add Nature paper reproduction parameter files"
```

---

### Task 9: Create Nature paper reproduction scripts

**Files:**
- Create: `scripts/benchmarking/scientific/reproduce_fig3e.sh`
- Create: `scripts/benchmarking/scientific/reproduce_fig4cd.sh`
- Create: `scripts/benchmarking/scientific/reproduce_fig1e.sh`

**Step 1: Create Fig 3e reproduction script**

```bash
#!/usr/bin/env bash
# Reproduce Nature paper Figure 3e: Monolayer/Multilayer transition
# Expected: layering transition at γ̃ ≈ 0.05
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
SIMUCELL3D="$PROJECT_ROOT/build/simucell3d"
RESULTS_DIR="$PROJECT_ROOT/simulation_results/nature_fig3e"
# ... runs monolayer and multilayer configs, validates output
```

**Step 2: Create Fig 4c-d reproduction script**

Similar structure, runs pseudostratified epithelium simulation.

**Step 3: Create Fig 1e scaling script**

Long-running benchmark script with progress monitoring.

**Step 4: Commit**

```bash
git add scripts/benchmarking/scientific/
git commit -m "feat(benchmarks): add Nature paper reproduction scripts"
```

---

### Task 10: Create baselines directory and golden master placeholder

**Files:**
- Create: `baselines/nature_paper/.gitkeep`
- Create: `baselines/nature_paper/README.md`

**Step 1: Create baselines directory**

The golden master VTK files will be generated by running the Nature reproduction scripts and stored here via Git LFS (already configured for .vtk files).

**Step 2: Create README with instructions**

Document how to generate and update golden masters.

**Step 3: Commit**

```bash
git add baselines/nature_paper/
git commit -m "feat(benchmarks): create baselines directory for Nature paper golden masters"
```

---

### Task 11: Create scientific validation documentation

**Files:**
- Create: `doc/benchmarking/scientific-validation.md`

**Step 1: Write documentation**

Cover:
- Overview of the 15 scientific tests and their physics rationale
- How to run: `cd build && ctest -R scientific_`
- Physics background for each test category (energy, pressure, geometry, contact)
- Nature paper reproduction instructions
- Threshold justification table
- How to update golden masters

**Step 2: Commit**

```bash
git add doc/benchmarking/scientific-validation.md
git commit -m "docs(benchmarks): add scientific validation guide"
```

---

### Task 12: Final verification - all 15 tests pass

**Step 1: Clean rebuild**

Run: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)`

**Step 2: Run all scientific tests with timing**

Run: `cd build && time ctest -R scientific_ --output-on-failure`
Expected: All 15 tests PASS, total time < 5 minutes

**Step 3: Verify test count**

Run: `cd build && ctest -R scientific_ -N | tail -1`
Expected: "Total Tests: 15"

**Step 4: Final commit**

```bash
git add -A
git commit -m "feat(benchmarks): complete Phase 2 scientific validation suite (15 tests)"
```
