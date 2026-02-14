# SimuCell3D Benchmark Dashboard

## Overview

The benchmark dashboard provides a unified view of simulation performance,
biological accuracy, and regression tracking. It is generated automatically
by CI workflows and can also be produced locally.

## Generating the dashboard locally

```bash
# 1. Run a benchmark
./build/simucell3d parameters/core/parameters_vesicle.xml \
    --schedule=adaptive \
    --diagnostics-csv=benchmark/sim_adaptive/performance_diagnostics.csv

# 2. Preprocess metrics
python scripts/preprocess_benchmark_metrics.py benchmark/

# 3. Generate plots (17 original + 5 new = 22 total)
python scripts/plot_benchmark_unified.py benchmark/ --quality draft --no-latex

# 4. Generate HTML dashboard
cd scripts && python -m ci.generate_dashboard \
    --output ../docs/benchmarks/index.html \
    --plots-dir ../benchmark/plots-unified/ \
    --commit $(git rev-parse HEAD) \
    --branch $(git branch --show-current)
```

## Plot inventory (22 plots)

### Biological Narrative (7 plots)

| ID | Name | Description |
|----|------|-------------|
| 01 | Pressure Evolution | Pressure homeostasis with 95% CI |
| 02 | Energy Landscape | Total energy with conservation testing |
| 02b | Cell Heterogeneity | Per-cell metric distributions |
| 03 | Biological Dashboard | 6-panel biological summary |
| 11 | Population Dynamics | Growth and division rates |
| 14 | Energy Conservation Timeline | Drift detection across runs |
| 16 | Contact Angle Distribution | Contact fraction validation |

### Computational Narrative (10 plots)

| ID | Name | Description |
|----|------|-------------|
| 04 | Scaling Analysis | O(N^4/3) power law regression |
| 05 | Phase Timing | Stacked phase timing breakdown |
| 06 | Roofline Model | Memory vs compute bound analysis |
| 07 | Load Balance | Thread workload distribution |
| 08 | Scheduler Comparison | Time series IPS comparison |
| 09 | Performance Ratio | Bootstrap significance testing |
| 10 | Scheduler Radar | Multi-dimensional scheduler comparison |
| 13 | Cache Efficiency | Time-per-cell vs population size |
| 15 | Roofline Trajectory | Operational intensity over time |
| 17 | IPS Regression Timeline | Historical performance tracking |
