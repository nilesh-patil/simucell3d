#!/usr/bin/env python3
"""
SimuCell3D Unified Benchmark Visualization Suite v2.0

Publication-quality visualization for benchmark analysis with:
- Automatic scheduler detection (supports N schedulers)
- Data validation and cleaning with audit trail
- 17 publication-ready plots organized in 3 narratives
- CVD-safe colormaps and Tufte-style minimal ink design

Usage:
    python plot_benchmark_unified.py <benchmark_directory>
    python plot_benchmark_unified.py <benchmark_directory> --quality publication
    python plot_benchmark_unified.py <benchmark_directory> --validate-only
    python plot_benchmark_unified.py <benchmark_directory> --plots biological

Output:
    <benchmark_dir>/plots-unified/
    ├── 01_pressure_evolution.png
    ├── 04_scaling_analysis.png
    ├── ... (17 plots)
    ├── validation_report.json
    ├── audit_trail.json
    └── manifest.json
"""

import sys
import argparse
import json
from datetime import datetime
from pathlib import Path
from typing import Dict, Any, List, Optional, Callable, Tuple, Union
from dataclasses import dataclass, field

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.cm as cm
from matplotlib.patches import Rectangle
from matplotlib.ticker import MaxNLocator
from cycler import cycler
from scipy import stats

# Import our modules
sys.path.insert(0, str(Path(__file__).parent))
from simucell3d_viz.data.validator import validate_benchmark, ValidationReport
from simucell3d_viz.data.cleaner import load_and_clean_data, AuditTrail
from simucell3d_viz.data.scheduler_detector import (
    detect_schedulers, generate_style_config, SchedulerMetadata, get_phase_colors
)
from simucell3d_viz.data.accessor import (
    SchedulerDataAccessor, TimeSeriesData, StatsSummary
)
from simucell3d_viz.stats.confidence_intervals import (
    compute_parametric_ci, add_ci_band
)
from simucell3d_viz.stats.hypothesis_tests import (
    test_energy_conservation, bootstrap_permutation_test
)
from simucell3d_viz.stats.regression_diagnostics import (
    compute_power_law_fit, compute_cooks_distance
)
from simucell3d_viz.utils.time_utils import compute_time_seconds, find_column


# =============================================================================
# Configuration
# =============================================================================

# Journal-standard figure sizes (inches)
FIGSIZE_SINGLE = (3.5, 2.625)    # Single column (4:3 aspect)
FIGSIZE_DOUBLE = (7.0, 5.25)     # Double column (4:3 aspect)
FIGSIZE_WIDE = (7.0, 4.0)        # Wide format (16:9 aspect)
FIGSIZE_TALL = (7.0, 8.0)        # For multi-panel dashboards

# Biological reference values (from SimuCell3D paper)
PHYSIOL_PRESSURE_MIN = 300   # Pa
PHYSIOL_PRESSURE_MAX = 2200  # Pa
PHYSIOL_VOLUME_MIN = 2.5e-16  # m³
PHYSIOL_VOLUME_MAX = 1.3e-15  # m³
THEORETICAL_COMPLEXITY_EXPONENT = 4/3  # O(N^(4/3))


# =============================================================================
# Publication Styling
# =============================================================================

def configure_publication_defaults(use_latex: bool = True, quality_mode: str = 'publication') -> None:
    """Configure matplotlib rcParams for publication or draft quality output."""
    dpi = 300 if quality_mode == 'publication' else 150

    base_config = {
        'font.size': 8,
        'axes.labelsize': 9,
        'axes.titlesize': 10,
        'xtick.labelsize': 7,
        'ytick.labelsize': 7,
        'legend.fontsize': 7,
        'lines.linewidth': 1.0,
        'lines.markersize': 4,
        'axes.linewidth': 0.6,
        'grid.linewidth': 0.4,
        'xtick.major.width': 0.6,
        'ytick.major.width': 0.6,
        'figure.dpi': dpi,
        'savefig.dpi': dpi,
        'savefig.bbox': 'tight',
        'savefig.pad_inches': 0.05,
        'legend.frameon': False,
        'axes.prop_cycle': cycler(color=[
            cm.viridis(0.2), cm.viridis(0.5), cm.viridis(0.8),
            cm.plasma(0.3), cm.plasma(0.6), cm.plasma(0.9)
        ]),
        'axes.grid': False,
    }

    if use_latex:
        try:
            latex_config = {
                'text.usetex': True,
                'text.latex.preamble': r'\usepackage{amsmath}\usepackage{amssymb}',
                'font.family': 'serif',
                'font.serif': ['Computer Modern Roman'],
            }
            base_config.update(latex_config)
        except Exception:
            pass  # LaTeX not available

    plt.rcParams.update(base_config)


def apply_tufte_style(ax: plt.Axes, grid: bool = False, integer_time_axis: bool = False) -> None:
    """Apply Tufte's minimal ink principles to an axes.

    Args:
        ax: Matplotlib axes to style
        grid: Whether to show grid lines
        integer_time_axis: If True, force x-axis to use integer tick marks (for time in seconds)
    """
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    ax.spines['left'].set_linewidth(0.5)
    ax.spines['bottom'].set_linewidth(0.5)

    if grid:
        ax.grid(True, alpha=0.15, linewidth=0.5, linestyle='-')
    else:
        ax.grid(False)

    ax.tick_params(direction='out', length=3, width=0.5)

    if integer_time_axis:
        # Force integer ticks on x-axis for cleaner time display
        ax.xaxis.set_major_locator(MaxNLocator(integer=True, nbins='auto'))


def format_si_label(quantity: str, unit: str, use_latex: bool = True) -> str:
    """Generate properly formatted axis label with SI units."""
    if use_latex:
        return rf'{quantity} ($\mathrm{{{unit}}}$)'
    return f'{quantity} ({unit})'


def save_figure(fig: plt.Figure, filepath: Path, formats: List[str] = None) -> None:
    """Save figure in multiple publication formats."""
    if formats is None:
        formats = ['png']

    filepath = Path(filepath)
    filepath.parent.mkdir(parents=True, exist_ok=True)

    for fmt in formats:
        output_path = filepath.with_suffix(f'.{fmt}')
        fig.savefig(output_path, format=fmt, bbox_inches='tight')
        print(f"    Saved: {output_path.name}")

    plt.close(fig)


# =============================================================================
# Helper Functions
# =============================================================================

# compute_time_seconds and find_column moved to simucell3d_viz/utils/time_utils.py
# to avoid circular imports with accessor.py


def compute_dynamic_limits(values: np.ndarray, padding_pct: float = 0.1) -> Tuple[float, float]:
    """Compute axis limits with padding, filtering invalid values."""
    valid = values[np.isfinite(values)]
    if len(valid) == 0:
        return (0, 1)
    vmin, vmax = np.min(valid), np.max(valid)
    padding = (vmax - vmin) * padding_pct
    if padding == 0:
        padding = abs(vmin) * 0.1 if vmin != 0 else 1.0
    return (vmin - padding, vmax + padding)


def filter_outliers_iqr(values: np.ndarray, multiplier: float = 3.0) -> np.ndarray:
    """Create a mask for values within IQR-based bounds."""
    valid = values[np.isfinite(values)]
    if len(valid) < 4:
        return np.ones(len(values), dtype=bool)
    q1, q3 = np.percentile(valid, [25, 75])
    iqr = q3 - q1
    lower = q1 - multiplier * iqr
    upper = q3 + multiplier * iqr
    return (values >= lower) & (values <= upper) & np.isfinite(values)


def add_error_band(ax: plt.Axes, x: np.ndarray, y_mean: np.ndarray,
                   y_std: np.ndarray, color: Any, alpha: float = 0.25) -> None:
    """Add ±1σ error band to time series plot."""
    ax.fill_between(x, y_mean - y_std, y_mean + y_std,
                    color=color, alpha=alpha, linewidth=0)


# =============================================================================
# Plot Registry
# =============================================================================

@dataclass
class PlotInfo:
    """Metadata for a plot function."""
    id: str
    name: str
    narrative: str  # 'biological', 'physical', 'computational'
    function: Callable
    requires: List[str]
    requires_sim_stats: bool = False
    description: str = ""


# Will be populated with plot functions
PLOT_REGISTRY: Dict[str, PlotInfo] = {}


def register_plot(id: str, name: str, narrative: str, requires: List[str],
                  requires_sim_stats: bool = False, description: str = ""):
    """Decorator to register a plot function."""
    def decorator(func):
        PLOT_REGISTRY[id] = PlotInfo(
            id=id,
            name=name,
            narrative=narrative,
            function=func,
            requires=requires,
            requires_sim_stats=requires_sim_stats,
            description=description,
        )
        return func
    return decorator


# =============================================================================
# Biological Narrative Plots
# =============================================================================

@register_plot(
    id='01',
    name='pressure_evolution',
    narrative='biological',
    requires=['biological'],
    description='Pressure homeostasis with 95% confidence intervals and physiological range'
)
def plot_pressure_evolution(data: Dict, style: Dict, output_dir: Path,
                            use_latex: bool = True) -> bool:
    """
    Plot pressure evolution with 95% parametric confidence intervals.

    Uses t-distribution based CIs which are more rigorous than ±1σ bands:
    - Accounts for finite sample size (narrower with more cells)
    - Provides true 95% confidence level vs ~68% from ±1σ
    - Appropriate for unknown population variance
    """
    print("  Plotting: 01_pressure_evolution")

    fig, ax = plt.subplots(figsize=FIGSIZE_DOUBLE)

    has_data = False
    all_pressures = []
    all_times = []

    for sched in data.get('schedulers', []):
        # Use SchedulerDataAccessor for type-safe data access
        accessor = SchedulerDataAccessor(data, sched.name, 'biological')

        ts = accessor.get_time_series(
            value_cols=['mean_pressure', 'avg_pressure', 'pressure'],
            std_cols=['std_pressure', 'pressure_std'],
            min_points=2,
            filter_zeros=True
        )

        if ts is None:
            print(f"    Warning: No valid pressure data for {sched.name}")
            continue

        color = style['colors'].get(sched.name, cm.viridis(0.5))
        label = style['labels'].get(sched.name, sched.name)

        # Plot mean line
        ax.plot(ts.time, ts.values, color=color, label=label, linewidth=1.2)

        # Add 95% confidence interval if std available
        if ts.std is not None and np.any(ts.std > 0):
            # Get number of cells at each timepoint for proper CI calculation
            bio_df = accessor.get_raw_dataframe()
            cells_col = accessor.get_column(['cells', 'cell_count', 'num_cells'])

            if cells_col and bio_df is not None:
                # Use actual cell count for CI (median value if it varies)
                n_cells = int(np.median(bio_df[cells_col].dropna()))
                if n_cells >= 2:
                    # Compute parametric 95% CI using t-distribution
                    ci_lower, ci_upper = compute_parametric_ci(
                        ts.values, ts.std, n_samples=n_cells, confidence=0.95
                    )
                    add_ci_band(ax, ts.time, ci_lower, ci_upper, color,
                               alpha=0.2, label=f'95% CI (N={n_cells})')
                else:
                    # Fallback to simple ±1σ if cell count unavailable
                    add_error_band(ax, ts.time, ts.values, ts.std, color, alpha=0.2)
            else:
                # Fallback to simple ±1σ if cell count unavailable
                add_error_band(ax, ts.time, ts.values, ts.std, color, alpha=0.2)

        all_pressures.extend(ts.values)
        all_times.extend(ts.time)
        has_data = True

    if not has_data:
        print("    Skipped: No valid pressure data available")
        plt.close(fig)
        return False

    # Dynamic axis limits based on actual data
    p_min, p_max = compute_dynamic_limits(np.array(all_pressures))
    t_max = max(all_times) if all_times else 1.0

    # Only show physiological range if data is in that range
    if p_max > PHYSIOL_PRESSURE_MIN * 0.5:
        ax.axhspan(PHYSIOL_PRESSURE_MIN, PHYSIOL_PRESSURE_MAX,
                   alpha=0.1, color='gray', label='Physiological range')
        ax.axhline(PHYSIOL_PRESSURE_MIN, color='gray', linestyle='--', linewidth=0.5)
        ax.axhline(PHYSIOL_PRESSURE_MAX, color='gray', linestyle='--', linewidth=0.5)

    ax.set_xlabel('Seconds Since Process Start')
    ax.set_ylabel(format_si_label('Mean Cell Pressure', 'Pa', use_latex))
    ax.legend(loc='best', frameon=False)
    ax.set_xlim(left=0, right=t_max * 1.02)
    ax.set_ylim(bottom=max(0, p_min), top=p_max)
    apply_tufte_style(ax, grid=True, integer_time_axis=True)

    save_figure(fig, output_dir / '01_pressure_evolution')
    return True


@register_plot(
    id='02',
    name='energy_landscape',
    narrative='biological',
    requires=['biological'],
    description='Total energy with statistical conservation testing'
)
def plot_energy_landscape(data: Dict, style: Dict, output_dir: Path,
                          use_latex: bool = True) -> bool:
    """
    Plot energy evolution with comprehensive conservation testing.

    Statistical Tests Applied:
    1. Linear regression: Tests if energy drifts linearly (H0: slope = 0)
    2. KPSS stationarity: Tests if series is stationary (H0: stationary)
    3. Relative drift rate: Quantifies (dE/E)/s for practical significance

    Energy is considered conserved if ALL three tests pass:
    - Regression p-value > 0.05 (no significant trend)
    - KPSS p-value > 0.05 (series is stationary)
    - Relative drift < 1e-6 /s (< 0.0001%/s)
    """
    print("  Plotting: 02_energy_landscape")

    fig, axes = plt.subplots(2, 1, figsize=FIGSIZE_DOUBLE,
                             gridspec_kw={'height_ratios': [2, 1]}, sharex=True)
    ax1, ax2 = axes

    has_data = False
    all_energies = []
    all_times = []
    all_rates = []
    conservation_tests = {}  # Store test results per scheduler

    for sched in data.get('schedulers', []):
        accessor = SchedulerDataAccessor(data, sched.name, 'biological')

        if not accessor.is_available():
            continue

        df = accessor.get_raw_dataframe()
        t = compute_time_seconds(df)

        # Get energy data (actual or estimated)
        # Note: Energy requires custom logic for KE+PE vs estimated, so we handle it manually
        E_total = None
        using_estimates = False

        # Try actual energy columns
        ke_col = accessor.get_column(['total_kinetic_energy', 'kinetic_energy'])
        pe_col = accessor.get_column(['total_potential_energy', 'potential_energy'])

        if ke_col and pe_col:
            E_kinetic = df[ke_col].values.astype(float)
            E_potential = df[pe_col].values.astype(float)
            E_total = E_kinetic + E_potential
            # Check if data is meaningful (not all zeros)
            if np.nansum(np.abs(E_total)) < 1e-20:
                E_total = None

        # Fall back to estimated energy
        if E_total is None:
            est_col = accessor.get_column(['estimated_total_energy', 'pv_energy', 'estimated_pv_energy'])
            if est_col:
                E_total = df[est_col].values.astype(float)
                using_estimates = True

        if E_total is None:
            print(f"    Warning: No energy data for {sched.name}")
            continue

        # Filter valid data
        valid_mask = np.isfinite(E_total)
        if np.sum(valid_mask) < 2:
            continue

        t_valid = t[valid_mask]
        E_valid = E_total[valid_mask]

        color = style['colors'].get(sched.name, cm.viridis(0.5))
        label = style['labels'].get(sched.name, sched.name)
        if using_estimates:
            label += ' (PV proxy)'

        ax1.plot(t_valid, E_valid, color=color, label=label, linewidth=1.2,
                 linestyle='--' if using_estimates else '-')

        all_energies.extend(E_valid)
        all_times.extend(t_valid)
        has_data = True

        # Statistical energy conservation test
        if len(t_valid) >= 10 and not using_estimates:
            # Only test actual energy (not PV proxy estimates)
            try:
                test_result = test_energy_conservation(t_valid, E_valid, significance=0.05)
                conservation_tests[sched.name] = test_result

                # Print results to console
                print(f"    {sched.name} Energy Conservation Test:")
                print(f"      Drift Rate: {test_result.slope:.2e} J/s (p={test_result.slope_pvalue:.4f})")
                print(f"      KPSS Stationary: p={test_result.kpss_pvalue:.4f}")
                print(f"      Relative Drift: {abs(test_result.rel_drift_rate):.2e}/s")
                print(f"      Verdict: {'✓ CONSERVED' if test_result.is_conserved else '⚠ NOT CONSERVED'}")
            except Exception as e:
                print(f"    Warning: Energy conservation test failed for {sched.name}: {e}")

        # Energy rate (derivative)
        if len(t_valid) > 2:
            dt = np.diff(t_valid)
            dt[dt == 0] = 1e-6  # Avoid division by zero
            dE = np.diff(E_valid)
            drift_rate = dE / dt
            # Filter extreme outliers in rate
            rate_mask = filter_outliers_iqr(drift_rate, multiplier=5.0)
            ax2.plot(t_valid[1:][rate_mask], drift_rate[rate_mask],
                     color=color, linewidth=0.8, alpha=0.7)
            all_rates.extend(drift_rate[rate_mask])

    if not has_data:
        print("    Skipped: No energy data available")
        plt.close(fig)
        return False

    # Dynamic axis limits
    if all_energies:
        e_min, e_max = compute_dynamic_limits(np.array(all_energies))
        ax1.set_ylim(e_min, e_max)

    if all_times:
        t_max = max(all_times)
        ax1.set_xlim(left=0, right=t_max * 1.02)

    ax1.set_ylabel(format_si_label('Total Energy', 'J', use_latex))
    ax1.legend(loc='best', frameon=False)
    apply_tufte_style(ax1, grid=True, integer_time_axis=True)

    ax2.set_xlabel('Seconds Since Process Start')
    ax2.set_ylabel(format_si_label('Energy Drift Rate', 'J/s', use_latex))
    ax2.axhline(0, color='gray', linestyle='--', linewidth=0.5)

    if all_rates:
        r_min, r_max = compute_dynamic_limits(np.array(all_rates))
        # Ensure zero is visible
        ax2.set_ylim(min(r_min, -abs(r_max)*0.1), max(r_max, abs(r_min)*0.1))

    apply_tufte_style(ax2, grid=True, integer_time_axis=True)

    # Add conservation test annotations to top panel
    if conservation_tests:
        annotation_lines = []
        for sched_name, test in conservation_tests.items():
            # Use checkmark or warning symbol
            symbol = '✓' if test.is_conserved else '⚠'
            # Format drift rate in scientific notation
            drift_str = f"{abs(test.rel_drift_rate):.1e}/s"
            # Create annotation line
            annotation_lines.append(
                f"{symbol} {sched_name}: drift={drift_str}, "
                f"p_slope={test.slope_pvalue:.3f}, p_KPSS={test.kpss_pvalue:.3f}"
            )

        # Add text box to upper panel
        annotation_text = "\n".join(annotation_lines)
        ax1.text(
            0.02, 0.98,
            "Energy Conservation Tests:\n" + annotation_text,
            transform=ax1.transAxes,
            verticalalignment='top',
            horizontalalignment='left',
            fontsize=8,
            bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.3),
            family='monospace'
        )

    plt.tight_layout()
    save_figure(fig, output_dir / '02_energy_landscape')
    return True


# =============================================================================
# Computational Narrative Plots
# =============================================================================

@register_plot(
    id='04',
    name='scaling_analysis',
    narrative='computational',
    requires=['computational'],
    description='O(N^4/3) scaling with full regression diagnostics'
)
def plot_scaling_analysis(data: Dict, style: Dict, output_dir: Path,
                          use_latex: bool = True) -> bool:
    """
    Plot computational complexity scaling with comprehensive diagnostic suite.

    2×2 Panel Layout:
    - Top-left: Scaling plot with 95% prediction intervals
    - Top-right: Q-Q plot for residual normality check
    - Bottom-left: Residual plot (standardized residuals vs cell count)
    - Bottom-right: Cook's distance for outlier detection

    Diagnostics validate O(N^4/3) scaling claim for publication.
    """
    print("  Plotting: 04_scaling_analysis (with diagnostics)")

    # Create 2×2 panel layout
    fig, axes = plt.subplots(2, 2, figsize=(14, 12))
    ax_main = axes[0, 0]      # Scaling plot
    ax_qq = axes[0, 1]        # Q-Q plot
    ax_resid = axes[1, 0]     # Residual plot
    ax_cooks = axes[1, 1]     # Cook's distance

    has_data = False
    all_cells = []
    all_times = []

    for sched in data.get('schedulers', []):
        accessor = SchedulerDataAccessor(data, sched.name, 'computational')

        # Get cells and time_ms as correlated columns
        result = accessor.get_two_column_series(
            col1_candidates=['cells', 'cell_count', 'num_cells'],
            col2_candidates=['total_time_ms', 'time_ms', 'iteration_time_ms'],
            min_points=3,
            filter_zeros=True
        )

        if result is None:
            # Try IPS fallback
            df = accessor.get_raw_dataframe()
            if df is None:
                continue

            cells_col = accessor.get_column(['cells', 'cell_count', 'num_cells'])
            ips_col = accessor.get_column(['iter_per_sec', 'iterations_per_sec', 'ips'])

            if cells_col and ips_col:
                cells = df[cells_col].values.astype(float)
                ips = df[ips_col].values.astype(float)
                ips[ips == 0] = np.nan
                time_per_iter = 1000.0 / ips  # Convert to ms

                valid = (cells > 0) & np.isfinite(time_per_iter) & np.isfinite(cells)
                if np.sum(valid) < 3:
                    continue

                t_unused = compute_time_seconds(df)  # Not used but matches signature
                cells_valid = cells[valid]
                time_valid = time_per_iter[valid]
            else:
                print(f"    Warning: No time/IPS column for {sched.name}")
                continue
        else:
            t_unused, cells_valid, time_valid = result

        # Additional filter: remove plateau artifacts (constant time values)
        if len(time_valid) > 10:
            unique_times, counts = np.unique(np.round(time_valid, 1), return_counts=True)
            plateau_threshold = 0.3 * len(time_valid)
            plateau_values = unique_times[counts > plateau_threshold]
            if len(plateau_values) > 0:
                plateau_mask = ~np.isin(np.round(time_valid, 1), plateau_values)
                cells_valid = cells_valid[plateau_mask]
                time_valid = time_valid[plateau_mask]
                if np.sum(~plateau_mask) > 0:
                    print(f"    Info: Filtered {np.sum(~plateau_mask)} plateau artifacts for {sched.name}")

        # Apply IQR outlier filtering
        outlier_mask = filter_outliers_iqr(time_valid, multiplier=3.0)
        cells_valid = cells_valid[outlier_mask]
        time_valid = time_valid[outlier_mask]

        if len(cells_valid) < 3:
            print(f"    Warning: Insufficient data for {sched.name} ({len(cells_valid)} points)")
            continue

        has_data = True
        color = style['colors'].get(sched.name, cm.viridis(0.5))
        label = style['labels'].get(sched.name, sched.name)

        # Add scatter to main panel
        ax_main.scatter(cells_valid, time_valid, color=color, label=label,
                        alpha=0.6, s=15, edgecolors='none')

        all_cells.extend(cells_valid)
        all_times.extend(time_valid)

    if not has_data:
        print("    Skipped: No scaling data available")
        plt.close(fig)
        return False

    # Fit power law and generate comprehensive diagnostics
    all_cells = np.array(all_cells)
    all_times = np.array(all_times)

    if len(all_cells) > 5:
        try:
            # Compute power-law fit with full diagnostics
            fit = compute_power_law_fit(all_cells, all_times, confidence=0.95)

            print(f"    Power-Law Fit Results:")
            print(f"      Exponent: {fit.exponent:.4f} (theoretical 4/3 = {4/3:.4f})")
            print(f"      R²: {fit.r_squared:.4f}")
            print(f"      RMSE: {fit.rmse:.4f}")
            print(f"      MAPE: {fit.mape:.2f}%")

            # Panel 1: Main scaling plot with prediction intervals
            x_theory = np.logspace(np.log10(np.min(all_cells)), np.log10(np.max(all_cells)), 100)
            y_fitted = fit.coefficient * (x_theory ** fit.exponent)

            ax_main.plot(x_theory, y_fitted, 'k--', linewidth=1.5, alpha=0.8,
                        label=f'Fit: O(N^{{{fit.exponent:.2f}}}) R²={fit.r_squared:.3f}')

            # Add 95% prediction intervals
            # Interpolate prediction intervals to match x_theory
            from scipy.interpolate import interp1d
            interp_lower = interp1d(fit.x_data, fit.pred_lower, kind='linear',
                                   bounds_error=False, fill_value='extrapolate')
            interp_upper = interp1d(fit.x_data, fit.pred_upper, kind='linear',
                                   bounds_error=False, fill_value='extrapolate')

            ax_main.fill_between(x_theory, interp_lower(x_theory), interp_upper(x_theory),
                                alpha=0.2, color='gray', label='95% Prediction Interval')

            # Theoretical O(N^4/3) line
            scale_theory = np.median(all_times) / (np.median(all_cells) ** THEORETICAL_COMPLEXITY_EXPONENT)
            y_ideal = scale_theory * (x_theory ** THEORETICAL_COMPLEXITY_EXPONENT)
            ax_main.plot(x_theory, y_ideal, 'gray', linestyle=':', linewidth=1.0, alpha=0.6,
                        label=f'Theory: O(N^{{4/3}})')

            ax_main.set_xscale('log')
            ax_main.set_yscale('log')
            ax_main.set_xlabel(format_si_label('Cell Count', 'N', use_latex))
            ax_main.set_ylabel(format_si_label('Time per Iteration', 'ms', use_latex))
            ax_main.legend(loc='upper left', frameon=False, fontsize=8)
            ax_main.set_title('(A) Scaling Analysis', loc='left', fontsize=10, fontweight='bold')
            apply_tufte_style(ax_main, grid=True)

            # Panel 2: Q-Q Plot for residual normality
            stats.probplot(fit.residuals, dist="norm", plot=ax_qq)
            ax_qq.set_title('(B) Residual Normality (Q-Q Plot)', loc='left', fontsize=10, fontweight='bold')
            ax_qq.set_xlabel('Theoretical Quantiles')
            ax_qq.set_ylabel('Sample Quantiles (Residuals)')
            apply_tufte_style(ax_qq, grid=True)

            # Panel 3: Residual plot
            ax_resid.scatter(fit.x_data, fit.residuals_normalized, alpha=0.6, s=20, edgecolors='none')
            ax_resid.axhline(0, color='red', linestyle='--', linewidth=1, alpha=0.7)
            ax_resid.axhline(2, color='gray', linestyle=':', linewidth=0.5, alpha=0.5)
            ax_resid.axhline(-2, color='gray', linestyle=':', linewidth=0.5, alpha=0.5)
            ax_resid.set_xscale('log')
            ax_resid.set_xlabel(format_si_label('Cell Count', 'N', use_latex))
            ax_resid.set_ylabel('Standardized Residuals')
            ax_resid.set_title('(C) Residual Plot', loc='left', fontsize=10, fontweight='bold')
            ax_resid.text(0.98, 0.98, 'Good fit: residuals should be\nrandomly scattered around 0',
                         transform=ax_resid.transAxes, ha='right', va='top',
                         fontsize=7, bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.3))
            apply_tufte_style(ax_resid, grid=True)

            # Panel 4: Cook's distance
            cooks_d = compute_cooks_distance(fit.x_data, fit.y_data, fit.y_pred, fit.residuals)
            threshold = 4 / len(fit.x_data)  # Common threshold

            ax_cooks.stem(range(len(cooks_d)), cooks_d, linefmt='C0-', markerfmt='C0o', basefmt='gray')
            ax_cooks.axhline(threshold, color='red', linestyle='--', linewidth=1, alpha=0.7,
                           label=f'Threshold: 4/n = {threshold:.3f}')
            ax_cooks.axhline(0.5, color='orange', linestyle=':', linewidth=0.5, alpha=0.5)
            ax_cooks.set_xlabel('Observation Index')
            ax_cooks.set_ylabel("Cook's Distance")
            ax_cooks.set_title('(D) Outlier Detection (Cook\'s Distance)', loc='left', fontsize=10, fontweight='bold')
            ax_cooks.legend(loc='upper right', fontsize=7)
            ax_cooks.text(0.98, 0.5, 'D > 1.0: Very influential\nD > 0.5: Check\nD > 4/n: Potentially influential',
                         transform=ax_cooks.transAxes, ha='right', va='center',
                         fontsize=7, bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.3))
            apply_tufte_style(ax_cooks, grid=True)

            # Identify and print influential points
            influential = np.where(cooks_d > threshold)[0]
            if len(influential) > 0:
                print(f"    Warning: {len(influential)} potentially influential outliers detected")
                print(f"      Indices: {influential[:10].tolist()}{'...' if len(influential) > 10 else ''}")

        except Exception as e:
            print(f"    Warning: Power law fit/diagnostics failed: {e}")
            # Fallback to simple plot on main panel only
            ax_main.set_xscale('log')
            ax_main.set_yscale('log')
            ax_main.set_xlabel(format_si_label('Cell Count', 'N', use_latex))
            ax_main.set_ylabel(format_si_label('Time per Iteration', 'ms', use_latex))
            ax_main.legend(loc='upper left', frameon=False)
            apply_tufte_style(ax_main, grid=True)

            # Hide other panels
            for ax in [ax_qq, ax_resid, ax_cooks]:
                ax.set_visible(False)
    else:
        # Not enough data for fitting
        ax_main.set_xscale('log')
        ax_main.set_yscale('log')
        ax_main.set_xlabel(format_si_label('Cell Count', 'N', use_latex))
        ax_main.set_ylabel(format_si_label('Time per Iteration', 'ms', use_latex))
        ax_main.legend(loc='upper left', frameon=False)
        apply_tufte_style(ax_main, grid=True)

        # Hide diagnostic panels
        for ax in [ax_qq, ax_resid, ax_cooks]:
            ax.set_visible(False)

    plt.tight_layout()
    save_figure(fig, output_dir / '04_scaling_analysis')
    return True


@register_plot(
    id='05',
    name='phase_timing',
    narrative='computational',
    requires=['phase'],
    description='Phase timing breakdown stacked plot - one column per scheduler'
)
def plot_phase_timing(data: Dict, style: Dict, output_dir: Path,
                      use_latex: bool = True) -> bool:
    """Plot phase timing breakdown as stacked area chart for ALL schedulers (one column each)."""
    print("  Plotting: 05_phase_timing_breakdown")

    # Phase column candidates
    phase_column_candidates = {
        'mesh_refinement': ['mesh_refinement', 'mesh_refinement_ms', 'mesh_time_ms', 'mesh_pct'],
        'contact_detection': ['contact_detection', 'contact_detection_ms', 'contact_time_ms', 'contact_pct'],
        'polarization': ['polarization_forces', 'polarization_internal_forces_ms', 'polarization_forces_ms',
                        'force_time_ms', 'force_pct', 'polarization_ms', 'polarization'],
        'time_integration': ['time_integration', 'time_integration_ms', 'integration_time_ms', 'integration_pct']
    }

    # Find all schedulers with phase data
    schedulers_with_data = []
    scheduler_phase_data = {}

    for sched in data.get('schedulers', []):
        # Try dedicated phase data first, then computational
        accessor_phase = SchedulerDataAccessor(data, sched.name, 'phase')
        accessor_comp = SchedulerDataAccessor(data, sched.name, 'computational')

        accessor = accessor_phase if accessor_phase.is_available() else accessor_comp
        if not accessor.is_available():
            continue

        df = accessor.get_raw_dataframe()

        # Check for phase columns using accessor
        available_phases = {}
        for phase_name, candidates in phase_column_candidates.items():
            col = accessor.get_column(candidates)
            if col:
                available_phases[phase_name] = col

        if len(available_phases) >= 2:
            schedulers_with_data.append(sched.name)
            scheduler_phase_data[sched.name] = (df, available_phases)

    if not schedulers_with_data:
        print("    Skipped: No phase timing data available for any scheduler")
        return False

    # Create figure with one column per scheduler
    n_schedulers = len(schedulers_with_data)
    fig, axes = plt.subplots(1, n_schedulers, figsize=(5 * n_schedulers, 5), squeeze=False)

    phase_colors = get_phase_colors()

    for idx, sched_name in enumerate(schedulers_with_data):
        ax = axes[0, idx]
        df, available_phases = scheduler_phase_data[sched_name]

        # Get time in seconds since start
        t = compute_time_seconds(df)

        # Filter valid rows
        valid_mask = np.ones(len(df), dtype=bool)
        for phase_name, col in available_phases.items():
            values = df[col].values.astype(float)
            valid_mask &= np.isfinite(values) & (values >= 0)

        if np.sum(valid_mask) < 2:
            ax.text(0.5, 0.5, 'Insufficient data', ha='center', va='center', transform=ax.transAxes)
            ax.set_title(style['labels'].get(sched_name, sched_name), fontsize=10)
            continue

        t_valid = t[valid_mask]

        # Determine if percentages
        is_percentage = any('pct' in col for col in available_phases.values())

        # Stack the phases
        y_stack = np.zeros(np.sum(valid_mask))
        for phase_name, col in available_phases.items():
            y_values = df[col].values[valid_mask].astype(float)
            color = phase_colors.get(phase_name, cm.viridis(0.5))
            label = phase_name.replace('_', ' ').title()
            ax.fill_between(t_valid, y_stack, y_stack + y_values, color=color, label=label, alpha=0.8)
            y_stack += y_values

        ax.set_xlabel('Seconds Since Process Start')

        if is_percentage:
            ax.set_ylabel('Time (%)')
            ax.set_ylim(0, 100)
        else:
            ax.set_ylabel(format_si_label('Time', 'ms', use_latex))
            ax.set_ylim(bottom=0)

        ax.set_xlim(left=0, right=np.max(t_valid) * 1.02)
        ax.set_title(style['labels'].get(sched_name, sched_name), fontsize=10)
        apply_tufte_style(ax, grid=True, integer_time_axis=True)

        # Only show legend on first plot
        if idx == 0:
            ax.legend(loc='upper left', frameon=False, fontsize=7)

    plt.tight_layout()
    save_figure(fig, output_dir / '05_phase_timing_breakdown')
    return True


@register_plot(
    id='08',
    name='scheduler_comparison',
    narrative='computational',
    requires=['computational'],
    description='Time series: time per iteration vs seconds since simulation start'
)
def plot_scheduler_comparison(data: Dict, style: Dict, output_dir: Path,
                              use_latex: bool = True) -> bool:
    """Plot time series of time per iteration vs seconds since simulation start for all schedulers."""
    print("  Plotting: 08_scheduler_comparison")

    fig, ax = plt.subplots(figsize=FIGSIZE_DOUBLE)

    has_data = False
    for sched in data.get('schedulers', []):
        accessor = SchedulerDataAccessor(data, sched.name, 'computational')

        # Try get_time_series with time_ms column
        ts = accessor.get_time_series(
            value_cols=['total_time_ms', 'time_ms', 'iteration_time_ms'],
            min_points=5,
            filter_zeros=True
        )

        if ts is None:
            # Try IPS fallback
            df = accessor.get_raw_dataframe()
            if df is None:
                continue

            ips_col = accessor.get_column(['iter_per_sec', 'iterations_per_sec', 'ips'])
            if ips_col:
                t = compute_time_seconds(df)
                ips = df[ips_col].values.astype(float)
                ips[ips == 0] = np.nan
                time_per_iter = 1000.0 / ips  # Convert to ms

                valid = np.isfinite(time_per_iter) & (time_per_iter > 0) & np.isfinite(t)
                if np.sum(valid) < 5:
                    continue

                t_valid = t[valid]
                time_valid = time_per_iter[valid]
            else:
                continue
        else:
            t_valid = ts.time
            time_valid = ts.values

        color = style['colors'].get(sched.name, cm.viridis(0.5))
        label = style['labels'].get(sched.name, sched.name)

        ax.plot(t_valid, time_valid, color=color, label=label, linewidth=1.5, alpha=0.9)
        ax.scatter(t_valid, time_valid, color=color, s=8, alpha=0.3)
        has_data = True

    if not has_data:
        print("    Skipped: No computational data available")
        plt.close(fig)
        return False

    ax.set_xlabel('Seconds Since Process Start')
    ax.set_ylabel(format_si_label('Time per Iteration', 'ms', use_latex))
    ax.set_title('Iteration Time Over Simulation Duration', fontsize=10)
    ax.legend(loc='upper left', frameon=False)
    ax.set_xlim(left=0)
    ax.set_ylim(bottom=0)
    apply_tufte_style(ax, grid=True, integer_time_axis=True)

    save_figure(fig, output_dir / '08_scheduler_comparison')
    return True


# =============================================================================
# Additional Plots (from plots-updated)
# =============================================================================

@register_plot(
    id='02b',
    name='cell_heterogeneity',
    narrative='biological',
    requires=['simulation_stats'],
    requires_sim_stats=True,
    description='2D pressure-volume hexbin scatter showing cell state distribution'
)
def plot_cell_heterogeneity(data: Dict, style: Dict, output_dir: Path,
                            use_latex: bool = True) -> bool:
    """Plot cell population heterogeneity as 2D hexbin (pressure vs volume)."""
    print("  Plotting: 02b_cell_heterogeneity")
    from matplotlib.patches import Rectangle

    # Find schedulers with simulation statistics
    schedulers_with_data = []
    for sched in data.get('schedulers', []):
        accessor = SchedulerDataAccessor(data, sched.name, 'simulation_stats')
        df = accessor.get_raw_dataframe()
        if df is not None and len(df) > 100:
            schedulers_with_data.append(sched.name)

    if not schedulers_with_data:
        print("    Skipped: No simulation_statistics data available")
        return False

    n_schedulers = len(schedulers_with_data)
    fig, axes = plt.subplots(1, n_schedulers, figsize=(5 * n_schedulers, 4.5), squeeze=False)

    has_data = False
    for idx, sched_name in enumerate(schedulers_with_data):
        ax = axes[0, idx]
        accessor = SchedulerDataAccessor(data, sched_name, 'simulation_stats')
        df = accessor.get_raw_dataframe()

        # Find pressure and volume columns using accessor
        pressure_col = accessor.get_column(['pressure', 'cell_pressure', 'avg_pressure'])
        volume_col = accessor.get_column(['volume', 'cell_volume', 'avg_volume'])

        if pressure_col is None or volume_col is None:
            ax.text(0.5, 0.5, 'Data not available', ha='center', va='center',
                    transform=ax.transAxes, fontsize=10, color='gray')
            ax.set_title(style['labels'].get(sched_name, sched_name))
            continue

        pressure = df[pressure_col].values.astype(float)
        volume = df[volume_col].values.astype(float)

        # Filter valid values
        valid = (pressure > 0) & (volume > 0) & np.isfinite(pressure) & np.isfinite(volume)
        pressure = pressure[valid]
        volume = volume[valid]

        if len(pressure) < 100:
            ax.text(0.5, 0.5, f'Insufficient data ({len(pressure)} points)',
                    ha='center', va='center', transform=ax.transAxes, fontsize=10, color='gray')
            continue

        # 2D hexbin plot with log color scale
        # Convert volume to femtoliters for readability
        volume_fl = volume * 1e15
        hb = ax.hexbin(volume_fl, pressure, gridsize=40, cmap='viridis',
                       mincnt=1, xscale='log', yscale='linear',
                       norm=plt.matplotlib.colors.LogNorm())

        cb = plt.colorbar(hb, ax=ax, label='Cell count')
        cb.ax.tick_params(labelsize=6)

        # Overlay physiological corridor (if data is in range)
        if pressure.max() > PHYSIOL_PRESSURE_MIN * 0.5:
            physiol_rect = Rectangle(
                (PHYSIOL_VOLUME_MIN * 1e15, PHYSIOL_PRESSURE_MIN),
                (PHYSIOL_VOLUME_MAX - PHYSIOL_VOLUME_MIN) * 1e15,
                PHYSIOL_PRESSURE_MAX - PHYSIOL_PRESSURE_MIN,
                linewidth=2, edgecolor='red', facecolor='none',
                linestyle='--', label='Physiological range'
            )
            ax.add_patch(physiol_rect)
            ax.legend(loc='upper right', fontsize=6)

        ax.set_xlabel(format_si_label('Volume', 'fL', use_latex))
        ax.set_ylabel(format_si_label('Pressure', 'Pa', use_latex))
        ax.set_title(f'{style["labels"].get(sched_name, sched_name)}: Cell State Distribution')
        apply_tufte_style(ax, grid=False)
        has_data = True

    if not has_data:
        print("    Skipped: No valid data for hexbin plot")
        plt.close(fig)
        return False

    plt.tight_layout()
    save_figure(fig, output_dir / '02b_cell_heterogeneity')
    return True


@register_plot(
    id='06',
    name='roofline_model',
    narrative='computational',
    requires=['computational'],
    description='Roofline model showing memory vs compute bound regions'
)
def plot_roofline_model(data: Dict, style: Dict, output_dir: Path,
                        use_latex: bool = True) -> bool:
    """Plot roofline model for performance ceiling analysis."""
    print("  Plotting: 06_roofline_model")

    fig, ax = plt.subplots(figsize=FIGSIZE_DOUBLE)

    # System specifications (typical workstation - could be made configurable)
    PEAK_GFLOPS = 250    # 8 cores × 3.9 GHz × 8 FLOP/cycle (AVX2)
    PEAK_BW_GBS = 45     # GB/s DDR4-2933

    # Arithmetic intensity range
    AI_range = np.logspace(-2, 2, 100)

    # Roofline ceilings
    compute_ceiling = np.full_like(AI_range, PEAK_GFLOPS)
    memory_ceiling = PEAK_BW_GBS * AI_range
    roofline = np.minimum(compute_ceiling, memory_ceiling)

    # Plot roofline
    ax.plot(AI_range, roofline, 'k-', linewidth=2, label='Roofline')
    ax.fill_between(AI_range, 0, roofline, alpha=0.1, color='gray')

    # Ridge point annotation
    ridge_AI = PEAK_GFLOPS / PEAK_BW_GBS
    ax.axvline(ridge_AI, color='gray', linestyle=':', linewidth=1)
    ax.text(ridge_AI * 1.1, PEAK_GFLOPS * 0.8, f'Ridge: {ridge_AI:.1f} FLOP/B', fontsize=7)

    # Region labels
    ax.text(0.05, PEAK_GFLOPS * 0.3, 'Memory\nBound', fontsize=8, ha='center', color='blue')
    ax.text(50, PEAK_GFLOPS * 0.7, 'Compute\nBound', fontsize=8, ha='center', color='red')

    # Estimate SimuCell3D performance for each scheduler
    estimated_AI = [5, 15, 30]  # Contact detection, force calc, integration
    for i, sched in enumerate(data.get('schedulers', [])):
        color = style['colors'].get(sched.name, cm.viridis(i / max(1, len(data.get('schedulers', [])))))
        estimated_perf = [PEAK_BW_GBS * ai if ai < ridge_AI else PEAK_GFLOPS * 0.6
                         for ai in estimated_AI]
        ax.scatter(estimated_AI, estimated_perf, s=80, c=[color],
                   marker='*', zorder=10, label=f'{style["labels"].get(sched.name, sched.name)} (est.)')

    ax.set_xlabel('Arithmetic Intensity (FLOP/Byte)')
    ax.set_ylabel('Performance (GFLOP/s)')
    ax.set_xscale('log')
    ax.set_yscale('log')
    ax.set_xlim(0.01, 100)
    ax.set_ylim(1, PEAK_GFLOPS * 2)
    ax.legend(loc='lower right', fontsize=7)
    ax.set_title(f'Roofline Model (Peak: {PEAK_GFLOPS} GFLOP/s, BW: {PEAK_BW_GBS} GB/s)', fontsize=9)
    apply_tufte_style(ax, grid=True)

    save_figure(fig, output_dir / '06_roofline_model')
    return True


@register_plot(
    id='07',
    name='load_balance',
    narrative='computational',
    requires=['computational'],
    description='Iteration rate over time showing computational throughput'
)
def plot_load_balance(data: Dict, style: Dict, output_dir: Path,
                      use_latex: bool = True) -> bool:
    """Plot computational throughput over time for each scheduler."""
    print("  Plotting: 07_load_balance")

    fig, ax = plt.subplots(figsize=FIGSIZE_DOUBLE)

    has_data = False
    for sched in data.get('schedulers', []):
        accessor = SchedulerDataAccessor(data, sched.name, 'computational')

        # Try direct IPS column
        ts = accessor.get_time_series(
            value_cols=['iter_per_sec', 'iterations_per_sec', 'ips'],
            min_points=5,
            filter_zeros=True
        )

        if ts is None:
            # Try time_ms fallback
            df = accessor.get_raw_dataframe()
            if df is None:
                continue

            time_col = accessor.get_column(['total_time_ms', 'time_ms'])
            if time_col:
                t = compute_time_seconds(df)
                time_ms = df[time_col].values.astype(float)
                time_ms[time_ms == 0] = np.nan
                iter_rate = 1000.0 / time_ms

                valid = np.isfinite(iter_rate) & (iter_rate > 0) & np.isfinite(t)
                if np.sum(valid) < 5:
                    continue

                t_valid = t[valid]
                iter_rate_valid = iter_rate[valid]
            else:
                continue
        else:
            t_valid = ts.time
            iter_rate_valid = ts.values

        color = style['colors'].get(sched.name, cm.viridis(0.5))
        label = style['labels'].get(sched.name, sched.name)

        ax.plot(t_valid, iter_rate_valid, color=color, label=label, linewidth=1.2, alpha=0.8)
        ax.scatter(t_valid, iter_rate_valid, color=color, s=5, alpha=0.3)
        has_data = True

    if not has_data:
        print("    Skipped: No iteration rate data available")
        plt.close(fig)
        return False

    ax.set_xlabel('Seconds Since Process Start')
    ax.set_ylabel(format_si_label('Iteration Rate', 'iter/s', use_latex))
    ax.set_yscale('log')
    ax.set_xscale('log')
    ax.legend(loc='upper right', frameon=False)
    ax.set_title('Computational Throughput Over Time', fontsize=9)
    apply_tufte_style(ax, grid=True)

    save_figure(fig, output_dir / '07_load_balance')
    return True


@register_plot(
    id='09',
    name='performance_ratio',
    narrative='computational',
    requires=['comparison'],
    description='Performance ratio with bootstrap permutation tests for statistical significance'
)
def plot_performance_ratio(data: Dict, style: Dict, output_dir: Path,
                           use_latex: bool = True) -> bool:
    """
    Plot performance ratio with statistical significance testing.

    Uses bootstrap permutation test (10,000 iterations) to determine if
    performance differences are statistically significant. Distribution-free
    approach that doesn't assume normality.

    Significance annotation:
    - *** p < 0.001 (highly significant)
    - ** p < 0.01 (very significant)
    - * p < 0.05 (significant)
    - ns: p >= 0.05 (not significant)
    """
    print("  Plotting: 09_performance_ratio (with permutation tests)")

    df = data.get('comparison')
    if df is None or len(df) < 5:
        print("    Skipped: No comparison data available")
        return False

    # Try to get time in seconds from computational data
    schedulers = data.get('schedulers', [])
    time_seconds = None

    # First, try to get timestamps from any scheduler's computational data
    for sched in schedulers:
        accessor = SchedulerDataAccessor(data, sched.name, 'computational')
        comp_df = accessor.get_raw_dataframe()
        if comp_df is not None:
            time_seconds_full = compute_time_seconds(comp_df)
            if time_seconds_full is not None and len(time_seconds_full) > 0:
                # Map to comparison.csv iterations
                iter_col = find_column(df, ['iteration', 'iter'])
                if iter_col:
                    comp_iter_col = find_column(comp_df, ['iteration', 'iter'])
                    if comp_iter_col:
                        # Create mapping from iteration to time_seconds
                        iter_to_time = dict(zip(comp_df[comp_iter_col].values, time_seconds_full))
                        time_seconds = np.array([iter_to_time.get(it, np.nan) for it in df[iter_col].values])
                        break

    # Fallback: estimate cumulative time from time_ms columns
    if time_seconds is None or np.all(np.isnan(time_seconds)):
        time_cols = [c for c in df.columns if c.endswith('_time_ms')]
        if time_cols:
            # Use average time across schedulers for cumulative estimate
            avg_time_ms = df[time_cols].mean(axis=1).values
            cumulative_ms = np.cumsum(avg_time_ms)
            time_seconds = cumulative_ms / 1000.0  # Convert ms to seconds
        else:
            iter_col = find_column(df, ['iteration', 'iter'])
            time_seconds = df[iter_col].values.astype(float) if iter_col else np.arange(len(df))

    # Look for speedup or ratio columns
    speedup_cols = [c for c in df.columns if 'speedup' in c.lower() or 'ratio' in c.lower()]

    if not speedup_cols:
        # Calculate from time columns
        time_cols = [c for c in df.columns if c.endswith('_time_ms')]
        if len(time_cols) >= 2:
            # Use first scheduler as baseline
            baseline_col = time_cols[0]
            baseline_name = baseline_col.replace('_time_ms', '')
            fig, ax = plt.subplots(figsize=FIGSIZE_DOUBLE)

            baseline = df[baseline_col].values.astype(float)
            baseline[baseline == 0] = np.nan

            # Store data for statistical testing
            scheduler_times = {baseline_name: baseline}
            plotted_schedulers = [baseline_name]

            for time_col in time_cols[1:]:
                sched_name = time_col.replace('_time_ms', '')
                compare_vals = df[time_col].values.astype(float)
                ratio = baseline / compare_vals  # >1 means compare is faster

                valid = np.isfinite(ratio) & np.isfinite(time_seconds)
                if np.sum(valid) < 5:
                    continue

                # Store times for statistical testing
                scheduler_times[sched_name] = compare_vals
                plotted_schedulers.append(sched_name)

                color = style['colors'].get(sched_name, cm.viridis(0.5))
                label = f'{style["labels"].get(sched_name, sched_name)} vs {baseline_name}'
                ax.plot(time_seconds[valid], ratio[valid], color=color, label=label, linewidth=1.2)

            ax.axhline(1.0, color='gray', linestyle='--', linewidth=1, label='Parity')
            ax.set_xlabel('Seconds Since Process Start')
            ax.set_ylabel('Speedup Ratio')
            ax.legend(loc='best', frameon=False)
            ax.set_title('Performance Ratio Over Time', fontsize=9)
            apply_tufte_style(ax, grid=True, integer_time_axis=True)

            # Perform pairwise bootstrap permutation tests
            if len(plotted_schedulers) >= 2:
                print("\n    Bootstrap Permutation Tests (H0: equal performance):")
                test_results = []

                # Test baseline vs each other scheduler
                for i in range(1, len(plotted_schedulers)):
                    sched_name = plotted_schedulers[i]
                    baseline_times = scheduler_times[baseline_name]
                    compare_times = scheduler_times[sched_name]

                    # Remove NaNs for testing
                    valid_both = np.isfinite(baseline_times) & np.isfinite(compare_times)
                    if np.sum(valid_both) < 5:
                        continue

                    try:
                        result = bootstrap_permutation_test(
                            baseline_times[valid_both],
                            compare_times[valid_both],
                            n_permutations=10000,
                            statistic='mean',
                            significance=0.05,
                            random_seed=42
                        )

                        # Determine significance stars
                        if result.pvalue_twosided < 0.001:
                            sig_str = '***'
                        elif result.pvalue_twosided < 0.01:
                            sig_str = '**'
                        elif result.pvalue_twosided < 0.05:
                            sig_str = '*'
                        else:
                            sig_str = 'ns'

                        # Interpret effect (negative diff means compare is faster)
                        if result.observed_diff < 0:
                            faster = sched_name
                            pct_faster = abs(result.observed_diff / np.mean(baseline_times[valid_both])) * 100
                        else:
                            faster = baseline_name
                            pct_faster = abs(result.observed_diff / np.mean(compare_times[valid_both])) * 100

                        print(f"      {baseline_name} vs {sched_name}:")
                        print(f"        Faster: {faster} by {pct_faster:.1f}%")
                        print(f"        Effect Size: {abs(result.effect_size):.3f}")
                        print(f"        P-value: {result.pvalue_twosided:.4f} {sig_str}")

                        test_results.append({
                            'pair': f'{baseline_name} vs {sched_name}',
                            'sig': sig_str,
                            'p': result.pvalue_twosided
                        })

                    except Exception as e:
                        print(f"      Warning: Test failed for {baseline_name} vs {sched_name}: {e}")

                # Add significance annotations to plot
                if test_results:
                    annotation_lines = [f"{r['pair']}: {r['sig']}" for r in test_results]
                    annotation_text = "\n".join(annotation_lines)
                    ax.text(
                        0.02, 0.02,
                        "Statistical Tests:\n" + annotation_text,
                        transform=ax.transAxes,
                        verticalalignment='bottom',
                        horizontalalignment='left',
                        fontsize=7,
                        bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.3),
                        family='monospace'
                    )

            save_figure(fig, output_dir / '09_performance_ratio')
            return True

        print("    Skipped: No speedup or time columns found")
        return False

    # Plot existing speedup columns
    fig, ax = plt.subplots(figsize=FIGSIZE_DOUBLE)

    for col in speedup_cols:
        values = df[col].values.astype(float)
        valid = np.isfinite(values) & (values > 0) & np.isfinite(time_seconds)
        if np.sum(valid) < 5:
            continue

        label = col.replace('_', ' ').title()
        ax.plot(time_seconds[valid], values[valid], label=label, linewidth=1.2)

    ax.axhline(1.0, color='gray', linestyle='--', linewidth=1, label='Parity')
    ax.set_xlabel('Seconds Since Process Start')
    ax.set_ylabel('Speedup')
    ax.legend(loc='best', frameon=False, fontsize=7)
    ax.set_title('Performance Ratio Over Time', fontsize=9)
    apply_tufte_style(ax, grid=True, integer_time_axis=True)

    save_figure(fig, output_dir / '09_performance_ratio')
    return True


@register_plot(
    id='10',
    name='scheduler_radar',
    narrative='computational',
    requires=['computational', 'biological'],
    description='Multi-dimensional radar chart comparing scheduler characteristics'
)
def plot_scheduler_radar(data: Dict, style: Dict, output_dir: Path,
                         use_latex: bool = True) -> bool:
    """Plot multi-dimensional scheduler comparison as radar chart."""
    print("  Plotting: 10_scheduler_radar")

    schedulers = data.get('schedulers', [])
    if len(schedulers) < 1:
        print("    Skipped: No scheduler data")
        return False

    # Define the 6 radar axes
    axes_labels = ['Scalability', 'Load Balance', 'Throughput',
                   'Cell Production', 'Energy Stability', 'Predictability']
    n_axes = len(axes_labels)

    # Compute metrics for each scheduler (normalized 0-1)
    sched_metrics = {}

    for sched in schedulers:
        name = sched.name
        metrics = []

        accessor_comp = SchedulerDataAccessor(data, name, 'computational')
        accessor_bio = SchedulerDataAccessor(data, name, 'biological')
        comp_df = accessor_comp.get_raw_dataframe()
        bio_df = accessor_bio.get_raw_dataframe()

        # 1. Scalability (inverse deviation from O(N^4/3))
        scalability = 0.5
        if comp_df is not None:
            cells_col = accessor_comp.get_column(['cells', 'cell_count'])
            ips_col = accessor_comp.get_column(['iter_per_sec', 'iterations_per_sec'])
            time_col = accessor_comp.get_column(['total_time_ms', 'time_ms'])

            if cells_col and (ips_col or time_col):
                cells = comp_df[cells_col].values.astype(float)
                if ips_col:
                    iter_rate = comp_df[ips_col].values.astype(float)
                    iter_rate[iter_rate == 0] = np.nan
                    time_per_iter = 1.0 / iter_rate
                else:
                    time_per_iter = comp_df[time_col].values.astype(float)

                valid = (cells > 10) & np.isfinite(time_per_iter) & (time_per_iter > 0)
                if np.sum(valid) > 10:
                    try:
                        log_cells = np.log10(cells[valid])
                        log_time = np.log10(time_per_iter[valid])
                        coeffs = np.polyfit(log_cells, log_time, 1)
                        deviation = abs(coeffs[0] - THEORETICAL_COMPLEXITY_EXPONENT)
                        scalability = max(0, min(1, 1 - deviation / 0.5))
                    except Exception:
                        pass
        metrics.append(scalability)

        # 2. Load Balance (1 - CV of iteration time)
        load_balance = 0.5
        if comp_df is not None:
            ips_col = accessor_comp.get_column(['iter_per_sec', 'iterations_per_sec'])
            if ips_col:
                rates = comp_df[ips_col].dropna().values.astype(float)
                rates = rates[rates > 0]
                if len(rates) > 10:
                    cv = np.std(rates) / np.mean(rates) if np.mean(rates) > 0 else 1.0
                    load_balance = max(0, min(1, 1 - cv))
        metrics.append(load_balance)

        # 3. Throughput (normalized median IPS)
        throughput = 0.5
        if comp_df is not None:
            ips_col = accessor_comp.get_column(['iter_per_sec', 'iterations_per_sec'])
            if ips_col:
                n = len(comp_df)
                final_rates = comp_df[ips_col].iloc[int(0.9 * n):].dropna().values
                final_rates = final_rates[final_rates > 0]
                if len(final_rates) > 0:
                    throughput = min(1.0, np.median(final_rates) / 10.0)
        metrics.append(throughput)

        # 4. Cell Production (cells produced per second, normalized)
        cell_production = 0.5
        if comp_df is not None:
            cells_col = accessor_comp.get_column(['cells', 'cell_count'])
            if cells_col:
                cells = comp_df[cells_col].values.astype(float)
                t = compute_time_seconds(comp_df)
                if len(cells) > 2 and len(t) > 2 and np.max(t) > 0:
                    production_rate = (np.max(cells) - np.min(cells)) / np.max(t)
                    cell_production = min(1.0, production_rate / 1000.0)
        metrics.append(cell_production)

        # 5. Energy Stability (inverse of energy drift)
        energy_stability = 0.5
        if bio_df is not None:
            ke_col = accessor_bio.get_column(['total_kinetic_energy', 'kinetic_energy'])
            pe_col = accessor_bio.get_column(['total_potential_energy', 'potential_energy'])
            if ke_col and pe_col:
                E_total = bio_df[ke_col].values + bio_df[pe_col].values
                if len(E_total) > 10 and np.mean(np.abs(E_total)) > 0:
                    dE = np.diff(E_total)
                    cv_dE = np.std(dE) / np.mean(np.abs(dE)) if np.mean(np.abs(dE)) > 0 else 1.0
                    energy_stability = max(0, min(1, 1 - cv_dE * 0.5))
        metrics.append(energy_stability)

        # 6. Predictability (consistency)
        metrics.append(load_balance)  # Reuse load balance as proxy

        sched_metrics[name] = metrics

    if not sched_metrics:
        print("    Skipped: Could not compute metrics for any scheduler")
        return False

    # Create radar chart
    angles = np.linspace(0, 2 * np.pi, n_axes, endpoint=False).tolist()
    angles += angles[:1]  # Complete the loop

    fig, ax = plt.subplots(figsize=(7, 7), subplot_kw=dict(polar=True))

    for sched_name, metrics in sched_metrics.items():
        values = metrics + metrics[:1]  # Complete the loop
        color = style['colors'].get(sched_name, cm.viridis(0.5))
        label = style['labels'].get(sched_name, sched_name)
        ax.plot(angles, values, 'o-', linewidth=2, label=label, color=color)
        ax.fill(angles, values, alpha=0.25, color=color)

    ax.set_xticks(angles[:-1])
    ax.set_xticklabels(axes_labels, fontsize=8)
    ax.set_ylim(0, 1)
    ax.set_yticks([0.25, 0.5, 0.75, 1.0])
    ax.set_yticklabels(['0.25', '0.50', '0.75', '1.00'], fontsize=6)
    ax.legend(loc='upper right', bbox_to_anchor=(1.3, 1.1), frameon=False)
    ax.set_title('Multi-Dimensional Scheduler Comparison', fontsize=10, y=1.08)

    plt.tight_layout()
    save_figure(fig, output_dir / '10_scheduler_radar')
    return True


@register_plot(
    id='11',
    name='population_dynamics',
    narrative='biological',
    requires=['biological'],
    description='Population growth and growth rate over time (1x2 layout)'
)
def plot_population_dynamics(data: Dict, style: Dict, output_dir: Path,
                             use_latex: bool = True) -> bool:
    """Plot population dynamics: cumulative cells and growth rate vs time."""
    print("  Plotting: 11_population_dynamics")

    fig, axes = plt.subplots(1, 2, figsize=(12, 5))

    has_data = False
    for sched in data.get('schedulers', []):
        accessor = SchedulerDataAccessor(data, sched.name, 'biological')

        # Get cell count time series
        ts = accessor.get_time_series(
            value_cols=['cells', 'cell_count', 'num_cells'],
            min_points=5,
            filter_zeros=True
        )

        if ts is None:
            continue

        t_valid = ts.time
        cells_valid = ts.values

        color = style['colors'].get(sched.name, cm.viridis(0.5))
        label = style['labels'].get(sched.name, sched.name)

        # Left panel: Cumulative cells vs time
        axes[0].plot(t_valid, cells_valid, color=color, label=label, linewidth=1.5)

        # Right panel: Growth rate (cells per second)
        if len(t_valid) > 2:
            dt = np.diff(t_valid)
            dt[dt == 0] = 1e-6  # Avoid division by zero
            dcells = np.diff(cells_valid)
            growth_rate = dcells / dt  # cells per second

            # Apply smoothing with rolling median for noisy data
            if len(growth_rate) > 10:
                window = min(5, len(growth_rate) // 5)
                growth_rate_smooth = pd.Series(growth_rate).rolling(window, center=True, min_periods=1).median().values
            else:
                growth_rate_smooth = growth_rate

            # Filter outliers
            rate_valid = filter_outliers_iqr(growth_rate_smooth, multiplier=3.0)
            t_mid = (t_valid[:-1] + t_valid[1:]) / 2

            axes[1].plot(t_mid[rate_valid], growth_rate_smooth[rate_valid],
                        color=color, label=label, linewidth=1.5)

        has_data = True

    if not has_data:
        print("    Skipped: No population data available")
        plt.close(fig)
        return False

    # Configure left panel (Cumulative Cells)
    axes[0].set_xlabel('Seconds Since Process Start')
    axes[0].set_ylabel(format_si_label('Cumulative Cells', '', use_latex))
    axes[0].set_title('A. Cell Population Over Time', fontsize=10, loc='left')
    axes[0].legend(loc='upper left', frameon=False)
    axes[0].set_xlim(left=0)
    axes[0].set_ylim(bottom=0)
    apply_tufte_style(axes[0], grid=True, integer_time_axis=True)

    # Configure right panel (Growth Rate)
    axes[1].set_xlabel('Seconds Since Process Start')
    axes[1].set_ylabel(format_si_label('Growth Rate', 'cells/sec', use_latex))
    axes[1].set_title('B. Cell Growth Rate Over Time', fontsize=10, loc='left')
    axes[1].legend(loc='upper right', frameon=False)
    axes[1].set_xlim(left=0)
    axes[1].axhline(0, color='gray', linestyle='--', linewidth=0.5, alpha=0.5)
    apply_tufte_style(axes[1], grid=True, integer_time_axis=True)

    plt.tight_layout()
    save_figure(fig, output_dir / '11_population_dynamics')
    return True


@register_plot(
    id='03',
    name='biological_dashboard',
    narrative='biological',
    requires=['biological'],
    description='6-panel biological summary dashboard'
)
def plot_biological_dashboard(data: Dict, style: Dict, output_dir: Path,
                              use_latex: bool = True) -> bool:
    """Plot 6-panel biological dashboard with key metrics for ALL schedulers."""
    print("  Plotting: 03_biological_dashboard")

    fig, axes = plt.subplots(2, 3, figsize=(12, 8))

    # Collect all schedulers with biological data
    schedulers_with_data = []
    for sched in data.get('schedulers', []):
        accessor = SchedulerDataAccessor(data, sched.name, 'biological')
        if accessor.is_available() and len(accessor.get_raw_dataframe()) > 5:
            schedulers_with_data.append(sched.name)

    if not schedulers_with_data:
        print("    Skipped: No biological data available")
        plt.close(fig)
        return False

    # Panel A: Pressure Evolution (all schedulers)
    ax = axes[0, 0]
    has_pressure = False
    for sched_name in schedulers_with_data:
        accessor = SchedulerDataAccessor(data, sched_name, 'biological')
        ts = accessor.get_time_series(
            value_cols=['avg_pressure', 'mean_pressure', 'pressure'],
            min_points=1
        )
        if ts is not None:
            color = style['colors'].get(sched_name, cm.viridis(0.5))
            label = style['labels'].get(sched_name, sched_name)
            ax.plot(ts.time, ts.values, color=color, linewidth=1.2, label=label)
            has_pressure = True

    if has_pressure:
        ax.set_ylabel('Pressure (Pa)')
        ax.legend(loc='upper left', frameon=False, fontsize=7)
    else:
        ax.text(0.5, 0.5, 'Data not available', ha='center', va='center', transform=ax.transAxes)
    ax.set_title('A. Pressure Evolution', fontsize=9, loc='left')
    apply_tufte_style(ax, grid=True, integer_time_axis=True)

    # Panel B: Population Growth (all schedulers)
    ax = axes[0, 1]
    has_cells = False
    for sched_name in schedulers_with_data:
        accessor = SchedulerDataAccessor(data, sched_name, 'biological')
        ts = accessor.get_time_series(
            value_cols=['cells', 'cell_count', 'num_cells'],
            min_points=1
        )
        if ts is not None:
            color = style['colors'].get(sched_name, cm.viridis(0.5))
            label = style['labels'].get(sched_name, sched_name)
            ax.plot(ts.time, ts.values, color=color, linewidth=1.2, label=label)
            has_cells = True

    if has_cells:
        ax.set_ylabel('Cell Count')
        ax.legend(loc='upper left', frameon=False, fontsize=7)
    else:
        ax.text(0.5, 0.5, 'Data not available', ha='center', va='center', transform=ax.transAxes)
    ax.set_title('B. Population Growth', fontsize=9, loc='left')
    apply_tufte_style(ax, grid=True, integer_time_axis=True)

    # Panel C: Volume Homeostasis (all schedulers)
    ax = axes[0, 2]
    has_volume = False
    for sched_name in schedulers_with_data:
        accessor = SchedulerDataAccessor(data, sched_name, 'biological')
        ts = accessor.get_time_series(
            value_cols=['avg_volume', 'mean_volume', 'volume'],
            min_points=1
        )
        if ts is not None:
            color = style['colors'].get(sched_name, cm.viridis(0.5))
            label = style['labels'].get(sched_name, sched_name)
            volume_fl = ts.values * 1e15  # Convert to fL
            ax.plot(ts.time, volume_fl, color=color, linewidth=1.2, label=label)
            has_volume = True

    if has_volume:
        ax.set_ylabel('Volume (fL)')
        ax.legend(loc='upper left', frameon=False, fontsize=7)
    else:
        ax.text(0.5, 0.5, 'Data not available', ha='center', va='center', transform=ax.transAxes)
    ax.set_title('C. Volume Homeostasis', fontsize=9, loc='left')
    apply_tufte_style(ax, grid=True, integer_time_axis=True)

    # Panel D: Pressure Distribution (histogram - all schedulers stacked)
    ax = axes[1, 0]
    has_hist = False
    for i, sched_name in enumerate(schedulers_with_data):
        accessor = SchedulerDataAccessor(data, sched_name, 'biological')
        bio_df = accessor.get_raw_dataframe()
        if bio_df is None:
            continue

        color = style['colors'].get(sched_name, cm.viridis(0.5))
        label = style['labels'].get(sched_name, sched_name)

        pressure_col = accessor.get_column(['avg_pressure', 'mean_pressure', 'pressure'])
        if pressure_col:
            final_pressure = bio_df[pressure_col].iloc[-min(50, len(bio_df)):].dropna().values
            if len(final_pressure) > 5:
                ax.hist(final_pressure, bins=15, color=color, alpha=0.5,
                        edgecolor='black', linewidth=0.5, label=label)
                has_hist = True

    if has_hist:
        ax.set_xlabel('Pressure (Pa)')
        ax.set_ylabel('Frequency')
        ax.legend(loc='upper right', frameon=False, fontsize=7)
    else:
        ax.text(0.5, 0.5, 'Insufficient data', ha='center', va='center', transform=ax.transAxes)
    ax.set_title('D. Pressure Distribution', fontsize=9, loc='left')
    apply_tufte_style(ax, grid=True)

    # Panel E: Energy Evolution (all schedulers)
    ax = axes[1, 1]
    has_energy = False
    for sched_name in schedulers_with_data:
        accessor = SchedulerDataAccessor(data, sched_name, 'biological')
        bio_df = accessor.get_raw_dataframe()
        if bio_df is None:
            continue

        color = style['colors'].get(sched_name, cm.viridis(0.5))
        label = style['labels'].get(sched_name, sched_name)

        ke_col = accessor.get_column(['total_kinetic_energy', 'kinetic_energy'])
        pe_col = accessor.get_column(['total_potential_energy', 'potential_energy'])
        if ke_col and pe_col:
            t = compute_time_seconds(bio_df)
            E_total = bio_df[ke_col].values + bio_df[pe_col].values
            valid = np.isfinite(E_total)
            if np.sum(valid) > 5 and np.sum(np.abs(E_total[valid])) > 0:
                ax.plot(t[valid], E_total[valid], color=color, linewidth=1.2, label=label)
                has_energy = True

    if has_energy:
        ax.set_ylabel('Energy (J)')
        ax.set_xlabel('Seconds Since Process Start')
        ax.legend(loc='upper left', frameon=False, fontsize=7)
    else:
        ax.text(0.5, 0.5, 'Data not available', ha='center', va='center', transform=ax.transAxes)
    ax.set_title('E. Energy Evolution', fontsize=9, loc='left')
    apply_tufte_style(ax, grid=True, integer_time_axis=True)

    # Panel F: Cell Growth Rate (all schedulers)
    ax = axes[1, 2]
    has_growth = False
    for sched_name in schedulers_with_data:
        accessor = SchedulerDataAccessor(data, sched_name, 'biological')
        ts = accessor.get_time_series(
            value_cols=['cells', 'cell_count', 'num_cells'],
            min_points=10
        )
        if ts is not None:
            color = style['colors'].get(sched_name, cm.viridis(0.5))
            label = style['labels'].get(sched_name, sched_name)

            dt = np.diff(ts.time)
            dt[dt == 0] = 1e-6
            growth_rate = np.diff(ts.values) / dt  # cells per second
            valid = np.isfinite(growth_rate)
            if np.sum(valid) > 5:
                ax.plot(ts.time[1:][valid], growth_rate[valid], color=color, linewidth=1.2, label=label)
                has_growth = True

    if has_growth:
        ax.set_ylabel('Growth Rate (cells/sec)')
        ax.set_xlabel('Seconds Since Process Start')
        ax.legend(loc='upper right', frameon=False, fontsize=7)
    else:
        ax.text(0.5, 0.5, 'Insufficient data', ha='center', va='center', transform=ax.transAxes)
    ax.set_title('F. Cell Growth Rate', fontsize=9, loc='left')
    apply_tufte_style(ax, grid=True, integer_time_axis=True)

    plt.tight_layout()
    save_figure(fig, output_dir / '03_biological_dashboard')
    return True


# =============================================================================
# Ongoing Simulation Detection
# =============================================================================

def detect_ongoing_simulation(bench_dir: Path) -> Dict[str, Any]:
    """Detect if simulations are still running and check data freshness.

    Returns a dict with:
        - is_ongoing: bool - whether any simulation appears to be running
        - schedulers_status: dict - per-scheduler status
        - data_staleness_seconds: dict - how stale the metrics data is
        - warnings: list - warning messages for the user
    """
    import subprocess
    from datetime import datetime

    result = {
        'is_ongoing': False,
        'schedulers_status': {},
        'data_staleness_seconds': {},
        'warnings': []
    }

    # Check for PID files
    pid_files = list(bench_dir.glob('*.pid'))

    for pid_file in pid_files:
        sched_name = pid_file.stem  # e.g., "adaptive" from "adaptive.pid"
        try:
            pid = int(pid_file.read_text().strip())

            # Check if process is running
            try:
                subprocess.run(['ps', '-p', str(pid)], check=True,
                              capture_output=True, timeout=5)
                is_running = True
            except (subprocess.CalledProcessError, subprocess.TimeoutExpired):
                is_running = False

            result['schedulers_status'][sched_name] = {
                'pid': pid,
                'is_running': is_running
            }

            if is_running:
                result['is_ongoing'] = True

        except (ValueError, FileNotFoundError):
            pass

    # Check for recently modified simulation files
    for sim_dir in bench_dir.glob('sim_*'):
        sched_name = sim_dir.name.replace('sim_', '')
        stats_file = sim_dir / 'simulation_statistics.csv'

        if stats_file.exists():
            stats_mtime = datetime.fromtimestamp(stats_file.stat().st_mtime)
            age_seconds = (datetime.now() - stats_mtime).total_seconds()

            # If file modified in last 5 minutes, simulation likely running
            if age_seconds < 300:
                if sched_name not in result['schedulers_status']:
                    result['schedulers_status'][sched_name] = {}
                result['schedulers_status'][sched_name]['sim_stats_age_sec'] = age_seconds
                result['schedulers_status'][sched_name]['sim_stats_rows'] = sum(1 for _ in open(stats_file)) - 1

                if age_seconds < 60:
                    result['is_ongoing'] = True

    # Check metrics data staleness
    metrics_dir = bench_dir / 'metrics'
    for sched_dir in metrics_dir.glob('*'):
        if sched_dir.is_dir():
            sched_name = sched_dir.name
            comp_file = sched_dir / 'computational.csv'

            if comp_file.exists():
                comp_mtime = datetime.fromtimestamp(comp_file.stat().st_mtime)
                staleness = (datetime.now() - comp_mtime).total_seconds()
                result['data_staleness_seconds'][sched_name] = staleness

                # Compare with raw simulation data
                sim_stats = bench_dir / f'sim_{sched_name}' / 'simulation_statistics.csv'
                if sim_stats.exists():
                    sim_mtime = datetime.fromtimestamp(sim_stats.stat().st_mtime)
                    if (sim_mtime - comp_mtime).total_seconds() > 60:
                        # Raw data is significantly newer than metrics
                        result['warnings'].append(
                            f"⚠️  {sched_name}: Metrics data is {staleness/60:.1f} min stale. "
                            f"Raw simulation data is more recent."
                        )

    return result


def print_ongoing_status(status: Dict[str, Any]) -> None:
    """Print human-readable status of ongoing simulations."""
    if status['is_ongoing']:
        print("\n" + "=" * 60)
        print("🔄 ONGOING SIMULATION DETECTED")
        print("=" * 60)

        for sched, info in status['schedulers_status'].items():
            if info.get('is_running'):
                print(f"  {sched}: Process {info.get('pid')} is RUNNING")
            if info.get('sim_stats_age_sec', float('inf')) < 300:
                age = info['sim_stats_age_sec']
                rows = info.get('sim_stats_rows', '?')
                print(f"  {sched}: simulation_statistics.csv updated {age:.0f}s ago ({rows} rows)")

        if status['warnings']:
            print("\n  Warnings:")
            for warn in status['warnings']:
                print(f"    {warn}")

        print("\n  Note: Metrics files may be stale. For live data, trigger")
        print("  the metrics aggregation or wait for the daemon to update.")
        print("=" * 60 + "\n")


# =============================================================================
# Main Pipeline
# =============================================================================

def generate_manifest(
    bench_dir: Path,
    output_dir: Path,
    plots_generated: List[str],
    validation_report: ValidationReport,
    audit_trail: AuditTrail,
    args: argparse.Namespace
) -> None:
    """Generate manifest.json for reproducibility."""
    manifest = {
        'generator': 'plot_benchmark_unified.py',
        'version': '2.0.0',
        'generated_at': datetime.now().isoformat(),
        'benchmark_directory': str(bench_dir),
        'output_directory': str(output_dir),
        'settings': {
            'quality_mode': args.quality,
            'use_latex': not args.no_latex,
            'formats': args.formats.split(','),
        },
        'plots_generated': plots_generated,
        'plots_skipped': [
            p.id for p in PLOT_REGISTRY.values()
            if p.id not in plots_generated
        ],
        'validation': {
            'is_valid': validation_report.is_valid,
            'total_issues': validation_report.total_issues,
            'severity_summary': validation_report.severity_summary(),
        },
        'corrections_applied': audit_trail.total_corrections,
    }

    manifest_path = output_dir / 'manifest.json'
    with open(manifest_path, 'w') as f:
        json.dump(manifest, f, indent=2)
    print(f"\n  Saved manifest: {manifest_path}")


def main():
    parser = argparse.ArgumentParser(
        description='SimuCell3D Unified Benchmark Visualization Suite v2.0',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s doc/working/parallel_benchmark_20260126_093337
  %(prog)s doc/working/parallel_benchmark_20260126_093337 --quality draft
  %(prog)s doc/working/parallel_benchmark_20260126_093337 --validate-only
  %(prog)s doc/working/parallel_benchmark_20260126_093337 --plots biological
        """
    )

    parser.add_argument('benchmark_dir', type=Path,
                        help='Path to benchmark directory')
    parser.add_argument('--output-dir', type=Path, default=None,
                        help='Output directory (default: <benchmark>/plots-unified)')
    parser.add_argument('--quality', choices=['publication', 'draft'],
                        default='publication',
                        help='Quality preset (publication=300 DPI, draft=150 DPI)')
    parser.add_argument('--plots', type=str, default='all',
                        help='Plots to generate: all, biological, computational, or comma-separated IDs')
    parser.add_argument('--skip-sim-stats', action='store_true',
                        help='Skip loading large simulation_statistics.csv files')
    parser.add_argument('--validate-only', action='store_true',
                        help='Run validation without generating plots')
    parser.add_argument('--no-latex', action='store_true',
                        help='Disable LaTeX rendering')
    parser.add_argument('--formats', type=str, default='png',
                        help='Output formats (comma-separated: png,pdf,svg)')
    parser.add_argument('--force', action='store_true',
                        help='Force regeneration, ignore cache')
    parser.add_argument('-v', '--verbose', action='store_true',
                        help='Verbose output')

    args = parser.parse_args()

    # Validate benchmark directory
    if not args.benchmark_dir.exists():
        print(f"Error: Benchmark directory not found: {args.benchmark_dir}")
        return 1

    # Set output directory
    output_dir = args.output_dir or (args.benchmark_dir / 'plots-unified')
    output_dir.mkdir(parents=True, exist_ok=True)

    print("=" * 60)
    print("SimuCell3D Unified Benchmark Visualization Suite v2.0")
    print("=" * 60)
    print(f"Benchmark: {args.benchmark_dir}")
    print(f"Output: {output_dir}")
    print(f"Quality: {args.quality}")

    # Check for ongoing simulations
    ongoing_status = detect_ongoing_simulation(args.benchmark_dir)
    print_ongoing_status(ongoing_status)

    # Step 1: Validate data
    print("\n" + "=" * 60)
    print("Step 1: Data Validation")
    print("=" * 60)

    validation_report = validate_benchmark(args.benchmark_dir, verbose=True)
    validation_report.export_json(output_dir / 'validation_report.json')
    validation_report.export_html(output_dir / 'validation_report.html')

    if args.validate_only:
        print("\n--validate-only specified, stopping after validation.")
        return 0 if validation_report.is_valid else 1

    # Step 2: Load and clean data
    print("\n" + "=" * 60)
    print("Step 2: Data Loading and Cleaning")
    print("=" * 60)

    data, audit_trail = load_and_clean_data(
        args.benchmark_dir,
        load_simulation_stats=not args.skip_sim_stats,
        verbose=True
    )
    audit_trail.export_json(output_dir / 'audit_trail.json')

    # Step 3: Configure plotting
    print("\n" + "=" * 60)
    print("Step 3: Generating Plots")
    print("=" * 60)

    configure_publication_defaults(
        use_latex=not args.no_latex,
        quality_mode=args.quality
    )

    # Determine which plots to generate
    plots_to_generate = []
    if args.plots == 'all':
        plots_to_generate = list(PLOT_REGISTRY.keys())
    elif args.plots == 'biological':
        plots_to_generate = [p.id for p in PLOT_REGISTRY.values() if p.narrative == 'biological']
    elif args.plots == 'computational':
        plots_to_generate = [p.id for p in PLOT_REGISTRY.values() if p.narrative == 'computational']
    elif args.plots == 'physical':
        plots_to_generate = [p.id for p in PLOT_REGISTRY.values() if p.narrative == 'physical']
    else:
        plots_to_generate = [p.strip() for p in args.plots.split(',')]

    # Generate plots
    plots_generated = []
    style = data.get('style_config', {})
    formats = args.formats.split(',')

    for plot_id in plots_to_generate:
        if plot_id not in PLOT_REGISTRY:
            print(f"  Warning: Unknown plot ID '{plot_id}', skipping")
            continue

        plot_info = PLOT_REGISTRY[plot_id]

        # Check if sim_stats required but not loaded
        if plot_info.requires_sim_stats and args.skip_sim_stats:
            print(f"  Skipping {plot_id}: requires simulation_statistics (use without --skip-sim-stats)")
            continue

        try:
            success = plot_info.function(data, style, output_dir, use_latex=not args.no_latex)
            if success:
                plots_generated.append(plot_id)
        except Exception as e:
            print(f"  Error generating {plot_id}: {e}")
            if args.verbose:
                import traceback
                traceback.print_exc()

    # Step 4: Generate manifest
    generate_manifest(
        args.benchmark_dir, output_dir, plots_generated,
        validation_report, audit_trail, args
    )

    # Summary
    print("\n" + "=" * 60)
    print("Summary")
    print("=" * 60)
    print(f"Plots generated: {len(plots_generated)}/{len(plots_to_generate)}")
    print(f"Validation issues: {validation_report.total_issues}")
    print(f"Corrections applied: {audit_trail.total_corrections}")
    print(f"Output directory: {output_dir}")

    return 0


if __name__ == '__main__':
    sys.exit(main())
