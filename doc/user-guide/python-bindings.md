# Python Bindings Guide

Comprehensive guide to running SimuCell3D simulations from Python using the pybind11-based API.

The Python bindings enable programmatic parameter sweeps, batch simulations, and integration with analysis pipelines - perfect for parameter screening and optimization workflows.

---

## Table of Contents

1. [Overview](#overview)
2. [Installation](#installation)
3. [Quick Start](#quick-start)
4. [Core API Reference](#core-api-reference)
5. [New Features (version-cpp-next)](#new-features-version-cpp-next)
6. [Batch Simulations](#batch-simulations)
7. [Integration with Visualization Package](#integration-with-visualization-package)
8. [Performance Optimization](#performance-optimization)
9. [Troubleshooting](#troubleshooting)
10. [Examples](#examples)

---

## Overview

### What Are Python Bindings?

SimuCell3D provides **Python bindings** using pybind11, allowing you to:

- Launch simulations programmatically from Python scripts
- Set parameters without XML files (or override XML parameters)
- Run parameter sweeps and batch simulations
- Retrieve results as pandas DataFrames
- Integrate with scientific Python stack (NumPy, SciPy, Matplotlib)
- Build automated workflows (HPC job submission, parameter optimization)

### When to Use Python vs CLI

| Use Case | Python Bindings | CLI (simucell3d binary) |
|----------|-----------------|-------------------------|
| **Single simulation** | ❌ Overkill | ✅ Recommended |
| **Parameter sweep (<100 sims)** | ✅ Ideal | ❌ Manual scripting needed |
| **Large-scale screening (>100 sims)** | ✅ Yes (local) | ✅ Yes (HPC cluster) |
| **Optimization algorithms** | ✅ Ideal | ❌ Not suitable |
| **Interactive exploration** | ✅ Jupyter notebooks | ❌ Not interactive |
| **Production pipelines** | ✅ Scriptable | ✅ Also scriptable (bash) |

### Compatibility

- **Python versions**: 3.8, 3.9, 3.10, 3.11 (tested with 3.10.6)
- **Operating systems**: Linux, macOS, Windows (WSL)
- **Dependencies**: NumPy, pandas (optional but recommended)
- **Backward compatibility**: 100% compatible with v1.0 code

---

## Installation

### Step 1: Build with Python Bindings Enabled

```bash
# Navigate to build directory
cd /path/to/SimuCell3D/build

# Configure with Python bindings enabled
cmake \
    -DENABLE_PYTHON_BINDINGS=TRUE \
    -DPYTHON_EXECUTABLE=$(which python3) \
    -DCMAKE_BUILD_TYPE=Release \
    ..

# Build (parallel compilation)
make -j$(nproc)
```

**Important:** The Python bindings will be built into `build/bin/python_bindings/`

### Step 2: Verify Installation

```bash
# Check if bindings were built
ls build/bin/python_bindings/simucell3d_python_wrapper*

# Expected output (platform-dependent):
# simucell3d_python_wrapper.cpython-310-x86_64-linux-gnu.so  (Linux)
# simucell3d_python_wrapper.cpython-310-darwin.so            (macOS)
```

### Step 3: Test Import

```python
import sys
from pathlib import Path

# Add bindings to Python path
sys.path.insert(0, str(Path('build/bin/python_bindings').resolve()))

# Import (should not raise error)
import simucell3d_python_wrapper as simucell3d
print("✓ SimuCell3D Python bindings imported successfully")
```

### Step 4: Install Python Dependencies (Optional)

```bash
# For data analysis and visualization
pip install pandas numpy matplotlib scipy

# For Jupyter integration
pip install jupyter ipywidgets

# For parameter optimization
pip install scikit-optimize bayesian-optimization
```

---

## Quick Start

### Minimal Example

```python
import sys
from os import path
sys.path.insert(0, "build/bin/python_bindings")
import simucell3d_python_wrapper as simucell3d

# 1. Create global parameters
global_params = simucell3d.global_simulation_parameters()
global_params.output_folder_path_ = "simulation_results"
global_params.input_mesh_path_ = "data/input_meshes/cube.vtk"
global_params.time_step_ = 1e-7
global_params.simulation_duration_ = 1e-5
global_params.sampling_period_ = 1e-6

# 2. Create cell type
cell_type = simucell3d.cell_type_parameters()
cell_type.name_ = "epithelial"
cell_type.global_type_id_ = 0
cell_type.bulk_modulus_ = 2500  # Pa
cell_type.avg_growth_rate_ = 1e-11  # m³/s

# 3. Create face type
face_type = simucell3d.face_type_parameters()
face_type.name_ = "lateral"
face_type.face_type_global_id_ = 0
face_type.surface_tension_ = 5e-5  # J/m²

cell_type.add_face_type(face_type)

# 4. Launch simulation
wrapper = simucell3d.simucell3d_wrapper(
    global_params,
    [cell_type],
    nb_threads=-1,  # Use all available cores
    verbose=True
)

# 5. Check results
outputs = wrapper.get_simulation_outputs()
if outputs.RETURN_CODE_ == 0:
    print("✓ Simulation completed successfully")
else:
    print(f"✗ Simulation failed with code {outputs.RETURN_CODE_}")
```

### Reading Results as DataFrame

```python
from io import StringIO
import pandas as pd

def get_cell_statistics_data(simulation_outputs):
    """Convert cell statistics to pandas DataFrame."""
    csv_string = simulation_outputs.cell_statistics_data_string_
    df = pd.read_csv(StringIO(csv_string))
    return df

# After simulation completes
outputs = wrapper.get_simulation_outputs()
df = get_cell_statistics_data(outputs)

# Analyze results
print(f"Final cell count: {df['cell_count'].iloc[-1]}")
print(f"Mean pressure: {df['avg_pressure'].mean():.1f} Pa")
print(f"Mean volume: {df['avg_volume'].mean():.2e} m³")
```

---

## Core API Reference

### Global Simulation Parameters

```python
global_params = simucell3d.global_simulation_parameters()

# Required parameters
global_params.output_folder_path_ = "simulation_results"  # Output directory
global_params.input_mesh_path_ = "data/cube.vtk"          # Initial mesh
global_params.time_step_ = 1e-7                           # Integration time step (s)
global_params.simulation_duration_ = 1e-5                 # Total simulation time (s)
global_params.sampling_period_ = 1e-6                     # Output writing period (s)

# Numerical parameters
global_params.damping_coefficient_ = 1e6                  # Damping (Pa·s)
global_params.min_edge_len_ = 5e-7                        # Min edge length (m)
global_params.contact_cutoff_adhesion_ = 2e-7             # Contact cutoff (m)
global_params.contact_cutoff_repulsion_ = 2e-7            # Repulsion cutoff (m)

# Optional parameters (with defaults)
global_params.enable_edge_swap_ = False                   # Edge swap operations
global_params.enable_face_subdivision_ = True             # Face subdivision
global_params.enable_face_collapse_ = True                # Face collapse
```

### Cell Type Parameters

```python
cell_type = simucell3d.cell_type_parameters()

# Identity
cell_type.name_ = "epithelial"                            # Cell type name
cell_type.global_type_id_ = 0                             # Unique ID (0, 1, 2, ...)

# Mechanical properties
cell_type.mass_density_ = 1e3                             # Density (kg/m³)
cell_type.bulk_modulus_ = 2500                            # Bulk modulus (Pa)
cell_type.max_pressure_ = 5000                            # Max pressure (Pa)

# Growth and division
cell_type.avg_growth_rate_ = 1e-11                        # Mean growth rate (m³/s)
cell_type.std_growth_rate_ = 0                            # Std dev growth rate (m³/s)
cell_type.avg_division_vol_ = 1.4e-15                     # Mean division volume (m³)
cell_type.std_division_vol_ = 0                           # Std dev division volume (m³)
cell_type.min_vol_ = 5e-17                                # Minimum volume (m³)

# Shape control
cell_type.target_isoperimetric_ratio_ = 150               # Target shape (sphere ≈ 1)
cell_type.area_elasticity_modulus_ = 0                    # Area constraint (Pa)

# Cortex properties
cell_type.cortex_thickness_ = 1.9e-7                      # Cortex thickness (m)
cell_type.cortex_dynamic_viscosity_ = 1e-2                # Viscosity (Pa·s)
```

### Face Type Parameters

```python
face_type = simucell3d.face_type_parameters()

# Identity
face_type.name_ = "apical"                                # Face type name
face_type.face_type_global_id_ = 0                        # Unique ID (0, 1, 2, ...)

# Mechanical properties
face_type.adherence_strength_ = 2.2e9                     # Adhesion energy density (Pa)
face_type.repulsion_strength_ = 1e9                       # Repulsion energy density (Pa)
face_type.surface_tension_ = 5e-5                         # Surface tension (J/m²)
face_type.bending_modulus_ = 1e-18                        # Bending rigidity (J·m)

# Add face type to cell type
cell_type.add_face_type(face_type)
```

### Simulation Wrapper

```python
wrapper = simucell3d.simucell3d_wrapper(
    global_simulation_parameters,   # Global params object
    [cell_type_1, cell_type_2],     # List of cell types
    nb_threads=-1,                  # Thread count (-1 = auto-detect)
    write_cell_stats_in_string=True, # Return stats as string (for DataFrame)
    verbose=True                     # Print progress messages
)

# Get outputs after simulation
outputs = wrapper.get_simulation_outputs()

# Check success
if outputs.RETURN_CODE_ == 0:
    # Simulation completed successfully
    stats_df = get_cell_statistics_data(outputs)
else:
    # Simulation failed
    print(f"Error code: {outputs.RETURN_CODE_}")
```

---

## New Features (version-cpp-next)

### Adaptive Scheduling

**NEW:** The current branch supports adaptive OpenMP scheduling for 1.5x-4.5x speedup.

**How to enable from Python:**

Unfortunately, the Python bindings don't yet expose the `--schedule` flag directly. **Workaround**:

**Option 1: Set environment variable before import**

```python
import os
# Set adaptive scheduling (current branch default)
os.environ['OMP_SCHEDULE'] = 'dynamic'  # Or 'guided', 'static'
os.environ['OMP_NUM_THREADS'] = '8'     # Explicit thread count

# Then import and run
import simucell3d_python_wrapper as simucell3d
# ... run simulation
```

**Option 2: Use subprocess with CLI flag**

```python
import subprocess

# For maximum control, use CLI with --schedule flag
cmd = [
    './build/simucell3d',
    'parameters.xml',
    '--schedule=adaptive',
    '--diagnostics-csv=perf.csv'
]
result = subprocess.run(cmd, capture_output=True, text=True)
print(result.stdout)
```

**Note:** Direct API support for `--schedule` is planned for a future update.

### Performance Diagnostics

**NEW:** Export per-iteration performance metrics to CSV.

**Workaround (until API support):**

```python
# Option 1: Use CLI wrapper
import subprocess

def run_with_diagnostics(param_file, output_csv='diagnostics.csv'):
    """Run simulation with performance diagnostics."""
    cmd = ['./build/simucell3d', param_file, f'--diagnostics-csv={output_csv}']
    subprocess.run(cmd, check=True)

    # Load diagnostics
    import pandas as pd
    diag_df = pd.read_csv(output_csv)
    return diag_df

# Use it
diag = run_with_diagnostics('parameters.xml', 'perf.csv')
print(f"Mean IPS: {diag['ips'].mean():.1f}")
print(f"Thread efficiency: {diag['thread_efficiency'].mean():.2f}")
```

### Thread Count Control

**IMPROVED:** Better thread detection and control.

```python
# Automatic detection (recommended)
wrapper = simucell3d.simucell3d_wrapper(
    params, cell_types,
    nb_threads=-1,  # Detects available cores (respects cgroups, Slurm)
    verbose=True
)

# Manual override
wrapper = simucell3d.simucell3d_wrapper(
    params, cell_types,
    nb_threads=8,   # Force 8 threads
    verbose=True
)

# Check actual thread count used
import os
print(f"Threads used: {os.environ.get('OMP_NUM_THREADS', 'auto')}")
```

---

## Batch Simulations

### Parameter Sweep Example

```python
import numpy as np
import pandas as pd

# Define parameter ranges
bulk_moduli = np.linspace(1000, 5000, 10)
adhesion_strengths = np.logspace(-5, -3, 10)

results = []

for i, bulk_mod in enumerate(bulk_moduli):
    for j, adhesion in enumerate(adhesion_strengths):
        print(f"Running simulation {i*10 + j + 1}/100...")

        # Set parameters
        cell_type = simucell3d.cell_type_parameters()
        cell_type.bulk_modulus_ = bulk_mod

        face_type = simucell3d.face_type_parameters()
        face_type.surface_tension_ = adhesion
        cell_type.add_face_type(face_type)

        # Run simulation
        try:
            wrapper = simucell3d.simucell3d_wrapper(
                global_params, [cell_type], verbose=False
            )
            outputs = wrapper.get_simulation_outputs()

            if outputs.RETURN_CODE_ == 0:
                df = get_cell_statistics_data(outputs)

                # Extract final state
                results.append({
                    'bulk_modulus': bulk_mod,
                    'adhesion': adhesion,
                    'final_pressure': df['avg_pressure'].iloc[-1],
                    'final_volume': df['avg_volume'].iloc[-1],
                    'final_cells': df['cell_count'].iloc[-1],
                    'success': True
                })
            else:
                results.append({
                    'bulk_modulus': bulk_mod,
                    'adhesion': adhesion,
                    'success': False
                })
        except Exception as e:
            print(f"Error: {e}")
            results.append({'bulk_modulus': bulk_mod, 'adhesion': adhesion, 'success': False})

# Save results
results_df = pd.DataFrame(results)
results_df.to_csv('parameter_sweep_results.csv', index=False)
print(f"Completed {results_df['success'].sum()}/{len(results_df)} simulations successfully")
```

### Parallel Batch Execution

```python
from multiprocessing import Pool

def run_single_simulation(params):
    """Run one simulation with given parameters."""
    bulk_mod, adhesion = params

    # Setup (reuse global_params, modify cell/face types)
    cell_type = simucell3d.cell_type_parameters()
    cell_type.bulk_modulus_ = bulk_mod
    # ... configure other parameters

    wrapper = simucell3d.simucell3d_wrapper(
        global_params, [cell_type],
        nb_threads=4,  # Use 4 threads per simulation
        verbose=False
    )

    outputs = wrapper.get_simulation_outputs()
    return {'bulk_modulus': bulk_mod, 'adhesion': adhesion, 'success': outputs.RETURN_CODE_ == 0}

# Run 8 simulations in parallel (each using 4 threads)
param_combinations = [(bm, adh) for bm in bulk_moduli for adh in adhesion_strengths]

with Pool(processes=8) as pool:
    results = pool.map(run_single_simulation, param_combinations)

results_df = pd.DataFrame(results)
results_df.to_csv('parallel_sweep_results.csv', index=False)
```

---

## Integration with Visualization Package

### Analyze Python Simulation Results

```python
# After running simulation via Python bindings
outputs = wrapper.get_simulation_outputs()
df = get_cell_statistics_data(outputs)

# Save to CSV for visualization package
df.to_csv('simulation_results/biological.csv', index=False)

# Generate visualizations using CLI
import subprocess
subprocess.run([
    'python', 'scripts/plot_benchmark_unified.py',
    'simulation_results/',
    '--plots', 'biological'
])

print("Plots saved to simulation_results/plots-unified/")
```

### Integrated Workflow Example

```python
import simucell3d_python_wrapper as simucell3d
import pandas as pd
import matplotlib.pyplot as plt

# 1. Run simulation
wrapper = simucell3d.simucell3d_wrapper(global_params, [cell_type], verbose=True)
outputs = wrapper.get_simulation_outputs()
df = get_cell_statistics_data(outputs)

# 2. Quick analysis with pandas
print("Simulation summary:")
print(df[['iteration', 'avg_pressure', 'avg_volume', 'cell_count']].describe())

# 3. Plot with matplotlib
fig, axes = plt.subplots(2, 2, figsize=(12, 10))

# Pressure over time
axes[0, 0].plot(df['iteration'], df['avg_pressure'])
axes[0, 0].set_ylabel('Pressure (Pa)')
axes[0, 0].set_xlabel('Iteration')

# Volume over time
axes[0, 1].plot(df['iteration'], df['avg_volume'])
axes[0, 1].set_ylabel('Volume (m³)')
axes[0, 1].set_xlabel('Iteration')

# Cell count over time
axes[1, 0].plot(df['iteration'], df['cell_count'])
axes[1, 0].set_ylabel('Cell Count')
axes[1, 0].set_xlabel('Iteration')

# Energy over time
axes[1, 1].plot(df['iteration'], df['total_energy'])
axes[1, 1].set_ylabel('Total Energy (J)')
axes[1, 1].set_xlabel('Iteration')

plt.tight_layout()
plt.savefig('simulation_analysis.png', dpi=300)
print("Analysis plots saved to simulation_analysis.png")
```

---

## Performance Optimization

### Thread Configuration

```python
import os

# Set OpenMP environment variables BEFORE importing simucell3d
os.environ['OMP_NUM_THREADS'] = '8'          # Use 8 threads
os.environ['OMP_PROC_BIND'] = 'close'        # Bind threads to cores
os.environ['OMP_PLACES'] = 'cores'           # Use physical cores only

# Now import and run
import simucell3d_python_wrapper as simucell3d
# ... run simulation
```

### Memory Management

```python
# For large batch simulations, explicitly clean up
for i, params in enumerate(parameter_list):
    wrapper = simucell3d.simucell3d_wrapper(global_params, [cell_type], verbose=False)
    outputs = wrapper.get_simulation_outputs()

    # Process results
    df = get_cell_statistics_data(outputs)
    results.append(extract_metrics(df))

    # Clean up (Python GC will handle this, but can be explicit)
    del wrapper, outputs, df

    if i % 10 == 0:
        import gc
        gc.collect()  # Force garbage collection every 10 simulations
```

### Reducing I/O Overhead

```python
# Set write_cell_stats_in_string=True to avoid disk writes
wrapper = simucell3d.simucell3d_wrapper(
    global_params, [cell_type],
    write_cell_stats_in_string=True,  # Keep results in memory (faster)
    verbose=False  # Suppress stdout (faster)
)

# For batch simulations, only write final results
# Not intermediate VTK files (controlled by sampling_period)
```

---

## Troubleshooting

### Import Error

**Problem:**
```python
ModuleNotFoundError: No module named 'simucell3d_python_wrapper'
```

**Solution:**
```python
# 1. Check if bindings were built
import os
print(os.path.exists('build/bin/python_bindings'))

# 2. If False, rebuild with -DENABLE_PYTHON_BINDINGS=TRUE
# cd build && cmake -DENABLE_PYTHON_BINDINGS=TRUE .. && make -j$(nproc)

# 3. Add to path explicitly
import sys
sys.path.insert(0, os.path.abspath('build/bin/python_bindings'))
import simucell3d_python_wrapper as simucell3d
```

### Simulation Crashes

**Problem:** Simulation exits with non-zero return code

**Diagnosis:**
```python
outputs = wrapper.get_simulation_outputs()
print(f"Return code: {outputs.RETURN_CODE_}")

# Common error codes:
# 0: Success
# 1: General error
# -11: Segmentation fault (SIGSEGV)
```

**Common causes:**
1. **Mesh refinement runaway** → Increase `min_edge_len_`
2. **Time step too large** → Reduce `time_step_`
3. **Unphysical parameters** → Check bulk modulus, surface tension

**Solution:**
```python
# Add error handling
try:
    wrapper = simucell3d.simucell3d_wrapper(params, [cell_type], verbose=True)
    outputs = wrapper.get_simulation_outputs()

    if outputs.RETURN_CODE_ != 0:
        print(f"Simulation failed with code {outputs.RETURN_CODE_}")
        # Try adjusting parameters
        params.min_edge_len_ *= 2  # Increase to prevent refinement
        params.time_step_ *= 0.5   # Decrease for stability
        # ... retry
except Exception as e:
    print(f"Exception caught: {e}")
```

### DataFrame Parsing Error

**Problem:** `get_cell_statistics_data()` fails with CSV parsing error

**Solution:**
```python
def get_cell_statistics_data_safe(simulation_outputs):
    """Robust DataFrame extraction with error handling."""
    try:
        csv_string = simulation_outputs.cell_statistics_data_string_

        if not csv_string or len(csv_string) < 10:
            print("Warning: Empty or invalid CSV string")
            return pd.DataFrame()  # Return empty DataFrame

        df = pd.read_csv(StringIO(csv_string))
        return df
    except Exception as e:
        print(f"Error parsing cell statistics: {e}")
        return pd.DataFrame()

# Use robust version
df = get_cell_statistics_data_safe(outputs)
if df.empty:
    print("No data returned - simulation may have crashed early")
```

---

## Examples

### Example 1: Parameter Optimization with SciPy

```python
from scipy.optimize import minimize
import simucell3d_python_wrapper as simucell3d

# Target experimental values
target_pressure = 1200  # Pa
target_volume = 8e-16   # m³

def objective_function(params):
    """Objective: minimize difference from experimental values."""
    bulk_modulus, adhesion = params

    # Setup simulation
    cell_type = simucell3d.cell_type_parameters()
    cell_type.bulk_modulus_ = bulk_modulus

    face_type = simucell3d.face_type_parameters()
    face_type.surface_tension_ = adhesion
    cell_type.add_face_type(face_type)

    # Run simulation
    wrapper = simucell3d.simucell3d_wrapper(
        global_params, [cell_type], verbose=False
    )
    outputs = wrapper.get_simulation_outputs()

    if outputs.RETURN_CODE_ != 0:
        return 1e10  # Penalty for failed simulation

    # Extract results
    df = get_cell_statistics_data(outputs)
    sim_pressure = df['avg_pressure'].iloc[-1]
    sim_volume = df['avg_volume'].iloc[-1]

    # Compute error (weighted MSE)
    error = (sim_pressure - target_pressure)**2 + \
            1e12 * (sim_volume - target_volume)**2  # Weight volume more

    return error

# Initial guess
x0 = [2500, 5e-5]  # bulk_modulus, adhesion

# Optimize
result = minimize(
    objective_function,
    x0,
    method='Nelder-Mead',
    bounds=[(1000, 5000), (1e-5, 1e-3)]
)

print(f"Optimal parameters:")
print(f"  Bulk modulus: {result.x[0]:.1f} Pa")
print(f"  Adhesion: {result.x[1]:.2e} J/m²")
print(f"  Error: {result.fun:.2e}")
```

### Example 2: Jupyter Notebook Integration

```python
# In Jupyter notebook

%matplotlib inline
import simucell3d_python_wrapper as simucell3d
import pandas as pd
import matplotlib.pyplot as plt

# Interactive parameter widget
from ipywidgets import interact, FloatSlider

@interact(
    bulk_modulus=FloatSlider(min=1000, max=5000, step=500, value=2500),
    adhesion=FloatSlider(min=1e-5, max=1e-3, step=1e-5, value=5e-5)
)
def run_and_plot(bulk_modulus, adhesion):
    """Interactive simulation with real-time plotting."""

    # Setup and run
    cell_type = simucell3d.cell_type_parameters()
    cell_type.bulk_modulus_ = bulk_modulus

    face_type = simucell3d.face_type_parameters()
    face_type.surface_tension_ = adhesion
    cell_type.add_face_type(face_type)

    wrapper = simucell3d.simucell3d_wrapper(
        global_params, [cell_type], verbose=False
    )
    outputs = wrapper.get_simulation_outputs()
    df = get_cell_statistics_data(outputs)

    # Plot
    fig, ax = plt.subplots(figsize=(10, 6))
    ax.plot(df['iteration'], df['avg_pressure'], label='Pressure')
    ax.set_xlabel('Iteration')
    ax.set_ylabel('Pressure (Pa)')
    ax.set_title(f'Bulk={bulk_modulus:.0f} Pa, Adhesion={adhesion:.2e} J/m²')
    plt.legend()
    plt.show()

    print(f"Final pressure: {df['avg_pressure'].iloc[-1]:.1f} Pa")
    print(f"Final cell count: {df['cell_count'].iloc[-1]}")
```

---

## Summary

### Key Takeaways

1. **Python bindings enable programmatic control** - Set parameters, run simulations, retrieve results
2. **100% backward compatible with v1.0** - All v1.0 code still works
3. **New features available via workarounds** - Adaptive scheduling, diagnostics (full API support coming)
4. **Ideal for parameter sweeps** - Batch simulations, optimization, screening
5. **Integrates with scientific Python** - pandas, NumPy, SciPy, Matplotlib, Jupyter

### Best Practices

- ✅ Use `write_cell_stats_in_string=True` for batch simulations (faster)
- ✅ Set `verbose=False` for parameter sweeps (cleaner output)
- ✅ Handle exceptions and check `RETURN_CODE_` for robustness
- ✅ Use `nb_threads=-1` for automatic thread detection
- ✅ Set OpenMP environment variables before import for performance control

### Next Steps

- **Parameter screening**: [parameter-screening.md](./parameter-screening.md) for optimization workflows
- **Visualization**: [visualization.md](./visualization.md) for analyzing Python results
- **Performance tuning**: [../developer/performance-tuning.md](../developer/performance-tuning.md) for OpenMP optimization

---

## API Roadmap (Future Enhancements)

**Planned additions** (not yet implemented):

```python
# Future API (not available yet)
wrapper = simucell3d.simucell3d_wrapper(
    params, cell_types,
    schedule_mode='adaptive',      # PLANNED: Direct adaptive scheduling
    diagnostics_csv='perf.csv',    # PLANNED: Performance export
    collision_detection='sap',     # PLANNED: Algorithm selection
    checkpoint_interval=100        # PLANNED: Checkpoint/resume
)
```

**Workarounds** until implemented: Use CLI via subprocess for these features.
