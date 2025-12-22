#!/usr/bin/env python3
"""
Preprocess raw simulation data into metrics format for visualization.

Transforms:
  sim_*/performance_diagnostics.csv  →  metrics/{sched}/computational.csv
  sim_*/performance_diagnostics.csv  →  metrics/{sched}/phase_timings.csv
  sim_*/simulation_statistics.csv    →  metrics/{sched}/biological.csv
  sim_*/performance_diagnostics.csv  →  metrics/{sched}/workload.csv
  All schedulers                     →  metrics/comparison.csv

Usage:
    python preprocess_benchmark_metrics.py <benchmark_directory>
"""

import sys
from pathlib import Path
from typing import Dict, List, Optional

import pandas as pd
import numpy as np


def detect_schedulers(bench_dir: Path) -> List[str]:
    """Find scheduler names from sim_* directories."""
    schedulers = []
    for sim_dir in sorted(bench_dir.glob("sim_*")):
        if sim_dir.is_dir():
            name = sim_dir.name.replace("sim_", "")
            # Accept directory if it has either performance_diagnostics.csv OR simulation_statistics.csv
            has_perf = (sim_dir / "performance_diagnostics.csv").exists()
            has_stats = (sim_dir / "simulation_statistics.csv").exists()
            if has_perf or has_stats:
                schedulers.append(name)
    return schedulers


def create_computational(perf_diag: pd.DataFrame) -> pd.DataFrame:
    """Transform performance_diagnostics → computational metrics."""
    df = perf_diag.copy()
    result = pd.DataFrame()

    # Handle timestamp
    if 'wall_epoch' in df.columns:
        result['timestamp'] = pd.to_datetime(df['wall_epoch'], unit='s')
        result['epoch'] = df['wall_epoch']
    elif 'sim_time' in df.columns:
        # Fallback: use sim_time as epoch
        result['timestamp'] = pd.to_datetime(df['sim_time'], unit='s')
        result['epoch'] = df['sim_time']

    result['iteration'] = df['iteration']
    result['cells'] = df['cells']

    # Calculate iterations per second
    if 'total_iteration_ms' in df.columns:
        result['iter_per_sec'] = 1000.0 / df['total_iteration_ms'].replace(0, np.nan)
        result['iter_per_sec'] = result['iter_per_sec'].fillna(0)
    else:
        result['iter_per_sec'] = 0

    # Estimate memory (empirical: ~1.7 MB per cell + 100 MB base)
    result['rss_mb'] = 100 + df['cells'] * 1.7

    return result


def create_biological(sim_stats: pd.DataFrame) -> pd.DataFrame:
    """Aggregate simulation_statistics → biological metrics per iteration."""
    # Build aggregation dict based on available columns
    agg_dict = {}

    if 'pressure' in sim_stats.columns:
        agg_dict['pressure'] = 'mean'
    if 'volume' in sim_stats.columns:
        agg_dict['volume'] = 'mean'
    if 'cell_id' in sim_stats.columns:
        agg_dict['cell_id'] = 'count'
    if 'kinetic_energy' in sim_stats.columns:
        agg_dict['kinetic_energy'] = 'mean'
    if 'total_potential_energy' in sim_stats.columns:
        agg_dict['total_potential_energy'] = 'mean'
    if 'area' in sim_stats.columns:
        agg_dict['area'] = 'mean'
    if 'cell_contact_area_fraction' in sim_stats.columns:
        agg_dict['cell_contact_area_fraction'] = 'mean'

    if not agg_dict:
        return pd.DataFrame()

    agg = sim_stats.groupby('iteration').agg(agg_dict).reset_index()

    # Rename columns to expected names
    rename_map = {
        'pressure': 'mean_pressure',
        'volume': 'mean_volume',
        'cell_id': 'cell_count',
        'kinetic_energy': 'mean_kinetic_energy',
        'total_potential_energy': 'mean_potential_energy',
        'area': 'mean_area',
        'cell_contact_area_fraction': 'mean_contact_fraction'
    }
    agg = agg.rename(columns={k: v for k, v in rename_map.items() if k in agg.columns})

    return agg


def create_phase_timings(perf_diag: pd.DataFrame) -> pd.DataFrame:
    """Extract phase timings from performance_diagnostics."""
    df = perf_diag.copy()
    result = pd.DataFrame()

    # Handle timestamp
    if 'wall_epoch' in df.columns:
        result['timestamp'] = pd.to_datetime(df['wall_epoch'], unit='s')

    result['iteration'] = df['iteration']

    # Copy phase timing columns
    timing_cols = [
        'mesh_refinement_ms',
        'contact_detection_ms',
        'polarization_internal_forces_ms',
        'time_integration_ms',
        'total_iteration_ms'
    ]
    for col in timing_cols:
        if col in df.columns:
            # Rename polarization column for consistency
            target = col.replace('polarization_internal_forces_ms', 'polarization_ms')
            result[target] = df[col]

    return result


def create_workload(perf_diag: pd.DataFrame) -> pd.DataFrame:
    """Extract workload metrics from performance_diagnostics."""
    df = perf_diag.copy()
    result = pd.DataFrame()

    # Handle timestamp
    if 'wall_epoch' in df.columns:
        result['timestamp'] = pd.to_datetime(df['wall_epoch'], unit='s')

    result['iteration'] = df['iteration']
    result['cells'] = df['cells']

    # Copy workload-related columns
    for col in ['cov', 'thread_imbalance_pct', 'divisions']:
        if col in df.columns:
            result[col] = df[col]

    return result


def create_comparison(all_computational: Dict[str, pd.DataFrame]) -> Optional[pd.DataFrame]:
    """Create cross-scheduler comparison from all computational data."""
    if not all_computational:
        return None

    # Start with the first scheduler's data
    result = None
    for name, df in all_computational.items():
        if df is None or len(df) == 0:
            continue

        cols = df[['epoch', 'iteration', 'cells', 'iter_per_sec']].copy()
        cols = cols.rename(columns={
            'iteration': f'{name}_iter',
            'cells': f'{name}_cells',
            'iter_per_sec': f'{name}_ips'
        })
        cols['timestamp'] = pd.to_datetime(cols['epoch'], unit='s')

        if result is None:
            result = cols
        else:
            # Merge on epoch with nearest match (tolerance 60 seconds)
            result = pd.merge_asof(
                result.sort_values('epoch'),
                cols.sort_values('epoch'),
                on='epoch',
                direction='nearest',
                tolerance=60,
                suffixes=('', f'_{name}')
            )

    return result


def load_csv_safe(path: Path) -> Optional[pd.DataFrame]:
    """Load CSV with error handling."""
    if not path.exists():
        return None
    try:
        return pd.read_csv(path)
    except Exception as e:
        print(f"    Warning: Could not load {path}: {e}")
        return None


def main():
    if len(sys.argv) < 2:
        print("Usage: python preprocess_benchmark_metrics.py <benchmark_directory>")
        print()
        print("Transforms raw simulation output into metrics format for visualization scripts.")
        sys.exit(1)

    bench_dir = Path(sys.argv[1])
    if not bench_dir.exists():
        print(f"Error: Directory not found: {bench_dir}")
        sys.exit(1)

    metrics_dir = bench_dir / "metrics"

    print(f"Preprocessing metrics for: {bench_dir}")

    # Detect schedulers
    schedulers = detect_schedulers(bench_dir)
    if not schedulers:
        print("  No simulation data found (looking for sim_*/performance_diagnostics.csv)")
        sys.exit(1)

    print(f"  Found schedulers: {schedulers}")

    all_computational = {}
    total_files = 0

    for sched in schedulers:
        sim_dir = bench_dir / f"sim_{sched}"
        out_dir = metrics_dir / sched
        out_dir.mkdir(parents=True, exist_ok=True)

        print(f"  Processing {sched}...")

        # Load performance_diagnostics
        perf_path = sim_dir / "performance_diagnostics.csv"
        perf_diag = load_csv_safe(perf_path)

        if perf_diag is not None and len(perf_diag) > 0:
            # Create computational metrics
            comp = create_computational(perf_diag)
            comp.to_csv(out_dir / "computational.csv", index=False)
            all_computational[sched] = comp
            print(f"    {sched}/computational.csv ({len(comp)} rows)")
            total_files += 1

            # Create phase timings
            phase = create_phase_timings(perf_diag)
            phase.to_csv(out_dir / "phase_timings.csv", index=False)
            print(f"    {sched}/phase_timings.csv ({len(phase)} rows)")
            total_files += 1

            # Create workload metrics
            work = create_workload(perf_diag)
            work.to_csv(out_dir / "workload.csv", index=False)
            print(f"    {sched}/workload.csv ({len(work)} rows)")
            total_files += 1

        # Load simulation_statistics
        stats_path = sim_dir / "simulation_statistics.csv"
        sim_stats = load_csv_safe(stats_path)

        if sim_stats is not None and len(sim_stats) > 0:
            # Create biological metrics (aggregated per iteration)
            bio = create_biological(sim_stats)
            if len(bio) > 0:
                bio.to_csv(out_dir / "biological.csv", index=False)
                print(f"    {sched}/biological.csv ({len(bio)} rows)")
                total_files += 1

    # Create cross-scheduler comparison
    if len(all_computational) > 0:
        comparison = create_comparison(all_computational)
        if comparison is not None and len(comparison) > 0:
            comparison.to_csv(metrics_dir / "comparison.csv", index=False)
            print(f"  comparison.csv ({len(comparison)} rows)")
            total_files += 1

    print(f"\nDone! Generated {total_files} metric files in {metrics_dir}")


if __name__ == "__main__":
    main()
