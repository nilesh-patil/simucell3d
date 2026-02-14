#!/usr/bin/env python3
"""
Computational Complexity Deep-Dive: Scaling Analysis.

Analyzes how performance scales with:
- Cell count (N-scaling)
- Iteration time variance
- Workload distribution over time
- Efficiency metrics

Usage:
    python plot_scaling_analysis.py /path/to/benchmark_directory

The benchmark directory should contain:
    sim_static/performance_diagnostics.csv
    sim_adaptive/performance_diagnostics.csv
"""
import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
from pathlib import Path
from scipy import stats
from scipy.optimize import curve_fit
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
            print(f"Loaded {mode}: {len(df)} iterations, "
                  f"cells {df['cells'].min()}-{df['cells'].max()}")
        else:
            print(f"Warning: {csv_path} not found")

    return data


def power_law(x, a, b):
    """Power law function: y = a * x^b"""
    return a * np.power(x, b)


def fit_complexity(cell_counts: np.ndarray, times: np.ndarray) -> tuple:
    """Fit power law to estimate algorithmic complexity.

    Args:
        cell_counts: Array of cell counts
        times: Array of iteration times

    Returns:
        Tuple of (coefficient, exponent, r_squared)
    """
    try:
        # Remove zeros and invalid values
        mask = (cell_counts > 0) & (times > 0)
        x = cell_counts[mask]
        y = times[mask]

        if len(x) < 10:
            return None, None, 0

        # Initial guess: linear scaling
        popt, _ = curve_fit(power_law, x, y, p0=[1e-6, 1.0], maxfev=5000)

        # Calculate R-squared
        y_pred = power_law(x, *popt)
        ss_res = np.sum((y - y_pred) ** 2)
        ss_tot = np.sum((y - np.mean(y)) ** 2)
        r_squared = 1 - (ss_res / ss_tot)

        return popt[0], popt[1], r_squared
    except Exception as e:
        print(f"Fit error: {e}")
        return None, None, 0


def create_n_scaling_plot(ax, perf_data: dict, colors: dict) -> dict:
    """Create N-scaling analysis plot.

    Args:
        ax: Matplotlib axis
        perf_data: Performance diagnostics dictionary
        colors: Color scheme dictionary

    Returns:
        Dictionary with fit parameters for each mode
    """
    fit_params = {}

    for mode, df in perf_data.items():
        if 'cells' not in df.columns or 'total_iteration_ms' not in df.columns:
            continue

        # Scatter plot of raw data (subsampled for clarity)
        sample_rate = max(1, len(df) // 500)
        df_sample = df.iloc[::sample_rate]

        ax.scatter(df_sample['cells'], df_sample['total_iteration_ms'],
                   alpha=0.3, label=f'{mode} (data)', color=colors[mode], s=10)

        # Fit power law
        coef, exp, r2 = fit_complexity(
            df['cells'].values,
            df['total_iteration_ms'].values
        )

        if coef is not None:
            fit_params[mode] = {'coef': coef, 'exp': exp, 'r2': r2}

            # Plot fit line
            x_fit = np.linspace(df['cells'].min(), df['cells'].max(), 100)
            y_fit = power_law(x_fit, coef, exp)
            ax.plot(x_fit, y_fit, '--', linewidth=2, color=colors[mode],
                    label=f'{mode} fit: O(N^{exp:.2f}), R²={r2:.3f}')

    ax.set_xlabel('Cell Count (N)', fontsize=11)
    ax.set_ylabel('Iteration Time (ms)', fontsize=11)
    ax.set_title('Algorithmic Complexity: Time vs Cell Count', fontsize=12, fontweight='bold')
    ax.legend(fontsize=9)
    ax.grid(True, alpha=0.3)

    # Add complexity reference lines
    if perf_data:
        first_mode = list(perf_data.keys())[0]
        df = perf_data[first_mode]
        n_min, n_max = df['cells'].min(), df['cells'].max()

        # Reference lines for O(N), O(N log N), O(N²)
        x_ref = np.linspace(n_min, n_max, 50)
        t_base = df['total_iteration_ms'].iloc[0] / n_min  # normalize to first point

        ax.plot(x_ref, t_base * x_ref, ':', color='gray', alpha=0.5, label='O(N)')
        ax.plot(x_ref, t_base * x_ref * np.log(x_ref) / np.log(n_min),
                ':', color='orange', alpha=0.5, label='O(N log N)')

    return fit_params


def create_loglog_scaling_plot(ax, perf_data: dict, colors: dict) -> None:
    """Create log-log scaling plot for clearer complexity visualization.

    Args:
        ax: Matplotlib axis
        perf_data: Performance diagnostics dictionary
        colors: Color scheme dictionary
    """
    for mode, df in perf_data.items():
        if 'cells' not in df.columns or 'total_iteration_ms' not in df.columns:
            continue

        # Bin data by cell count for cleaner visualization
        df_valid = df[(df['cells'] > 0) & (df['total_iteration_ms'] > 0)].copy()

        if len(df_valid) < 20:
            continue

        # Create bins
        n_bins = 30
        df_valid['bin'] = pd.cut(df_valid['cells'], bins=n_bins)
        binned = df_valid.groupby('bin').agg({
            'cells': 'mean',
            'total_iteration_ms': ['mean', 'std']
        }).dropna()

        binned.columns = ['cells', 'time_mean', 'time_std']

        ax.loglog(binned['cells'], binned['time_mean'],
                  'o-', label=mode, color=colors[mode], markersize=5, linewidth=1.5)

        # Error bars
        ax.fill_between(binned['cells'],
                        binned['time_mean'] - binned['time_std'],
                        binned['time_mean'] + binned['time_std'],
                        alpha=0.2, color=colors[mode])

    ax.set_xlabel('Cell Count (N, log scale)', fontsize=11)
    ax.set_ylabel('Iteration Time (ms, log scale)', fontsize=11)
    ax.set_title('Log-Log Scaling (Slope = Complexity Exponent)', fontsize=12, fontweight='bold')
    ax.legend(fontsize=9)
    ax.grid(True, alpha=0.3, which='both')

    # Add slope reference lines
    ax.annotate('Slope 1.0 → O(N)\nSlope 2.0 → O(N²)',
                xy=(0.05, 0.85), xycoords='axes fraction',
                fontsize=9, alpha=0.7,
                bbox=dict(boxstyle='round', facecolor='lightyellow', alpha=0.7))


def create_time_variance_plot(ax, perf_data: dict, colors: dict) -> None:
    """Create iteration time variance analysis.

    Args:
        ax: Matplotlib axis
        perf_data: Performance diagnostics dictionary
        colors: Color scheme dictionary
    """
    for mode, df in perf_data.items():
        if 'total_iteration_ms' not in df.columns:
            continue

        # Rolling coefficient of variation
        window = max(10, len(df) // 100)
        rolling_mean = df['total_iteration_ms'].rolling(window=window).mean()
        rolling_std = df['total_iteration_ms'].rolling(window=window).std()
        rolling_cv = (rolling_std / rolling_mean) * 100  # percentage

        ax.plot(df['iteration'], rolling_cv,
                label=f'{mode} (CV)', color=colors[mode], linewidth=1.5, alpha=0.8)

    ax.set_xlabel('Iteration', fontsize=11)
    ax.set_ylabel('Coefficient of Variation (%)', fontsize=11)
    ax.set_title('Iteration Time Variability (Lower = More Predictable)', fontsize=12, fontweight='bold')
    ax.legend(fontsize=9)
    ax.grid(True, alpha=0.3)

    # Add reference lines
    ax.axhline(y=10, color='green', linestyle='--', alpha=0.5, label='Good (<10%)')
    ax.axhline(y=30, color='orange', linestyle='--', alpha=0.5, label='Moderate (<30%)')
    ax.axhline(y=50, color='red', linestyle='--', alpha=0.5, label='High (>50%)')


def create_workload_distribution(ax, perf_data: dict, colors: dict) -> None:
    """Create workload distribution histogram.

    Args:
        ax: Matplotlib axis
        perf_data: Performance diagnostics dictionary
        colors: Color scheme dictionary
    """
    for mode, df in perf_data.items():
        if 'total_iteration_ms' not in df.columns:
            continue

        # Normalize iteration times for comparison
        times = df['total_iteration_ms'].values
        normalized = (times - times.mean()) / times.std()

        ax.hist(normalized, bins=50, alpha=0.6, label=mode, color=colors[mode],
                density=True, edgecolor='white', linewidth=0.5)

        # Fit normal distribution
        x_range = np.linspace(normalized.min(), normalized.max(), 100)
        ax.plot(x_range, stats.norm.pdf(x_range), '--', color=colors[mode], linewidth=2)

    ax.set_xlabel('Normalized Iteration Time (z-score)', fontsize=11)
    ax.set_ylabel('Density', fontsize=11)
    ax.set_title('Workload Distribution (Closer to Normal = Better Load Balance)',
                 fontsize=12, fontweight='bold')
    ax.legend(fontsize=9)
    ax.grid(True, alpha=0.3)


def create_efficiency_over_time(ax, perf_data: dict, colors: dict) -> None:
    """Create efficiency over time plot.

    Args:
        ax: Matplotlib axis
        perf_data: Performance diagnostics dictionary
        colors: Color scheme dictionary
    """
    for mode, df in perf_data.items():
        if 'cells' not in df.columns or 'total_iteration_ms' not in df.columns:
            continue

        # Efficiency: cells processed per ms
        efficiency = df['cells'] / df['total_iteration_ms']

        # Smooth
        window = max(10, len(df) // 50)
        efficiency_smooth = efficiency.rolling(window=window, min_periods=1).mean()

        ax.plot(df['iteration'], efficiency_smooth,
                label=mode, color=colors[mode], linewidth=2, alpha=0.8)

    ax.set_xlabel('Iteration', fontsize=11)
    ax.set_ylabel('Efficiency (cells/ms)', fontsize=11)
    ax.set_title('Processing Efficiency Over Simulation', fontsize=12, fontweight='bold')
    ax.legend(fontsize=9)
    ax.grid(True, alpha=0.3)

    # Annotation
    ax.annotate('Decreasing efficiency may indicate\nincreasing contact complexity',
                xy=(0.55, 0.85), xycoords='axes fraction',
                fontsize=9, alpha=0.7,
                bbox=dict(boxstyle='round', facecolor='lightyellow', alpha=0.7))


def create_summary_panel(ax, perf_data: dict, fit_params: dict) -> None:
    """Create summary statistics panel.

    Args:
        ax: Matplotlib axis
        perf_data: Performance diagnostics dictionary
        fit_params: Fit parameters from complexity analysis
    """
    ax.axis('off')

    text = "═" * 50 + "\n"
    text += "      COMPUTATIONAL COMPLEXITY SUMMARY\n"
    text += "═" * 50 + "\n\n"

    for mode, df in perf_data.items():
        text += f"【{mode}】\n"

        if 'total_iteration_ms' in df.columns:
            times = df['total_iteration_ms']
            text += f"  Iteration time:\n"
            text += f"    Mean:   {times.mean():.2f} ms\n"
            text += f"    Median: {times.median():.2f} ms\n"
            text += f"    Std:    {times.std():.2f} ms\n"
            text += f"    CV:     {times.std()/times.mean()*100:.1f}%\n"

        if mode in fit_params:
            p = fit_params[mode]
            text += f"  Complexity fit:\n"
            text += f"    Exponent: {p['exp']:.3f}\n"
            text += f"    R²:       {p['r2']:.3f}\n"

            # Interpret complexity
            if p['exp'] < 1.1:
                complexity = "O(N) - Linear"
            elif p['exp'] < 1.5:
                complexity = "O(N log N) - Near-linear"
            elif p['exp'] < 2.1:
                complexity = "O(N²) - Quadratic"
            else:
                complexity = "O(N^k) - Super-quadratic"
            text += f"    Class:    {complexity}\n"

        if 'cells' in df.columns:
            text += f"  Cell range: {int(df['cells'].min())} → {int(df['cells'].max())}\n"

        text += "\n"

    # Comparison
    if 'Static' in perf_data and 'Adaptive' in perf_data:
        static_mean = perf_data['Static']['total_iteration_ms'].mean()
        adaptive_mean = perf_data['Adaptive']['total_iteration_ms'].mean()
        speedup = static_mean / adaptive_mean

        text += "─" * 50 + "\n"
        text += f"Overall speedup: {speedup:.2f}x\n"
        if speedup > 1:
            text += f"Adaptive is {(speedup-1)*100:.1f}% faster\n"
        else:
            text += f"Static is {(1/speedup-1)*100:.1f}% faster\n"

    ax.text(0.05, 0.95, text, transform=ax.transAxes,
            fontsize=8, verticalalignment='top', family='monospace',
            bbox=dict(boxstyle='round', facecolor='#f8f9fa', edgecolor='#dee2e6', alpha=0.95))


def main(bench_dir: str) -> None:
    """Generate computational complexity analysis plots.

    Args:
        bench_dir: Path to benchmark directory
    """
    bench_path = Path(bench_dir)

    # Load data
    perf_data = load_performance_data(bench_path)

    if not perf_data:
        print(f"Error: No performance data found in {bench_path}")
        return

    # Color scheme
    colors = {
        'Static': '#4CAF50',    # Green
        'Adaptive': '#F44336'   # Red
    }

    # Create figure: 3x2 layout
    fig, axes = plt.subplots(3, 2, figsize=(16, 14))

    # Create visualizations
    fit_params = create_n_scaling_plot(axes[0, 0], perf_data, colors)
    create_loglog_scaling_plot(axes[0, 1], perf_data, colors)
    create_time_variance_plot(axes[1, 0], perf_data, colors)
    create_workload_distribution(axes[1, 1], perf_data, colors)
    create_efficiency_over_time(axes[2, 0], perf_data, colors)
    create_summary_panel(axes[2, 1], perf_data, fit_params)

    plt.suptitle('SimuCell3D: Computational Complexity & Scaling Analysis\n'
                 'Static vs Adaptive Scheduling',
                 fontsize=14, fontweight='bold')
    plt.tight_layout()

    plots_dir = bench_path / 'plots'
    plots_dir.mkdir(exist_ok=True)
    output_path = plots_dir / 'scaling_analysis.png'
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    print(f"Saved analysis to: {output_path}")

    plt.close()


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python plot_scaling_analysis.py /path/to/benchmark_dir")
        print("\nExample:")
        print("  python plot_scaling_analysis.py ../doc/working/64k_growth_benchmark_20251130/")
        sys.exit(1)

    main(sys.argv[1])
