#!/usr/bin/env python3
"""
Plot biological metrics: volume distribution, pressure, isoperimetric ratio.
Reads from simulation_statistics.csv files.

Enhanced with log scale options and statistical annotations for comparative analysis.

Usage:
    python plot_biological_metrics.py /path/to/benchmark_directory

The benchmark directory should contain:
    sim_static/simulation_statistics.csv
    sim_adaptive/simulation_statistics.csv
"""
import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
from pathlib import Path
from scipy import stats
import sys


def compute_statistics(data: np.ndarray) -> dict:
    """Compute statistical moments for distribution characterization.

    Args:
        data: Array of values

    Returns:
        Dictionary with mean, std, skewness, kurtosis
    """
    return {
        'mean': np.mean(data),
        'std': np.std(data),
        'skewness': stats.skew(data),
        'kurtosis': stats.kurtosis(data),
        'median': np.median(data),
        'cv': np.std(data) / np.mean(data) if np.mean(data) != 0 else 0  # Coefficient of variation
    }


def add_stats_annotation(ax, data: np.ndarray, position: tuple = (0.02, 0.98),
                         fontsize: int = 8, include_skew: bool = True) -> None:
    """Add statistical annotation box to axis.

    Args:
        ax: Matplotlib axis
        data: Data array
        position: (x, y) position in axes fraction
        fontsize: Font size for annotation
        include_skew: Whether to include skewness/kurtosis
    """
    s = compute_statistics(data)
    if include_skew:
        text = f"μ={s['mean']:.2e}\nσ={s['std']:.2e}\nCV={s['cv']:.2f}\nskew={s['skewness']:.2f}"
    else:
        text = f"μ={s['mean']:.2e}\nσ={s['std']:.2e}\nCV={s['cv']:.2f}"

    ax.annotate(text, xy=position, xycoords='axes fraction',
                fontsize=fontsize, verticalalignment='top',
                bbox=dict(boxstyle='round', facecolor='white', alpha=0.8, edgecolor='gray'))


def main(bench_dir: str) -> None:
    """Generate biological metrics comparison plots.

    Enhanced with log-scale options and statistical annotations.

    Args:
        bench_dir: Path to benchmark directory
    """
    bench_path = Path(bench_dir)

    # Expanded layout: 3 rows × 3 columns for comprehensive analysis
    # Row 1: Volume distribution (linear), Volume distribution (log-x), Volume error
    # Row 2: Pressure distribution (linear), Pressure distribution (log-x), Volume-Pressure scatter
    # Row 3: Contact area fraction, Energy per cell, Statistical summary
    fig, axes = plt.subplots(3, 3, figsize=(18, 14))

    csv_paths = {
        'Static': 'sim_static/simulation_statistics.csv',
        'Adaptive': 'sim_adaptive/simulation_statistics.csv'
    }

    colors = {
        'Static': '#4CAF50',   # Green
        'Adaptive': '#F44336'  # Red
    }

    found_any = False
    all_data = {}  # Store data for cross-mode analysis

    for mode, csv_rel in csv_paths.items():
        csv_path = bench_path / csv_rel
        if not csv_path.exists():
            print(f"Warning: {csv_path} not found, skipping {mode}")
            continue

        found_any = True
        df = pd.read_csv(csv_path)

        # Get the last iteration for final state analysis
        last_iter = df['iteration'].max()
        df_last = df[df['iteration'] == last_iter].copy()
        all_data[mode] = df_last

        cell_count = len(df_last)
        print(f"{mode}: {cell_count} cells at iteration {last_iter}")

        # === ROW 1: Volume Analysis ===

        # Plot [0,0]: Volume histogram (linear scale)
        if 'volume' in df_last.columns:
            volumes_nl = df_last['volume'] * 1e9  # Convert to nL
            axes[0, 0].hist(volumes_nl, bins=50, alpha=0.6,
                           label=f'{mode} (n={cell_count})', color=colors[mode],
                           edgecolor='white', linewidth=0.5)

        # Plot [0,1]: Volume histogram (log-x scale for span visualization)
        if 'volume' in df_last.columns:
            volumes_pos = df_last['volume'][df_last['volume'] > 0]
            if len(volumes_pos) > 0:
                log_bins = np.logspace(np.log10(volumes_pos.min()),
                                       np.log10(volumes_pos.max()), 50)
                axes[0, 1].hist(volumes_pos, bins=log_bins, alpha=0.6,
                               label=mode, color=colors[mode],
                               edgecolor='white', linewidth=0.5)

        # Plot [0,2]: Volume error relative to target
        if 'volume' in df_last.columns and 'target_volume' in df_last.columns:
            vol_error = (df_last['volume'] - df_last['target_volume']) / df_last['target_volume'] * 100
            axes[0, 2].hist(vol_error, bins=50, alpha=0.6,
                           label=f'{mode} (σ={vol_error.std():.1f}%)', color=colors[mode],
                           edgecolor='white', linewidth=0.5)

        # === ROW 2: Pressure Analysis ===

        # Plot [1,0]: Pressure histogram (linear scale)
        if 'pressure' in df_last.columns:
            axes[1, 0].hist(df_last['pressure'], bins=50, alpha=0.6,
                           label=mode, color=colors[mode],
                           edgecolor='white', linewidth=0.5)

        # Plot [1,1]: Pressure histogram (log-x scale for log-normal analysis)
        if 'pressure' in df_last.columns:
            pressure_pos = df_last['pressure'][df_last['pressure'] > 0]
            if len(pressure_pos) > 0:
                log_bins = np.logspace(np.log10(pressure_pos.min()),
                                       np.log10(pressure_pos.max()), 50)
                axes[1, 1].hist(pressure_pos, bins=log_bins, alpha=0.6,
                               label=mode, color=colors[mode],
                               edgecolor='white', linewidth=0.5)

        # Plot [1,2]: Volume vs Pressure scatter
        if 'volume' in df_last.columns and 'pressure' in df_last.columns:
            sample_size = min(500, len(df_last))
            df_sample = df_last.sample(sample_size) if len(df_last) > sample_size else df_last
            axes[1, 2].scatter(df_sample['volume'] * 1e9, df_sample['pressure'],
                              alpha=0.3, label=mode, color=colors[mode], s=15)

        # === ROW 3: Contact & Energy Analysis ===

        # Plot [2,0]: Cell contact area fraction
        if 'cell_contact_area_fraction' in df_last.columns:
            axes[2, 0].hist(df_last['cell_contact_area_fraction'], bins=50, alpha=0.6,
                           label=mode, color=colors[mode],
                           edgecolor='white', linewidth=0.5)
        elif 'area' in df_last.columns and 'target_area' in df_last.columns:
            area_ratio = df_last['area'] / df_last['target_area']
            axes[2, 0].hist(area_ratio, bins=50, alpha=0.6,
                           label=f'{mode} (Area Ratio)', color=colors[mode],
                           edgecolor='white', linewidth=0.5)

        # Plot [2,1]: Total energy per cell (log scale)
        if 'total_potential_energy' in df_last.columns:
            energy_pos = df_last['total_potential_energy'][df_last['total_potential_energy'] > 0]
            if len(energy_pos) > 0:
                log_bins = np.logspace(np.log10(energy_pos.min()),
                                       np.log10(energy_pos.max()), 50)
                axes[2, 1].hist(energy_pos, bins=log_bins, alpha=0.6,
                               label=mode, color=colors[mode],
                               edgecolor='white', linewidth=0.5)

    if not found_any:
        print(f"No simulation_statistics.csv files found in {bench_path}")
        return

    # === ROW 1 CONFIGURATION ===

    # [0,0]: Volume distribution (linear)
    axes[0, 0].set_xlabel('Volume (×10⁻⁹ m³)', fontsize=10)
    axes[0, 0].set_ylabel('Cell Count', fontsize=10)
    axes[0, 0].set_title('Volume Distribution (Linear)', fontsize=11)
    axes[0, 0].legend(fontsize=8)
    axes[0, 0].grid(True, alpha=0.3)
    # Add division threshold reference
    div_vol = 1.4e-14 * 1e9
    if div_vol < axes[0, 0].get_xlim()[1]:
        axes[0, 0].axvline(x=div_vol, color='gray', linestyle='--', alpha=0.7)
        axes[0, 0].annotate('Div', xy=(div_vol, axes[0, 0].get_ylim()[1] * 0.9),
                           fontsize=8, alpha=0.7)

    # [0,1]: Volume distribution (log-x)
    axes[0, 1].set_xlabel('Volume (m³)', fontsize=10)
    axes[0, 1].set_ylabel('Cell Count', fontsize=10)
    axes[0, 1].set_title('Volume Distribution (Log Scale)', fontsize=11)
    axes[0, 1].set_xscale('log')
    axes[0, 1].legend(fontsize=8)
    axes[0, 1].grid(True, alpha=0.3, which='both')
    axes[0, 1].annotate('Log scale reveals\nvolume span across\ncell cycle',
                       xy=(0.02, 0.98), xycoords='axes fraction',
                       fontsize=8, verticalalignment='top', alpha=0.7,
                       bbox=dict(boxstyle='round', facecolor='lightyellow', alpha=0.5))

    # [0,2]: Volume error
    axes[0, 2].set_xlabel('Volume Error (%)', fontsize=10)
    axes[0, 2].set_ylabel('Cell Count', fontsize=10)
    axes[0, 2].set_title('Volume Error vs Target', fontsize=11)
    axes[0, 2].axvline(x=0, color='black', linestyle='-', alpha=0.5, linewidth=1)
    axes[0, 2].legend(fontsize=8)
    axes[0, 2].grid(True, alpha=0.3)
    axes[0, 2].annotate('±0 = Perfect\nvolume regulation',
                       xy=(0.02, 0.98), xycoords='axes fraction',
                       fontsize=8, verticalalignment='top', alpha=0.7,
                       bbox=dict(boxstyle='round', facecolor='lightgreen', alpha=0.5))

    # === ROW 2 CONFIGURATION ===

    # [1,0]: Pressure distribution (linear)
    axes[1, 0].set_xlabel('Pressure (Pa)', fontsize=10)
    axes[1, 0].set_ylabel('Cell Count', fontsize=10)
    axes[1, 0].set_title('Pressure Distribution (Linear)', fontsize=11)
    axes[1, 0].legend(fontsize=8)
    axes[1, 0].grid(True, alpha=0.3)
    axes[1, 0].annotate('Epithelial range\n100-500 Pa',
                       xy=(0.75, 0.85), xycoords='axes fraction',
                       fontsize=8, alpha=0.7,
                       bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))

    # [1,1]: Pressure distribution (log-x) - for log-normal analysis
    axes[1, 1].set_xlabel('Pressure (Pa)', fontsize=10)
    axes[1, 1].set_ylabel('Cell Count', fontsize=10)
    axes[1, 1].set_title('Pressure Distribution (Log Scale)', fontsize=11)
    axes[1, 1].set_xscale('log')
    axes[1, 1].legend(fontsize=8)
    axes[1, 1].grid(True, alpha=0.3, which='both')
    axes[1, 1].annotate('Pressure often\nlog-normal in\ncompressed tissue',
                       xy=(0.02, 0.98), xycoords='axes fraction',
                       fontsize=8, verticalalignment='top', alpha=0.7,
                       bbox=dict(boxstyle='round', facecolor='lightyellow', alpha=0.5))

    # [1,2]: Volume vs Pressure scatter
    axes[1, 2].set_xlabel('Volume (×10⁻⁹ m³)', fontsize=10)
    axes[1, 2].set_ylabel('Pressure (Pa)', fontsize=10)
    axes[1, 2].set_title('Volume-Pressure Correlation', fontsize=11)
    axes[1, 2].legend(fontsize=8)
    axes[1, 2].grid(True, alpha=0.3)
    axes[1, 2].annotate('Smaller cells =\nhigher pressure',
                       xy=(0.7, 0.1), xycoords='axes fraction',
                       fontsize=8, alpha=0.7,
                       bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))

    # === ROW 3 CONFIGURATION ===

    # [2,0]: Contact area fraction
    axes[2, 0].set_xlabel('Contact Area Fraction', fontsize=10)
    axes[2, 0].set_ylabel('Cell Count', fontsize=10)
    axes[2, 0].set_title('Contact Area Distribution', fontsize=11)
    axes[2, 0].legend(fontsize=8)
    axes[2, 0].grid(True, alpha=0.3)
    axes[2, 0].axvline(x=0.85, color='gray', linestyle='--', alpha=0.7)
    axes[2, 0].annotate('Healthy ~85%',
                       xy=(0.85, axes[2, 0].get_ylim()[1] * 0.7),
                       fontsize=8, alpha=0.7,
                       bbox=dict(boxstyle='round', facecolor='lightgreen', alpha=0.5))

    # [2,1]: Energy per cell (log scale)
    axes[2, 1].set_xlabel('Total Potential Energy (J)', fontsize=10)
    axes[2, 1].set_ylabel('Cell Count', fontsize=10)
    axes[2, 1].set_title('Energy Distribution (Log Scale)', fontsize=11)
    axes[2, 1].set_xscale('log')
    axes[2, 1].legend(fontsize=8)
    axes[2, 1].grid(True, alpha=0.3, which='both')

    # [2,2]: Statistical summary table
    axes[2, 2].axis('off')
    summary_text = "Statistical Summary\n" + "=" * 30 + "\n"
    for mode, df_last in all_data.items():
        summary_text += f"\n{mode}:\n"
        if 'volume' in df_last.columns:
            s = compute_statistics(df_last['volume'].values)
            summary_text += f"  Vol CV: {s['cv']:.3f}, skew: {s['skewness']:.2f}\n"
        if 'pressure' in df_last.columns:
            s = compute_statistics(df_last['pressure'].values)
            summary_text += f"  Press CV: {s['cv']:.3f}, skew: {s['skewness']:.2f}\n"

    axes[2, 2].text(0.1, 0.9, summary_text, transform=axes[2, 2].transAxes,
                   fontsize=9, verticalalignment='top', family='monospace',
                   bbox=dict(boxstyle='round', facecolor='whitesmoke', alpha=0.8))
    axes[2, 2].set_title('Statistical Summary', fontsize=11)

    plt.suptitle('Biological Metrics Analysis (Enhanced)\nSimuCell3D Benchmark Comparison', fontsize=14)
    plt.tight_layout()

    plots_dir = bench_path / 'plots'
    plots_dir.mkdir(exist_ok=True)
    output_path = plots_dir / 'biological_metrics.png'
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    print(f"Saved plot to: {output_path}")

    plt.show()


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python plot_biological_metrics.py /path/to/benchmark_dir")
        print("\nExample:")
        print("  python plot_biological_metrics.py ../doc/working/64k_growth_benchmark_20251130/")
        sys.exit(1)

    main(sys.argv[1])
