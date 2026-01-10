#!/usr/bin/env python3
"""
Division Event Synchronization Analysis.
Validates biological realism of cell proliferation patterns.

Panels:
- A: Inter-Division Interval Distribution (log-log histogram)
- B: Division Burst Size Distribution
- C: Population Synchrony Index (Fano factor: variance/mean)
- D: Division Rate Over Time with Cell Count Overlay

Usage:
    python plot_division_dynamics.py /path/to/benchmark_directory
"""
import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
from pathlib import Path
import sys
import re


def parse_divisions_from_log(log_path: Path) -> pd.DataFrame:
    """Parse division events from simulation log file.

    Args:
        log_path: Path to simulation log file

    Returns:
        DataFrame with iteration, cell_count, divisions columns
    """
    # Pattern to match: "iteration:  XXX | ... | nb cells  YYY"
    pattern = r'iteration:\s*(\d+).*nb cells\s+(\d+)'

    data = []
    with open(log_path, 'r') as f:
        for line in f:
            match = re.search(pattern, line)
            if match:
                iteration = int(match.group(1))
                cells = int(match.group(2))
                data.append((iteration, cells))

    if not data:
        return pd.DataFrame(columns=['iteration', 'cells', 'divisions'])

    df = pd.DataFrame(data, columns=['iteration', 'cells'])

    # Calculate divisions as change in cell count
    df['divisions'] = df['cells'].diff().fillna(0).clip(lower=0).astype(int)

    return df


def parse_divisions_from_diagnostics(csv_path: Path) -> pd.DataFrame:
    """Parse division events from performance diagnostics CSV.

    Args:
        csv_path: Path to performance_diagnostics.csv

    Returns:
        DataFrame with iteration, cells, divisions columns
    """
    df = pd.read_csv(csv_path)

    if 'divisions' not in df.columns:
        # Calculate from cell count changes
        df['divisions'] = df['cells'].diff().fillna(0).clip(lower=0).astype(int)

    return df[['iteration', 'cells', 'divisions']].copy()


def main(bench_dir: str) -> None:
    """Generate division dynamics analysis plots.

    Args:
        bench_dir: Path to benchmark directory
    """
    bench_path = Path(bench_dir)

    # Create 2x2 grid for division analysis
    fig, axes = plt.subplots(2, 2, figsize=(14, 12))

    # Try multiple data sources
    data_sources = {
        'Static': [
            ('sim_static/performance_diagnostics.csv', 'csv'),
            ('logs/static.log', 'log')
        ],
        'Adaptive': [
            ('sim_adaptive/performance_diagnostics.csv', 'csv'),
            ('logs/adaptive.log', 'log')
        ]
    }

    colors = {
        'Static': '#4CAF50',
        'Adaptive': '#F44336'
    }

    division_data = {}

    for mode, sources in data_sources.items():
        for source_rel, source_type in sources:
            source_path = bench_path / source_rel
            if source_path.exists():
                try:
                    if source_type == 'csv':
                        df = parse_divisions_from_diagnostics(source_path)
                    else:
                        df = parse_divisions_from_log(source_path)

                    if len(df) > 0 and 'divisions' in df.columns:
                        division_data[mode] = (df, colors[mode])
                        print(f"Loaded {len(df)} records from: {source_path}")
                        break
                except Exception as e:
                    print(f"Warning: Could not parse {source_path}: {e}")
                    continue

    if not division_data:
        print(f"No division data found in {bench_path}")
        print("Looking for performance_diagnostics.csv or log files")
        return

    # =========================================================================
    # Panel A: Inter-Division Interval Distribution (log-log histogram)
    # =========================================================================
    ax_idi = axes[0, 0]

    for mode, (df, color) in division_data.items():
        # Find division events
        division_iters = df.loc[df['divisions'] > 0, 'iteration'].values

        if len(division_iters) > 2:
            # Calculate inter-division intervals
            intervals = np.diff(division_iters)
            intervals = intervals[intervals > 0]  # Filter zeros

            if len(intervals) > 10:
                # Log-log histogram (power-law check)
                bins = np.logspace(np.log10(max(1, intervals.min())),
                                  np.log10(intervals.max()), 30)
                counts, bin_edges = np.histogram(intervals, bins=bins)

                # Filter zeros for log scale
                valid = counts > 0
                bin_centers = (bin_edges[:-1] + bin_edges[1:]) / 2

                ax_idi.loglog(bin_centers[valid], counts[valid],
                             'o-', color=color, alpha=0.7,
                             label=f'{mode} (n={len(intervals)})', markersize=5)

                # Report mean interval
                mean_interval = np.mean(intervals)
                ax_idi.axvline(mean_interval, color=color, linestyle='--', alpha=0.5)
                ax_idi.annotate(f'{mode} mean: {mean_interval:.0f}',
                               xy=(mean_interval, counts.max() * 0.5),
                               fontsize=8, color=color, rotation=90, va='bottom')

    ax_idi.set_xlabel('Inter-Division Interval (iterations)', fontsize=10)
    ax_idi.set_ylabel('Count', fontsize=10)
    ax_idi.set_title('Inter-Division Interval Distribution', fontsize=12)
    ax_idi.legend(loc='upper right', fontsize=9)
    ax_idi.grid(True, alpha=0.3, which='both')
    ax_idi.annotate('Exponential → random (Poisson) divisions\n'
                   'Power-law tail → synchronized bursts\n'
                   'Narrow peak → cell cycle timing',
                   xy=(0.02, 0.05), xycoords='axes fraction',
                   fontsize=8, alpha=0.7,
                   bbox=dict(boxstyle='round', facecolor='lightblue', alpha=0.5))

    # =========================================================================
    # Panel B: Division Burst Size Distribution
    # =========================================================================
    ax_burst = axes[0, 1]

    for mode, (df, color) in division_data.items():
        # Group consecutive division events into bursts
        divisions = df['divisions'].values

        # Find bursts (consecutive non-zero divisions)
        burst_sizes = []
        current_burst = 0

        for d in divisions:
            if d > 0:
                current_burst += d
            elif current_burst > 0:
                burst_sizes.append(current_burst)
                current_burst = 0

        if current_burst > 0:
            burst_sizes.append(current_burst)

        burst_sizes = np.array(burst_sizes)

        if len(burst_sizes) > 5:
            # Histogram of burst sizes
            max_burst = min(burst_sizes.max(), 50)
            bins = np.arange(1, max_burst + 2) - 0.5
            counts, bin_edges = np.histogram(burst_sizes, bins=bins)

            bin_centers = (bin_edges[:-1] + bin_edges[1:]) / 2
            valid = counts > 0

            ax_burst.bar(bin_centers[valid] + (0.2 if mode == 'Adaptive' else -0.2),
                        counts[valid], width=0.35, color=color, alpha=0.7,
                        label=f'{mode} (n={len(burst_sizes)})')

            # Report statistics
            mean_burst = np.mean(burst_sizes)
            max_burst_val = burst_sizes.max()
            ax_burst.annotate(f'{mode}: mean={mean_burst:.1f}, max={max_burst_val}',
                             xy=(0.95, 0.95 - 0.08 * list(division_data.keys()).index(mode)),
                             xycoords='axes fraction', ha='right',
                             fontsize=9, color=color,
                             bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))

    ax_burst.set_xlabel('Burst Size (cells divided)', fontsize=10)
    ax_burst.set_ylabel('Count', fontsize=10)
    ax_burst.set_title('Division Burst Size Distribution', fontsize=12)
    ax_burst.legend(loc='upper right', fontsize=9)
    ax_burst.grid(True, alpha=0.3)
    ax_burst.annotate('Burst size = 1 → independent divisions\n'
                     'Large bursts → synchronization events\n'
                     '(common in epithelial tissues)',
                     xy=(0.02, 0.05), xycoords='axes fraction',
                     fontsize=8, alpha=0.7,
                     bbox=dict(boxstyle='round', facecolor='lightyellow', alpha=0.5))

    # =========================================================================
    # Panel C: Population Synchrony Index (Fano factor over sliding window)
    # =========================================================================
    ax_fano = axes[1, 0]

    for mode, (df, color) in division_data.items():
        divisions = df['divisions'].values
        iterations = df['iteration'].values

        # Calculate Fano factor (variance/mean) in sliding windows
        window_size = max(100, len(df) // 50)
        fano_factors = []
        fano_iters = []

        for i in range(0, len(divisions) - window_size, window_size // 2):
            window = divisions[i:i + window_size]
            window_sum = window.sum()
            if window_sum > 0:
                mean_div = window.mean()
                var_div = window.var()
                fano = var_div / mean_div if mean_div > 0 else 0
                fano_factors.append(fano)
                fano_iters.append(iterations[i + window_size // 2])

        if len(fano_factors) > 3:
            ax_fano.semilogy(fano_iters, fano_factors, 'o-',
                            color=color, alpha=0.7, linewidth=1.5,
                            markersize=4, label=mode)

            # Report mean Fano factor
            mean_fano = np.mean(fano_factors)
            ax_fano.annotate(f'{mode} mean: {mean_fano:.2f}',
                            xy=(0.95, 0.95 - 0.08 * list(division_data.keys()).index(mode)),
                            xycoords='axes fraction', ha='right',
                            fontsize=9, color=color,
                            bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))

    # Reference lines
    ax_fano.axhline(y=1.0, color='gray', linestyle='--', alpha=0.7, label='Poisson (F=1)')
    ax_fano.axhline(y=0.5, color='green', linestyle=':', alpha=0.5, label='Sub-Poisson')

    ax_fano.set_xlabel('Iteration', fontsize=10)
    ax_fano.set_ylabel('Fano Factor (var/mean, log scale)', fontsize=10)
    ax_fano.set_title('Population Synchrony Index (Fano Factor)', fontsize=12)
    ax_fano.legend(loc='upper right', fontsize=8)
    ax_fano.grid(True, alpha=0.3, which='both')
    ax_fano.annotate('F = 1 → Poisson (random timing)\n'
                    'F > 1 → Super-Poisson (synchronized)\n'
                    'F < 1 → Sub-Poisson (inhibited/regulated)',
                    xy=(0.02, 0.05), xycoords='axes fraction',
                    fontsize=8, alpha=0.7,
                    bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))

    # =========================================================================
    # Panel D: Division Rate and Cell Count Over Time
    # =========================================================================
    ax_rate = axes[1, 1]

    for mode, (df, color) in division_data.items():
        # Sample for clarity
        sample_rate = max(1, len(df) // 500)
        df_s = df.iloc[::sample_rate]

        # Plot cell count on primary y-axis
        ax_rate.plot(df_s['iteration'], df_s['cells'],
                    color=color, alpha=0.8, linewidth=2, label=f'{mode} cells')

    ax_rate.set_xlabel('Iteration', fontsize=10)
    ax_rate.set_ylabel('Cell Count', fontsize=10, color='black')
    ax_rate.tick_params(axis='y', labelcolor='black')

    # Create secondary y-axis for division rate
    ax_rate2 = ax_rate.twinx()

    for mode, (df, color) in division_data.items():
        # Calculate rolling division rate
        window = max(100, len(df) // 100)
        df_copy = df.copy()
        df_copy['div_rate'] = df_copy['divisions'].rolling(window, center=True).mean()

        # Sample for clarity
        sample_rate = max(1, len(df_copy) // 500)
        df_s = df_copy.iloc[::sample_rate]

        valid = df_s['div_rate'] > 0
        if valid.sum() > 5:
            ax_rate2.semilogy(df_s.loc[valid, 'iteration'], df_s.loc[valid, 'div_rate'],
                             color=color, alpha=0.4, linewidth=1.5, linestyle='--',
                             label=f'{mode} rate')

    ax_rate2.set_ylabel('Division Rate (log, rolling avg)', fontsize=10, color='gray')
    ax_rate2.tick_params(axis='y', labelcolor='gray')

    ax_rate.set_title('Cell Population and Division Rate', fontsize=12)

    # Combined legend
    lines1, labels1 = ax_rate.get_legend_handles_labels()
    lines2, labels2 = ax_rate2.get_legend_handles_labels()
    ax_rate.legend(lines1 + lines2, labels1 + labels2, loc='upper left', fontsize=8)

    ax_rate.grid(True, alpha=0.3)
    ax_rate.annotate('Exponential growth → constant rate\n'
                    'Rate peaks → synchronization waves\n'
                    'Rate plateaus → contact inhibition',
                    xy=(0.5, 0.05), xycoords='axes fraction',
                    fontsize=8, alpha=0.7, ha='center',
                    bbox=dict(boxstyle='round', facecolor='lightgreen', alpha=0.5))

    plt.suptitle('Cell Division Dynamics Analysis\nSimuCell3D Benchmark',
                fontsize=14, fontweight='bold')
    plt.tight_layout()

    plots_dir = bench_path / 'plots'
    plots_dir.mkdir(exist_ok=True)
    output_path = plots_dir / 'division_dynamics_analysis.png'
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    print(f"Saved plot to: {output_path}")

    plt.close()


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python plot_division_dynamics.py /path/to/benchmark_dir")
        print("\nExample:")
        print("  python plot_division_dynamics.py ../doc/working/64k_growth_benchmark_20251130/")
        sys.exit(1)

    main(sys.argv[1])
