#!/usr/bin/env python3
"""
Time-Resolved Dynamics Analysis.

Analyzes temporal evolution of simulation with:
- Phase detection and segmentation
- Cell division event detection
- Autocorrelation analysis
- Moving average comparisons

Usage:
    python plot_temporal_analysis.py /path/to/benchmark_directory

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
from scipy import signal
import sys


def load_all_data(bench_path: Path) -> dict:
    """Load both performance and simulation data for each mode.

    Args:
        bench_path: Path to benchmark directory

    Returns:
        Dictionary with performance and simulation data for each mode
    """
    data = {}
    modes = ['Static', 'Adaptive']
    paths = {
        'Static': {
            'perf': 'sim_static/performance_diagnostics.csv',
            'sim': 'sim_static/simulation_statistics.csv'
        },
        'Adaptive': {
            'perf': 'sim_adaptive/performance_diagnostics.csv',
            'sim': 'sim_adaptive/simulation_statistics.csv'
        }
    }

    for mode in modes:
        data[mode] = {}
        for dtype, rel_path in paths[mode].items():
            csv_path = bench_path / rel_path
            if csv_path.exists():
                data[mode][dtype] = pd.read_csv(csv_path)
                print(f"Loaded {mode} {dtype}: {len(data[mode][dtype])} rows")

    return data


def detect_division_events(df: pd.DataFrame) -> np.ndarray:
    """Detect cell division events from cell count jumps.

    Args:
        df: Performance diagnostics dataframe

    Returns:
        Array of iteration indices where divisions occurred
    """
    if 'cells' not in df.columns:
        return np.array([])

    cell_counts = df['cells'].values
    diff = np.diff(cell_counts)

    # Divisions are where cell count increases
    division_iters = df['iteration'].values[1:][diff > 0]

    return division_iters


def compute_autocorrelation(series: np.ndarray, max_lag: int = 100) -> np.ndarray:
    """Compute autocorrelation function.

    Args:
        series: Time series data
        max_lag: Maximum lag to compute

    Returns:
        Autocorrelation values for lags 0 to max_lag
    """
    n = len(series)
    max_lag = min(max_lag, n // 4)

    # Normalize
    series_centered = series - np.mean(series)
    var = np.var(series)

    if var == 0:
        return np.ones(max_lag + 1)

    acf = np.zeros(max_lag + 1)
    for lag in range(max_lag + 1):
        if n - lag > 0:
            acf[lag] = np.mean(series_centered[:n - lag] * series_centered[lag:]) / var

    return acf


def create_cell_growth_plot(ax, data: dict, colors: dict) -> None:
    """Create cell growth over time plot with division detection.

    Args:
        ax: Matplotlib axis
        data: Combined data dictionary
        colors: Color scheme dictionary
    """
    for mode in ['Static', 'Adaptive']:
        if 'perf' not in data.get(mode, {}):
            continue

        df = data[mode]['perf']
        if 'cells' not in df.columns:
            continue

        ax.plot(df['iteration'], df['cells'],
                label=mode, color=colors[mode], linewidth=2, alpha=0.8)

        # Mark division events
        div_events = detect_division_events(df)
        if len(div_events) > 0:
            # Sample for visibility
            sample_rate = max(1, len(div_events) // 20)
            div_sample = div_events[::sample_rate]
            for div in div_sample[:10]:  # Limit markers
                ax.axvline(x=div, color=colors[mode], linestyle=':', alpha=0.3)

    ax.set_xlabel('Iteration', fontsize=10)
    ax.set_ylabel('Cell Count', fontsize=10)
    ax.set_title('Cell Growth Over Time', fontsize=12, fontweight='bold')
    ax.legend(fontsize=9)
    ax.grid(True, alpha=0.3)

    # Exponential growth reference
    ax.annotate('Exponential growth:\nN(t) = N₀ · 2^(t/τ)',
                xy=(0.02, 0.85), xycoords='axes fraction',
                fontsize=8, alpha=0.7,
                bbox=dict(boxstyle='round', facecolor='lightyellow', alpha=0.7))


def create_iteration_time_evolution(ax, data: dict, colors: dict) -> None:
    """Create iteration time evolution with moving averages.

    Args:
        ax: Matplotlib axis
        data: Combined data dictionary
        colors: Color scheme dictionary
    """
    for mode in ['Static', 'Adaptive']:
        if 'perf' not in data.get(mode, {}):
            continue

        df = data[mode]['perf']
        if 'total_iteration_ms' not in df.columns:
            continue

        # Raw data (subsampled)
        sample_rate = max(1, len(df) // 500)
        df_sample = df.iloc[::sample_rate]
        ax.scatter(df_sample['iteration'], df_sample['total_iteration_ms'],
                   alpha=0.2, color=colors[mode], s=5)

        # Moving average
        window = max(10, len(df) // 50)
        ma = df['total_iteration_ms'].rolling(window=window, min_periods=1).mean()
        ax.plot(df['iteration'], ma, color=colors[mode], linewidth=2,
                label=f'{mode} (MA-{window})')

    ax.set_xlabel('Iteration', fontsize=10)
    ax.set_ylabel('Iteration Time (ms)', fontsize=10)
    ax.set_title('Iteration Time Evolution', fontsize=12, fontweight='bold')
    ax.legend(fontsize=9)
    ax.grid(True, alpha=0.3)


def create_speedup_over_time(ax, data: dict, colors: dict) -> None:
    """Create running speedup ratio over time.

    Args:
        ax: Matplotlib axis
        data: Combined data dictionary
        colors: Color scheme dictionary
    """
    if 'Static' not in data or 'Adaptive' not in data:
        ax.text(0.5, 0.5, 'Need both Static and Adaptive data',
                ha='center', va='center', transform=ax.transAxes)
        return

    static_perf = data.get('Static', {}).get('perf')
    adaptive_perf = data.get('Adaptive', {}).get('perf')

    if static_perf is None or adaptive_perf is None:
        ax.text(0.5, 0.5, 'Performance data missing',
                ha='center', va='center', transform=ax.transAxes)
        return

    # Align by iteration
    min_len = min(len(static_perf), len(adaptive_perf))
    if min_len == 0:
        return

    static_times = static_perf['total_iteration_ms'].iloc[:min_len].values
    adaptive_times = adaptive_perf['total_iteration_ms'].iloc[:min_len].values
    iterations = static_perf['iteration'].iloc[:min_len].values

    # Compute rolling speedup
    window = max(10, min_len // 50)
    with np.errstate(divide='ignore', invalid='ignore'):
        speedup = np.where(adaptive_times > 0, static_times / adaptive_times, 1)

    # Rolling average of speedup
    speedup_ma = pd.Series(speedup).rolling(window=window, min_periods=1).mean().values

    ax.plot(iterations, speedup_ma, color='purple', linewidth=2)
    ax.axhline(y=1.0, color='gray', linestyle='--', alpha=0.7)
    ax.fill_between(iterations, 1.0, speedup_ma,
                    where=speedup_ma > 1, alpha=0.3, color=colors['Adaptive'],
                    label='Adaptive faster')
    ax.fill_between(iterations, 1.0, speedup_ma,
                    where=speedup_ma < 1, alpha=0.3, color=colors['Static'],
                    label='Static faster')

    ax.set_xlabel('Iteration', fontsize=10)
    ax.set_ylabel('Speedup (Static/Adaptive)', fontsize=10)
    ax.set_title('Running Speedup Over Simulation', fontsize=12, fontweight='bold')
    ax.legend(fontsize=9)
    ax.grid(True, alpha=0.3)

    # Add mean speedup annotation
    mean_speedup = np.nanmean(speedup)
    ax.annotate(f'Mean speedup: {mean_speedup:.2f}x',
                xy=(0.95, 0.95), xycoords='axes fraction',
                ha='right', va='top', fontsize=10, fontweight='bold',
                bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))


def create_autocorrelation_plot(ax, data: dict, colors: dict) -> None:
    """Create autocorrelation analysis plot.

    Args:
        ax: Matplotlib axis
        data: Combined data dictionary
        colors: Color scheme dictionary
    """
    max_lag = 50

    for mode in ['Static', 'Adaptive']:
        if 'perf' not in data.get(mode, {}):
            continue

        df = data[mode]['perf']
        if 'total_iteration_ms' not in df.columns:
            continue

        times = df['total_iteration_ms'].values
        acf = compute_autocorrelation(times, max_lag)
        lags = np.arange(len(acf))

        ax.plot(lags, acf, 'o-', label=mode, color=colors[mode],
                markersize=4, linewidth=1.5, alpha=0.8)

    # 95% confidence interval for white noise
    n = max(1000, min(len(df) for mode in data if 'perf' in data.get(mode, {})))
    ci = 1.96 / np.sqrt(n)
    ax.axhline(y=ci, color='gray', linestyle='--', alpha=0.5)
    ax.axhline(y=-ci, color='gray', linestyle='--', alpha=0.5)
    ax.axhline(y=0, color='black', linestyle='-', alpha=0.3)

    ax.set_xlabel('Lag (iterations)', fontsize=10)
    ax.set_ylabel('Autocorrelation', fontsize=10)
    ax.set_title('Iteration Time Autocorrelation', fontsize=12, fontweight='bold')
    ax.legend(fontsize=9)
    ax.grid(True, alpha=0.3)

    ax.annotate('High ACF → predictable timing\nLow ACF → random variation',
                xy=(0.55, 0.85), xycoords='axes fraction',
                fontsize=8, alpha=0.7,
                bbox=dict(boxstyle='round', facecolor='lightyellow', alpha=0.7))


def create_energy_evolution(ax, data: dict, colors: dict) -> None:
    """Create total system energy evolution plot.

    Args:
        ax: Matplotlib axis
        data: Combined data dictionary
        colors: Color scheme dictionary
    """
    for mode in ['Static', 'Adaptive']:
        if 'sim' not in data.get(mode, {}):
            continue

        df = data[mode]['sim']
        if 'total_potential_energy' not in df.columns:
            continue

        # Aggregate by iteration
        energy_by_iter = df.groupby('iteration')['total_potential_energy'].sum()

        ax.semilogy(energy_by_iter.index, energy_by_iter.values,
                    label=mode, color=colors[mode], linewidth=2, alpha=0.8)

    ax.set_xlabel('Iteration', fontsize=10)
    ax.set_ylabel('Total System Energy (J, log)', fontsize=10)
    ax.set_title('System Energy Evolution', fontsize=12, fontweight='bold')
    ax.legend(fontsize=9)
    ax.grid(True, alpha=0.3, which='both')


def create_phase_detection(ax, data: dict, colors: dict) -> None:
    """Create phase detection visualization.

    Args:
        ax: Matplotlib axis
        data: Combined data dictionary
        colors: Color scheme dictionary
    """
    mode = list(data.keys())[0] if data else None
    if not mode or 'perf' not in data[mode]:
        ax.text(0.5, 0.5, 'Insufficient data', ha='center', va='center',
                transform=ax.transAxes)
        return

    df = data[mode]['perf']

    if 'cells' not in df.columns or 'total_iteration_ms' not in df.columns:
        ax.text(0.5, 0.5, 'Missing required columns', ha='center', va='center',
                transform=ax.transAxes)
        return

    # Detect phases based on growth rate
    cell_counts = df['cells'].values
    iterations = df['iteration'].values

    # Compute growth rate (derivative of cell count)
    window = max(10, len(df) // 50)
    growth_rate = pd.Series(cell_counts).diff().rolling(window=window).mean().values

    # Normalize for visualization
    growth_rate_norm = growth_rate / (np.nanmax(np.abs(growth_rate)) + 1e-10)

    # Plot cell count (left y-axis)
    ax.plot(iterations, cell_counts, color='blue', linewidth=2, label='Cell Count')
    ax.set_xlabel('Iteration', fontsize=10)
    ax.set_ylabel('Cell Count', fontsize=10, color='blue')
    ax.tick_params(axis='y', labelcolor='blue')

    # Plot growth rate (right y-axis)
    ax2 = ax.twinx()
    ax2.plot(iterations, growth_rate_norm, color='red', linewidth=1.5,
             alpha=0.7, label='Growth Rate (norm)')
    ax2.set_ylabel('Growth Rate (normalized)', fontsize=10, color='red')
    ax2.tick_params(axis='y', labelcolor='red')

    # Detect phase transitions (significant changes in growth rate)
    ax.set_title(f'Growth Phases ({mode})', fontsize=12, fontweight='bold')
    ax.grid(True, alpha=0.3)

    # Combined legend
    lines1, labels1 = ax.get_legend_handles_labels()
    lines2, labels2 = ax2.get_legend_handles_labels()
    ax.legend(lines1 + lines2, labels1 + labels2, loc='upper left', fontsize=8)


def main(bench_dir: str) -> None:
    """Generate temporal dynamics analysis.

    Args:
        bench_dir: Path to benchmark directory
    """
    bench_path = Path(bench_dir)

    # Load data
    data = load_all_data(bench_path)

    if not any('perf' in d or 'sim' in d for d in data.values()):
        print(f"Error: No data found in {bench_path}")
        return

    # Color scheme
    colors = {
        'Static': '#4CAF50',    # Green
        'Adaptive': '#F44336'   # Red
    }

    # Create figure: 3x2 layout
    fig, axes = plt.subplots(3, 2, figsize=(16, 14))

    # Create visualizations
    create_cell_growth_plot(axes[0, 0], data, colors)
    create_iteration_time_evolution(axes[0, 1], data, colors)
    create_speedup_over_time(axes[1, 0], data, colors)
    create_autocorrelation_plot(axes[1, 1], data, colors)
    create_energy_evolution(axes[2, 0], data, colors)
    create_phase_detection(axes[2, 1], data, colors)

    plt.suptitle('SimuCell3D: Time-Resolved Dynamics Analysis\n'
                 'Static vs Adaptive Scheduling',
                 fontsize=14, fontweight='bold')
    plt.tight_layout()

    plots_dir = bench_path / 'plots'
    plots_dir.mkdir(exist_ok=True)
    output_path = plots_dir / 'temporal_analysis.png'
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    print(f"Saved analysis to: {output_path}")

    plt.close()


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python plot_temporal_analysis.py /path/to/benchmark_dir")
        print("\nExample:")
        print("  python plot_temporal_analysis.py ../doc/working/64k_growth_benchmark_20251130/")
        sys.exit(1)

    main(sys.argv[1])
