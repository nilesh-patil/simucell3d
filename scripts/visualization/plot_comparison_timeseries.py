#!/usr/bin/env python3
"""
plot_comparison_timeseries.py

Generate comprehensive visualization plots from comparison timeseries data.
Reads comparison_timeseries.csv and generates multi-panel plots showing:
- Iteration progress over time (v1.0 vs current)
- Speedup evolution
- Cell count growth
- Phase timing
- CoV (workload heterogeneity) trends
- CPU utilization

Usage:
    python3 plot_comparison_timeseries.py <comparison_output_dir>

Example:
    python3 plot_comparison_timeseries.py /path/to/12hour_comparison_20260118_123456
"""

import sys
import os
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from datetime import datetime, timedelta

def load_timeseries(comparison_dir):
    """Load comparison timeseries CSV file."""
    csv_path = os.path.join(comparison_dir, 'metrics', 'comparison_timeseries.csv')

    if not os.path.exists(csv_path):
        print(f"ERROR: Comparison timeseries file not found: {csv_path}")
        sys.exit(1)

    df = pd.read_csv(csv_path)

    # Convert elapsed_sec to hours for better readability
    df['elapsed_hours'] = df['elapsed_sec'] / 3600

    return df

def create_comparison_plots(df, output_dir):
    """Generate comprehensive comparison plots."""

    # Create output directory for plots
    plots_dir = os.path.join(output_dir, 'plots')
    os.makedirs(plots_dir, exist_ok=True)

    # Set style
    plt.style.use('seaborn-v0_8-darkgrid')

    # Create multi-panel figure
    fig, axes = plt.subplots(3, 2, figsize=(14, 12))
    fig.suptitle('SimuCell3D Performance Comparison: v1.0 vs Current (Adaptive)',
                 fontsize=16, fontweight='bold')

    # 1. Iteration progress over time
    ax = axes[0, 0]
    ax.plot(df['elapsed_hours'], df['v1_iter'], label='v1.0 (static)',
            linewidth=2, marker='o', markersize=3, alpha=0.7)
    ax.plot(df['elapsed_hours'], df['current_iter'], label='Current (adaptive)',
            linewidth=2, marker='s', markersize=3, alpha=0.7)
    ax.set_xlabel('Time (hours)', fontsize=10)
    ax.set_ylabel('Iterations', fontsize=10)
    ax.set_title('Iteration Progress', fontsize=12, fontweight='bold')
    ax.legend(loc='upper left')
    ax.grid(True, alpha=0.3)

    # 2. Speedup evolution
    ax = axes[0, 1]
    ax.plot(df['elapsed_hours'], df['speedup'], linewidth=2, color='green',
            marker='o', markersize=3, alpha=0.7)
    ax.axhline(1.0, color='red', linestyle='--', linewidth=1.5, label='No speedup', alpha=0.7)
    mean_speedup = df['speedup'].mean()
    ax.axhline(mean_speedup, color='blue', linestyle=':', linewidth=1.5,
               label=f'Mean: {mean_speedup:.2f}x', alpha=0.7)
    ax.set_xlabel('Time (hours)', fontsize=10)
    ax.set_ylabel('Speedup (current/v1.0)', fontsize=10)
    ax.set_title('Speedup Evolution', fontsize=12, fontweight='bold')
    ax.legend(loc='best')
    ax.grid(True, alpha=0.3)

    # 3. Cell count growth
    ax = axes[1, 0]
    ax.plot(df['elapsed_hours'], df['v1_cells'], label='v1.0',
            linewidth=2, marker='o', markersize=3, alpha=0.7)
    ax.plot(df['elapsed_hours'], df['current_cells'], label='Current',
            linewidth=2, marker='s', markersize=3, alpha=0.7)
    ax.set_xlabel('Time (hours)', fontsize=10)
    ax.set_ylabel('Cell Count', fontsize=10)
    ax.set_title('Cell Population Growth', fontsize=12, fontweight='bold')
    ax.legend(loc='upper left')
    ax.grid(True, alpha=0.3)

    # 4. Division count over time
    ax = axes[1, 1]
    ax.plot(df['elapsed_hours'], df['v1_divs'], label='v1.0',
            linewidth=2, marker='o', markersize=3, alpha=0.7)
    ax.plot(df['elapsed_hours'], df['current_divs'], label='Current',
            linewidth=2, marker='s', markersize=3, alpha=0.7)
    ax.set_xlabel('Time (hours)', fontsize=10)
    ax.set_ylabel('Cumulative Divisions', fontsize=10)
    ax.set_title('Cell Division Events', fontsize=12, fontweight='bold')
    ax.legend(loc='upper left')
    ax.grid(True, alpha=0.3)

    # 5. CoV (workload heterogeneity) over time
    ax = axes[2, 0]
    ax.plot(df['elapsed_hours'], df['v1_cov'], label='v1.0',
            linewidth=2, marker='o', markersize=3, alpha=0.7)
    ax.plot(df['elapsed_hours'], df['current_cov'], label='Current',
            linewidth=2, marker='s', markersize=3, alpha=0.7)
    ax.set_xlabel('Time (hours)', fontsize=10)
    ax.set_ylabel('CoV (Coefficient of Variation)', fontsize=10)
    ax.set_title('Workload Heterogeneity (CoV)', fontsize=12, fontweight='bold')
    ax.legend(loc='best')
    ax.grid(True, alpha=0.3)

    # 6. CPU utilization
    ax = axes[2, 1]
    ax.plot(df['elapsed_hours'], df['v1_cpu'], label='v1.0',
            linewidth=2, marker='o', markersize=3, alpha=0.7)
    ax.plot(df['elapsed_hours'], df['current_cpu'], label='Current',
            linewidth=2, marker='s', markersize=3, alpha=0.7)
    ax.set_xlabel('Time (hours)', fontsize=10)
    ax.set_ylabel('CPU Utilization (%)', fontsize=10)
    ax.set_title('CPU Utilization', fontsize=12, fontweight='bold')
    ax.legend(loc='best')
    ax.grid(True, alpha=0.3)

    # Adjust layout
    plt.tight_layout(rect=[0, 0, 1, 0.97])

    # Save figure
    output_path = os.path.join(plots_dir, 'comparison_timeseries.png')
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    print(f"✓ Saved comparison plot: {output_path}")

    # Close figure to free memory
    plt.close(fig)

def create_phase_analysis_plot(df, output_dir):
    """Create a focused plot on simulation phases."""

    plots_dir = os.path.join(output_dir, 'plots')

    fig, ax = plt.subplots(1, 1, figsize=(12, 6))

    # Plot iteration progress with phase coloring
    # Create scatter plot colored by phase
    phases_v1 = df['v1_phase'].unique()
    phases_current = df['current_phase'].unique()

    # Use different markers for v1 and current
    for phase in phases_v1:
        mask = df['v1_phase'] == phase
        ax.scatter(df.loc[mask, 'elapsed_hours'], df.loc[mask, 'v1_iter'],
                  label=f'v1.0 - {phase}', alpha=0.6, s=30)

    for phase in phases_current:
        mask = df['current_phase'] == phase
        ax.scatter(df.loc[mask, 'elapsed_hours'], df.loc[mask, 'current_iter'],
                  label=f'Current - {phase}', alpha=0.6, s=30, marker='s')

    ax.set_xlabel('Time (hours)', fontsize=12)
    ax.set_ylabel('Iterations', fontsize=12)
    ax.set_title('Simulation Phase Progression', fontsize=14, fontweight='bold')
    ax.legend(loc='upper left', ncol=2)
    ax.grid(True, alpha=0.3)

    plt.tight_layout()

    output_path = os.path.join(plots_dir, 'phase_analysis.png')
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    print(f"✓ Saved phase analysis plot: {output_path}")

    plt.close(fig)

def create_performance_metrics_plot(df, output_dir):
    """Create detailed performance metrics plot."""

    plots_dir = os.path.join(output_dir, 'plots')

    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    fig.suptitle('Detailed Performance Metrics', fontsize=16, fontweight='bold')

    # 1. Iteration time comparison
    ax = axes[0, 0]
    ax.plot(df['elapsed_hours'], df['v1_total_ms'], label='v1.0',
            linewidth=2, alpha=0.7)
    ax.plot(df['elapsed_hours'], df['current_total_ms'], label='Current',
            linewidth=2, alpha=0.7)
    ax.set_xlabel('Time (hours)', fontsize=10)
    ax.set_ylabel('Iteration Time (ms)', fontsize=10)
    ax.set_title('Per-Iteration Execution Time', fontsize=12, fontweight='bold')
    ax.legend()
    ax.grid(True, alpha=0.3)

    # 2. Speedup vs cell count
    ax = axes[0, 1]
    ax.scatter(df['current_cells'], df['speedup'], alpha=0.5, s=20)
    ax.set_xlabel('Cell Count', fontsize=10)
    ax.set_ylabel('Speedup', fontsize=10)
    ax.set_title('Speedup vs Cell Count', fontsize=12, fontweight='bold')
    ax.axhline(1.0, color='red', linestyle='--', alpha=0.5)
    ax.grid(True, alpha=0.3)

    # 3. Speedup vs CoV
    ax = axes[1, 0]
    ax.scatter(df['current_cov'], df['speedup'], alpha=0.5, s=20, c=df['current_cells'],
              cmap='viridis')
    ax.set_xlabel('CoV (Workload Heterogeneity)', fontsize=10)
    ax.set_ylabel('Speedup', fontsize=10)
    ax.set_title('Speedup vs Workload Heterogeneity', fontsize=12, fontweight='bold')
    ax.axhline(1.0, color='red', linestyle='--', alpha=0.5)
    cbar = plt.colorbar(ax.collections[0], ax=ax)
    cbar.set_label('Cell Count', fontsize=9)
    ax.grid(True, alpha=0.3)

    # 4. Cumulative speedup gain
    ax = axes[1, 1]
    cumulative_gain = (df['current_iter'] - df['v1_iter']).cumsum()
    ax.plot(df['elapsed_hours'], cumulative_gain, linewidth=2, color='green')
    ax.fill_between(df['elapsed_hours'], 0, cumulative_gain, alpha=0.3, color='green')
    ax.set_xlabel('Time (hours)', fontsize=10)
    ax.set_ylabel('Cumulative Iteration Gain', fontsize=10)
    ax.set_title('Cumulative Performance Advantage', fontsize=12, fontweight='bold')
    ax.grid(True, alpha=0.3)

    plt.tight_layout(rect=[0, 0, 1, 0.97])

    output_path = os.path.join(plots_dir, 'performance_metrics.png')
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    print(f"✓ Saved performance metrics plot: {output_path}")

    plt.close(fig)

def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    comparison_dir = sys.argv[1]

    if not os.path.isdir(comparison_dir):
        print(f"ERROR: Directory not found: {comparison_dir}")
        sys.exit(1)

    print(f"Generating comparison plots from: {comparison_dir}")
    print("")

    # Load data
    df = load_timeseries(comparison_dir)
    print(f"Loaded {len(df)} data points from timeseries")
    print("")

    # Generate plots
    create_comparison_plots(df, comparison_dir)
    create_phase_analysis_plot(df, comparison_dir)
    create_performance_metrics_plot(df, comparison_dir)

    print("")
    print("=" * 80)
    print("Visualization complete!")
    print(f"Plots saved to: {os.path.join(comparison_dir, 'plots')}/")
    print("=" * 80)

if __name__ == '__main__':
    main()
