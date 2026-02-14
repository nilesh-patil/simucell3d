# Visualization Package Guide

Comprehensive guide to the **SimuCell3D Visualization Package** (`simucell3d_viz`) - a Python library for publication-quality benchmark visualization and statistical analysis.

This package turns raw simulation CSV files into 17 publication-ready plots with automatic data validation, cleaning, and statistical testing.

---

## Table of Contents

1. [Overview](#overview)
2. [Installation](#installation)
3. [Quick Start](#quick-start)
4. [Package Structure](#package-structure)
5. [Data Loading](#data-loading)
6. [Statistical Analysis](#statistical-analysis)
7. [Visualization Examples](#visualization-examples)
8. [Advanced Usage](#advanced-usage)
9. [Troubleshooting](#troubleshooting)

---

## Overview

### What is simucell3d_viz?

A Python package providing:

1. **Automatic scheduler detection** - Supports any number of schedulers (v1, static, adaptive, custom)
2. **Data validation & cleaning** - Removes invalid data with audit trail
3. **17 publication-quality plots** - Biological, computational, and statistical narratives
4. **Statistical analysis** - Bootstrap CI, hypothesis tests, regression diagnostics
5. **CVD-safe color palettes** - Colorblind-friendly plots
6. **Tufte-style design** - Minimal ink, maximum clarity

### Key Features

| Feature | Description |
|---------|-------------|
| **Auto-validation** | Detects missing columns, invalid values, outliers |
| **Audit trail** | JSON log of all cleaning decisions |
| **Type-safe API** | Dataclasses prevent runtime errors |
| **Flexible plotting** | Generate all 17 plots or select specific ones |
| **Publication-ready** | 300 DPI, PDF export, journal-standard dimensions |
| **Statistical rigor** | Bootstrap resampling, permutation tests, regression |

### When to Use This Package

**Use simucell3d_viz when you:**
- Ran benchmarks with `run_parallel_benchmark_comparison.sh`
- Have CSV files in `metrics/*/` directories
- Want publication-quality visualizations
- Need statistical validation of performance claims
- Want to compare multiple schedulers

**Don't use this for:**
- Single simulation visualization (use ParaView for VTK files)
- Real-time monitoring (use `parallel_benchmark_monitor.sh`)
- Interactive exploration (use Jupyter with pandas/seaborn)

---

## Installation

### Prerequisites

**Python 3.8+** with the following packages:

```bash
# Core dependencies
pip install pandas numpy scipy matplotlib seaborn

# Or use conda
conda install pandas numpy scipy matplotlib seaborn
```

### Package Installation

The package is located at `scripts/simucell3d_viz/` and doesn't need installation - it's used directly:

```bash
# Run from scripts/ directory
cd scripts/
python plot_benchmark_unified.py ../doc/working/parallel_benchmark_TIMESTAMP/
```

### Verify Installation

```bash
# Check dependencies
python -c "import pandas, numpy, scipy, matplotlib, seaborn; print('✓ All dependencies installed')"

# Run validation test
python plot_benchmark_unified.py --help
```

### Optional Dependencies

```bash
# For LaTeX rendering in plots (publication quality)
sudo apt-get install texlive-latex-base texlive-fonts-recommended cm-super dvipng

# For faster numerical operations
pip install numba

# For interactive notebooks
pip install jupyter ipywidgets
```

---

## Quick Start

### Basic Usage (CLI)

```bash
# Generate all 17 plots
python plot_benchmark_unified.py /path/to/benchmark_dir/

# Output: benchmark_dir/plots-unified/*.png
```

### Publication Quality

```bash
# 300 DPI, PDF output, optimized for paper submission
python plot_benchmark_unified.py benchmark_dir/ --quality publication

# Output: benchmark_dir/plots-unified/*.pdf
```

### Selective Plotting

```bash
# Generate only biological plots (01-04)
python plot_benchmark_unified.py benchmark_dir/ --plots biological

# Generate specific plots by ID
python plot_benchmark_unified.py benchmark_dir/ --plots 01,05,13
```

### Validation Only

```bash
# Check data quality without generating plots
python plot_benchmark_unified.py benchmark_dir/ --validate-only

# Output: validation_report.json
```

---

## Package Structure

### Directory Layout

```
simucell3d_viz/
├── __init__.py              # Package entry point
├── data/                    # Data loading and validation
│   ├── accessor.py          # Type-safe data access API
│   ├── cleaner.py           # Data cleaning with audit trail
│   ├── scheduler_detector.py # Automatic scheduler detection
│   └── validator.py         # Data validation rules
├── stats/                   # Statistical analysis
│   ├── confidence_intervals.py   # Bootstrap CI computation
│   ├── hypothesis_tests.py       # Permutation tests, energy conservation
│   └── regression_diagnostics.py # Linear models, Cook's distance
└── utils/                   # Utility functions
    └── time_utils.py        # Timestamp parsing, time series alignment
```

### Module Overview

#### data.accessor

**Purpose:** Type-safe, validated access to benchmark data

**Key classes:**
- `SchedulerDataAccessor` - Main API for data retrieval
- `TimeSeriesData` - Validated time series with uncertainty
- `StatsSummary` - Statistical summary of data columns

**Example:**
```python
from simucell3d_viz.data.accessor import SchedulerDataAccessor

accessor = SchedulerDataAccessor(data, 'adaptive', 'computational')
ts = accessor.get_time_series(['ips', 'iterations_per_second'], min_points=10)

if ts:
    print(f"Mean IPS: {ts.values.mean():.1f}")
    print(f"Std IPS: {ts.std.mean():.1f}")
```

#### data.validator

**Purpose:** Validate benchmark data structure and content

**Key functions:**
- `validate_benchmark(benchmark_dir)` - Check directory structure, CSV files, column names
- `ValidationReport` - Dataclass with validation results (errors, warnings)

**Example:**
```python
from simucell3d_viz.data.validator import validate_benchmark

report = validate_benchmark('/path/to/benchmark/')

if report.is_valid:
    print("✓ Data is valid")
else:
    print(f"✗ Validation failed: {report.errors}")
```

#### data.cleaner

**Purpose:** Clean invalid data with audit trail

**Key functions:**
- `load_and_clean_data(benchmark_dir)` - Load CSVs, remove invalid values, generate audit trail
- `AuditTrail` - JSON log of cleaning decisions

**Example:**
```python
from simucell3d_viz.data.cleaner import load_and_clean_data

data = load_and_clean_data('/path/to/benchmark/')

# Check audit trail
audit = data['audit_trail']
print(f"Removed {audit.num_invalid_rows} invalid rows")
print(f"Cleaned {audit.num_outliers} outliers")
```

#### stats.confidence_intervals

**Purpose:** Bootstrap confidence interval computation

**Key functions:**
- `compute_parametric_ci(data, confidence=0.95)` - Parametric CI (assumes normality)
- `compute_bootstrap_ci(data, n_bootstrap=1000, confidence=0.95)` - Non-parametric bootstrap

**Example:**
```python
from simucell3d_viz.stats.confidence_intervals import compute_bootstrap_ci

mean, ci_lower, ci_upper = compute_bootstrap_ci(ips_data, n_bootstrap=1000)
print(f"Mean IPS: {mean:.1f} [{ci_lower:.1f}, {ci_upper:.1f}]")
```

#### stats.hypothesis_tests

**Purpose:** Statistical hypothesis testing

**Key functions:**
- `bootstrap_permutation_test(group1, group2, n_perm=10000)` - Non-parametric test
- `test_energy_conservation(energy_series, threshold=0.01)` - Check energy drift

**Example:**
```python
from simucell3d_viz.stats.hypothesis_tests import bootstrap_permutation_test

# Test if adaptive is faster than v1.0
p_value, effect_size = bootstrap_permutation_test(adaptive_ips, v1_ips)

if p_value < 0.05:
    print(f"✓ Adaptive is significantly faster (p={p_value:.4f}, d={effect_size:.2f})")
```

#### stats.regression_diagnostics

**Purpose:** Linear regression validation

**Key functions:**
- `compute_power_law_fit(x, y)` - Fit power law relationship
- `compute_cooks_distance(model, threshold=0.5)` - Identify influential outliers

**Example:**
```python
from simucell3d_viz.stats.regression_diagnostics import compute_power_law_fit

# Fit IPS ~ cell_count^alpha
alpha, r_squared, residuals = compute_power_law_fit(cell_counts, ips_values)
print(f"Scaling exponent: {alpha:.2f}, R²: {r_squared:.3f}")
```

---

## Data Loading

### Benchmark Data Structure

The visualization package expects this directory structure:

```
benchmark_dir/
├── metrics/
│   ├── v1/
│   │   ├── biological.csv
│   │   ├── computational.csv
│   │   ├── phase_timings.csv
│   │   └── workload.csv
│   ├── static/
│   │   └── (same files)
│   ├── adaptive/
│   │   └── (same files)
│   └── comparison.csv
└── README.txt
```

### CSV File Formats

#### biological.csv

**Columns:**
- `iteration` - Simulation iteration number
- `timestamp` - Epoch seconds
- `avg_pressure` - Mean cell pressure (Pa)
- `std_pressure` - Std dev of pressure
- `avg_volume` - Mean cell volume (m³)
- `total_energy` - System total energy (J)

#### computational.csv

**Columns:**
- `iteration` - Simulation iteration number
- `timestamp` - Epoch seconds
- `ips` - Iterations per second
- `thread_efficiency` - Actual/ideal speedup
- `speedup` - vs single-threaded baseline

#### phase_timings.csv

**Columns:**
- `iteration` - Simulation iteration number
- `contact_time` - Contact detection time (s)
- `integration_time` - Force integration time (s)
- `refinement_time` - Mesh refinement time (s)
- `io_time` - Output writing time (s)

#### workload.csv

**Columns:**
- `iteration` - Simulation iteration number
- `num_cells` - Current cell count
- `num_triangles` - Total triangle count
- `num_divisions` - Cumulative cell divisions

### Loading Data Programmatically

```python
import sys
from pathlib import Path

# Add package to path
sys.path.insert(0, str(Path('scripts/')))

from simucell3d_viz.data.cleaner import load_and_clean_data
from simucell3d_viz.data.scheduler_detector import detect_schedulers

# Load benchmark data
benchmark_dir = Path('doc/working/parallel_benchmark_20260127_192747/')
data = load_and_clean_data(benchmark_dir)

# Detect schedulers
schedulers = detect_schedulers(data)
print(f"Found {len(schedulers)} schedulers: {[s.name for s in schedulers]}")

# Access data for specific scheduler
adaptive_bio = data['adaptive']['biological']
print(f"Adaptive IPS: {adaptive_bio['ips'].mean():.1f}")
```

### Handling Missing Data

The cleaner automatically handles missing data:

```python
# Missing columns are detected
# Invalid values (NaN, Inf, negative pressure) are removed
# Audit trail records all decisions

audit = data['audit_trail']

print(f"Cleaning summary:")
print(f"  Valid rows: {audit.num_valid_rows}")
print(f"  Invalid rows removed: {audit.num_invalid_rows}")
print(f"  Outliers removed: {audit.num_outliers}")
print(f"  Columns imputed: {audit.num_imputed_columns}")

# Save audit trail
audit_path = benchmark_dir / 'plots-unified' / 'audit_trail.json'
audit.save(audit_path)
```

---

## Statistical Analysis

### Bootstrap Confidence Intervals

**When to use:** Estimate uncertainty in mean IPS without assuming normality

```python
from simucell3d_viz.stats.confidence_intervals import compute_bootstrap_ci

# Compute 95% CI with 1000 bootstrap resamples
mean_ips, ci_lower, ci_upper = compute_bootstrap_ci(
    ips_data,
    n_bootstrap=1000,
    confidence=0.95,
    random_seed=42
)

print(f"Mean IPS: {mean_ips:.1f}")
print(f"95% CI: [{ci_lower:.1f}, {ci_upper:.1f}]")

# Interpretation:
# - Narrow CI → precise estimate
# - CI not overlapping with baseline → statistically significant improvement
```

### Hypothesis Testing

**When to use:** Test if adaptive is significantly faster than v1.0

```python
from simucell3d_viz.stats.hypothesis_tests import bootstrap_permutation_test

# Test H0: no difference in IPS
# H1: adaptive IPS > v1.0 IPS
p_value, effect_size = bootstrap_permutation_test(
    group1=adaptive_ips,
    group2=v1_ips,
    n_permutations=10000,
    alternative='greater',  # one-tailed test
    random_seed=42
)

print(f"p-value: {p_value:.4f}")
print(f"Effect size (Cohen's d): {effect_size:.2f}")

# Interpretation:
# - p < 0.05 → reject H0 (statistically significant)
# - |d| > 0.8 → large effect (practically significant)

if p_value < 0.05 and effect_size > 0.8:
    print("✓ Adaptive is significantly and substantially faster")
```

### Energy Conservation Check

**When to use:** Validate that scheduler doesn't affect physics

```python
from simucell3d_viz.stats.hypothesis_tests import test_energy_conservation

# Check energy drift < 1%
drift_percent, is_conserved, p_value = test_energy_conservation(
    energy_series=total_energy,
    threshold=0.01  # 1% drift threshold
)

print(f"Energy drift: {drift_percent:.2f}%")
print(f"Conserved: {is_conserved}")
print(f"p-value: {p_value:.4f}")

# Interpretation:
# - |drift| < 1% → excellent conservation
# - p > 0.05 → no significant drift (good)
# - All schedulers should pass
```

### Regression Diagnostics

**When to use:** Validate IPS ~ cell_count model

```python
from simucell3d_viz.stats.regression_diagnostics import (
    compute_power_law_fit,
    compute_cooks_distance
)

# Fit power law: IPS = a * cell_count^alpha
alpha, r_squared, residuals = compute_power_law_fit(
    x=cell_counts,
    y=ips_values
)

print(f"Scaling exponent: {alpha:.2f}")
print(f"R²: {r_squared:.3f}")

# Check for influential outliers
cooks_d = compute_cooks_distance(residuals)
outliers = np.where(cooks_d > 0.5)[0]

if len(outliers) == 0:
    print("✓ No influential outliers")
else:
    print(f"⚠ {len(outliers)} outliers with Cook's D > 0.5")
```

---

## Visualization Examples

### Example 1: Pressure Evolution Plot

**Purpose:** Show mean pressure ± 95% CI over time

```python
import matplotlib.pyplot as plt
from simucell3d_viz.data.accessor import SchedulerDataAccessor
from simucell3d_viz.stats.confidence_intervals import add_ci_band

# Load data for adaptive scheduler
accessor = SchedulerDataAccessor(data, 'adaptive', 'biological')
pressure_ts = accessor.get_time_series(
    value_columns=['avg_pressure', 'pressure'],
    std_columns=['std_pressure'],
    min_points=10
)

# Plot with confidence band
fig, ax = plt.subplots(figsize=(7, 5))
ax.plot(pressure_ts.time, pressure_ts.values, label='Adaptive', color='#2E86AB')

if pressure_ts.std is not None:
    add_ci_band(ax, pressure_ts.time, pressure_ts.values, pressure_ts.std, alpha=0.2)

ax.set_xlabel('Time (s)')
ax.set_ylabel('Pressure (Pa)')
ax.legend()
plt.tight_layout()
plt.savefig('pressure_evolution.png', dpi=300)
```

### Example 2: Scheduler Comparison Boxplot

**Purpose:** Compare IPS distribution across schedulers

```python
import seaborn as sns

# Extract IPS for all schedulers
ips_data = []
for sched_name in ['v1', 'static', 'adaptive']:
    accessor = SchedulerDataAccessor(data, sched_name, 'computational')
    ips_ts = accessor.get_time_series(['ips'], min_points=10)
    if ips_ts:
        ips_data.extend([
            {'Scheduler': sched_name, 'IPS': val}
            for val in ips_ts.values
        ])

df = pd.DataFrame(ips_data)

# Boxplot with CVD-safe colors
fig, ax = plt.subplots(figsize=(7, 5))
sns.boxplot(data=df, x='Scheduler', y='IPS', palette='colorblind', ax=ax)
ax.set_title('Scheduler Performance Comparison')
plt.tight_layout()
plt.savefig('scheduler_comparison.png', dpi=300)
```

### Example 3: Scaling Analysis

**Purpose:** Show IPS vs cell count for each scheduler

```python
# Collect data
scaling_data = []
for sched_name in ['v1', 'static', 'adaptive']:
    comp = data[sched_name]['computational']
    work = data[sched_name]['workload']

    scaling_data.extend([
        {
            'Scheduler': sched_name,
            'Cell Count': cells,
            'IPS': ips
        }
        for cells, ips in zip(work['num_cells'], comp['ips'])
        if cells > 0 and ips > 0
    ])

df = pd.DataFrame(scaling_data)

# Scatter plot with regression lines
fig, ax = plt.subplots(figsize=(7, 5))
for sched, group in df.groupby('Scheduler'):
    ax.scatter(group['Cell Count'], group['IPS'], label=sched, alpha=0.5)

    # Fit power law
    alpha, r2, _ = compute_power_law_fit(group['Cell Count'], group['IPS'])
    x_fit = np.logspace(np.log10(group['Cell Count'].min()),
                         np.log10(group['Cell Count'].max()), 100)
    y_fit = group['IPS'].mean() * (x_fit / group['Cell Count'].mean())**alpha
    ax.plot(x_fit, y_fit, '--', label=f'{sched} fit (α={alpha:.2f})')

ax.set_xscale('log')
ax.set_yscale('log')
ax.set_xlabel('Cell Count')
ax.set_ylabel('Iterations/sec')
ax.legend()
plt.tight_layout()
plt.savefig('scaling_analysis.png', dpi=300)
```

---

## Advanced Usage

### Custom Plot Styling

```python
# Use CVD-safe color palette
from cycler import cycler

colors = ['#2E86AB', '#A23B72', '#F18F01', '#C73E1D', '#6A994E']
plt.rc('axes', prop_cycle=cycler('color', colors))

# Tufte-style minimal design
plt.rc('axes', linewidth=0.5, edgecolor='#333333')
plt.rc('xtick', direction='out')
plt.rc('ytick', direction='out')
plt.rc('grid', linewidth=0.5, alpha=0.3, linestyle=':')
```

### Batch Processing

```python
from pathlib import Path

# Process multiple benchmarks
benchmark_dirs = Path('doc/working/').glob('parallel_benchmark_*/')

for bench_dir in benchmark_dirs:
    print(f"Processing {bench_dir.name}...")

    # Generate plots
    !python plot_benchmark_unified.py {bench_dir} --quality publication --plots 01,05,13

    print(f"✓ Plots saved to {bench_dir}/plots-unified/")
```

### Jupyter Integration

```python
# In Jupyter notebook
%matplotlib inline

from simucell3d_viz.data.cleaner import load_and_clean_data

# Load data interactively
data = load_and_clean_data('path/to/benchmark/')

# Explore interactively
df = data['adaptive']['computational']
df.describe()

# Quick plot
df.plot(x='iteration', y='ips', figsize=(10, 6), title='Adaptive IPS over time')
```

---

## Troubleshooting

### Module Import Errors

**Problem:**
```
ModuleNotFoundError: No module named 'simucell3d_viz'
```

**Solution:**
```bash
# Ensure you're running from scripts/ directory
cd scripts/
python plot_benchmark_unified.py ../doc/working/benchmark_dir/

# Or add to PYTHONPATH
export PYTHONPATH="${PYTHONPATH}:$(pwd)/scripts"
```

### Missing CSV Files

**Problem:**
```
FileNotFoundError: metrics/adaptive/biological.csv not found
```

**Solution:**
```bash
# Check benchmark structure
ls -R path/to/benchmark/metrics/

# If metrics/ is empty, simulation didn't write output
# Check parameter file: <writing_frequency> should be > 0

# Re-extract metrics from simulation output
python preprocess_benchmark_metrics.py path/to/benchmark/
```

### Invalid Data Errors

**Problem:**
```
ValueError: All data is invalid after cleaning
```

**Solution:**
```bash
# Run validation to diagnose
python plot_benchmark_unified.py benchmark_dir/ --validate-only

# Check validation_report.json
cat benchmark_dir/plots-unified/validation_report.json

# Common issues:
# - Simulation crashed early (< 10 data points)
# - Column names don't match expected (check CSV headers)
# - All values are NaN/Inf (simulation diverged)
```

### Plot Rendering Issues

**Problem:** Plots look wrong, labels overlap, axes incorrect

**Solution:**
```bash
# Try different backend
export MPLBACKEND=Agg  # Non-interactive backend

# Clear matplotlib cache
rm -rf ~/.cache/matplotlib/

# Update matplotlib
pip install --upgrade matplotlib

# Use preview quality for debugging
python plot_benchmark_unified.py benchmark_dir/ --quality preview
```

---

## Summary

### Key Takeaways

1. **simucell3d_viz** automates benchmark visualization with 17 publication-quality plots
2. **Data validation & cleaning** ensures robust analysis with audit trail
3. **Statistical rigor** provides bootstrap CI, hypothesis tests, regression diagnostics
4. **Type-safe API** (`SchedulerDataAccessor`) prevents runtime errors
5. **CVD-safe & Tufte-style** design for accessible, minimal-ink plots

### Common Workflows

**Workflow 1: Generate all plots (default)**
```bash
python plot_benchmark_unified.py benchmark_dir/
```

**Workflow 2: Publication-quality subset**
```bash
python plot_benchmark_unified.py benchmark_dir/ \
    --quality publication \
    --plots 01,05,13,14
```

**Workflow 3: Validate before plotting**
```bash
python plot_benchmark_unified.py benchmark_dir/ --validate-only
# Check validation_report.json
python plot_benchmark_unified.py benchmark_dir/ --plots biological
```

### Next Steps

- **Benchmarking guide**: [doc/benchmarking/openmp-benchmarks.md](../benchmarking/openmp-benchmarks.md)
- **Performance tuning**: [doc/developer/performance-tuning.md](../developer/performance-tuning.md)
- **Python bindings**: [doc/user-guide/python-bindings.md](./python-bindings.md)
