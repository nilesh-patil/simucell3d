# Parameter Screening Guide

Comprehensive guide to systematic parameter space exploration for matching SimuCell3D simulations to experimental data.

This guide covers grid search, Latin hypercube sampling, Bayesian optimization, and visualization techniques for efficient parameter screening.

---

## Table of Contents

1. [Overview](#overview)
2. [Why Screen Parameters?](#why-screen-parameters)
3. [Quick Start](#quick-start)
4. [Screening Workflows](#screening-workflows)
5. [Parameter Space Visualization](#parameter-space-visualization)
6. [Matching Experimental Data](#matching-experimental-data)
7. [Best Practices](#best-practices)
8. [Tools and Scripts](#tools-and-scripts)
9. [Example Workflows](#example-workflows)
10. [Troubleshooting](#troubleshooting)

---

## Overview

### What is Parameter Screening?

**Parameter screening** is the systematic exploration of parameter space to find values that match experimental observations.

**Key challenges:**
- High-dimensional space (10-20 parameters)
- Expensive simulations (minutes to hours per run)
- Noisy experimental data (biological variability)
- Multiple conflicting objectives (pressure, volume, shape)

**This guide provides:**
- Efficient sampling strategies (grid search, LHS, Bayesian)
- Automated workflow on HPC clusters
- Visualization dashboards for parameter sensitivity
- Objective functions for matching experimental data

---

## Why Screen Parameters?

### Common Use Cases

#### 1. Matching Tissue Morphology

**Goal:** Find parameters that reproduce observed epithelial sheet shape

**Parameters to screen:**
- `surface_tension` - Controls cell-cell adhesion strength
- `bulk_modulus` - Cell compressibility
- `bending_modulus` - Cell membrane stiffness
- `contact_adhesion_strength` - Cell-cell contact energy

**Observables to match:**
- Cell shape (aspect ratio, roundness)
- Tissue thickness
- Cell packing (hexagonal vs irregular)
- Apical/basal area ratio

#### 2. Matching Growth Dynamics

**Goal:** Find growth rate that matches experimental time-lapse

**Parameters to screen:**
- `avg_growth_rate` - Mean volumetric growth rate
- `std_growth_rate` - Growth rate variability
- `avg_division_volume` - Target volume for division
- `std_division_volume` - Division volume variability

**Observables to match:**
- Cell count over time
- Doubling time
- Growth phase duration
- Final cell density

#### 3. Matching Mechanical Response

**Goal:** Find stiffness that matches AFM measurements

**Parameters to screen:**
- `epi_bulk_modulus` - Epithelial cell bulk modulus
- `mes_bulk_modulus` - Mesenchymal cell bulk modulus
- `ecm_bulk_modulus` - ECM bulk modulus
- `damping` - Viscous damping coefficient

**Observables to match:**
- Young's modulus (AFM indentation)
- Stress-strain curves (compression)
- Relaxation time (stress relaxation)
- Viscoelastic ratio

---

## Quick Start

### Method 1: Python Bindings (Recommended)

**Best for:** Small-scale screening (< 100 simulations) on local machine

```python
import simucell3d

# Define parameter grid
bulk_moduli = [1000, 2000, 3000, 4000, 5000]  # Pa
adhesion_strengths = [5e-5, 1e-4, 2e-4, 5e-4]  # J/m²

results = []

for bulk_mod in bulk_moduli:
    for adhesion in adhesion_strengths:
        # Load base parameter file
        params = simucell3d.ParameterSet('parameters_base.xml')

        # Override parameters
        params.set('epi_bulk_modulus', bulk_mod)
        params.set('contact_adhesion_strength', adhesion)

        # Run simulation
        result = simucell3d.run_simulation(params, max_iterations=1000)

        # Extract observables
        final_pressure = result.get_mean_pressure(iteration=-1)
        final_shape = result.get_mean_shape_index(iteration=-1)

        results.append({
            'bulk_modulus': bulk_mod,
            'adhesion': adhesion,
            'pressure': final_pressure,
            'shape_index': final_shape
        })

# Save results
import pandas as pd
df = pd.DataFrame(results)
df.to_csv('screening_results.csv', index=False)
```

### Method 2: HPC Cluster (For Large Screening)

**Best for:** Large-scale screening (100-10,000 simulations) on Slurm/PBS clusters

```bash
# 1. Prepare parameter table (CSV)
cat > parameter_table.csv << EOF
sim_id,input_mesh,bulk_modulus,adhesion
1,data/cube.vtk,1000,5e-5
2,data/cube.vtk,2000,5e-5
3,data/cube.vtk,3000,5e-5
...
EOF

# 2. Launch screening on cluster
cd scripts/parameter_screening/
python3 launch_parameter_screening.py \
    parameter_table.csv \
    my_screening_name \
    /cluster/scratch/username/

# 3. Collect results after jobs complete
python3 collect_screening_data.py /cluster/scratch/username/my_screening_name/
```

---

## Screening Workflows

### Workflow 1: Grid Search (Exhaustive)

**When to use:** Few parameters (2-3), small ranges, need complete coverage

**Pros:**
- Guarantees finding global optimum (within grid resolution)
- Easy to visualize (2D/3D heatmaps)
- No assumptions about parameter relationships

**Cons:**
- Exponential growth in simulations (curse of dimensionality)
- Wastes effort in uninteresting regions
- Example: 5 parameters × 10 values each = 100,000 simulations

**Example: 2D Grid Search**

```python
# Screen bulk_modulus × adhesion_strength
import numpy as np

bulk_moduli = np.linspace(1000, 5000, 20)      # 20 values
adhesions = np.logspace(-5, -3, 20)            # 20 values (log scale)

# Total: 20 × 20 = 400 simulations

parameter_grid = [
    {'bulk_modulus': bm, 'adhesion': adh}
    for bm in bulk_moduli
    for adh in adhesions
]

# Visualize parameter space coverage
import matplotlib.pyplot as plt

fig, ax = plt.subplots()
ax.scatter([p['bulk_modulus'] for p in parameter_grid],
           [p['adhesion'] for p in parameter_grid],
           alpha=0.5)
ax.set_xlabel('Bulk Modulus (Pa)')
ax.set_ylabel('Adhesion Strength (J/m²)')
ax.set_yscale('log')
plt.savefig('grid_search_coverage.png')
```

**Visualization:**

<!-- Dashboard visualization (image not available):
     Figure 1 would show a 2D grid search results heatmap with:
     - X-axis: Bulk Modulus (Pa)
     - Y-axis: Adhesion Strength (J/m²)
     - Color scale: Objective function value (MSE from experimental data)
     - Red/hot colors indicating best parameter fit regions
     Use the code example above to generate your own visualization. -->
*Figure 1: 2D grid search results showing parameter sensitivity heatmap. Color indicates objective function value (e.g., MSE from experimental data). Red regions show best fit.*

### Workflow 2: Latin Hypercube Sampling (Efficient)

**When to use:** Many parameters (5-10), large ranges, limited budget

**Pros:**
- Space-filling design (covers parameter space evenly)
- Works in high dimensions (10+ parameters)
- Only N samples needed for N-dimensional space

**Cons:**
- May miss narrow optimal regions
- Requires surrogate model for optimization
- Harder to visualize than grid search

**Example: 5D Latin Hypercube**

```python
from scipy.stats import qmc

# Define parameter bounds
param_bounds = {
    'bulk_modulus': (1000, 5000),
    'adhesion': (1e-5, 1e-3),
    'surface_tension': (1e-5, 1e-4),
    'bending_modulus': (1e-19, 1e-17),
    'damping': (1e5, 1e7)
}

# Generate 100 samples (efficient for 5D space)
n_samples = 100
sampler = qmc.LatinHypercube(d=len(param_bounds), seed=42)
unit_samples = sampler.random(n=n_samples)

# Scale to parameter bounds
samples = []
for i, (pname, (low, high)) in enumerate(param_bounds.items()):
    scaled = qmc.scale(unit_samples[:, i], low, high)
    samples.append(scaled)

samples = np.array(samples).T

# Create parameter table
import pandas as pd
df = pd.DataFrame(samples, columns=param_bounds.keys())
df['sim_id'] = range(1, n_samples + 1)
df.to_csv('lhs_parameter_table.csv', index=False)

print(f"Generated {n_samples} samples for {len(param_bounds)}D parameter space")
print(f"Grid search would need {10**len(param_bounds)} samples for similar coverage!")
```

**Visualization:**

<!-- Dashboard visualization (image not available):
     Figure 2 would show Latin hypercube sampling results with:
     - Parallel coordinates plot for 5D parameter space
     - Each vertical axis represents one parameter (normalized 0-1)
     - Lines connecting parameter values for each simulation
     - Color-coded by objective function value (green=good, red=poor)
     Use the parallel coordinates plotting code in "Parameter Space Visualization" section to create your own. -->
*Figure 2: Latin hypercube sampling results for 5D parameter space. Parallel coordinates plot shows parameter relationships. Color indicates objective function value.*

### Workflow 3: Bayesian Optimization (Adaptive)

**When to use:** Expensive simulations, smooth objective landscape, sequential budget

**Pros:**
- Exploits surrogate model (Gaussian process) to guide search
- Balances exploration (uncertain regions) vs exploitation (known good regions)
- Typically finds optimum in 10-50 iterations

**Cons:**
- Requires sequential execution (no parallelization)
- Assumes smooth objective function
- Can get stuck in local optima

**Example: Bayesian Optimization**

```python
from bayes_opt import BayesianOptimization

def objective_function(bulk_modulus, adhesion, surface_tension):
    """
    Run simulation with given parameters and return objective value.
    Lower is better (e.g., MSE from experimental data).
    """
    # Load base parameters
    params = simucell3d.ParameterSet('parameters_base.xml')
    params.set('epi_bulk_modulus', bulk_modulus)
    params.set('contact_adhesion_strength', adhesion)
    params.set('surface_tension', surface_tension)

    # Run simulation
    result = simucell3d.run_simulation(params, max_iterations=1000)

    # Compute MSE from experimental data
    exp_pressure = 1200  # Pa (from experiment)
    exp_shape_index = 3.8

    sim_pressure = result.get_mean_pressure(iteration=-1)
    sim_shape_index = result.get_mean_shape_index(iteration=-1)

    mse = (sim_pressure - exp_pressure)**2 + (sim_shape_index - exp_shape_index)**2

    # Bayesian optimization maximizes, so return negative MSE
    return -mse

# Define parameter bounds
pbounds = {
    'bulk_modulus': (1000, 5000),
    'adhesion': (1e-5, 1e-3),
    'surface_tension': (1e-5, 1e-4)
}

# Initialize optimizer
optimizer = BayesianOptimization(
    f=objective_function,
    pbounds=pbounds,
    random_state=42,
    verbose=2
)

# Run optimization (typically 20-50 iterations)
optimizer.maximize(
    init_points=5,    # Random exploration first
    n_iter=20         # Then adaptive search
)

print("Best parameters found:")
print(optimizer.max)

# Save optimization history
history = pd.DataFrame(optimizer.res)
history.to_csv('bayesian_optimization_history.csv', index=False)
```

**Visualization:**

<!-- Dashboard visualization (image not available):
     Figure 3 would show Bayesian optimization convergence with:
     - Top panel: Best objective function value vs iteration number (should show convergence)
     - Bottom panel: Acquisition function value highlighting exploration/exploitation balance
     - X-axis: Iteration number (0-50 typical)
     - Demonstrates how algorithm converges to optimal parameters
     Bayesian optimization libraries (e.g., bayes_opt) typically include built-in plotting utilities. -->
*Figure 3: Bayesian optimization convergence plot. Top: Objective function value over iterations (converging to optimum). Bottom: Acquisition function showing exploration vs exploitation trade-off.*

---

## Parameter Space Visualization

### 1D Sensitivity Analysis

**Purpose:** Understand effect of single parameter while holding others fixed

```python
import matplotlib.pyplot as plt

# Vary bulk_modulus, fix all others
bulk_moduli = np.linspace(1000, 5000, 20)
pressures = []

for bm in bulk_moduli:
    # Run simulation (pseudocode)
    result = run_simulation(bulk_modulus=bm)
    pressures.append(result.mean_pressure)

# Plot
fig, ax = plt.subplots()
ax.plot(bulk_moduli, pressures, marker='o')
ax.set_xlabel('Bulk Modulus (Pa)')
ax.set_ylabel('Mean Pressure (Pa)')
ax.axhline(1200, color='red', linestyle='--', label='Experimental target')
ax.legend()
plt.savefig('1d_sensitivity_bulk_modulus.png')
```

### 2D Heatmaps

**Purpose:** Visualize interaction between two parameters

```python
import seaborn as sns

# Load screening results
df = pd.read_csv('screening_results.csv')

# Pivot for heatmap
pivot = df.pivot(index='bulk_modulus', columns='adhesion', values='pressure')

# Plot heatmap
fig, ax = plt.subplots(figsize=(8, 6))
sns.heatmap(pivot, cmap='viridis', annot=True, fmt='.0f', ax=ax)
ax.set_title('Mean Pressure (Pa) vs Bulk Modulus & Adhesion')
plt.savefig('2d_heatmap_pressure.png', dpi=300)
```

### Parallel Coordinates Plot

**Purpose:** Visualize high-dimensional parameter relationships

```python
from pandas.plotting import parallel_coordinates

# Load results
df = pd.read_csv('screening_results.csv')

# Normalize parameters to [0, 1] for visualization
df_norm = df.copy()
for col in df.columns[:-1]:  # All except objective
    df_norm[col] = (df[col] - df[col].min()) / (df[col].max() - df[col].min())

# Color by objective value
df_norm['objective_category'] = pd.cut(df['objective'], bins=5, labels=['worst', 'bad', 'ok', 'good', 'best'])

# Plot
fig, ax = plt.subplots(figsize=(12, 6))
parallel_coordinates(df_norm, 'objective_category', colormap='RdYlGn', ax=ax)
ax.set_ylabel('Normalized Parameter Value')
ax.legend(loc='upper right')
plt.savefig('parallel_coordinates.png', dpi=300)
```

### Contour Plots

**Purpose:** Show objective function landscape in 2D

```python
from scipy.interpolate import griddata

# Load results
df = pd.read_csv('screening_results.csv')

# Create regular grid for interpolation
bulk_grid = np.linspace(df['bulk_modulus'].min(), df['bulk_modulus'].max(), 100)
adhesion_grid = np.linspace(df['adhesion'].min(), df['adhesion'].max(), 100)
BM, ADH = np.meshgrid(bulk_grid, adhesion_grid)

# Interpolate objective values onto grid
objective_grid = griddata(
    (df['bulk_modulus'], df['adhesion']),
    df['objective'],
    (BM, ADH),
    method='cubic'
)

# Contour plot
fig, ax = plt.subplots(figsize=(8, 6))
contour = ax.contourf(BM, ADH, objective_grid, levels=20, cmap='viridis')
ax.scatter(df['bulk_modulus'], df['adhesion'], c='red', s=10, alpha=0.3, label='Samples')
ax.set_xlabel('Bulk Modulus (Pa)')
ax.set_ylabel('Adhesion Strength (J/m²)')
plt.colorbar(contour, label='Objective Function')
ax.legend()
plt.savefig('contour_objective.png', dpi=300)
```

---

## Matching Experimental Data

### Objective Functions

#### Mean Squared Error (MSE)

**Use case:** Match quantitative measurements (pressure, volume, cell count)

```python
def mse_objective(sim_result, exp_data):
    """
    Compute MSE between simulation and experiment.
    Lower is better.
    """
    mse = 0.0

    # Match mean pressure
    sim_pressure = sim_result.get_mean_pressure()
    exp_pressure = exp_data['mean_pressure']
    mse += (sim_pressure - exp_pressure)**2

    # Match cell count
    sim_cells = sim_result.get_cell_count()
    exp_cells = exp_data['cell_count']
    mse += (sim_cells - exp_cells)**2

    return mse
```

#### Weighted MSE (Multiple Observables)

**Use case:** Multiple observables with different units and importance

```python
def weighted_mse_objective(sim_result, exp_data, weights):
    """
    Weighted MSE for multiple observables.
    """
    wmse = 0.0

    # Pressure (Pa) - high priority
    sim_pressure = sim_result.get_mean_pressure()
    exp_pressure = exp_data['mean_pressure']
    wmse += weights['pressure'] * ((sim_pressure - exp_pressure) / exp_pressure)**2

    # Volume (m³) - medium priority
    sim_volume = sim_result.get_mean_volume()
    exp_volume = exp_data['mean_volume']
    wmse += weights['volume'] * ((sim_volume - exp_volume) / exp_volume)**2

    # Cell count - low priority (integer, wide tolerance)
    sim_cells = sim_result.get_cell_count()
    exp_cells = exp_data['cell_count']
    wmse += weights['cell_count'] * ((sim_cells - exp_cells) / exp_cells)**2

    return wmse

# Example weights
weights = {
    'pressure': 10.0,     # Most important
    'volume': 1.0,        # Moderate importance
    'cell_count': 0.1     # Least important (integer, noisy)
}
```

#### Time-Series MSE

**Use case:** Match time-lapse data (growth curves, morphogenesis dynamics)

```python
def timeseries_mse_objective(sim_result, exp_data):
    """
    MSE over time series.
    """
    # Extract time series
    sim_times = sim_result.get_times()
    sim_pressures = sim_result.get_mean_pressure_timeseries()

    exp_times = exp_data['times']
    exp_pressures = exp_data['pressures']

    # Interpolate simulation to experimental time points
    from scipy.interpolate import interp1d
    sim_interp = interp1d(sim_times, sim_pressures, bounds_error=False, fill_value='extrapolate')
    sim_at_exp_times = sim_interp(exp_times)

    # Compute MSE
    mse = np.mean((sim_at_exp_times - exp_pressures)**2)

    return mse
```

### Uncertainty Quantification

**Important:** Experimental data has uncertainty - account for it!

```python
def robust_objective_with_uncertainty(sim_result, exp_data):
    """
    Objective function accounting for experimental uncertainty.
    """
    # Experimental mean ± std
    exp_pressure_mean = exp_data['pressure_mean']
    exp_pressure_std = exp_data['pressure_std']

    # Simulation value
    sim_pressure = sim_result.get_mean_pressure()

    # Z-score: how many standard deviations away?
    z_score = (sim_pressure - exp_pressure_mean) / exp_pressure_std

    # Penalize based on z-score (squared for smoothness)
    objective = z_score**2

    return objective
```

---

## Best Practices

### 1. Start with Literature Values

**Don't screen blindly** - use published parameter ranges:

```python
# Literature-informed parameter bounds (from SimuCell3D paper)
param_ranges = {
    'epi_bulk_modulus': (1000, 5000),          # Pa (epithelial cells)
    'mes_bulk_modulus': (500, 2500),           # Pa (mesenchymal cells)
    'surface_tension': (1e-5, 1e-4),           # J/m² (cell-cell adhesion)
    'bending_modulus': (1e-19, 1e-17),         # J·m (membrane stiffness)
    'contact_adhesion': (1e-5, 5e-4),          # J/m² (cell-cell contact)
    'damping': (1e5, 1e7),                     # Pa·s (viscous damping)
    'avg_growth_rate': (1e-12, 1e-10),         # m³/s (volumetric growth)
    'time_step': (1e-8, 1e-6)                  # s (numerical stability)
}
```

### 2. Screen One Parameter at a Time (Initially)

**Avoid curse of dimensionality** - understand single-parameter effects first:

```python
# Phase 1: Individual parameter sensitivity (8 × 10 = 80 simulations)
for param_name, (low, high) in param_ranges.items():
    values = np.linspace(low, high, 10)
    run_1d_screen(param_name, values)

# Phase 2: Identify most sensitive parameters (e.g., bulk_modulus, adhesion)

# Phase 3: 2D grid search on top 2 parameters (20 × 20 = 400 simulations)
run_2d_grid_search('bulk_modulus', 'adhesion')

# Phase 4: Refine with Bayesian optimization (50 simulations)
run_bayesian_optimization(['bulk_modulus', 'adhesion', 'surface_tension'])
```

### 3. Validate on Multiple Observables

**Don't overfit to single metric** - ensure physical consistency:

```python
def multi_objective_validation(sim_result, exp_data):
    """
    Check multiple observables, not just one.
    """
    checks = {}

    # Primary objective (what you're optimizing)
    checks['pressure_match'] = abs(sim_result.pressure - exp_data.pressure) < 100  # Pa

    # Secondary checks (constraints)
    checks['volume_physiological'] = 2.5e-16 < sim_result.volume < 1.3e-15  # m³
    checks['energy_conserved'] = sim_result.energy_drift < 0.01  # <1% drift
    checks['no_crashes'] = sim_result.completed_successfully

    # All checks must pass
    all_valid = all(checks.values())

    return all_valid, checks
```

### 4. Check Physical Consistency

**Sanity checks** to avoid unphysical parameter combinations:

```python
def validate_physical_consistency(params):
    """
    Reject unphysical parameter combinations.
    """
    # Bulk modulus should be > surface tension (compressibility check)
    if params['bulk_modulus'] < params['surface_tension'] * 1e4:
        return False, "Bulk modulus too low relative to surface tension"

    # Time step must satisfy CFL condition
    max_velocity = 1e-6  # m/s (typical)
    min_edge_len = params['min_edge_len']
    max_time_step = min_edge_len / max_velocity
    if params['time_step'] > max_time_step:
        return False, f"Time step {params['time_step']} exceeds CFL limit {max_time_step}"

    # Damping should be sufficient for stability
    if params['damping'] < params['bulk_modulus'] * 1e-2:
        return False, "Damping too low, simulation may be unstable"

    return True, "Physically consistent"
```

### 5. Use Resume-able Workflows

**Checkpoint progress** for long-running screenings:

```python
# Save partial results after each simulation
results_file = 'screening_results.csv'

# Check if file exists (resuming)
if os.path.exists(results_file):
    df = pd.read_csv(results_file)
    completed_ids = set(df['sim_id'])
    print(f"Resuming from {len(completed_ids)} completed simulations")
else:
    df = pd.DataFrame()
    completed_ids = set()

# Run only uncompleted simulations
for sim_id, params in enumerate(parameter_table):
    if sim_id in completed_ids:
        continue  # Skip already completed

    result = run_simulation(params)

    # Append result immediately (don't wait for all to finish)
    new_row = pd.DataFrame([result])
    df = pd.concat([df, new_row], ignore_index=True)
    df.to_csv(results_file, index=False)
```

---

## Tools and Scripts

### HPC Cluster Scripts (Slurm)

Located in `scripts/parameter_screening/`

#### 1. Setup (One-Time)

```bash
# Load modules (Euler cluster example)
env2lmod
module load gcc/9.3.0 openblas/0.3.20 python/3.11.2 cmake/3.25.0

# Create virtual environment
cd scripts/parameter_screening/
python -m venv --system-site-packages venv_screening

# Activate
source ./venv_screening/bin/activate

# Install dependencies
OPENBLAS=$OPENBLAS_ROOT/lib/libopenblas.so pip install --ignore-installed --no-deps pandas==1.4.4
```

#### 2. Prepare Parameter Table

**CSV format:**

```csv
sim_id,input_mesh,time_step,damping,epi_bulk_modulus,surface_tension
1,./data/cube.vtk,1e-7,1e6,2500,5e-5
2,./data/cube.vtk,1e-7,1e6,5000,5e-5
3,./data/cube.vtk,1e-7,1e6,7500,5e-5
```

**Important:** Each row = one simulation job

#### 3. Launch Screening

```bash
# Load modules
env2lmod
module load gcc/9.3.0 openblas/0.3.20 python/3.11.2 cmake/3.25.0

# Activate venv
cd scripts/parameter_screening/
source ./venv_screening/bin/activate

# Launch
python3 launch_parameter_screening.py \
    parameter_table.csv \
    my_screening_name \
    /cluster/scratch/username/output_dir/

# This submits N jobs (one per row) with:
# - 4 CPUs per job
# - 28GB RAM (7GB per CPU)
# - 120 hour time limit
```

**Customize job resources** by editing `launch_parameter_screening.py`:

```python
# In launch_parameter_screening.py, modify:
sbatch_cmd = f"sbatch -n 1 --cpus-per-task=8 --time=72:00:00 --mem-per-cpu=8192"
#                                        ^^        ^^                  ^^^^
#                                      CPUs    Hours                RAM/CPU
```

#### 4. Monitor Progress

```bash
# Check job status
squeue -u username

# Check completed jobs
ls /cluster/scratch/username/output_dir/my_screening_name/*/simulation_statistics.csv | wc -l
```

#### 5. Collect Results

```bash
# After all jobs complete
python3 collect_screening_data.py /cluster/scratch/username/output_dir/my_screening_name/

# Creates summary/ folder with last timestep from each simulation
ls /cluster/scratch/username/output_dir/my_screening_name/summary/
```

### Python Bindings (Local Screening)

**For smaller screenings on local machine:**

See [Python Bindings Guide](./python-bindings.md) for complete API documentation.

---

## Example Workflows

### Example 1: Match Epithelial Sheet Pressure

**Goal:** Find bulk modulus and adhesion that produce mean pressure = 1200 Pa

```python
# 1. Define parameter grid (2D)
bulk_moduli = [1000, 2000, 3000, 4000, 5000]
adhesions = [5e-5, 1e-4, 2e-4, 5e-4, 1e-3]

# 2. Run simulations
results = []
for bm in bulk_moduli:
    for adh in adhesions:
        result = run_simulation(bulk_modulus=bm, adhesion=adh)
        pressure = result.mean_pressure
        error = abs(pressure - 1200)

        results.append({
            'bulk_modulus': bm,
            'adhesion': adh,
            'pressure': pressure,
            'error': error
        })

# 3. Find best match
df = pd.DataFrame(results)
best = df.loc[df['error'].idxmin()]

print(f"Best parameters:")
print(f"  Bulk modulus: {best['bulk_modulus']} Pa")
print(f"  Adhesion: {best['adhesion']:.2e} J/m²")
print(f"  Resulting pressure: {best['pressure']:.1f} Pa")
print(f"  Error: {best['error']:.1f} Pa")

# 4. Visualize
pivot = df.pivot(index='bulk_modulus', columns='adhesion', values='pressure')
sns.heatmap(pivot, annot=True, fmt='.0f', cmap='viridis')
plt.title('Mean Pressure (Pa)')
plt.savefig('pressure_heatmap.png')
```

### Example 2: Match Growth Curve

**Goal:** Find growth rate that matches experimental cell count over time

```python
# Experimental data (from time-lapse microscopy)
exp_times = [0, 12, 24, 36, 48]  # hours
exp_cell_counts = [64, 78, 95, 115, 140]  # cells

# Convert to seconds
exp_times_sec = [t * 3600 for t in exp_times]

# Objective function
def objective(growth_rate):
    result = run_simulation(avg_growth_rate=growth_rate, max_time=48*3600)

    # Extract cell count time series
    sim_times = result.get_times()
    sim_cell_counts = result.get_cell_counts()

    # Interpolate to experimental time points
    from scipy.interpolate import interp1d
    sim_interp = interp1d(sim_times, sim_cell_counts, kind='linear')
    sim_at_exp = sim_interp(exp_times_sec)

    # Compute MSE
    mse = np.mean((sim_at_exp - exp_cell_counts)**2)
    return mse

# Screen growth rates
growth_rates = np.logspace(-12, -10, 20)
errors = [objective(gr) for gr in growth_rates]

# Best growth rate
best_idx = np.argmin(errors)
best_growth_rate = growth_rates[best_idx]

print(f"Best growth rate: {best_growth_rate:.2e} m³/s")

# Plot
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5))

# Growth rate sensitivity
ax1.plot(growth_rates, errors, marker='o')
ax1.set_xscale('log')
ax1.set_xlabel('Growth Rate (m³/s)')
ax1.set_ylabel('MSE (cells²)')
ax1.axvline(best_growth_rate, color='red', linestyle='--', label='Best')
ax1.legend()

# Best fit vs experiment
result_best = run_simulation(avg_growth_rate=best_growth_rate, max_time=48*3600)
ax2.plot(exp_times, exp_cell_counts, 'ro-', label='Experiment')
ax2.plot(result_best.times / 3600, result_best.cell_counts, 'b-', label='Simulation (best fit)')
ax2.set_xlabel('Time (hours)')
ax2.set_ylabel('Cell Count')
ax2.legend()

plt.tight_layout()
plt.savefig('growth_curve_fit.png')
```

---

## Troubleshooting

### Screening Jobs Crash

**Problem:** Many jobs fail with segfault or timeout

**Diagnosis:**
```bash
# Check logs
tail -50 /cluster/scratch/username/my_screening_name/1/simulation.log

# Common errors:
# - Segfault: mesh refinement runaway (increase min_edge_len)
# - Timeout: time_step too small (increase to 5e-7 or 1e-6)
# - OOM: too many cells (reduce max_iterations or increase RAM)
```

**Solutions:**
```bash
# Increase min_edge_len to prevent refinement explosion
<min_edge_len>1.0e-6</min_edge_len>  <!-- Was 5e-7 -->

# Increase time_step to reach target time faster
<time_step>5.0e-7</time_step>  <!-- Was 1e-7 -->

# Request more RAM per job
sbatch --mem-per-cpu=16384  # 16GB per CPU
```

### Objective Function Not Converging

**Problem:** Bayesian optimization or grid search doesn't find good parameters

**Diagnosis:**
```python
# Plot objective function landscape
plt.scatter(df['bulk_modulus'], df['objective'])
plt.xlabel('Bulk Modulus')
plt.ylabel('Objective')
plt.show()

# Check for:
# - Flat landscape (no sensitivity) → change observable
# - Multiple minima (multimodal) → use global optimizer
# - Noise (rough surface) → run longer simulations
```

**Solutions:**
- **Flat landscape:** Parameter doesn't affect observable much, try different parameter
- **Multimodal:** Use grid search or multi-start Bayesian optimization
- **Noisy:** Increase `max_iterations` to reduce stochastic noise

### Unphysical Results

**Problem:** Best parameters produce weird simulation behavior

**Diagnosis:**
```python
# Check parameter values
print(f"Bulk modulus: {best_params['bulk_modulus']}")
print(f"Surface tension: {best_params['surface_tension']}")
print(f"Ratio: {best_params['bulk_modulus'] / best_params['surface_tension']}")

# Expected: ratio > 10,000 (bulk >> surface)
# If ratio < 1000, cells will be too soft
```

**Solutions:**
- Add physical constraints to optimization
- Manually inspect simulation videos of "best" parameters
- Check energy conservation (should be < 1% drift)

---

## Summary

### Key Takeaways

1. **Start simple:** 1D sensitivity → 2D grid → high-D optimization
2. **Use literature values:** Don't screen blindly, use published ranges
3. **Validate on multiple observables:** Don't overfit to single metric
4. **Check physical consistency:** Reject unphysical combinations
5. **Visualize parameter space:** Heatmaps, parallel coordinates, contours

### Recommended Workflow

```
1. Define observables (what to match)
   ↓
2. Identify sensitive parameters (literature + 1D screening)
   ↓
3. Choose sampling strategy (grid for 2-3D, LHS for 4-8D, Bayesian for >8D)
   ↓
4. Run screening (local for <100 sims, HPC for >100 sims)
   ↓
5. Visualize results (heatmaps, dashboards, convergence plots)
   ↓
6. Validate best parameters (multiple observables, physical consistency)
   ↓
7. Refine if needed (zoom into promising region)
```

### Next Steps

- **Python API**: [python-bindings.md](./python-bindings.md) for local screening
- **Benchmarking**: [openmp-benchmarks.md](../benchmarking/openmp-benchmarks.md) for performance
- **Parameter reference**: [parameter-reference.md](./parameter-reference.md) for all parameters
