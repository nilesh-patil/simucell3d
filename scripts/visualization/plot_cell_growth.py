#!/usr/bin/env python3
"""
Plot cell count progression over time for all simulation modes.
Generates both linear and log-scale plots to capture exponential growth dynamics.

Usage:
    python plot_cell_growth.py /path/to/benchmark_directory

The benchmark directory should contain:
    logs/v1.log
    logs/static.log
    logs/adaptive.log
"""
import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
import re
import sys
from pathlib import Path


def parse_log_for_cells(log_path: Path) -> pd.DataFrame:
    """Extract iteration, cell count pairs from log file.

    Args:
        log_path: Path to simulation log file

    Returns:
        DataFrame with 'iteration' and 'cells' columns
    """
    pattern = r'iteration:\s*(\d+).*nb cells\s+(\d+)'
    data = []
    with open(log_path, 'r') as f:
        for line in f:
            match = re.search(pattern, line)
            if match:
                data.append((int(match.group(1)), int(match.group(2))))
    return pd.DataFrame(data, columns=['iteration', 'cells'])


def main(bench_dir: str) -> None:
    """Generate cell growth comparison plots (linear and log scale).

    Args:
        bench_dir: Path to benchmark directory
    """
    bench_path = Path(bench_dir)

    # Create figure with 2x2 subplots
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))

    modes = {
        'v1.0': ('logs/v1.log', '#2196F3'),      # Blue
        'Static': ('logs/static.log', '#4CAF50'),  # Green
        'Adaptive': ('logs/adaptive.log', '#F44336')  # Red
    }

    final_counts = {}
    all_data = {}

    for mode, (log_rel, color) in modes.items():
        log_path = bench_path / log_rel
        if not log_path.exists():
            print(f"Warning: {log_path} not found, skipping {mode}")
            continue

        df = parse_log_for_cells(log_path)
        if df.empty:
            print(f"Warning: No data parsed from {log_path}")
            continue

        all_data[mode] = df
        final_counts[mode] = df['cells'].iloc[-1]

        # Plot 1: Linear scale (top-left)
        axes[0, 0].plot(df['iteration'], df['cells'],
                       label=f'{mode} (final: {final_counts[mode]:,})',
                       color=color, alpha=0.8, linewidth=1.5)

        # Plot 2: Log scale Y-axis (top-right) - shows exponential growth
        axes[0, 1].semilogy(df['iteration'], df['cells'],
                           label=mode, color=color, alpha=0.8, linewidth=1.5)

        # Plot 3: Log-log scale (bottom-left) - shows power-law relationships
        axes[1, 0].loglog(df['iteration'] + 1, df['cells'],  # +1 to avoid log(0)
                         label=mode, color=color, alpha=0.8, linewidth=1.5)

    # Plot 4: Growth rate comparison (bottom-right)
    for mode, df in all_data.items():
        color = modes[mode][1]
        # Calculate growth rate (cells per 1000 iterations)
        if len(df) > 100:
            df_sampled = df.iloc[::100].copy()
            df_sampled['growth_rate'] = df_sampled['cells'].diff() / 100
            df_sampled = df_sampled.dropna()
            # Filter out zero/negative growth rates for log scale
            df_pos = df_sampled[df_sampled['growth_rate'] > 0]
            if not df_pos.empty:
                axes[1, 1].semilogy(df_pos['iteration'], df_pos['growth_rate'],
                                   label=mode, color=color, alpha=0.8, linewidth=1.5)

    # Configure Plot 1: Linear scale
    axes[0, 0].set_xlabel('Iteration', fontsize=10)
    axes[0, 0].set_ylabel('Cell Count', fontsize=10)
    axes[0, 0].set_title('Cell Population Growth (Linear Scale)', fontsize=12)
    axes[0, 0].legend(loc='upper left')
    axes[0, 0].grid(True, alpha=0.3)
    axes[0, 0].yaxis.set_major_formatter(plt.FuncFormatter(lambda x, p: f'{int(x):,}'))

    # Add exponential growth annotation
    axes[0, 0].annotate('Exponential growth phase\n(cell division dominant)',
                       xy=(0.65, 0.3), xycoords='axes fraction',
                       fontsize=9, alpha=0.7,
                       bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))

    # Configure Plot 2: Log scale Y
    axes[0, 1].set_xlabel('Iteration', fontsize=10)
    axes[0, 1].set_ylabel('Cell Count (log scale)', fontsize=10)
    axes[0, 1].set_title('Cell Population Growth (Semi-log)', fontsize=12)
    axes[0, 1].legend(loc='upper left')
    axes[0, 1].grid(True, alpha=0.3, which='both')

    # Annotation for exponential interpretation
    axes[0, 1].annotate('Straight line = exponential growth\n(constant doubling time)',
                       xy=(0.05, 0.85), xycoords='axes fraction',
                       fontsize=9, alpha=0.7,
                       bbox=dict(boxstyle='round', facecolor='lightblue', alpha=0.5))

    # Configure Plot 3: Log-log scale
    axes[1, 0].set_xlabel('Iteration (log scale)', fontsize=10)
    axes[1, 0].set_ylabel('Cell Count (log scale)', fontsize=10)
    axes[1, 0].set_title('Cell Population Growth (Log-Log)', fontsize=12)
    axes[1, 0].legend(loc='upper left')
    axes[1, 0].grid(True, alpha=0.3, which='both')

    # Add power-law reference line
    axes[1, 0].annotate('Slope indicates growth exponent\n(slope=1 → linear, slope=2 → quadratic)',
                       xy=(0.05, 0.15), xycoords='axes fraction',
                       fontsize=9, alpha=0.7,
                       bbox=dict(boxstyle='round', facecolor='lightyellow', alpha=0.5))

    # Configure Plot 4: Growth rate
    axes[1, 1].set_xlabel('Iteration', fontsize=10)
    axes[1, 1].set_ylabel('Growth Rate (cells/100 iter, log scale)', fontsize=10)
    axes[1, 1].set_title('Instantaneous Growth Rate', fontsize=12)
    axes[1, 1].legend(loc='upper left')
    axes[1, 1].grid(True, alpha=0.3, which='both')

    plt.suptitle('Cell Population Growth Comparison\nSimuCell3D Benchmark', fontsize=14)
    plt.tight_layout()

    plots_dir = bench_path / 'plots'
    plots_dir.mkdir(exist_ok=True)
    output_path = plots_dir / 'cell_growth_comparison.png'
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    print(f"Saved plot to: {output_path}")

    plt.close()


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python plot_cell_growth.py /path/to/benchmark_dir")
        print("\nExample:")
        print("  python plot_cell_growth.py ../doc/working/64k_growth_benchmark_20251130/")
        sys.exit(1)

    main(sys.argv[1])
