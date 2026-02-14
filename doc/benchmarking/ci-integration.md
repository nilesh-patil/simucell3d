# CI/CD Benchmark Integration Guide

## Overview

SimuCell3D uses three automated benchmark workflows to track performance:

| Workflow | Trigger | Duration | Purpose |
|----------|---------|----------|---------|
| `benchmark-pr.yml` | Pull requests to main | <5 min | Quick regression check |
| `benchmark-nightly.yml` | Daily at 2 AM UTC | <2 hr | Comprehensive multi-scheduler |
| `benchmark-weekly.yml` | Weekly (Sunday midnight) | <24 hr | Stress testing |

## Architecture

```
PR/Nightly/Weekly Trigger
    |
    +-- Build SimuCell3D (Release)
    +-- Run simulation with --diagnostics-csv
    +-- Preprocess metrics (preprocess_benchmark_metrics.py)
    +-- Ingest into SQLite (ci/aggregate_results.py)
    +-- Detect regressions (ci/regression_detector.py)
    +-- Generate plots (plot_benchmark_unified.py)
    +-- Generate dashboard (ci/generate_dashboard.py)
```

## Regression Detection

The regression detector uses **bootstrap permutation testing** with dual thresholds:

1. **Statistical significance**: p-value < 0.05 (10,000 bootstrap iterations)
2. **Practical significance**: >10% performance change

Both conditions must be met to flag a regression. This prevents false alarms from
natural performance variance while catching meaningful slowdowns.

### How it works

1. Latest benchmark IPS values are compared against the previous 5 runs (baseline)
2. A permutation test shuffles combined samples 10,000 times to build a null distribution
3. The p-value represents how likely the observed difference arose by chance
4. Phase-level timing breakdowns identify which simulation phase regressed

### PR workflow behavior

- Regression detected: PR check **fails**, comment posted with details
- No regression: PR check **passes**, comment updated with results
- Insufficient baseline: Check passes with warning (needs 5+ historical runs)

## SQLite Metrics Database

Historical benchmark data is stored in `doc/working/benchmark_metrics.db`.

### Schema

- `benchmark_runs` - Run metadata (commit, branch, scheduler, timestamp)
- `computational_metrics` - Per-iteration IPS, cell count, memory
- `biological_metrics` - Pressure, volume, cell count per iteration
- `phase_timings` - Per-phase timing breakdown per iteration

### Manual ingestion

```bash
cd scripts && python -m ci.aggregate_results \
    --db ../doc/working/benchmark_metrics.db \
    --benchmark-dir ../<benchmark_directory> \
    --commit $(git rev-parse HEAD) \
    --branch $(git branch --show-current) \
    --scheduler adaptive
```

## Manual workflow dispatch

All workflows support manual triggering via GitHub Actions UI or CLI:

```bash
# Trigger nightly benchmark
gh workflow run benchmark-nightly.yml

# Trigger weekly stress test
gh workflow run benchmark-weekly.yml
```

## Extending the system

### Adding a new metric

1. Add column to relevant table in `scripts/ci/aggregate_results.py`
2. Add ingestion logic in `ingest_benchmark()`
3. Add plot in `scripts/plot_benchmark_unified.py` using `@register_plot`
4. Update regression detector if metric should trigger alerts

### Adding a new scheduler

No changes needed - the system auto-detects schedulers from `sim_*` directories.
