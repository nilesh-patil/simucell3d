#!/usr/bin/env python3
"""
Plot iteration timing breakdown by computational phase.
Includes log scale views and computational complexity analysis.

Usage:
    python plot_iteration_timing.py /path/to/benchmark_directory

The benchmark directory should contain:
    sim_static/performance_diagnostics.csv
    sim_adaptive/performance_diagnostics.csv
"""
import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
from pathlib import Path
import sys


def main(bench_dir: str) -> None:
    """Generate performance timing breakdown plots with log scale options.

    Args:
        bench_dir: Path to benchmark directory
    """
    bench_path = Path(bench_dir)

    # Create 3x2 grid for comprehensive timing analysis
    fig, axes = plt.subplots(3, 2, figsize=(14, 15))

    # Try both static and adaptive diagnostics
    csv_paths = [
        ('Static', 'sim_static/performance_diagnostics.csv', '#4CAF50'),
        ('Adaptive', 'sim_adaptive/performance_diagnostics.csv', '#F44336')
    ]

    all_data = {}

    for mode, csv_rel, color in csv_paths:
        csv_path = bench_path / csv_rel
        if csv_path.exists():
            all_data[mode] = (pd.read_csv(csv_path), color)
            print(f"Loaded diagnostics from: {csv_path}")

    if not all_data:
        print(f"No performance_diagnostics.csv found in {bench_path}")
        print("Looking for files in sim_static/ or sim_adaptive/")
        return

    # Use first available mode for detailed analysis
    mode_name, (df_main, main_color) = list(all_data.items())[0]

    # Define phases and colors
    phases = ['mesh_refinement_ms', 'contact_detection_ms',
              'polarization_internal_forces_ms', 'time_integration_ms']
    phase_labels = ['Mesh Refinement', 'Contact Detection',
                   'Polarization/Forces', 'Time Integration']
    phase_colors = ['#2ECC71', '#E74C3C', '#3498DB', '#9B59B6']

    # Check which columns exist
    available_phases = [p for p in phases if p in df_main.columns]
    available_labels = [phase_labels[phases.index(p)] for p in available_phases]
    available_colors = [phase_colors[phases.index(p)] for p in available_phases]

    if not available_phases:
        print(f"No timing columns found. Available columns: {df_main.columns.tolist()}")
        return

    # Sampling for clarity
    sample_rate = max(1, len(df_main) // 500)
    df_sampled = df_main.iloc[::sample_rate].copy()

    # Plot 1: Stacked area chart - Linear scale (top-left)
    axes[0, 0].stackplot(df_sampled['iteration'],
                        [df_sampled[p] for p in available_phases],
                        labels=available_labels, colors=available_colors, alpha=0.8)
    axes[0, 0].set_xlabel('Iteration', fontsize=10)
    axes[0, 0].set_ylabel('Time (ms)', fontsize=10)
    axes[0, 0].set_title(f'Iteration Timing Breakdown - Linear ({mode_name})', fontsize=12)
    axes[0, 0].legend(loc='upper left')
    axes[0, 0].grid(True, alpha=0.3)

    # Plot 2: Total iteration time - Log scale (top-right)
    if 'total_iteration_ms' in df_main.columns:
        for mode, (df, color) in all_data.items():
            df_s = df.iloc[::sample_rate]
            df_pos = df_s[df_s['total_iteration_ms'] > 0]
            axes[0, 1].semilogy(df_pos['iteration'], df_pos['total_iteration_ms'],
                               label=mode, color=color, alpha=0.8, linewidth=1.5)
        axes[0, 1].set_ylabel('Total Time (ms, log scale)', fontsize=10)
        axes[0, 1].legend()
    else:
        # Sum phases for total
        df_sampled['total_time'] = df_sampled[available_phases].sum(axis=1)
        df_pos = df_sampled[df_sampled['total_time'] > 0]
        axes[0, 1].semilogy(df_pos['iteration'], df_pos['total_time'],
                           color=main_color, alpha=0.8, linewidth=1.5)
        axes[0, 1].set_ylabel('Total Phase Time (ms, log scale)', fontsize=10)

    axes[0, 1].set_xlabel('Iteration', fontsize=10)
    axes[0, 1].set_title('Total Iteration Time (Log Scale)', fontsize=12)
    axes[0, 1].grid(True, alpha=0.3, which='both')
    axes[0, 1].annotate('Log scale reveals timing variations\nacross orders of magnitude',
                       xy=(0.05, 0.85), xycoords='axes fraction',
                       fontsize=9, alpha=0.7,
                       bbox=dict(boxstyle='round', facecolor='lightblue', alpha=0.5))

    # Plot 3: Individual phase timing - Log scale (mid-left)
    for phase, label, color in zip(available_phases, available_labels, available_colors):
        df_pos = df_sampled[df_sampled[phase] > 0]
        if len(df_pos) > 0:
            axes[1, 0].semilogy(df_pos['iteration'], df_pos[phase],
                               label=label, color=color, alpha=0.7, linewidth=1)

    axes[1, 0].set_xlabel('Iteration', fontsize=10)
    axes[1, 0].set_ylabel('Phase Time (ms, log scale)', fontsize=10)
    axes[1, 0].set_title('Individual Phase Timing (Log Scale)', fontsize=12)
    axes[1, 0].legend(loc='upper left')
    axes[1, 0].grid(True, alpha=0.3, which='both')
    axes[1, 0].annotate('Phase dominance shifts\nas cell count grows',
                       xy=(0.55, 0.15), xycoords='axes fraction',
                       fontsize=9, alpha=0.7,
                       bbox=dict(boxstyle='round', facecolor='lightyellow', alpha=0.5))

    # Plot 4: Pie chart of average breakdown (mid-right)
    avg_times = [df_main[p].mean() for p in available_phases]
    total_time = sum(avg_times)

    max_idx = avg_times.index(max(avg_times))
    explode = [0.1 if i == max_idx else 0 for i in range(len(avg_times))]

    wedges, texts, autotexts = axes[1, 1].pie(
        avg_times,
        labels=available_labels,
        autopct='%1.1f%%',
        colors=available_colors,
        explode=explode,
        shadow=True
    )
    axes[1, 1].set_title(f'Average Phase Distribution\n(Total: {total_time:.1f}ms avg per iteration)', fontsize=12)

    # Plot 5: Thread imbalance / CoV - Log scale (bottom-left)
    ax_imbalance = axes[2, 0]
    if 'thread_imbalance_pct' in df_main.columns:
        for mode, (df, color) in all_data.items():
            df_s = df.iloc[::sample_rate]
            df_pos = df_s[df_s['thread_imbalance_pct'] > 0]
            if len(df_pos) > 0:
                ax_imbalance.semilogy(df_pos['iteration'], df_pos['thread_imbalance_pct'],
                                     label=mode, color=color, alpha=0.7, linewidth=1)
        ax_imbalance.set_ylabel('Thread Imbalance (%, log scale)', fontsize=10)
        ax_imbalance.axhline(y=10, color='orange', linestyle='--', alpha=0.7, label='10% threshold')
        ax_imbalance.axhline(y=5, color='gray', linestyle='--', alpha=0.5, label='5% target')
    elif 'cov' in df_main.columns:
        for mode, (df, color) in all_data.items():
            df_s = df.iloc[::sample_rate]
            df_pos = df_s[df_s['cov'] > 0]
            if len(df_pos) > 0:
                ax_imbalance.semilogy(df_pos['iteration'], df_pos['cov'],
                                     label=mode, color=color, alpha=0.7, linewidth=1)
        ax_imbalance.set_ylabel('CoV (log scale)', fontsize=10)
        ax_imbalance.axhline(y=0.1, color='orange', linestyle='--', alpha=0.7, label='10% CoV')
        ax_imbalance.axhline(y=0.05, color='gray', linestyle='--', alpha=0.5, label='5% CoV')
    else:
        ax_imbalance.text(0.5, 0.5, 'No thread imbalance data available',
                         ha='center', va='center', fontsize=12,
                         transform=ax_imbalance.transAxes)

    ax_imbalance.set_xlabel('Iteration', fontsize=10)
    ax_imbalance.set_title('Workload Imbalance (Log Scale)', fontsize=12)
    ax_imbalance.legend(loc='upper left')
    ax_imbalance.grid(True, alpha=0.3, which='both')
    ax_imbalance.annotate('Higher imbalance = less parallel efficiency\nAdaptive scheduling helps here',
                         xy=(0.45, 0.85), xycoords='axes fraction',
                         fontsize=9, alpha=0.7,
                         bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))

    # Plot 6: Scaling analysis - Time vs Cells (bottom-right)
    ax_scaling = axes[2, 1]
    if 'cells' in df_main.columns:
        total_col = 'total_iteration_ms' if 'total_iteration_ms' in df_main.columns else None
        if total_col is None:
            df_main['total_iteration_ms'] = df_main[available_phases].sum(axis=1)
            total_col = 'total_iteration_ms'

        for mode, (df, color) in all_data.items():
            df_s = df.iloc[::sample_rate]
            df_pos = df_s[(df_s['cells'] > 0) & (df_s[total_col] > 0)]
            if len(df_pos) > 0:
                ax_scaling.loglog(df_pos['cells'], df_pos[total_col],
                                 'o', alpha=0.3, markersize=3, color=color, label=f'{mode} data')

                # Fit power law: time = a * cells^b
                if len(df_pos) > 10:
                    try:
                        log_cells = np.log10(df_pos['cells'])
                        log_time = np.log10(df_pos[total_col])
                        coeffs = np.polyfit(log_cells, log_time, 1)
                        slope = coeffs[0]

                        # Plot fit line
                        x_fit = np.logspace(np.log10(df_pos['cells'].min()),
                                           np.log10(df_pos['cells'].max()), 50)
                        y_fit = 10**(coeffs[1]) * x_fit**slope
                        ax_scaling.loglog(x_fit, y_fit, '--', color=color, alpha=0.8,
                                         label=f'{mode} fit: O(N^{slope:.2f})')
                    except Exception:
                        pass

        ax_scaling.set_xlabel('Cell Count (log scale)', fontsize=10)
        ax_scaling.set_ylabel('Iteration Time (ms, log scale)', fontsize=10)
        ax_scaling.set_title('Computational Complexity Analysis', fontsize=12)
        ax_scaling.legend(loc='upper left', fontsize=8)
        ax_scaling.grid(True, alpha=0.3, which='both')

        # Add reference lines for common complexities
        if 'cells' in df_main.columns:
            x_ref = np.array([df_main['cells'].min(), df_main['cells'].max()])
            ax_scaling.annotate('Slope indicates complexity:\n'
                               '  1.0 = O(N) linear\n'
                               '  2.0 = O(N²) quadratic\n'
                               '  1.5 = O(N^1.5) typical contact',
                               xy=(0.55, 0.05), xycoords='axes fraction',
                               fontsize=8, alpha=0.7,
                               bbox=dict(boxstyle='round', facecolor='lightgreen', alpha=0.5))
    else:
        # Fallback: show timing statistics
        stats_text = "Phase Timing Statistics:\n\n"
        for phase, label in zip(available_phases, available_labels):
            mean_time = df_main[phase].mean()
            std_time = df_main[phase].std()
            max_time = df_main[phase].max()
            stats_text += f"{label}:\n"
            stats_text += f"  Mean: {mean_time:.2f}ms\n"
            stats_text += f"  Std:  {std_time:.2f}ms\n"
            stats_text += f"  Max:  {max_time:.2f}ms\n\n"
        ax_scaling.text(0.1, 0.9, stats_text, ha='left', va='top', fontsize=10,
                       transform=ax_scaling.transAxes, family='monospace')
        ax_scaling.set_title('Phase Timing Statistics', fontsize=12)
        ax_scaling.axis('off')

    plt.suptitle('Performance Bottleneck Analysis\nSimuCell3D Benchmark', fontsize=14)
    plt.tight_layout()

    plots_dir = bench_path / 'plots'
    plots_dir.mkdir(exist_ok=True)
    output_path = plots_dir / 'performance_breakdown.png'
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    print(f"Saved plot to: {output_path}")

    plt.close()


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python plot_iteration_timing.py /path/to/benchmark_dir")
        print("\nExample:")
        print("  python plot_iteration_timing.py ../doc/working/64k_growth_benchmark_20251130/")
        sys.exit(1)

    main(sys.argv[1])
