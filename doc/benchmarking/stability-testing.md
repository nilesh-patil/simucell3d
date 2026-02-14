# Stability & Convergence Testing

Tests that verify numerical stability, deterministic reproducibility, and convergence properties of the SimuCell3D solver.

## Test Suite Overview

### Determinism Tests (`test/test_stability/determinism_test.cpp`)

| Test | What it verifies |
|------|-----------------|
| `stability_determinism_single_thread` | Bitwise identical positions after 200 iterations (1 thread, static schedule) |
| `stability_determinism_energy_reproducibility` | Bitwise identical kinetic/pressure/surface tension energies between runs |

These tests use `std::memcmp` for exact equality — any floating-point non-determinism (thread scheduling, reduction order) causes failure.

### Timestep Convergence Tests (`test/test_stability/timestep_convergence_test.cpp`)

| Test | What it verifies |
|------|-----------------|
| `stability_timestep_convergence_order` | Richardson extrapolation gives p in [0.3, 1.7] for centroid positions |
| `stability_timestep_energy_convergence` | Volume errors decrease (or reach noise floor) as dt shrinks |

Runs with dt, dt/2, dt/4 (base dt = 1e-7, duration = 5e-6). Convergence order computed as `p = log2(e_coarse / e_fine)`. Semi-implicit Euler is 1st order, so p ~ 1.0.

### Extreme Parameter Tests (`test/test_stability/extreme_parameters_test.cpp`)

| Test | What it verifies |
|------|-----------------|
| `stability_extreme_zero_bulk_modulus` | Solver completes without crash when `bulk_modulus = 0` |
| `stability_extreme_high_pressure` | No energy explosion with 10x physiological pressure |
| `stability_extreme_degenerate_mesh` | Mesh refinement handles 0.5x min_edge_len without crash |
| `stability_extreme_contact_cutoff_boundary` | Contact cutoff = min_edge_len doesn't cause NaN/Inf |

## Running Tests

```bash
# Build
cmake --build build -j$(nproc)

# All stability tests
ctest -R stability --output-on-failure

# Individual categories
ctest -R stability_determinism
ctest -R stability_timestep
ctest -R stability_extreme
```

## Shell Scripts

### 24-Hour Stability Test

```bash
scripts/benchmarking/stability/run_24hr_test.sh parameters/core/parameters_vesicle.xml \
    --duration-hours 24 --build-dir build
```

Runs a long-duration simulation with periodic memory monitoring. Outputs:
- `memory.csv` — RSS/VSZ time series
- `simulation.log` — Full simulation output
- Summary report with peak memory and growth percentage

### OOM Monitor

```bash
scripts/benchmarking/stability/oom_monitor.sh <PID> \
    --threshold-mb 2048 --interval-sec 10 --output memory.csv
```

Standalone memory monitor that alerts if RSS exceeds threshold or doubles from initial value.

### Valgrind Check

```bash
scripts/benchmarking/stability/run_valgrind_check.sh \
    --build-dir build --test-pattern stability
```

Runs stability tests under Valgrind memcheck with OpenMP/TinyXML suppressions.

## Parameter File

All stability tests use `test/test_scientific_benchmarks/params_single_sphere.xml` (single isolated sphere, no initial triangulation, no edge swap). This configuration is known to be numerically stable with face-face coupling (CONTACT_MODEL_INDEX=2).

## Design Decisions

- **`run_iteration()` instead of `solver::run()`**: Tests call `run_iteration()` for a controlled number of steps rather than running to completion. This avoids the mesh refinement instability that occurs with the 2-sphere test mesh at high iteration counts.
- **Single-sphere parameter file**: The standard `test_parameter_file.xml` (2 spheres with initial triangulation) is inherently unstable with face-face coupling. The single-sphere file provides a stable baseline.
- **Noise floor for convergence**: Volume errors below 1e-17 are at floating-point noise level; the ratio test is skipped in this regime.
- **Zero bulk modulus**: Cells may collapse below `min_vol` and be deleted. The test asserts the solver completes without crash, not that cells survive.
