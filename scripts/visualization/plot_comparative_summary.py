#!/usr/bin/env python3
"""
Executive Dashboard: Comparative Summary of Static vs Adaptive Scheduling.

Generates a high-level overview with:
- Speedup bar chart
- Multi-dimensional radar chart
- Throughput efficiency
- Key statistics summary

Usage:
    python plot_comparative_summary.py /path/to/benchmark_directory

The benchmark directory should contain:
    sim_static/simulation_statistics.csv
    sim_static/performance_diagnostics.csv
    sim_adaptive/simulation_statistics.csv
    sim_adaptive/performance_diagnostics.csv
"""
import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
from pathlib import Path
from matplotlib.patches import Patch
import sys


def load_performance_data(bench_path: Path) -> dict:
    """Load performance diagnostics for both modes.

    Args:
        bench_path: Path to benchmark directory

    Returns:
        Dictionary with performance data for each mode
    """
    data = {}
    modes = {
        'Static': 'sim_static/performance_diagnostics.csv',
        'Adaptive': 'sim_adaptive/performance_diagnostics.csv'
    }

    for mode, rel_path in modes.items():
        csv_path = bench_path / rel_path
        if csv_path.exists():
            df = pd.read_csv(csv_path)
            data[mode] = df
            print(f"Loaded {mode}: {len(df)} iterations")
        else:
            print(f"Warning: {csv_path} not found")

    return data


def load_simulation_stats(bench_path: Path) -> dict:
    """Load simulation statistics for biological metrics.

    Args:
        bench_path: Path to benchmark directory

    Returns:
        Dictionary with simulation statistics for each mode
    """
    data = {}
    modes = {
        'Static': 'sim_static/simulation_statistics.csv',
        'Adaptive': 'sim_adaptive/simulation_statistics.csv'
    }

    for mode, rel_path in modes.items():
        csv_path = bench_path / rel_path
        if csv_path.exists():
            df = pd.read_csv(csv_path)
            data[mode] = df
        else:
            print(f"Warning: {csv_path} not found")

    return data


def compute_metrics(perf_data: dict, sim_data: dict) -> dict:
    """Compute comparison metrics from raw data.

    Args:
        perf_data: Performance diagnostics dictionary
        sim_data: Simulation statistics dictionary

    Returns:
        Dictionary with computed metrics for each mode
    """
    metrics = {}

    for mode in perf_data.keys():
        perf_df = perf_data[mode]
        sim_df = sim_data.get(mode, pd.DataFrame())

        m = {}

        # Performance metrics
        if 'iteration_time_ms' in perf_df.columns:
            m['avg_iter_time_ms'] = perf_df['iteration_time_ms'].mean()
            m['std_iter_time_ms'] = perf_df['iteration_time_ms'].std()
            m['total_time_s'] = perf_df['iteration_time_ms'].sum() / 1000

        if 'cell_count' in perf_df.columns:
            m['final_cells'] = perf_df['cell_count'].iloc[-1]
            m['initial_cells'] = perf_df['cell_count'].iloc[0]
            # Throughput: cells processed per second
            total_cell_iterations = perf_df['cell_count'].sum()
            if m.get('total_time_s', 0) > 0:
                m['throughput'] = total_cell_iterations / m['total_time_s']

        # Biological accuracy metrics (from last iteration)
        if len(sim_df) > 0:
            last_iter = sim_df['iteration'].max()
            df_last = sim_df[sim_df['iteration'] == last_iter]

            if 'volume' in df_last.columns and 'target_volume' in df_last.columns:
                vol_error = np.abs(df_last['volume'] - df_last['target_volume']) / df_last['target_volume']
                m['vol_error_mean'] = vol_error.mean() * 100  # percentage
                m['vol_error_std'] = vol_error.std() * 100

            if 'pressure' in df_last.columns:
                m['pressure_cv'] = df_last['pressure'].std() / df_last['pressure'].mean()

            if 'kinetic_energy' in df_last.columns and 'total_potential_energy' in df_last.columns:
                ke_pe_ratio = df_last['kinetic_energy'].sum() / df_last['total_potential_energy'].sum()
                m['ke_pe_ratio'] = ke_pe_ratio * 100  # percentage

        metrics[mode] = m

    return metrics


def create_speedup_chart(ax, metrics: dict, colors: dict) -> None:
    """Create speedup comparison bar chart.

    Args:
        ax: Matplotlib axis
        metrics: Computed metrics dictionary
        colors: Color scheme dictionary
    """
    if 'Static' not in metrics or 'Adaptive' not in metrics:
        ax.text(0.5, 0.5, 'Insufficient data for comparison',
                ha='center', va='center', transform=ax.transAxes)
        return

    static = metrics['Static']
    adaptive = metrics['Adaptive']

    # Compute speedups (Static as baseline)
    categories = []
    speedups = []
    bar_colors = []

    if 'avg_iter_time_ms' in static and 'avg_iter_time_ms' in adaptive:
        speedup = static['avg_iter_time_ms'] / adaptive['avg_iter_time_ms']
        categories.append('Iteration\nTime')
        speedups.append(speedup)
        bar_colors.append(colors['Adaptive'] if speedup > 1 else colors['Static'])

    if 'total_time_s' in static and 'total_time_s' in adaptive:
        speedup = static['total_time_s'] / adaptive['total_time_s']
        categories.append('Total\nRuntime')
        speedups.append(speedup)
        bar_colors.append(colors['Adaptive'] if speedup > 1 else colors['Static'])

    if 'throughput' in static and 'throughput' in adaptive:
        speedup = adaptive['throughput'] / static['throughput']
        categories.append('Cell\nThroughput')
        speedups.append(speedup)
        bar_colors.append(colors['Adaptive'] if speedup > 1 else colors['Static'])

    if not categories:
        ax.text(0.5, 0.5, 'No performance data available',
                ha='center', va='center', transform=ax.transAxes)
        return

    x = np.arange(len(categories))
    bars = ax.bar(x, speedups, color=bar_colors, edgecolor='black', linewidth=1.5)

    # Add value labels on bars
    for bar, speedup in zip(bars, speedups):
        height = bar.get_height()
        ax.annotate(f'{speedup:.2f}x',
                    xy=(bar.get_x() + bar.get_width() / 2, height),
                    xytext=(0, 3), textcoords='offset points',
                    ha='center', va='bottom', fontsize=11, fontweight='bold')

    # Reference line at 1.0
    ax.axhline(y=1.0, color='gray', linestyle='--', linewidth=2, alpha=0.7)
    ax.annotate('Baseline (Static)', xy=(len(categories) - 0.5, 1.02),
                fontsize=9, alpha=0.7)

    ax.set_xticks(x)
    ax.set_xticklabels(categories, fontsize=10)
    ax.set_ylabel('Speedup Factor', fontsize=11)
    ax.set_title('Performance Speedup: Adaptive vs Static', fontsize=12, fontweight='bold')
    ax.set_ylim(0, max(speedups) * 1.2)
    ax.grid(True, axis='y', alpha=0.3)

    # Legend
    legend_elements = [
        Patch(facecolor=colors['Adaptive'], edgecolor='black', label='Adaptive wins'),
        Patch(facecolor=colors['Static'], edgecolor='black', label='Static wins')
    ]
    ax.legend(handles=legend_elements, loc='upper right', fontsize=9)


def create_radar_chart(ax, metrics: dict, colors: dict) -> None:
    """Create radar/spider chart for multi-dimensional comparison.

    Args:
        ax: Matplotlib axis (should be polar projection)
        metrics: Computed metrics dictionary
        colors: Color scheme dictionary
    """
    # Define dimensions to compare (normalized 0-1, higher is better)
    dimensions = ['Speed', 'Efficiency', 'Vol Accuracy', 'Pressure\nStability', 'Energy\nBalance']
    num_dims = len(dimensions)

    # Compute normalized scores for each mode
    scores = {}
    for mode in metrics.keys():
        m = metrics[mode]
        mode_scores = []

        # Speed: inverse of iteration time (higher = faster)
        if 'avg_iter_time_ms' in m:
            # Normalize: assume 1-100ms range
            speed = 1 - min(m['avg_iter_time_ms'] / 100, 1)
            mode_scores.append(max(0.1, speed))
        else:
            mode_scores.append(0.5)

        # Efficiency: throughput normalized
        if 'throughput' in m:
            # Normalize: assume 0-100k cells/s range
            eff = min(m['throughput'] / 100000, 1)
            mode_scores.append(max(0.1, eff))
        else:
            mode_scores.append(0.5)

        # Volume accuracy: inverse of error
        if 'vol_error_mean' in m:
            # Error typically 0-20%, lower is better
            acc = 1 - min(m['vol_error_mean'] / 20, 1)
            mode_scores.append(max(0.1, acc))
        else:
            mode_scores.append(0.5)

        # Pressure stability: inverse of CV
        if 'pressure_cv' in m:
            # CV typically 0-1, lower is better
            stab = 1 - min(m['pressure_cv'], 1)
            mode_scores.append(max(0.1, stab))
        else:
            mode_scores.append(0.5)

        # Energy balance: inverse of KE/PE ratio
        if 'ke_pe_ratio' in m:
            # Ratio typically 0-10%, lower is better
            bal = 1 - min(m['ke_pe_ratio'] / 10, 1)
            mode_scores.append(max(0.1, bal))
        else:
            mode_scores.append(0.5)

        scores[mode] = mode_scores

    # Create radar chart
    angles = np.linspace(0, 2 * np.pi, num_dims, endpoint=False).tolist()
    angles += angles[:1]  # Complete the loop

    ax.set_theta_offset(np.pi / 2)
    ax.set_theta_direction(-1)

    # Draw dimension labels
    ax.set_xticks(angles[:-1])
    ax.set_xticklabels(dimensions, fontsize=9)

    # Plot each mode
    for mode, mode_scores in scores.items():
        values = mode_scores + mode_scores[:1]  # Complete the loop
        ax.plot(angles, values, 'o-', linewidth=2, label=mode, color=colors[mode])
        ax.fill(angles, values, alpha=0.25, color=colors[mode])

    ax.set_ylim(0, 1)
    ax.set_title('Multi-Dimensional Comparison', fontsize=12, fontweight='bold', pad=20)
    ax.legend(loc='upper right', bbox_to_anchor=(1.3, 1.0), fontsize=9)


def create_throughput_timeline(ax, perf_data: dict, colors: dict) -> None:
    """Create throughput over time visualization.

    Args:
        ax: Matplotlib axis
        perf_data: Performance diagnostics dictionary
        colors: Color scheme dictionary
    """
    for mode, df in perf_data.items():
        if 'iteration_time_ms' not in df.columns or 'cell_count' not in df.columns:
            continue

        # Compute instantaneous throughput (cells per second)
        throughput = df['cell_count'] / (df['iteration_time_ms'] / 1000)

        # Smooth with rolling average
        window = max(1, len(df) // 50)
        throughput_smooth = throughput.rolling(window=window, min_periods=1).mean()

        ax.semilogy(df['iteration'], throughput_smooth,
                    label=mode, color=colors[mode], linewidth=2, alpha=0.8)

    ax.set_xlabel('Iteration', fontsize=10)
    ax.set_ylabel('Throughput (cells/s, log)', fontsize=10)
    ax.set_title('Cell Processing Throughput Over Time', fontsize=12, fontweight='bold')
    ax.legend(fontsize=9)
    ax.grid(True, alpha=0.3, which='both')


def create_summary_table(ax, metrics: dict) -> None:
    """Create summary statistics table.

    Args:
        ax: Matplotlib axis
        metrics: Computed metrics dictionary
    """
    ax.axis('off')

    # Build summary text
    text = "═" * 45 + "\n"
    text += "        EXECUTIVE SUMMARY\n"
    text += "═" * 45 + "\n\n"

    # Determine winner in each category
    if 'Static' in metrics and 'Adaptive' in metrics:
        static = metrics['Static']
        adaptive = metrics['Adaptive']

        # Speed comparison
        if 'avg_iter_time_ms' in static and 'avg_iter_time_ms' in adaptive:
            speedup = static['avg_iter_time_ms'] / adaptive['avg_iter_time_ms']
            winner = 'Adaptive' if speedup > 1 else 'Static'
            text += f"⏱ SPEED\n"
            text += f"  Static:   {static['avg_iter_time_ms']:.2f} ms/iter\n"
            text += f"  Adaptive: {adaptive['avg_iter_time_ms']:.2f} ms/iter\n"
            text += f"  Winner:   {winner} ({speedup:.2f}x)\n\n"

        # Total runtime
        if 'total_time_s' in static and 'total_time_s' in adaptive:
            text += f"⏲ TOTAL RUNTIME\n"
            text += f"  Static:   {static['total_time_s']:.1f}s\n"
            text += f"  Adaptive: {adaptive['total_time_s']:.1f}s\n"
            saved = static['total_time_s'] - adaptive['total_time_s']
            text += f"  Saved:    {saved:.1f}s ({saved/static['total_time_s']*100:.1f}%)\n\n"

        # Cell count
        if 'final_cells' in static and 'final_cells' in adaptive:
            text += f"🔬 FINAL CELL COUNT\n"
            text += f"  Static:   {int(static['final_cells']):,}\n"
            text += f"  Adaptive: {int(adaptive['final_cells']):,}\n\n"

        # Biological accuracy
        if 'vol_error_mean' in static and 'vol_error_mean' in adaptive:
            text += f"📊 VOLUME ACCURACY\n"
            text += f"  Static:   {static['vol_error_mean']:.2f}% error\n"
            text += f"  Adaptive: {adaptive['vol_error_mean']:.2f}% error\n"
            winner = 'Adaptive' if adaptive['vol_error_mean'] < static['vol_error_mean'] else 'Static'
            text += f"  Winner:   {winner}\n\n"

        # Energy balance
        if 'ke_pe_ratio' in static and 'ke_pe_ratio' in adaptive:
            text += f"⚡ ENERGY BALANCE (KE/PE)\n"
            text += f"  Static:   {static['ke_pe_ratio']:.4f}%\n"
            text += f"  Adaptive: {adaptive['ke_pe_ratio']:.4f}%\n"
            winner = 'Adaptive' if adaptive['ke_pe_ratio'] < static['ke_pe_ratio'] else 'Static'
            text += f"  Winner:   {winner} (lower = more stable)\n"

    else:
        text += "Insufficient data for comparison.\n"
        text += "Need both Static and Adaptive results."

    text += "\n" + "═" * 45

    ax.text(0.05, 0.95, text, transform=ax.transAxes,
            fontsize=9, verticalalignment='top', family='monospace',
            bbox=dict(boxstyle='round', facecolor='#f8f9fa', edgecolor='#dee2e6', alpha=0.95))


def main(bench_dir: str) -> None:
    """Generate executive dashboard comparison.

    Args:
        bench_dir: Path to benchmark directory
    """
    bench_path = Path(bench_dir)

    # Load data
    perf_data = load_performance_data(bench_path)
    sim_data = load_simulation_stats(bench_path)

    if not perf_data:
        print(f"Error: No performance data found in {bench_path}")
        return

    # Compute metrics
    metrics = compute_metrics(perf_data, sim_data)

    # Color scheme
    colors = {
        'Static': '#4CAF50',    # Green
        'Adaptive': '#F44336'   # Red
    }

    # Create figure with mixed layout
    fig = plt.figure(figsize=(16, 12))

    # Layout: 2x2 grid with radar chart needing polar projection
    # [Speedup Bar] [Radar Chart]
    # [Throughput]  [Summary Table]

    ax1 = fig.add_subplot(2, 2, 1)  # Speedup bar chart
    ax2 = fig.add_subplot(2, 2, 2, projection='polar')  # Radar chart
    ax3 = fig.add_subplot(2, 2, 3)  # Throughput timeline
    ax4 = fig.add_subplot(2, 2, 4)  # Summary table

    # Create visualizations
    create_speedup_chart(ax1, metrics, colors)
    create_radar_chart(ax2, metrics, colors)
    create_throughput_timeline(ax3, perf_data, colors)
    create_summary_table(ax4, metrics)

    plt.suptitle('SimuCell3D Benchmark: Executive Dashboard\nStatic vs Adaptive Scheduling Comparison',
                 fontsize=14, fontweight='bold')
    plt.tight_layout()

    plots_dir = bench_path / 'plots'
    plots_dir.mkdir(exist_ok=True)
    output_path = plots_dir / 'comparative_summary.png'
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    print(f"Saved dashboard to: {output_path}")

    plt.close()


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python plot_comparative_summary.py /path/to/benchmark_dir")
        print("\nExample:")
        print("  python plot_comparative_summary.py ../doc/working/64k_growth_benchmark_20251130/")
        sys.exit(1)

    main(sys.argv[1])
