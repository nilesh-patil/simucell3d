#!/usr/bin/env python3
"""
Plot energy distribution and conservation across simulation.
Generates multi-scale visualizations including log scale for energy magnitudes.

Usage:
    python plot_energy_distribution.py /path/to/benchmark_directory

The benchmark directory should contain:
    sim_static/simulation_statistics.csv
    sim_adaptive/simulation_statistics.csv
"""
import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
from pathlib import Path
import sys


def load_statistics(csv_path: Path) -> tuple:
    """Load and aggregate simulation statistics by iteration.

    Args:
        csv_path: Path to simulation_statistics.csv

    Returns:
        Tuple of (aggregated DataFrame with mean energy values, raw DataFrame)
    """
    df = pd.read_csv(csv_path)

    # Group by iteration, compute mean energies and cell counts
    agg_dict = {
        'kinetic_energy': ['mean', 'std'],
        'surface_tension_energy': ['mean', 'std'],
        'pressure_energy': ['mean', 'std'],
        'total_potential_energy': ['mean', 'sum'],  # sum for total system energy
        'membrane_elasticity_energy': ['mean', 'std'],
        'bending_energy': ['mean', 'std'],
        'cell_id': 'count'  # cell count per iteration
    }

    grouped = df.groupby('iteration').agg(agg_dict)
    # Flatten column names
    grouped.columns = ['_'.join(col).strip() for col in grouped.columns.values]
    grouped = grouped.rename(columns={'cell_id_count': 'cell_count'})
    grouped = grouped.reset_index()

    return grouped, df


def main(bench_dir: str) -> None:
    """Generate energy distribution analysis plots with log scale options.

    Args:
        bench_dir: Path to benchmark directory
    """
    bench_path = Path(bench_dir)

    # Expanded 4x2 grid for comprehensive energy analysis with cell count scaling
    fig, axes = plt.subplots(4, 2, figsize=(14, 18))

    csv_paths = {
        'Static': 'sim_static/simulation_statistics.csv',
        'Adaptive': 'sim_adaptive/simulation_statistics.csv'
    }

    colors = {
        'Static': '#4CAF50',   # Green
        'Adaptive': '#F44336'  # Red
    }

    energy_data = {}
    raw_data = {}

    for mode, csv_rel in csv_paths.items():
        csv_path = bench_path / csv_rel
        if not csv_path.exists():
            print(f"Warning: {csv_path} not found, skipping {mode}")
            continue

        df, df_raw = load_statistics(csv_path)
        energy_data[mode] = df
        raw_data[mode] = df_raw

        # Plot 1: Total Potential Energy - Linear scale (top-left)
        axes[0, 0].plot(df['iteration'], df['total_potential_energy_mean'],
                       label=mode, color=colors[mode], alpha=0.8, linewidth=2)

        # Plot 2: Total Potential Energy - Log scale (top-right)
        df_pos = df[df['total_potential_energy_mean'] > 0]
        axes[0, 1].semilogy(df_pos['iteration'], df_pos['total_potential_energy_mean'],
                           label=mode, color=colors[mode], alpha=0.8, linewidth=2)

        # Plot 3: Kinetic Energy - Linear scale (mid-left)
        axes[1, 0].plot(df['iteration'], df['kinetic_energy_mean'],
                       label=mode, color=colors[mode], alpha=0.8, linewidth=2)

        # Plot 4: Kinetic Energy - Log scale (mid-right)
        df_ke = df[df['kinetic_energy_mean'] > 0]
        axes[1, 1].semilogy(df_ke['iteration'], df_ke['kinetic_energy_mean'],
                           label=mode, color=colors[mode], alpha=0.8, linewidth=2)

        # Plot 5: Energy ratio (KE/PE) - important for stability (bottom-left)
        with np.errstate(divide='ignore', invalid='ignore'):
            ratio = np.where(df['total_potential_energy_mean'] > 0,
                           df['kinetic_energy_mean'] / df['total_potential_energy_mean'] * 100,
                           0)
        ratio_pos = ratio[ratio > 0]
        iter_pos = df['iteration'][ratio > 0]
        if len(ratio_pos) > 0:
            axes[2, 0].semilogy(iter_pos, ratio_pos,
                               label=mode, color=colors[mode], alpha=0.8, linewidth=2)

        # NEW Plot 7: Energy vs Cell Count (log-log) - scaling analysis
        df_valid = df[(df['cell_count'] > 0) & (df['total_potential_energy_sum'] > 0)]
        if len(df_valid) > 0:
            axes[3, 0].loglog(df_valid['cell_count'], df_valid['total_potential_energy_sum'],
                             'o', markersize=3, alpha=0.5, label=mode, color=colors[mode])
            # Fit power law: E ~ N^alpha
            if len(df_valid) > 10:
                log_n = np.log(df_valid['cell_count'])
                log_e = np.log(df_valid['total_potential_energy_sum'])
                slope, intercept = np.polyfit(log_n, log_e, 1)
                fit_line = np.exp(intercept) * df_valid['cell_count']**slope
                axes[3, 0].loglog(df_valid['cell_count'], fit_line, '--',
                                 color=colors[mode], alpha=0.8, linewidth=1.5,
                                 label=f'{mode} fit: N^{slope:.2f}')

    # Configure Plot 1: Total Potential Energy - Linear
    axes[0, 0].set_title('Total Potential Energy (Linear Scale)', fontsize=12)
    axes[0, 0].set_ylabel('Energy (J)', fontsize=10)
    axes[0, 0].set_xlabel('Iteration', fontsize=10)
    axes[0, 0].legend()
    axes[0, 0].grid(True, alpha=0.3)
    axes[0, 0].ticklabel_format(style='scientific', axis='y', scilimits=(0, 0))

    # Configure Plot 2: Total Potential Energy - Log scale
    axes[0, 1].set_title('Total Potential Energy (Log Scale)', fontsize=12)
    axes[0, 1].set_ylabel('Energy (J, log scale)', fontsize=10)
    axes[0, 1].set_xlabel('Iteration', fontsize=10)
    axes[0, 1].legend()
    axes[0, 1].grid(True, alpha=0.3, which='both')
    axes[0, 1].annotate('Log scale reveals growth phases\nand energy accumulation rate',
                       xy=(0.05, 0.85), xycoords='axes fraction',
                       fontsize=9, alpha=0.7,
                       bbox=dict(boxstyle='round', facecolor='lightblue', alpha=0.5))

    # Configure Plot 3: Kinetic Energy - Linear
    axes[1, 0].set_title('Kinetic Energy (Linear Scale)', fontsize=12)
    axes[1, 0].set_ylabel('Energy (J)', fontsize=10)
    axes[1, 0].set_xlabel('Iteration', fontsize=10)
    axes[1, 0].legend()
    axes[1, 0].grid(True, alpha=0.3)
    axes[1, 0].ticklabel_format(style='scientific', axis='y', scilimits=(0, 0))

    # Configure Plot 4: Kinetic Energy - Log scale
    axes[1, 1].set_title('Kinetic Energy (Log Scale)', fontsize=12)
    axes[1, 1].set_ylabel('Energy (J, log scale)', fontsize=10)
    axes[1, 1].set_xlabel('Iteration', fontsize=10)
    axes[1, 1].legend()
    axes[1, 1].grid(True, alpha=0.3, which='both')
    axes[1, 1].annotate('KE spikes indicate division events\nor mechanical instabilities',
                       xy=(0.05, 0.85), xycoords='axes fraction',
                       fontsize=9, alpha=0.7,
                       bbox=dict(boxstyle='round', facecolor='lightyellow', alpha=0.5))

    # Configure Plot 5: KE/PE Ratio - Log scale
    axes[2, 0].set_title('Kinetic/Potential Energy Ratio (Log Scale)', fontsize=12)
    axes[2, 0].set_ylabel('Ratio (%, log scale)', fontsize=10)
    axes[2, 0].set_xlabel('Iteration', fontsize=10)
    axes[2, 0].legend()
    axes[2, 0].grid(True, alpha=0.3, which='both')
    axes[2, 0].axhline(y=1.0, color='orange', linestyle='--', alpha=0.7,
                       label='1% threshold')
    axes[2, 0].axhline(y=0.1, color='gray', linestyle='--', alpha=0.5,
                       label='0.1% stability target')
    axes[2, 0].annotate('KE/PE < 0.1% → quasi-static equilibrium\n'
                       'KE/PE > 1% → dynamic instability',
                       xy=(0.55, 0.15), xycoords='axes fraction',
                       fontsize=9, alpha=0.7,
                       bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))

    # Plot 6: Energy composition stacked area (row 2, right)
    ax_stack = axes[2, 1]
    if energy_data:
        mode = list(energy_data.keys())[0]  # Use first available mode
        df = energy_data[mode]

        # Sample for clarity
        sample_rate = max(1, len(df) // 200)
        df_sampled = df.iloc[::sample_rate].copy()

        # Create stacked area of energy components (using new column names)
        energy_components = ['pressure_energy_mean', 'surface_tension_energy_mean', 'kinetic_energy_mean']
        component_labels = ['Pressure', 'Surface Tension', 'Kinetic']
        component_colors = ['#E74C3C', '#3498DB', '#F1C40F']

        # Filter valid data
        valid_mask = df_sampled[energy_components].apply(lambda x: x > 0).all(axis=1)
        df_valid = df_sampled[valid_mask]

        if len(df_valid) > 10:
            # Normalize to percentages
            total = df_valid[energy_components].sum(axis=1)
            percentages = df_valid[energy_components].div(total, axis=0) * 100

            ax_stack.stackplot(df_valid['iteration'],
                              [percentages[c] for c in energy_components],
                              labels=component_labels,
                              colors=component_colors,
                              alpha=0.8)
            ax_stack.set_ylim(0, 100)
            ax_stack.legend(loc='upper right')
            ax_stack.set_title(f'Energy Composition Over Time ({mode})', fontsize=12)
        else:
            ax_stack.text(0.5, 0.5, 'Insufficient data for composition plot',
                         ha='center', va='center', fontsize=12,
                         transform=ax_stack.transAxes)
    else:
        ax_stack.text(0.5, 0.5, 'Energy Analysis\nrequires simulation_statistics.csv',
                     ha='center', va='center', fontsize=12,
                     transform=ax_stack.transAxes)

    ax_stack.set_xlabel('Iteration', fontsize=10)
    ax_stack.set_ylabel('Composition (%)', fontsize=10)
    ax_stack.grid(True, alpha=0.3)

    # Configure Plot 7: Energy vs Cell Count (log-log)
    axes[3, 0].set_title('System Energy vs Cell Count (Log-Log)', fontsize=12)
    axes[3, 0].set_xlabel('Cell Count', fontsize=10)
    axes[3, 0].set_ylabel('Total System Energy (J)', fontsize=10)
    axes[3, 0].legend(fontsize=8)
    axes[3, 0].grid(True, alpha=0.3, which='both')
    axes[3, 0].annotate('Slope α indicates\nscaling: E ∝ N^α\nα≈1 → linear\nα≈2 → quadratic',
                       xy=(0.02, 0.75), xycoords='axes fraction',
                       fontsize=8, alpha=0.7,
                       bbox=dict(boxstyle='round', facecolor='lightcyan', alpha=0.5))

    # Plot 8: Energy balance summary (row 3, right)
    ax_summary = axes[3, 1]
    ax_summary.axis('off')
    summary_text = "Energy Balance Summary\n" + "=" * 35 + "\n\n"

    for mode, df in energy_data.items():
        summary_text += f"{mode}:\n"
        # Initial and final energies
        initial_pe = df['total_potential_energy_mean'].iloc[0] if len(df) > 0 else 0
        final_pe = df['total_potential_energy_mean'].iloc[-1] if len(df) > 0 else 0
        pe_change = ((final_pe - initial_pe) / initial_pe * 100) if initial_pe > 0 else 0

        initial_ke = df['kinetic_energy_mean'].iloc[0] if len(df) > 0 else 0
        final_ke = df['kinetic_energy_mean'].iloc[-1] if len(df) > 0 else 0

        # Cell count change
        initial_cells = df['cell_count'].iloc[0] if len(df) > 0 else 0
        final_cells = df['cell_count'].iloc[-1] if len(df) > 0 else 0

        summary_text += f"  Cells: {int(initial_cells)} → {int(final_cells)}\n"
        summary_text += f"  PE change: {pe_change:+.1f}%\n"
        summary_text += f"  Final KE/PE: {(final_ke/final_pe*100 if final_pe > 0 else 0):.3f}%\n\n"

    ax_summary.text(0.1, 0.9, summary_text, transform=ax_summary.transAxes,
                   fontsize=9, verticalalignment='top', family='monospace',
                   bbox=dict(boxstyle='round', facecolor='whitesmoke', alpha=0.8))
    ax_summary.set_title('Energy Balance Summary', fontsize=12)

    plt.suptitle('Energy Conservation Analysis (Enhanced)\nSimuCell3D Benchmark', fontsize=14)
    plt.tight_layout()

    plots_dir = bench_path / 'plots'
    plots_dir.mkdir(exist_ok=True)
    output_path = plots_dir / 'energy_analysis.png'
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    print(f"Saved plot to: {output_path}")

    plt.close()


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python plot_energy_distribution.py /path/to/benchmark_dir")
        print("\nExample:")
        print("  python plot_energy_distribution.py ../doc/working/64k_growth_benchmark_20251130/")
        sys.exit(1)

    main(sys.argv[1])
