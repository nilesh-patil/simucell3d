#!/usr/bin/env python3
"""
Statistical Distribution Analysis: Rigorous comparison of Static vs Adaptive.

Performs statistical tests and generates distribution comparisons:
- Two-sample Kolmogorov-Smirnov test
- Effect size (Cohen's d)
- Q-Q plots for normality assessment
- Box plots with significance annotations

Usage:
    python plot_statistical_comparison.py /path/to/benchmark_directory

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
from scipy import stats
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


def cohens_d(group1: np.ndarray, group2: np.ndarray) -> float:
    """Calculate Cohen's d effect size.

    Args:
        group1: First sample array
        group2: Second sample array

    Returns:
        Cohen's d value
    """
    n1, n2 = len(group1), len(group2)
    var1, var2 = group1.var(), group2.var()

    # Pooled standard deviation
    pooled_std = np.sqrt(((n1 - 1) * var1 + (n2 - 1) * var2) / (n1 + n2 - 2))

    if pooled_std == 0:
        return 0

    return (group1.mean() - group2.mean()) / pooled_std


def interpret_cohens_d(d: float) -> str:
    """Interpret Cohen's d effect size.

    Args:
        d: Cohen's d value

    Returns:
        Interpretation string
    """
    d_abs = abs(d)
    if d_abs < 0.2:
        return "negligible"
    elif d_abs < 0.5:
        return "small"
    elif d_abs < 0.8:
        return "medium"
    else:
        return "large"


def create_iteration_time_comparison(ax, data: dict, colors: dict) -> dict:
    """Create iteration time box plot comparison.

    Args:
        ax: Matplotlib axis
        data: Combined data dictionary
        colors: Color scheme dictionary

    Returns:
        Dictionary with statistical test results
    """
    results = {}

    # Collect iteration times
    times = {}
    for mode in ['Static', 'Adaptive']:
        if 'perf' in data.get(mode, {}):
            df = data[mode]['perf']
            if 'total_iteration_ms' in df.columns:
                times[mode] = df['total_iteration_ms'].values

    if len(times) < 2:
        ax.text(0.5, 0.5, 'Insufficient data', ha='center', va='center',
                transform=ax.transAxes)
        return results

    # Box plot
    box_data = [times['Static'], times['Adaptive']]
    bp = ax.boxplot(box_data, labels=['Static', 'Adaptive'], patch_artist=True)

    # Color boxes
    bp['boxes'][0].set_facecolor(colors['Static'])
    bp['boxes'][1].set_facecolor(colors['Adaptive'])
    for box in bp['boxes']:
        box.set_alpha(0.7)

    # Statistical tests
    ks_stat, ks_pvalue = stats.ks_2samp(times['Static'], times['Adaptive'])
    d = cohens_d(times['Static'], times['Adaptive'])

    results['ks_stat'] = ks_stat
    results['ks_pvalue'] = ks_pvalue
    results['cohens_d'] = d
    results['effect_size'] = interpret_cohens_d(d)

    # Add annotation
    sig = "***" if ks_pvalue < 0.001 else "**" if ks_pvalue < 0.01 else "*" if ks_pvalue < 0.05 else "ns"
    ax.annotate(f'KS p={ks_pvalue:.2e} {sig}\nCohen\'s d={d:.2f} ({results["effect_size"]})',
                xy=(0.5, 0.95), xycoords='axes fraction',
                ha='center', va='top', fontsize=9,
                bbox=dict(boxstyle='round', facecolor='lightyellow', alpha=0.8))

    ax.set_ylabel('Iteration Time (ms)', fontsize=11)
    ax.set_title('Iteration Time Distribution', fontsize=12, fontweight='bold')
    ax.grid(True, axis='y', alpha=0.3)

    return results


def create_qq_plots(axes, data: dict, colors: dict) -> None:
    """Create Q-Q plots for normality assessment.

    Args:
        axes: Array of matplotlib axes [static_ax, adaptive_ax]
        data: Combined data dictionary
        colors: Color scheme dictionary
    """
    for i, mode in enumerate(['Static', 'Adaptive']):
        ax = axes[i]

        if 'perf' in data.get(mode, {}):
            df = data[mode]['perf']
            if 'total_iteration_ms' in df.columns:
                times = df['total_iteration_ms'].dropna().values

                # Q-Q plot
                stats.probplot(times, dist="norm", plot=ax)

                # Style
                ax.get_lines()[0].set_markerfacecolor(colors[mode])
                ax.get_lines()[0].set_markeredgecolor(colors[mode])
                ax.get_lines()[0].set_markersize(4)
                ax.get_lines()[0].set_alpha(0.5)

                # Shapiro-Wilk test (use sample for large datasets)
                sample = times[:5000] if len(times) > 5000 else times
                try:
                    sw_stat, sw_pvalue = stats.shapiro(sample)
                    normality = "Normal" if sw_pvalue > 0.05 else "Non-normal"
                    ax.annotate(f'Shapiro-Wilk p={sw_pvalue:.2e}\n{normality}',
                                xy=(0.05, 0.95), xycoords='axes fraction',
                                ha='left', va='top', fontsize=8,
                                bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))
                except Exception:
                    pass

        ax.set_title(f'{mode} Q-Q Plot', fontsize=11)
        ax.grid(True, alpha=0.3)


def create_volume_comparison(ax, data: dict, colors: dict) -> dict:
    """Create volume distribution comparison.

    Args:
        ax: Matplotlib axis
        data: Combined data dictionary
        colors: Color scheme dictionary

    Returns:
        Dictionary with statistical test results
    """
    results = {}

    volumes = {}
    for mode in ['Static', 'Adaptive']:
        if 'sim' in data.get(mode, {}):
            df = data[mode]['sim']
            if 'volume' in df.columns:
                # Get final iteration
                last_iter = df['iteration'].max()
                df_last = df[df['iteration'] == last_iter]
                volumes[mode] = df_last['volume'].values * 1e9  # Convert to nL

    if len(volumes) < 2:
        ax.text(0.5, 0.5, 'Insufficient data', ha='center', va='center',
                transform=ax.transAxes)
        return results

    # Overlapping histograms
    bins = np.linspace(
        min(volumes['Static'].min(), volumes['Adaptive'].min()),
        max(volumes['Static'].max(), volumes['Adaptive'].max()),
        50
    )

    ax.hist(volumes['Static'], bins=bins, alpha=0.6, label='Static',
            color=colors['Static'], density=True, edgecolor='white')
    ax.hist(volumes['Adaptive'], bins=bins, alpha=0.6, label='Adaptive',
            color=colors['Adaptive'], density=True, edgecolor='white')

    # KS test
    ks_stat, ks_pvalue = stats.ks_2samp(volumes['Static'], volumes['Adaptive'])
    d = cohens_d(volumes['Static'], volumes['Adaptive'])

    results['ks_stat'] = ks_stat
    results['ks_pvalue'] = ks_pvalue
    results['cohens_d'] = d

    sig = "***" if ks_pvalue < 0.001 else "**" if ks_pvalue < 0.01 else "*" if ks_pvalue < 0.05 else "ns"
    ax.annotate(f'KS p={ks_pvalue:.2e} {sig}\nd={d:.3f}',
                xy=(0.95, 0.95), xycoords='axes fraction',
                ha='right', va='top', fontsize=9,
                bbox=dict(boxstyle='round', facecolor='lightyellow', alpha=0.8))

    ax.set_xlabel('Volume (nL)', fontsize=10)
    ax.set_ylabel('Density', fontsize=10)
    ax.set_title('Final Cell Volume Distribution', fontsize=12, fontweight='bold')
    ax.legend(fontsize=9)
    ax.grid(True, alpha=0.3)

    return results


def create_pressure_comparison(ax, data: dict, colors: dict) -> dict:
    """Create pressure distribution comparison.

    Args:
        ax: Matplotlib axis
        data: Combined data dictionary
        colors: Color scheme dictionary

    Returns:
        Dictionary with statistical test results
    """
    results = {}

    pressures = {}
    for mode in ['Static', 'Adaptive']:
        if 'sim' in data.get(mode, {}):
            df = data[mode]['sim']
            if 'pressure' in df.columns:
                last_iter = df['iteration'].max()
                df_last = df[df['iteration'] == last_iter]
                pressures[mode] = df_last['pressure'].values

    if len(pressures) < 2:
        ax.text(0.5, 0.5, 'Insufficient data', ha='center', va='center',
                transform=ax.transAxes)
        return results

    # Violin plots for detailed distribution view
    parts = ax.violinplot([pressures['Static'], pressures['Adaptive']],
                          positions=[1, 2], showmeans=True, showmedians=True)

    # Color violins
    for i, pc in enumerate(parts['bodies']):
        color = colors['Static'] if i == 0 else colors['Adaptive']
        pc.set_facecolor(color)
        pc.set_alpha(0.7)

    # KS test
    ks_stat, ks_pvalue = stats.ks_2samp(pressures['Static'], pressures['Adaptive'])
    d = cohens_d(pressures['Static'], pressures['Adaptive'])

    results['ks_stat'] = ks_stat
    results['ks_pvalue'] = ks_pvalue
    results['cohens_d'] = d

    sig = "***" if ks_pvalue < 0.001 else "**" if ks_pvalue < 0.01 else "*" if ks_pvalue < 0.05 else "ns"
    ax.annotate(f'KS p={ks_pvalue:.2e} {sig}\nd={d:.3f}',
                xy=(0.95, 0.95), xycoords='axes fraction',
                ha='right', va='top', fontsize=9,
                bbox=dict(boxstyle='round', facecolor='lightyellow', alpha=0.8))

    ax.set_xticks([1, 2])
    ax.set_xticklabels(['Static', 'Adaptive'])
    ax.set_ylabel('Pressure (Pa)', fontsize=10)
    ax.set_title('Final Cell Pressure Distribution', fontsize=12, fontweight='bold')
    ax.grid(True, axis='y', alpha=0.3)

    return results


def create_energy_comparison(ax, data: dict, colors: dict) -> dict:
    """Create energy distribution comparison.

    Args:
        ax: Matplotlib axis
        data: Combined data dictionary
        colors: Color scheme dictionary

    Returns:
        Dictionary with statistical test results
    """
    results = {}

    energies = {}
    for mode in ['Static', 'Adaptive']:
        if 'sim' in data.get(mode, {}):
            df = data[mode]['sim']
            if 'total_potential_energy' in df.columns:
                last_iter = df['iteration'].max()
                df_last = df[df['iteration'] == last_iter]
                energies[mode] = df_last['total_potential_energy'].values

    if len(energies) < 2:
        ax.text(0.5, 0.5, 'Insufficient data', ha='center', va='center',
                transform=ax.transAxes)
        return results

    # Log-scale histograms for energy (often spans orders of magnitude)
    for mode in ['Static', 'Adaptive']:
        e = energies[mode]
        e_pos = e[e > 0]
        if len(e_pos) > 0:
            log_bins = np.logspace(np.log10(e_pos.min()), np.log10(e_pos.max()), 40)
            ax.hist(e_pos, bins=log_bins, alpha=0.6, label=mode,
                    color=colors[mode], edgecolor='white')

    ax.set_xscale('log')

    # KS test
    e1_pos = energies['Static'][energies['Static'] > 0]
    e2_pos = energies['Adaptive'][energies['Adaptive'] > 0]

    if len(e1_pos) > 0 and len(e2_pos) > 0:
        ks_stat, ks_pvalue = stats.ks_2samp(e1_pos, e2_pos)
        d = cohens_d(np.log(e1_pos), np.log(e2_pos))  # Use log for effect size

        results['ks_stat'] = ks_stat
        results['ks_pvalue'] = ks_pvalue
        results['cohens_d'] = d

        sig = "***" if ks_pvalue < 0.001 else "**" if ks_pvalue < 0.01 else "*" if ks_pvalue < 0.05 else "ns"
        ax.annotate(f'KS p={ks_pvalue:.2e} {sig}',
                    xy=(0.95, 0.95), xycoords='axes fraction',
                    ha='right', va='top', fontsize=9,
                    bbox=dict(boxstyle='round', facecolor='lightyellow', alpha=0.8))

    ax.set_xlabel('Total Potential Energy (J, log)', fontsize=10)
    ax.set_ylabel('Count', fontsize=10)
    ax.set_title('Final Cell Energy Distribution', fontsize=12, fontweight='bold')
    ax.legend(fontsize=9)
    ax.grid(True, alpha=0.3, which='both')

    return results


def create_summary_panel(ax, all_results: dict) -> None:
    """Create comprehensive statistical summary.

    Args:
        ax: Matplotlib axis
        all_results: Dictionary with all statistical test results
    """
    ax.axis('off')

    text = "═" * 55 + "\n"
    text += "         STATISTICAL SIGNIFICANCE SUMMARY\n"
    text += "═" * 55 + "\n\n"

    text += "Hypothesis: H₀ = Static and Adaptive produce\n"
    text += "            identical distributions\n\n"

    text += "─" * 55 + "\n"
    cohens_label = "Cohen's d"
    text += f"{'Metric':<20} {'KS p-value':<15} {cohens_label:<12} {'Sig.':<8}\n"
    text += "─" * 55 + "\n"

    metrics = {
        'Iteration Time': 'iter_time',
        'Cell Volume': 'volume',
        'Cell Pressure': 'pressure',
        'Cell Energy': 'energy'
    }

    for label, key in metrics.items():
        if key in all_results:
            r = all_results[key]
            pval = r.get('ks_pvalue', np.nan)
            d = r.get('cohens_d', np.nan)

            if pval < 0.001:
                sig = "***"
            elif pval < 0.01:
                sig = "**"
            elif pval < 0.05:
                sig = "*"
            else:
                sig = "ns"

            text += f"{label:<20} {pval:<15.2e} {d:<12.3f} {sig:<8}\n"
        else:
            text += f"{label:<20} {'N/A':<15} {'N/A':<12} {'N/A':<8}\n"

    text += "─" * 55 + "\n\n"

    text += "Significance levels:\n"
    text += "  *** p < 0.001  ** p < 0.01  * p < 0.05  ns = not significant\n\n"

    text += "Effect size interpretation (Cohen's d):\n"
    text += "  |d| < 0.2 = negligible\n"
    text += "  |d| < 0.5 = small\n"
    text += "  |d| < 0.8 = medium\n"
    text += "  |d| >= 0.8 = large\n"

    ax.text(0.05, 0.95, text, transform=ax.transAxes,
            fontsize=8, verticalalignment='top', family='monospace',
            bbox=dict(boxstyle='round', facecolor='#f8f9fa', edgecolor='#dee2e6', alpha=0.95))


def main(bench_dir: str) -> None:
    """Generate statistical comparison analysis.

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

    # Create figure: 3x2 layout plus summary
    fig = plt.figure(figsize=(16, 14))

    # Layout
    ax1 = fig.add_subplot(3, 2, 1)  # Iteration time box plot
    ax2 = fig.add_subplot(3, 2, 2)  # Volume comparison
    ax3 = fig.add_subplot(3, 2, 3)  # Q-Q Static
    ax4 = fig.add_subplot(3, 2, 4)  # Q-Q Adaptive
    ax5 = fig.add_subplot(3, 2, 5)  # Pressure violin
    ax6 = fig.add_subplot(3, 2, 6)  # Summary panel

    # Collect all results
    all_results = {}

    # Create visualizations
    all_results['iter_time'] = create_iteration_time_comparison(ax1, data, colors)
    all_results['volume'] = create_volume_comparison(ax2, data, colors)
    create_qq_plots([ax3, ax4], data, colors)
    all_results['pressure'] = create_pressure_comparison(ax5, data, colors)
    create_summary_panel(ax6, all_results)

    plt.suptitle('SimuCell3D: Statistical Comparison Analysis\n'
                 'Static vs Adaptive Scheduling - Distribution Tests',
                 fontsize=14, fontweight='bold')
    plt.tight_layout()

    plots_dir = bench_path / 'plots'
    plots_dir.mkdir(exist_ok=True)
    output_path = plots_dir / 'statistical_comparison.png'
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    print(f"Saved analysis to: {output_path}")

    plt.close()


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python plot_statistical_comparison.py /path/to/benchmark_dir")
        print("\nExample:")
        print("  python plot_statistical_comparison.py ../doc/working/64k_growth_benchmark_20251130/")
        sys.exit(1)

    main(sys.argv[1])
