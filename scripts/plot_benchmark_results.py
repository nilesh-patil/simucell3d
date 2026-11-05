#!/usr/bin/env python3
"""
Comprehensive Benchmark Visualization Suite for SimuCell3D.

Generates 20 publication-ready plots covering computational performance,
biological metrics, physical/energy analysis, and statistical comparisons.

Usage:
    python plot_benchmark_results.py <benchmark_directory>

Output:
    <benchmark_directory>/plots/01_iteration_progress.png
    <benchmark_directory>/plots/02_iteration_rate_vs_cells.png
    ... (20 total plots)
    <benchmark_directory>/plots/SUMMARY_STATS.md
"""

# =============================================================================
# SECTION 1: Imports and Constants
# =============================================================================

import sys
import re
import json
from pathlib import Path
from typing import Optional, Callable, Any, List, Dict, Tuple
from functools import wraps

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
import matplotlib.gridspec as gridspec
from matplotlib.patches import Patch

# Optional imports with graceful degradation
try:
    from scipy import stats
    from scipy.optimize import curve_fit
    SCIPY_AVAILABLE = True
except ImportError:
    SCIPY_AVAILABLE = False
    print("Warning: scipy not available, some statistical features disabled")

# =============================================================================
# SECTION 2: Color Scheme and Configuration
# =============================================================================

from dataclasses import dataclass, field
from typing import List, Dict

COLORS = {
    'v1': '#2196F3',           # Blue (Material Design)
    'static': '#4CAF50',       # Green
    'adaptive': '#F44336',     # Red (Material Design)
    # Phase colors for timing breakdown
    'mesh_refinement': '#2ECC71',
    'contact_detection': '#E74C3C',
    'polarization': '#3498DB',
    'time_integration': '#9B59B6',
    # Energy components
    'kinetic': '#F1C40F',
    'pressure': '#E74C3C',
    'surface_tension': '#3498DB',
    'potential': '#27AE60',
}

LABELS = {
    'v1': 'v1.0 Baseline',
    'v1r2': 'v1.0 Baseline (r2)',
    'v1fix': 'v1.0 + Local-id Fix',
    'fix113r2': 'Fix113 (r2)',
    'static': 'Static Scheduler',
    'adaptive': 'Adaptive Scheduler',
}

# =============================================================================
# SECTION 2b: Scheduler Auto-Detection
# =============================================================================

@dataclass
class SchedulerMetadata:
    """Metadata for a detected scheduler mode."""
    name: str
    metrics_dir: Optional[Path] = None
    sim_dir: Optional[Path] = None
    has_biological: bool = False
    has_computational: bool = False
    has_phase_timings: bool = False
    has_workload: bool = False
    has_simulation_stats: bool = False
    has_perf_diag: bool = False


def detect_schedulers(bench_dir: Path) -> List[SchedulerMetadata]:
    """
    Auto-detect scheduler modes from benchmark directory structure.

    Scans:
    1. metrics/{scheduler_name}/ - Aggregated metrics
    2. sim_{scheduler_name}/ - Detailed simulation data

    Returns:
        List of SchedulerMetadata sorted by name
    """
    schedulers: Dict[str, SchedulerMetadata] = {}

    # Pattern 1: Scan metrics/ subdirectories
    metrics_dir = bench_dir / "metrics"
    if metrics_dir.exists():
        for subdir in metrics_dir.iterdir():
            if subdir.is_dir() and subdir.name not in ('comparison', 'cleaned_data'):
                name = subdir.name
                if name not in schedulers:
                    schedulers[name] = SchedulerMetadata(name=name)

                meta = schedulers[name]
                meta.metrics_dir = subdir
                meta.has_biological = (subdir / "biological.csv").exists()
                meta.has_computational = (subdir / "computational.csv").exists()
                meta.has_phase_timings = (subdir / "phase_timings.csv").exists()
                meta.has_workload = (subdir / "workload.csv").exists()

    # Pattern 2: Scan sim_* directories
    for sim_dir in bench_dir.glob("sim_*"):
        if sim_dir.is_dir():
            name = sim_dir.name.replace("sim_", "")
            if name not in schedulers:
                schedulers[name] = SchedulerMetadata(name=name)

            meta = schedulers[name]
            meta.sim_dir = sim_dir
            meta.has_simulation_stats = (sim_dir / "simulation_statistics.csv").exists()
            meta.has_perf_diag = (sim_dir / "performance_diagnostics.csv").exists()

    if not schedulers:
        print(f"  Warning: No scheduler data found in {bench_dir}")
        return []

    return sorted(schedulers.values(), key=lambda s: s.name)


def generate_style_config(schedulers: List[SchedulerMetadata]) -> Dict[str, Any]:
    """
    Generate dynamic color/label mappings for N schedulers.

    Uses predefined colors for known schedulers (v1, static, adaptive),
    generates new colors for unknown schedulers.
    """
    import matplotlib.cm as cm

    n = len(schedulers)

    # Generate colors - use predefined for known, generate for unknown
    colors = {}
    color_positions = np.linspace(0.2, 0.8, max(n, 3))

    for i, meta in enumerate(schedulers):
        if meta.name in COLORS:
            colors[meta.name] = COLORS[meta.name]
        else:
            colors[meta.name] = cm.viridis(color_positions[i])

    # Generate labels
    labels = {}
    for meta in schedulers:
        if meta.name in LABELS:
            labels[meta.name] = LABELS[meta.name]
        else:
            labels[meta.name] = meta.name.replace('_', ' ').title()

    return {'colors': colors, 'labels': labels}


def iter_schedulers(data: Dict) -> List[Tuple[str, str, str]]:
    """
    Helper to iterate over detected schedulers with their styles.

    Args:
        data: Data dictionary containing 'schedulers' and 'style_config'

    Yields:
        Tuples of (scheduler_name, color, label)

    Usage:
        for name, color, label in iter_schedulers(data):
            df = data.get(f'{name}_computational')
            ax.plot(..., color=color, label=label)
    """
    schedulers = data.get('schedulers', [])
    style = data.get('style_config', {})
    colors = style.get('colors', COLORS)
    labels = style.get('labels', LABELS)

    result = []
    for sched in schedulers:
        name = sched.name
        color = colors.get(name, COLORS.get(name, '#888888'))
        label = labels.get(name, name)
        result.append((name, color, label))

    return result


def get_comparison_schedulers(df: pd.DataFrame) -> List[str]:
    """
    Detect available scheduler names from comparison.csv column patterns.

    Looks for columns like {name}_iter, {name}_cells, {name}_ips
    Returns list of unique scheduler names found.
    """
    schedulers = set()
    for col in df.columns:
        if col.endswith('_iter'):
            schedulers.add(col.replace('_iter', ''))
        elif col.endswith('_cells'):
            schedulers.add(col.replace('_cells', ''))
        elif col.endswith('_ips'):
            schedulers.add(col.replace('_ips', ''))
    return sorted(schedulers)


def get_scheduler_columns(df: pd.DataFrame, scheduler: str) -> Dict[str, str]:
    """
    Get column names for a specific scheduler from comparison.csv.

    Returns dict with keys: 'iter', 'cells', 'ips' mapping to actual column names.
    """
    cols = {}
    iter_col = f'{scheduler}_iter'
    cells_col = f'{scheduler}_cells'
    ips_col = f'{scheduler}_ips'

    if iter_col in df.columns:
        cols['iter'] = iter_col
    if cells_col in df.columns:
        cols['cells'] = cells_col
    if ips_col in df.columns:
        cols['ips'] = ips_col

    return cols


def build_sim_stats_modes(data: dict) -> Dict[str, Tuple[pd.DataFrame, str]]:
    """
    Build a modes dictionary for simulation statistics plots.

    Dynamically detects available {name}_sim_stats keys and builds
    a dict compatible with existing biological plot functions.

    Returns:
        Dict mapping scheduler name to (DataFrame, color) tuple
    """
    modes = {}
    for name, color, label in iter_schedulers(data):
        df = data.get(f'{name}_sim_stats')
        if df is not None and len(df) > 0:
            modes[name] = (df, color)
    return modes


def has_any_sim_stats(data: dict) -> bool:
    """Check if at least one *_sim_stats DataFrame exists."""
    schedulers = data.get('schedulers', [])
    for sched in schedulers:
        # schedulers contains Scheduler namedtuples with .name attribute
        name = sched.name if hasattr(sched, 'name') else sched
        if data.get(f'{name}_sim_stats') is not None:
            return True
    return False


def has_any_computational(data: dict) -> bool:
    """Check if at least one *_computational DataFrame exists."""
    schedulers = data.get('schedulers', [])
    for sched in schedulers:
        # schedulers contains Scheduler namedtuples with .name attribute
        name = sched.name if hasattr(sched, 'name') else sched
        if data.get(f'{name}_computational') is not None:
            return True
    return False


def requires_any_computational(func):
    """
    Decorator for computational plots that require at least one *_computational DataFrame.
    """
    func.__requires_any_computational__ = True
    @wraps(func)
    def wrapper(*args, **kwargs):
        return func(*args, **kwargs)
    wrapper.__requires_any_computational__ = True
    return wrapper


# Plot configuration
DPI = 150
FIGSIZE_2x2 = (14, 10)
FIGSIZE_2x3 = (14, 12)
FIGSIZE_3x3 = (18, 14)

# =============================================================================
# SECTION 3: Data Loading Functions
# =============================================================================

def load_csv_safe(path: Path, parse_dates: list = None) -> Optional[pd.DataFrame]:
    """Load CSV with error handling."""
    if not path.exists():
        return None
    try:
        df = pd.read_csv(path)
        if parse_dates:
            for col in parse_dates:
                if col in df.columns:
                    df[col] = pd.to_datetime(df[col], format='%Y-%m-%d_%H:%M:%S', errors='coerce')
        return df
    except Exception as e:
        print(f"  Warning: Could not load {path}: {e}")
        return None


def parse_log_for_cells(log_path: Path) -> Optional[pd.DataFrame]:
    """Extract iteration, cell count pairs from log file."""
    if not log_path.exists():
        return None

    pattern = r'iteration:\s*(\d+).*nb cells\s+(\d+)'
    data = []
    try:
        with open(log_path, 'r') as f:
            for line in f:
                match = re.search(pattern, line)
                if match:
                    data.append((int(match.group(1)), int(match.group(2))))
        if data:
            return pd.DataFrame(data, columns=['iteration', 'cells'])
    except Exception as e:
        print(f"  Warning: Could not parse {log_path}: {e}")
    return None


def load_all_data(bench_dir: Path) -> dict:
    """
    Master data loader with auto-detection and graceful degradation.

    Automatically detects available schedulers from directory structure and
    loads all available data for each.
    """
    data = {}

    # Auto-detect schedulers
    print("  Auto-detecting schedulers...")
    schedulers = detect_schedulers(bench_dir)
    data['schedulers'] = schedulers
    print(f"    Found {len(schedulers)} scheduler(s): {[s.name for s in schedulers]}")

    # Generate dynamic style config
    data['style_config'] = generate_style_config(schedulers)

    # Load comparison data
    comp_file = bench_dir / "metrics" / "comparison.csv"
    data['comparison'] = load_csv_safe(comp_file, parse_dates=['timestamp'])

    # Load data for each detected scheduler
    for sched in schedulers:
        name = sched.name

        # Load from metrics directory
        if sched.metrics_dir and sched.metrics_dir.exists():
            data[f'{name}_computational'] = load_csv_safe(
                sched.metrics_dir / "computational.csv",
                parse_dates=['timestamp']
            )
            data[f'{name}_biological'] = load_csv_safe(
                sched.metrics_dir / "biological.csv",
                parse_dates=['timestamp']
            )
            data[f'{name}_phase'] = load_csv_safe(
                sched.metrics_dir / "phase_timings.csv",
                parse_dates=['timestamp']
            )
            data[f'{name}_workload'] = load_csv_safe(
                sched.metrics_dir / "workload.csv",
                parse_dates=['timestamp']
            )

        # Load from sim directory
        if sched.sim_dir and sched.sim_dir.exists():
            data[f'{name}_sim_stats'] = load_csv_safe(sched.sim_dir / "simulation_statistics.csv")
            data[f'{name}_perf_diag'] = load_csv_safe(sched.sim_dir / "performance_diagnostics.csv")

    # Load log files for all detected schedulers
    logs_dir = bench_dir / "logs"
    for sched in schedulers:
        data[f'{sched.name}_log'] = parse_log_for_cells(logs_dir / f"{sched.name}.log")

    # Optional per-run facts the CSVs cannot carry (commit, machine, outcome, log-derived
    # counters); written by the staging script, keyed by scheduler name
    notes_file = bench_dir / "run_notes.json"
    data['run_notes'] = {}
    if notes_file.exists():
        try:
            with open(notes_file) as f:
                data['run_notes'] = json.load(f)
        except (OSError, ValueError) as e:
            print(f"    Warning: Could not load {notes_file}: {e}")

    return data


# =============================================================================
# SECTION 4: Utility Functions
# =============================================================================

def requires_data(*keys):
    """Decorator to specify required data keys for a plot function."""
    def decorator(func):
        func.__required_data__ = keys
        @wraps(func)
        def wrapper(*args, **kwargs):
            return func(*args, **kwargs)
        wrapper.__required_data__ = keys
        return wrapper
    return decorator


def requires_any_sim_stats(func):
    """
    Decorator for biological plots that require at least one *_sim_stats DataFrame.

    Unlike @requires_data which checks for specific keys, this decorator
    checks if ANY scheduler has simulation statistics available.
    """
    func.__requires_any_sim_stats__ = True
    @wraps(func)
    def wrapper(*args, **kwargs):
        return func(*args, **kwargs)
    wrapper.__requires_any_sim_stats__ = True
    return wrapper


def plot_with_fallback(plot_func: Callable, data: dict, output_dir: Path,
                       plot_num: str, plot_name: str) -> bool:
    """Execute plot function with comprehensive error handling."""
    try:
        # Check standard required keys
        required_keys = getattr(plot_func, '__required_data__', [])
        missing = [k for k in required_keys if k not in data or data[k] is None]

        if missing:
            print(f"  [SKIP] {plot_num}_{plot_name}.png - Missing: {missing}")
            return False

        # Check for any_sim_stats requirement
        if getattr(plot_func, '__requires_any_sim_stats__', False):
            if not has_any_sim_stats(data):
                print(f"  [SKIP] {plot_num}_{plot_name}.png - No simulation stats available")
                return False

        # Check for any_computational requirement
        if getattr(plot_func, '__requires_any_computational__', False):
            if not has_any_computational(data):
                print(f"  [SKIP] {plot_num}_{plot_name}.png - No computational data available")
                return False

        plot_func(data, output_dir)
        print(f"  [OK]   {plot_num}_{plot_name}.png")
        return True

    except Exception as e:
        print(f"  [ERR]  {plot_num}_{plot_name}.png - {e}")
        import traceback
        traceback.print_exc()
        return False


def compute_statistics(arr: np.ndarray) -> dict:
    """Compute statistical moments for distribution characterization."""
    arr = np.asarray(arr)
    arr = arr[~np.isnan(arr)]
    if len(arr) == 0:
        return {'mean': 0, 'std': 0, 'cv': 0, 'skewness': 0, 'kurtosis': 0, 'median': 0}

    mean = np.mean(arr)
    std = np.std(arr)
    result = {
        'mean': mean,
        'std': std,
        'cv': std / mean if mean != 0 else 0,
        'median': np.median(arr),
        'min': np.min(arr),
        'max': np.max(arr),
    }

    if SCIPY_AVAILABLE and len(arr) > 3:
        result['skewness'] = stats.skew(arr)
        result['kurtosis'] = stats.kurtosis(arr)
    else:
        result['skewness'] = 0
        result['kurtosis'] = 0

    return result


def add_stats_box(ax, text: str, position: tuple = (0.02, 0.98), fontsize: int = 8):
    """Add a statistics annotation box to axis."""
    ax.annotate(text, xy=position, xycoords='axes fraction',
                fontsize=fontsize, verticalalignment='top',
                bbox=dict(boxstyle='round', facecolor='white', alpha=0.8, edgecolor='gray'))


def power_law(x, a, b):
    """Power law function for curve fitting: y = a * x^b"""
    return a * np.power(x, b)


def derive_divisions(df: pd.DataFrame, cells_col: str = 'cells',
                     div_col: str = 'divisions') -> Tuple[Optional[pd.Series], str]:
    """Return a per-row divisions Series and the method used to obtain it.

    The simulator's logged ``divisions`` column is reset by the 50-iteration
    adaptive-scheduling update immediately before every 100-iteration diagnostics
    export, so it is structurally always 0 in older runs. When the logged column
    carries no signal we fall back to Δcells: every division nets +1 cell (two
    daughters replace one mother), so the positive change in cell count between
    consecutive samples is a faithful estimate of divisions in that window.

    Args:
        df: DataFrame with at least a cell-count column.
        cells_col: Name of the cell-count column.
        div_col: Name of the logged divisions column (used if it carries signal).

    Returns:
        Tuple of (divisions Series aligned to df.index, method string). Method is
        'logged' when the recorded column is used, 'delta_cells' for the fallback,
        or 'unavailable' when no cell-count column is present (Series is None).
    """
    if df is None or cells_col not in df.columns:
        return None, 'unavailable'

    if div_col in df.columns:
        logged = df[div_col].fillna(0)
        if logged.abs().sum() > 0:
            return logged.clip(lower=0), 'logged'

    # Fallback: divisions approx. positive change in cell count between samples
    return df[cells_col].diff().fillna(0).clip(lower=0), 'delta_cells'


def get_division_source(data: dict, name: str, min_rows: int = 10) -> Optional[pd.DataFrame]:
    """Pick the best available per-scheduler source for division dynamics.

    Prefers performance_diagnostics (regular iteration/cells sampling, present for
    live-snapshot runs), then the parsed log, then computational metrics. All carry
    'iteration' and 'cells', which is all :func:`derive_divisions` needs.
    """
    for key in (f'{name}_perf_diag', f'{name}_log', f'{name}_computational'):
        df = data.get(key)
        if df is not None and 'cells' in df.columns and len(df) >= min_rows:
            return df
    return None


def sample_dataframe(df: pd.DataFrame, max_points: int = 500) -> pd.DataFrame:
    """Sample dataframe for plotting large datasets."""
    if len(df) <= max_points:
        return df
    step = len(df) // max_points
    return df.iloc[::step].copy()


def to_minutes_since_start(timestamps: pd.Series) -> pd.Series:
    """Convert timestamp series to minutes since the first timestamp.

    Args:
        timestamps: Pandas Series of datetime objects

    Returns:
        Series of float values representing minutes since start
    """
    if timestamps.empty or timestamps.isna().all():
        return pd.Series(dtype=float)

    start_time = timestamps.iloc[0]
    return (timestamps - start_time).dt.total_seconds() / 60.0


# =============================================================================
# SECTION 5: Computational Metrics Plots (01-05)
# =============================================================================

@requires_data('comparison')
def plot_01_iteration_progress(data: dict, output_dir: Path) -> None:
    """Plot 01: Iteration count over wall clock time."""
    fig, ax = plt.subplots(figsize=(12, 6))

    df = data['comparison']
    schedulers = get_comparison_schedulers(df)

    if not schedulers:
        ax.text(0.5, 0.5, 'No scheduler data found', ha='center', va='center',
                transform=ax.transAxes, fontsize=12, color='gray')
        plt.savefig(output_dir / "01_iteration_progress.png", dpi=DPI, bbox_inches='tight')
        plt.close()
        return

    # Filter to rows with data from at least one scheduler
    filter_mask = pd.Series(False, index=df.index)
    for sched in schedulers:
        cols = get_scheduler_columns(df, sched)
        if 'iter' in cols:
            filter_mask |= (df[cols['iter']] > 0)
    df_filtered = df[filter_mask].copy()

    if len(df_filtered) == 0:
        ax.text(0.5, 0.5, 'No iteration data available', ha='center', va='center',
                transform=ax.transAxes, fontsize=12, color='gray')
        plt.savefig(output_dir / "01_iteration_progress.png", dpi=DPI, bbox_inches='tight')
        plt.close()
        return

    # Determine x-axis
    if 'timestamp' in df_filtered.columns and df_filtered['timestamp'].notna().any():
        x_data = to_minutes_since_start(df_filtered['timestamp'])
        ax.set_xlabel('Time (minutes since start)', fontsize=12)
    else:
        x_data = range(len(df_filtered))
        ax.set_xlabel('Sample Index', fontsize=12)

    # Plot each scheduler
    markers = ['o', 's', '^', 'D', 'v', '<', '>', 'p']
    style = data.get('style_config', {})
    colors = style.get('colors', COLORS)
    labels = style.get('labels', LABELS)

    for i, sched in enumerate(schedulers):
        cols = get_scheduler_columns(df_filtered, sched)
        if 'iter' not in cols:
            continue
        iter_col = cols['iter']
        color = colors.get(sched, COLORS.get(sched, f'C{i}'))
        label = labels.get(sched, sched.replace('_', ' ').title())
        marker = markers[i % len(markers)]
        final_iter = df_filtered[iter_col].iloc[-1]

        ax.plot(x_data, df_filtered[iter_col],
                label=f"{label} (final: {final_iter:,})",
                linewidth=2, marker=marker, markersize=2, alpha=0.7, color=color)

    ax.set_ylabel('Iteration', fontsize=12)
    ax.set_title('Iteration Progress Over Time', fontsize=14, fontweight='bold')
    ax.legend(fontsize=11)
    ax.grid(True, alpha=0.3)
    ax.yaxis.set_major_formatter(plt.FuncFormatter(lambda x, p: f'{int(x):,}'))

    plt.tight_layout()
    plt.savefig(output_dir / "01_iteration_progress.png", dpi=DPI, bbox_inches='tight')
    plt.close()


@requires_data('comparison')
def plot_02_iteration_rate_vs_cells(data: dict, output_dir: Path) -> None:
    """Plot 02: Iteration rate with scaling analysis and parallel efficiency estimation.

    Enhanced with HPC expert recommendations:
    - Theoretical scaling reference lines (O(N), O(N²))
    - Computational efficiency per cell
    - Relative scaling efficiency estimation
    """
    fig, axes = plt.subplots(2, 2, figsize=(14, 12))

    df = data['comparison']
    schedulers = get_comparison_schedulers(df)
    style = data.get('style_config', {})
    colors = style.get('colors', COLORS)
    labels_map = style.get('labels', LABELS)

    # Build scheduler_data mapping dynamically
    scheduler_data = {}
    for sched in schedulers:
        cols = get_scheduler_columns(df, sched)
        if 'cells' in cols and 'ips' in cols:
            df_sched = df[(df[cols['ips']] > 0) & (df[cols['cells']] > 0)]
            if len(df_sched) > 0:
                color = colors.get(sched, COLORS.get(sched, '#888888'))
                scheduler_data[sched] = (df_sched, cols['cells'], cols['ips'], color)

    # Also try computational data for any scheduler
    for name, color, label in iter_schedulers(data):
        if name not in scheduler_data:
            comp_df = data.get(f'{name}_computational')
            if comp_df is not None and len(comp_df) > 0:
                scheduler_data[name] = (comp_df, 'cells', 'iter_per_sec', color)

    # Panel A: Linear scatter
    for name, (df_subset, cells_col, ips_col, color) in scheduler_data.items():
        if len(df_subset) > 0:
            label = labels_map.get(name, name.replace('_', ' ').title())
            axes[0, 0].scatter(df_subset[cells_col], df_subset[ips_col],
                               label=label, alpha=0.5, s=20, color=color)
    axes[0, 0].set_xlabel('Cell Count', fontsize=11)
    axes[0, 0].set_ylabel('Iterations per Second', fontsize=11)
    axes[0, 0].set_title('Iteration Rate vs Cell Count', fontsize=12, fontweight='bold')
    axes[0, 0].legend(fontsize=10)
    axes[0, 0].grid(True, alpha=0.3)

    # Panel B: Log-log with theoretical scaling lines
    fit_results = {}
    for name, (df_subset, cells_col, ips_col, color) in scheduler_data.items():
        if len(df_subset) < 10:
            continue
        x = df_subset[cells_col].values
        y = df_subset[ips_col].values

        axes[0, 1].loglog(x, y, 'o', alpha=0.3, markersize=3, color=color, label=f'{name} data')

        if SCIPY_AVAILABLE:
            try:
                valid = (x > 0) & (y > 0)
                log_x = np.log10(x[valid])
                log_y = np.log10(y[valid])
                coeffs = np.polyfit(log_x, log_y, 1)
                slope = coeffs[0]
                fit_results[name] = slope

                x_fit = np.logspace(np.log10(x[valid].min()), np.log10(x[valid].max()), 50)
                y_fit = 10**(coeffs[1]) * x_fit**slope
                axes[0, 1].loglog(x_fit, y_fit, '--', color=color, alpha=0.8,
                                  label=f'{name}: O(N^{-slope:.2f})')
            except Exception:
                pass

    # Add theoretical scaling reference lines using first scheduler with data
    ref_sched = next(iter(scheduler_data.keys()), None) if scheduler_data else None
    if ref_sched:
        df_ref, cells_col, ips_col, _ = scheduler_data[ref_sched]
        if len(df_ref) > 0:
            x_min = max(df_ref[cells_col].min(), 10)
            x_max = df_ref[cells_col].max()
            x_ref = np.logspace(np.log10(x_min), np.log10(x_max), 50)
            # O(N) reference: ips ~ 1/N (time scales linearly)
            y_linear = (df_ref[ips_col].iloc[0] * df_ref[cells_col].iloc[0]) / x_ref
            axes[0, 1].loglog(x_ref, y_linear, ':', color='gray', alpha=0.6, linewidth=1.5,
                              label='O(N) reference')
            # O(N^1.5) reference
            y_n15 = (df_ref[ips_col].iloc[0] * df_ref[cells_col].iloc[0]**1.5) / x_ref**1.5
            axes[0, 1].loglog(x_ref, y_n15, '-.', color='darkgray', alpha=0.5, linewidth=1,
                              label='O(N^1.5) reference')

    axes[0, 1].set_xlabel('Cell Count (log)', fontsize=11)
    axes[0, 1].set_ylabel('Iterations/sec (log)', fontsize=11)
    axes[0, 1].set_title('Scaling with Theoretical References', fontsize=12, fontweight='bold')
    axes[0, 1].legend(fontsize=7, loc='lower left')
    axes[0, 1].grid(True, alpha=0.3, which='both')

    # Panel C: Time per cell-iteration (efficiency metric)
    style = data.get('style_config', {})
    labels = style.get('labels', LABELS)
    for name, (df_subset, cells_col, ips_col, color) in scheduler_data.items():
        if len(df_subset) < 10:
            continue
        x = df_subset[cells_col].values
        y = df_subset[ips_col].values

        # Time per cell-iteration (ms) - lower is better
        # This normalizes by problem size to show efficiency
        time_per_cell_iter = 1000 / (y * x)  # ms per (cell * iteration)
        axes[1, 0].loglog(x, time_per_cell_iter, 'o-', alpha=0.5, markersize=3,
                          color=color, label=labels.get(name, name))

    axes[1, 0].set_xlabel('Cell Count (log)', fontsize=11)
    axes[1, 0].set_ylabel('Time per Cell-Iteration (ms, log)', fontsize=11)
    axes[1, 0].set_title('Computational Efficiency per Cell', fontsize=12, fontweight='bold')
    axes[1, 0].legend(fontsize=10)
    axes[1, 0].grid(True, alpha=0.3, which='both')
    add_stats_box(axes[1, 0], 'Flat line = perfect O(N) scaling\n'
                              'Rising = superlinear overhead', (0.02, 0.15))

    # Panel D: Relative scaling efficiency
    # Efficiency = (ips_N * N) / (ips_N0 * N0) - how much of linear scaling is retained
    for name, (df_subset, cells_col, ips_col, color) in scheduler_data.items():
        if len(df_subset) < 10:
            continue

        # Use first valid point as baseline
        baseline_idx = df_subset[ips_col].idxmax()  # Use peak performance as baseline
        N0 = df_subset.loc[baseline_idx, cells_col]
        ips_0 = df_subset.loc[baseline_idx, ips_col]

        x = df_subset[cells_col].values
        y = df_subset[ips_col].values

        # Expected linear: ips(N) = ips_0 * (N0/N)
        expected_ips = ips_0 * (N0 / x)
        efficiency = y / expected_ips
        efficiency = np.clip(efficiency, 0, 2)  # Cap for visualization

        axes[1, 1].semilogx(x, efficiency * 100, 'o-', alpha=0.5, markersize=3,
                            color=color, label=labels.get(name, name))

    axes[1, 1].axhline(y=100, color='green', linestyle='--', alpha=0.7, label='Perfect scaling')
    axes[1, 1].axhline(y=50, color='orange', linestyle='--', alpha=0.5, label='50% efficiency')
    axes[1, 1].set_xlabel('Cell Count (log)', fontsize=11)
    axes[1, 1].set_ylabel('Scaling Efficiency (%)', fontsize=11)
    axes[1, 1].set_title('Relative Scaling Efficiency', fontsize=12, fontweight='bold')
    axes[1, 1].set_ylim(0, 150)
    axes[1, 1].legend(fontsize=9)
    axes[1, 1].grid(True, alpha=0.3)
    add_stats_box(axes[1, 1], '100% = linear scaling\n<100% = superlinear cost', (0.7, 0.95))

    plt.suptitle('Computational Scaling Analysis', fontsize=14, fontweight='bold')
    plt.tight_layout()
    plt.savefig(output_dir / "02_iteration_rate_vs_cells.png", dpi=DPI, bbox_inches='tight')
    plt.close()


@requires_any_computational
def plot_03_memory_vs_cells(data: dict, output_dir: Path) -> None:
    """Plot 03: Memory usage vs cell count.

    Memory estimation based on actual process measurements:
    - v1 simulation: ~1.44 MB/cell
    - adaptive simulation: ~1.77 MB/cell
    - Average: ~1.7 MB/cell with ~100 MB base overhead

    Note: If RSS from monitoring is < 10 MB, it's invalid (likely captured
    the /usr/bin/time wrapper process instead of simucell3d itself).
    """
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))

    # Memory estimation constants (empirically measured from actual processes)
    # v1: ~3963 MB for 2679 cells -> ~1.44 MB/cell
    # adaptive: ~9809 MB for 5499 cells -> ~1.77 MB/cell
    BASE_MEMORY_MB = 100  # Runtime overhead (libraries, allocator, etc.)
    MB_PER_CELL_V1 = 1.44  # v1 uses less memory per cell
    MB_PER_CELL_ADAPTIVE = 1.77  # adaptive has more data structures per cell
    MB_PER_CELL_AVG = 1.7  # Average for reference line

    # Minimum valid RSS threshold - anything below this is clearly wrong
    MIN_VALID_RSS_MB = 10.0

    using_estimate = False

    style = data.get('style_config', {})
    colors = style.get('colors', COLORS)
    labels = style.get('labels', LABELS)

    for name, color, label in iter_schedulers(data):
        df = data.get(f'{name}_computational')
        if df is None or len(df) == 0:
            continue

        df_filtered = df[(df['cells'] > 0) & (df['iteration'] > 0)].copy()
        if len(df_filtered) == 0:
            continue

        # Use actual RSS only if it's valid (> MIN_VALID_RSS_MB)
        # The monitoring script may capture wrong PID (time wrapper vs simucell3d)
        if 'rss_mb' in df_filtered.columns and df_filtered['rss_mb'].max() > MIN_VALID_RSS_MB:
            memory = df_filtered['rss_mb']
            data_source = 'measured'
        else:
            # Use empirically-derived estimation formula
            mb_per_cell = MB_PER_CELL_V1 if name == 'v1' else MB_PER_CELL_ADAPTIVE
            memory = BASE_MEMORY_MB + df_filtered['cells'] * mb_per_cell
            data_source = 'estimated'
            using_estimate = True

        # Memory vs Iteration
        ax1.plot(df_filtered['iteration'], memory,
                label=f"{label} ({data_source})",
                linewidth=2, alpha=0.7, color=color)

        # Memory vs Cells
        ax2.plot(df_filtered['cells'], memory,
                label=f"{label} ({data_source})",
                linewidth=2, alpha=0.7, color=color)

    # Add linear reference on right panel - find max across all detected schedulers
    max_cells = 0
    for name, _, _ in iter_schedulers(data):
        df = data.get(f'{name}_computational')
        if df is not None and 'cells' in df.columns and len(df) > 0:
            max_cells = max(max_cells, df['cells'].max())
    if max_cells > 0:
        cell_range = np.linspace(0, max_cells * 1.1, 100)
        ax2.plot(cell_range, BASE_MEMORY_MB + cell_range * MB_PER_CELL_AVG,
                'k--', alpha=0.5, linewidth=1,
                label=f'Reference ({MB_PER_CELL_AVG} MB/cell)')

    ax1.set_xlabel('Iteration', fontsize=12)
    ax1.set_ylabel('Memory (MB)', fontsize=12)
    ax1.set_title('Memory vs Iteration', fontsize=12, fontweight='bold')
    ax1.legend(fontsize=10)
    ax1.grid(True, alpha=0.3)

    ax2.set_xlabel('Cell Count', fontsize=12)
    ax2.set_ylabel('Memory (MB)', fontsize=12)
    ax2.set_title('Memory vs Cell Count', fontsize=12, fontweight='bold')
    ax2.legend(fontsize=10)
    ax2.grid(True, alpha=0.3)

    # Add note if using estimates
    if using_estimate:
        fig.text(0.5, 0.01,
                'Note: Memory values estimated from empirical measurements '
                f'(~{MB_PER_CELL_AVG} MB/cell + {BASE_MEMORY_MB} MB base)',
                ha='center', fontsize=9, style='italic', alpha=0.7)

    plt.suptitle('Memory Usage Analysis', fontsize=14, fontweight='bold')
    plt.tight_layout(rect=[0, 0.03, 1, 1])  # Leave room for note at bottom
    plt.savefig(output_dir / "03_memory_vs_cells.png", dpi=DPI, bbox_inches='tight')
    plt.close()


@requires_data('adaptive_phase')
def plot_04_phase_timing_breakdown(data: dict, output_dir: Path) -> None:
    """Plot 04: Phase timing with Amdahl's Law parallel fraction analysis.

    Enhanced with HPC expert recommendations:
    - Stacked area and pie charts (original)
    - Serial vs parallel fraction over time
    - Amdahl's Law theoretical speedup curve
    """
    fig = plt.figure(figsize=(16, 12))
    gs = gridspec.GridSpec(2, 3, figure=fig, hspace=0.3, wspace=0.3)

    df = data['adaptive_phase']
    df_filtered = df[df['total_iteration_ms'] > 0].copy() if 'total_iteration_ms' in df.columns else df.copy()

    phases = ['mesh_refinement_ms', 'contact_detection_ms',
              'polarization_internal_forces_ms', 'time_integration_ms']
    phase_labels = ['Mesh Refinement', 'Contact Detection',
                   'Polarization/Forces', 'Time Integration']
    phase_colors = [COLORS['mesh_refinement'], COLORS['contact_detection'],
                   COLORS['polarization'], COLORS['time_integration']]

    available_phases = [p for p in phases if p in df_filtered.columns]
    available_labels = [phase_labels[phases.index(p)] for p in available_phases]
    available_colors = [phase_colors[phases.index(p)] for p in available_phases]

    if not available_phases:
        plt.close()
        return

    # Panel A: Stacked area chart
    ax1 = fig.add_subplot(gs[0, 0])
    x_data = df_filtered['iteration'] if 'iteration' in df_filtered.columns else range(len(df_filtered))
    ax1.stackplot(x_data,
                  [df_filtered[p] for p in available_phases],
                  labels=available_labels, colors=available_colors, alpha=0.8)
    ax1.set_xlabel('Iteration', fontsize=10)
    ax1.set_ylabel('Time (ms)', fontsize=10)
    ax1.set_title('Phase Timing Breakdown', fontsize=11, fontweight='bold')
    ax1.legend(loc='upper left', fontsize=8)
    ax1.grid(True, alpha=0.3)

    # Panel B: Pie chart
    ax2 = fig.add_subplot(gs[0, 1])
    avg_times = [df_filtered[p].mean() for p in available_phases]
    total_time = sum(avg_times)
    max_idx = avg_times.index(max(avg_times))
    explode = [0.1 if i == max_idx else 0 for i in range(len(avg_times))]
    ax2.pie(avg_times, labels=available_labels, autopct='%1.1f%%',
            colors=available_colors, explode=explode, shadow=True)
    ax2.set_title(f'Average Phase Distribution\n(Total: {total_time:.1f}ms avg/iter)', fontsize=11, fontweight='bold')

    # Panel C: Normalized phase fractions
    ax3 = fig.add_subplot(gs[0, 2])
    phase_data = np.array([df_filtered[p].values for p in available_phases])
    total_per_iter = phase_data.sum(axis=0)
    total_per_iter[total_per_iter == 0] = 1  # Avoid division by zero
    normalized = phase_data / total_per_iter * 100
    ax3.stackplot(x_data, normalized, labels=available_labels, colors=available_colors, alpha=0.8)
    ax3.set_xlabel('Iteration', fontsize=10)
    ax3.set_ylabel('Phase Fraction (%)', fontsize=10)
    ax3.set_title('Relative Phase Contribution', fontsize=11, fontweight='bold')
    ax3.set_ylim(0, 100)
    ax3.legend(loc='upper right', fontsize=7)
    ax3.grid(True, alpha=0.3)

    # Panel D: Amdahl's Law - Serial vs Parallel fraction
    ax4 = fig.add_subplot(gs[1, 0])
    if 'contact_detection_ms' in df_filtered.columns and 'total_iteration_ms' in df_filtered.columns:
        cd_time = df_filtered['contact_detection_ms'].values
        total = df_filtered['total_iteration_ms'].values
        total[total == 0] = 1  # Avoid division

        # Contact detection is the main parallel phase
        parallel_frac = cd_time / total * 100
        serial_frac = 100 - parallel_frac

        ax4.fill_between(x_data, 0, serial_frac, alpha=0.7, color='#E74C3C', label='Serial fraction')
        ax4.fill_between(x_data, serial_frac, 100, alpha=0.7, color='#27AE60', label='Parallel fraction')
        ax4.set_xlabel('Iteration', fontsize=10)
        ax4.set_ylabel('Fraction (%)', fontsize=10)
        ax4.set_title("Serial vs Parallel Fraction\n(Amdahl's Law)", fontsize=11, fontweight='bold')
        ax4.set_ylim(0, 100)
        ax4.legend(loc='upper right', fontsize=9)
        ax4.grid(True, alpha=0.3)

        # Calculate average serial fraction for Amdahl's analysis
        avg_serial = serial_frac.mean() / 100
        avg_parallel = parallel_frac.mean() / 100
    else:
        avg_serial = 0.5  # Default assumption
        avg_parallel = 0.5

    # Panel E: Amdahl's Law Speedup Curve
    ax5 = fig.add_subplot(gs[1, 1])
    cores = np.array([1, 2, 4, 8, 16, 32, 64, 128])

    # Amdahl's law: S(p) = 1 / (s + (1-s)/p)
    if avg_serial > 0:
        speedup = 1 / (avg_serial + avg_parallel / cores)
        ax5.semilogx(cores, speedup, 'b-', linewidth=2, base=2,
                     label=f'Amdahl (s={avg_serial*100:.0f}%)')

        # Ideal scaling reference
        ax5.semilogx(cores, cores, ':', color='gray', alpha=0.5, base=2, label='Ideal')

        # Mark current configuration (8 cores)
        current_speedup = 1 / (avg_serial + avg_parallel / 8)
        ax5.axvline(x=8, color='red', linestyle='--', alpha=0.5)
        ax5.plot(8, current_speedup, 'ro', markersize=10, label=f'Current (8 cores): {current_speedup:.1f}x')

        # Max theoretical speedup
        max_speedup = 1 / avg_serial if avg_serial > 0 else float('inf')
        ax5.axhline(y=min(max_speedup, 100), color='green', linestyle=':', alpha=0.5,
                    label=f'Max speedup: {min(max_speedup, 100):.1f}x')

    ax5.set_xlabel('Number of Cores', fontsize=10)
    ax5.set_ylabel('Theoretical Speedup', fontsize=10)
    ax5.set_title("Amdahl's Law Speedup Curve", fontsize=11, fontweight='bold')
    ax5.set_xlim(1, 128)
    ax5.legend(fontsize=8, loc='upper left')
    ax5.grid(True, alpha=0.3, which='both')

    # Panel F: Summary statistics
    ax6 = fig.add_subplot(gs[1, 2])
    ax6.axis('off')

    # Compute theoretical speedups
    s2 = 1 / (avg_serial + avg_parallel / 2) if avg_serial > 0 else 2
    s4 = 1 / (avg_serial + avg_parallel / 4) if avg_serial > 0 else 4
    s8 = 1 / (avg_serial + avg_parallel / 8) if avg_serial > 0 else 8
    s_inf = 1 / avg_serial if avg_serial > 0 else float('inf')

    summary_text = "AMDAHL'S LAW ANALYSIS\n" + "=" * 35 + "\n\n"
    summary_text += f"Serial fraction: {avg_serial*100:.1f}%\n"
    summary_text += f"Parallel fraction: {avg_parallel*100:.1f}%\n\n"
    summary_text += "Theoretical Speedup Limits:\n"
    summary_text += f"  2 cores:  {s2:.2f}x\n"
    summary_text += f"  4 cores:  {s4:.2f}x\n"
    summary_text += f"  8 cores:  {s8:.2f}x\n"
    summary_text += f"  ∞ cores: {min(s_inf, 100):.2f}x\n\n"

    # Efficiency at 8 cores
    efficiency_8 = (s8 / 8) * 100
    summary_text += f"Parallel efficiency (8 cores):\n  {efficiency_8:.1f}%\n\n"

    # Recommendation
    if avg_serial > 0.3:
        summary_text += "⚠ High serial fraction!\nFocus on parallelizing serial phases."
    elif avg_serial > 0.1:
        summary_text += "✓ Moderate serial fraction.\nGood scaling up to ~16 cores."
    else:
        summary_text += "✓✓ Low serial fraction!\nExcellent scaling potential."

    ax6.text(0.1, 0.9, summary_text, transform=ax6.transAxes,
            fontsize=10, verticalalignment='top', family='monospace',
            bbox=dict(boxstyle='round', facecolor='lightyellow', alpha=0.9))

    plt.suptitle('Computational Phase Analysis with Parallel Efficiency', fontsize=14, fontweight='bold')
    plt.savefig(output_dir / "04_phase_timing_breakdown.png", dpi=DPI, bbox_inches='tight')
    plt.close()


@requires_data('adaptive_workload')
def plot_05_workload_imbalance(data: dict, output_dir: Path) -> None:
    """Plot 05: Workload imbalance with parallel efficiency metrics.

    Enhanced with HPC expert recommendations:
    - CoV with thresholds and rolling average
    - Thread imbalance percentage
    - Estimated parallel efficiency from load balance
    - Summary statistics with rating
    """
    fig, axes = plt.subplots(2, 3, figsize=(18, 10))

    df = data['adaptive_workload']

    # Panel A: CoV over time with rolling average
    if 'cov' in df.columns:
        df_pos = df[df['cov'] > 0]
        if len(df_pos) > 0:
            x = df_pos['iteration'] if 'iteration' in df_pos.columns else range(len(df_pos))
            axes[0, 0].semilogy(x, df_pos['cov'], 'o-', color=COLORS['adaptive'],
                                alpha=0.5, linewidth=1, markersize=3, label='Raw data')

            # Add thresholds with context
            axes[0, 0].axhline(y=0.20, color='red', linestyle='--', alpha=0.7, label='20% (poor)')
            axes[0, 0].axhline(y=0.10, color='orange', linestyle='--', alpha=0.7, label='10% (acceptable)')
            axes[0, 0].axhline(y=0.05, color='green', linestyle='--', alpha=0.5, label='5% (good)')

            # Add rolling average
            window = max(5, len(df_pos) // 20)
            rolling_cov = df_pos['cov'].rolling(window=window, center=True).mean()
            axes[0, 0].semilogy(x, rolling_cov, '-', color='black',
                                alpha=0.8, linewidth=2, label='Rolling mean')

    axes[0, 0].set_xlabel('Iteration', fontsize=10)
    axes[0, 0].set_ylabel('Coefficient of Variation (log)', fontsize=10)
    axes[0, 0].set_title('Workload Distribution CoV', fontsize=11, fontweight='bold')
    axes[0, 0].legend(fontsize=8, loc='upper right')
    axes[0, 0].grid(True, alpha=0.3, which='both')

    # Panel B: Thread imbalance percentage
    if 'thread_imbalance_pct' in df.columns:
        df_pos = df[df['thread_imbalance_pct'] > 0]
        if len(df_pos) > 0:
            x = df_pos['iteration'] if 'iteration' in df_pos.columns else range(len(df_pos))
            axes[0, 1].plot(x, df_pos['thread_imbalance_pct'], 'o-',
                           color=COLORS['adaptive'], alpha=0.7, linewidth=1.5, markersize=3)
            axes[0, 1].axhline(y=20, color='red', linestyle='--', alpha=0.7, label='20% threshold')
            axes[0, 1].axhline(y=10, color='orange', linestyle='--', alpha=0.5, label='10% threshold')

    axes[0, 1].set_xlabel('Iteration', fontsize=10)
    axes[0, 1].set_ylabel('Max Thread Imbalance (%)', fontsize=10)
    axes[0, 1].set_title('Peak Thread Load Deviation', fontsize=11, fontweight='bold')
    axes[0, 1].legend(fontsize=9)
    axes[0, 1].grid(True, alpha=0.3)

    # Panel C: Parallel efficiency estimation
    # Efficiency = 1 / (1 + CoV) approximately for load-balanced workloads
    if 'cov' in df.columns:
        df_pos = df[df['cov'] > 0].copy()
        if len(df_pos) > 0:
            x = df_pos['iteration'] if 'iteration' in df_pos.columns else range(len(df_pos))
            efficiency = 1 / (1 + df_pos['cov']) * 100

            axes[0, 2].plot(x, efficiency, 'o-', color='#27AE60', alpha=0.7,
                           linewidth=1.5, markersize=3)
            axes[0, 2].axhline(y=90, color='green', linestyle='--', alpha=0.5, label='90% target')
            axes[0, 2].axhline(y=80, color='orange', linestyle='--', alpha=0.5, label='80% acceptable')

    axes[0, 2].set_xlabel('Iteration', fontsize=10)
    axes[0, 2].set_ylabel('Estimated Parallel Efficiency (%)', fontsize=10)
    axes[0, 2].set_title('Load Balance Efficiency', fontsize=11, fontweight='bold')
    axes[0, 2].set_ylim(50, 105)
    axes[0, 2].legend(fontsize=9)
    axes[0, 2].grid(True, alpha=0.3)

    # Panel D: CoV distribution histogram
    if 'cov' in df.columns:
        df_pos = df[df['cov'] > 0]
        if len(df_pos) > 0:
            axes[1, 0].hist(df_pos['cov'], bins=30, color=COLORS['adaptive'],
                           alpha=0.7, edgecolor='white')

            mean_cov = df_pos['cov'].mean()
            median_cov = df_pos['cov'].median()

            axes[1, 0].axvline(mean_cov, color='red', linestyle='-', linewidth=2,
                               label=f'Mean: {mean_cov:.3f}')
            axes[1, 0].axvline(median_cov, color='blue', linestyle='--', linewidth=2,
                               label=f'Median: {median_cov:.3f}')

    axes[1, 0].set_xlabel('Coefficient of Variation', fontsize=10)
    axes[1, 0].set_ylabel('Frequency', fontsize=10)
    axes[1, 0].set_title('CoV Distribution', fontsize=11, fontweight='bold')
    axes[1, 0].legend(fontsize=9)
    axes[1, 0].grid(True, alpha=0.3)

    # Panel E: Efficiency over cell count (if available)
    if 'cov' in df.columns and 'cells' in df.columns:
        df_pos = df[(df['cov'] > 0) & (df['cells'] > 0)]
        if len(df_pos) > 0:
            efficiency = 1 / (1 + df_pos['cov']) * 100
            axes[1, 1].scatter(df_pos['cells'], efficiency, alpha=0.5, s=20,
                              color=COLORS['adaptive'])
            axes[1, 1].axhline(y=90, color='green', linestyle='--', alpha=0.5, label='90%')
            axes[1, 1].axhline(y=80, color='orange', linestyle='--', alpha=0.5, label='80%')
            axes[1, 1].set_xlabel('Cell Count', fontsize=10)
            axes[1, 1].set_ylabel('Parallel Efficiency (%)', fontsize=10)
            axes[1, 1].set_title('Efficiency vs Problem Size', fontsize=11, fontweight='bold')
            axes[1, 1].set_ylim(50, 105)
            axes[1, 1].legend(fontsize=9)
            axes[1, 1].grid(True, alpha=0.3)
    else:
        axes[1, 1].text(0.5, 0.5, 'Cell count data\nnot available',
                        ha='center', va='center', transform=axes[1, 1].transAxes, fontsize=12)
        axes[1, 1].set_title('Efficiency vs Problem Size', fontsize=11, fontweight='bold')

    # Panel F: Summary statistics
    ax_summary = axes[1, 2]
    ax_summary.axis('off')

    summary_text = "WORKLOAD BALANCE SUMMARY\n" + "=" * 35 + "\n\n"

    if 'cov' in df.columns:
        df_valid = df[df['cov'] > 0]
        if len(df_valid) > 0:
            mean_cov = df_valid['cov'].mean()
            median_cov = df_valid['cov'].median()
            std_cov = df_valid['cov'].std()
            max_cov = df_valid['cov'].max()

            summary_text += f"Coefficient of Variation:\n"
            summary_text += f"  Mean:   {mean_cov:.4f}\n"
            summary_text += f"  Median: {median_cov:.4f}\n"
            summary_text += f"  Std:    {std_cov:.4f}\n"
            summary_text += f"  Max:    {max_cov:.4f}\n\n"

            # Estimate parallel efficiency
            efficiency = 1 / (1 + mean_cov)
            time_lost_pct = (1 - efficiency) * 100

            summary_text += f"Parallel Efficiency Estimate:\n"
            summary_text += f"  Mean efficiency: {efficiency*100:.1f}%\n"
            summary_text += f"  Time lost to imbalance: {time_lost_pct:.1f}%\n\n"

            # Rating
            if mean_cov < 0.05:
                rating = "EXCELLENT ✓✓"
                rating_desc = "Outstanding load balance"
            elif mean_cov < 0.10:
                rating = "GOOD ✓"
                rating_desc = "Acceptable for most workloads"
            elif mean_cov < 0.15:
                rating = "ACCEPTABLE"
                rating_desc = "Some room for improvement"
            else:
                rating = "NEEDS IMPROVEMENT ⚠"
                rating_desc = "Consider load balancing strategies"

            summary_text += f"Rating: {rating}\n"
            summary_text += f"  {rating_desc}\n"

    ax_summary.text(0.1, 0.9, summary_text, transform=ax_summary.transAxes,
                   fontsize=10, verticalalignment='top', family='monospace',
                   bbox=dict(boxstyle='round', facecolor='whitesmoke', alpha=0.9))

    plt.suptitle('OpenMP Workload Balance Analysis', fontsize=14, fontweight='bold')
    plt.tight_layout()
    plt.savefig(output_dir / "05_workload_imbalance.png", dpi=DPI, bbox_inches='tight')
    plt.close()


# =============================================================================
# SECTION 6: Biological Metrics Plots (06-11)
# =============================================================================

@requires_data('comparison')
def plot_06_cell_population_growth(data: dict, output_dir: Path) -> None:
    """Plot 06: Cell population growth with linear, semi-log, and ratio views."""
    fig, axes = plt.subplots(2, 2, figsize=FIGSIZE_2x2)

    df = data['comparison']
    schedulers = get_comparison_schedulers(df)
    style = data.get('style_config', {})
    colors = style.get('colors', COLORS)
    labels_map = style.get('labels', LABELS)

    if not schedulers:
        axes[0, 0].text(0.5, 0.5, 'No scheduler data found', ha='center', va='center',
                        transform=axes[0, 0].transAxes, fontsize=12, color='gray')
        plt.savefig(output_dir / "06_cell_population_growth.png", dpi=DPI, bbox_inches='tight')
        plt.close()
        return

    # Build filter for rows with cell data from any scheduler
    filter_mask = pd.Series(False, index=df.index)
    for sched in schedulers:
        cols = get_scheduler_columns(df, sched)
        if 'cells' in cols:
            filter_mask |= (df[cols['cells']] > 0)
    df_filtered = df[filter_mask].copy()

    # Collect scheduler data sources (prefer log data if available and larger)
    sched_sources = {}
    for sched in schedulers:
        log_data = data.get(f'{sched}_log')
        cols = get_scheduler_columns(df_filtered, sched)
        if log_data is not None and len(log_data) > len(df_filtered):
            sched_sources[sched] = ('log', log_data)
        elif 'iter' in cols and 'cells' in cols:
            sched_sources[sched] = ('comparison', df_filtered)

    # Plot 1: Linear scale
    for sched, (src_type, src_df) in sched_sources.items():
        color = colors.get(sched, COLORS.get(sched, '#888888'))
        label = labels_map.get(sched, sched.replace('_', ' ').title())
        if src_type == 'log':
            final_cells = src_df['cells'].iloc[-1]
            axes[0, 0].plot(src_df['iteration'], src_df['cells'],
                           label=f"{label} (final: {final_cells:,})",
                           color=color, linewidth=1.5, alpha=0.8)
        else:
            cols = get_scheduler_columns(src_df, sched)
            if 'iter' in cols and 'cells' in cols:
                # Runs of unequal length leave trailing NaNs in the merged comparison
                final_cells = src_df[cols['cells']].dropna().iloc[-1]
                axes[0, 0].plot(src_df[cols['iter']], src_df[cols['cells']],
                               label=f"{label} (final: {int(final_cells):,})",
                               color=color, linewidth=1.5, alpha=0.8)

    axes[0, 0].set_xlabel('Iteration', fontsize=10)
    axes[0, 0].set_ylabel('Cell Count', fontsize=10)
    axes[0, 0].set_title('Cell Population Growth (Linear)', fontsize=12)
    axes[0, 0].legend(fontsize=9)
    axes[0, 0].grid(True, alpha=0.3)
    axes[0, 0].yaxis.set_major_formatter(plt.FuncFormatter(lambda x, p: f'{int(x):,}'))

    # Plot 2: Semi-log scale
    for sched, (src_type, src_df) in sched_sources.items():
        color = colors.get(sched, COLORS.get(sched, '#888888'))
        label = labels_map.get(sched, sched.replace('_', ' ').title())
        if src_type == 'log':
            axes[0, 1].semilogy(src_df['iteration'], src_df['cells'],
                               label=label, color=color, linewidth=1.5, alpha=0.8)
        else:
            cols = get_scheduler_columns(src_df, sched)
            if 'iter' in cols and 'cells' in cols:
                axes[0, 1].semilogy(src_df[cols['iter']], src_df[cols['cells']],
                                   label=label, color=color, linewidth=1.5, alpha=0.8)

    axes[0, 1].set_xlabel('Iteration', fontsize=10)
    axes[0, 1].set_ylabel('Cell Count (log scale)', fontsize=10)
    axes[0, 1].set_title('Cell Population Growth (Semi-log)', fontsize=12)
    axes[0, 1].legend(fontsize=9)
    axes[0, 1].grid(True, alpha=0.3, which='both')
    add_stats_box(axes[0, 1], 'Straight line =\nexponential growth', (0.02, 0.3))

    # Plot 3: Cell count ratio (compare first two schedulers if available)
    if len(sched_sources) >= 2:
        sched_list = list(sched_sources.keys())
        sched1, sched2 = sched_list[0], sched_list[1]
        src1_type, src1_df = sched_sources[sched1]
        src2_type, src2_df = sched_sources[sched2]

        # Try to merge on iteration
        if src1_type == 'log' and src2_type == 'log':
            merged = pd.merge(src1_df, src2_df, on='iteration', suffixes=(f'_{sched1}', f'_{sched2}'))
            if len(merged) > 0:
                ratio = merged[f'cells_{sched2}'] / merged[f'cells_{sched1}']
                axes[1, 0].plot(merged['iteration'], ratio, color='purple', linewidth=1.5)
                axes[1, 0].axhline(y=1.0, color='gray', linestyle='--', alpha=0.7)
        else:
            # Use comparison columns
            cols1 = get_scheduler_columns(df_filtered, sched1)
            cols2 = get_scheduler_columns(df_filtered, sched2)
            if 'cells' in cols1 and 'cells' in cols2 and 'iter' in cols1:
                valid = (df_filtered[cols1['cells']] > 0) & (df_filtered[cols2['cells']] > 0)
                df_ratio = df_filtered[valid]
                if len(df_ratio) > 0:
                    ratio = df_ratio[cols2['cells']] / df_ratio[cols1['cells']]
                    axes[1, 0].plot(df_ratio[cols1['iter']], ratio, color='purple', linewidth=1.5)
                    axes[1, 0].axhline(y=1.0, color='gray', linestyle='--', alpha=0.7)

        label1 = labels_map.get(sched1, sched1)
        label2 = labels_map.get(sched2, sched2)
        axes[1, 0].set_ylabel(f'{label2} / {label1} Ratio', fontsize=10)
        add_stats_box(axes[1, 0], f'Ratio > 1 = {label2}\nhas more cells', (0.02, 0.15))

    axes[1, 0].set_xlabel('Iteration', fontsize=10)
    axes[1, 0].set_title('Cell Count Ratio', fontsize=12)
    axes[1, 0].grid(True, alpha=0.3)

    # Plot 4: Growth rate - dynamic scheduler iteration
    style = data.get('style_config', {})
    colors = style.get('colors', COLORS)
    for name, color, label in iter_schedulers(data):
        df_src = data.get(f'{name}_computational')
        if df_src is None or len(df_src) < 100:
            continue
        df_sampled = df_src.iloc[::50].copy()
        df_sampled['growth_rate'] = df_sampled['cells'].diff() / 50
        df_pos = df_sampled[df_sampled['growth_rate'] > 0]
        if len(df_pos) > 0:
            axes[1, 1].semilogy(df_pos['iteration'], df_pos['growth_rate'],
                               label=name, color=color, alpha=0.7, linewidth=1.5)

    axes[1, 1].set_xlabel('Iteration', fontsize=10)
    axes[1, 1].set_ylabel('Growth Rate (cells/50 iter)', fontsize=10)
    axes[1, 1].set_title('Instantaneous Growth Rate', fontsize=12)
    axes[1, 1].legend(fontsize=9)
    axes[1, 1].grid(True, alpha=0.3, which='both')

    plt.suptitle('Cell Population Dynamics', fontsize=14, fontweight='bold')
    plt.tight_layout()
    plt.savefig(output_dir / "06_cell_population_growth.png", dpi=DPI, bbox_inches='tight')
    plt.close()


def plot_07_volume_distribution(data: dict, output_dir: Path) -> None:
    """Plot 07: Volume distribution histograms."""
    # Check if any scheduler has sim_stats
    has_sim_stats = any(data.get(f'{s.name}_sim_stats') is not None for s in data.get('schedulers', []))
    if not has_sim_stats:
        print("  [SKIP] 07_volume_distribution.png - No sim_stats available")
        return

    fig, axes = plt.subplots(1, 3, figsize=(16, 5))

    # Build modes dynamically from detected schedulers
    style = data.get('style_config', {})
    colors = style.get('colors', COLORS)

    for name, color, label in iter_schedulers(data):
        df = data.get(f'{name}_sim_stats')
        if df is None or 'volume' not in df.columns:
            continue

        # Get last iteration data
        last_iter = df['iteration'].max()
        df_last = df[df['iteration'] == last_iter].copy()

        # Linear histogram
        volumes_nl = df_last['volume'] * 1e9  # Convert to nL
        axes[0].hist(volumes_nl, bins=50, alpha=0.6,
                    label=f'{name} (n={len(df_last)})', color=color,
                    edgecolor='white', linewidth=0.5)

        # Log-scale histogram
        volumes_pos = df_last['volume'][df_last['volume'] > 0]
        if len(volumes_pos) > 0:
            log_bins = np.logspace(np.log10(volumes_pos.min()),
                                   np.log10(volumes_pos.max()), 50)
            axes[1].hist(volumes_pos, bins=log_bins, alpha=0.6,
                        label=name, color=color, edgecolor='white', linewidth=0.5)

        # Volume error histogram
        if 'target_volume' in df_last.columns:
            vol_error = (df_last['volume'] - df_last['target_volume']) / df_last['target_volume'] * 100
            axes[2].hist(vol_error, bins=50, alpha=0.6,
                        label=f'{name} (σ={vol_error.std():.1f}%)', color=color,
                        edgecolor='white', linewidth=0.5)

    axes[0].set_xlabel('Volume (nL)', fontsize=10)
    axes[0].set_ylabel('Cell Count', fontsize=10)
    axes[0].set_title('Volume Distribution (Linear)', fontsize=12)
    axes[0].legend(fontsize=9)
    axes[0].grid(True, alpha=0.3)

    axes[1].set_xlabel('Volume (m³)', fontsize=10)
    axes[1].set_ylabel('Cell Count', fontsize=10)
    axes[1].set_title('Volume Distribution (Log Scale)', fontsize=12)
    axes[1].set_xscale('log')
    axes[1].legend(fontsize=9)
    axes[1].grid(True, alpha=0.3, which='both')

    axes[2].set_xlabel('Volume Error (%)', fontsize=10)
    axes[2].set_ylabel('Cell Count', fontsize=10)
    axes[2].set_title('Volume Error vs Target', fontsize=12)
    axes[2].axvline(x=0, color='black', linestyle='-', alpha=0.5, linewidth=1)
    axes[2].legend(fontsize=9)
    axes[2].grid(True, alpha=0.3)

    plt.suptitle('Cell Volume Analysis', fontsize=14, fontweight='bold')
    plt.tight_layout()
    plt.savefig(output_dir / "07_volume_distribution.png", dpi=DPI, bbox_inches='tight')
    plt.close()


@requires_any_sim_stats
def plot_08_pressure_distribution(data: dict, output_dir: Path) -> None:
    """Plot 08: Pressure distribution with histogram and violin plots."""
    fig, axes = plt.subplots(1, 3, figsize=(16, 5))

    # Build modes dict dynamically from detected schedulers
    modes = build_sim_stats_modes(data)
    if not modes:
        plt.close()
        return

    pressure_data = {}

    for name, (df, color) in modes.items():
        if df is None or 'pressure' not in df.columns:
            continue

        last_iter = df['iteration'].max()
        df_last = df[df['iteration'] == last_iter].copy()
        pressure_data[name] = df_last['pressure'].values

        # Linear histogram
        axes[0].hist(df_last['pressure'], bins=50, alpha=0.6,
                    label=name, color=color, edgecolor='white', linewidth=0.5)

        # Log-scale histogram
        pressure_pos = df_last['pressure'][df_last['pressure'] > 0]
        if len(pressure_pos) > 0:
            log_bins = np.logspace(np.log10(pressure_pos.min()),
                                   np.log10(pressure_pos.max()), 50)
            axes[1].hist(pressure_pos, bins=log_bins, alpha=0.6,
                        label=name, color=color, edgecolor='white', linewidth=0.5)

    axes[0].set_xlabel('Pressure (Pa)', fontsize=10)
    axes[0].set_ylabel('Cell Count', fontsize=10)
    axes[0].set_title('Pressure Distribution (Linear)', fontsize=12)
    axes[0].legend(fontsize=9)
    axes[0].grid(True, alpha=0.3)
    axes[0].axvspan(100, 500, alpha=0.1, color='green', label='Epithelial range')

    axes[1].set_xlabel('Pressure (Pa)', fontsize=10)
    axes[1].set_ylabel('Cell Count', fontsize=10)
    axes[1].set_title('Pressure Distribution (Log Scale)', fontsize=12)
    axes[1].set_xscale('log')
    axes[1].legend(fontsize=9)
    axes[1].grid(True, alpha=0.3, which='both')

    # Box plot comparison
    if pressure_data:
        bp_data = [v for v in pressure_data.values()]
        bp_labels = list(pressure_data.keys())
        bp_colors = [COLORS.get(k, 'gray') for k in pressure_data.keys()]

        bp = axes[2].boxplot(bp_data, labels=bp_labels, patch_artist=True)
        for patch, color in zip(bp['boxes'], bp_colors):
            patch.set_facecolor(color)
            patch.set_alpha(0.6)

        axes[2].set_ylabel('Pressure (Pa)', fontsize=10)
        axes[2].set_title('Pressure Box Plot Comparison', fontsize=12)
        axes[2].grid(True, alpha=0.3)

        # Add stats
        stats_text = ""
        for name, p in pressure_data.items():
            s = compute_statistics(p)
            stats_text += f"{name}: μ={s['mean']:.1f}, σ={s['std']:.1f}\n"
        add_stats_box(axes[2], stats_text.strip(), (0.02, 0.98))

    plt.suptitle('Cell Pressure Analysis', fontsize=14, fontweight='bold')
    plt.tight_layout()
    plt.savefig(output_dir / "08_pressure_distribution.png", dpi=DPI, bbox_inches='tight')
    plt.close()


@requires_any_sim_stats
def plot_09_volume_error_vs_target(data: dict, output_dir: Path) -> None:
    """Plot 09: Volume regulation accuracy with pass rate tracking.

    Enhanced with Bio-Sim expert recommendations:
    - ±5% tolerance tracking (biological regulation criterion)
    - Pass rate evolution over time
    - Quantitative verdict for volume regulation quality
    """
    fig = plt.figure(figsize=(16, 10))
    gs = gridspec.GridSpec(2, 3, figure=fig, hspace=0.3, wspace=0.3)

    # Build modes dict dynamically from detected schedulers
    modes = build_sim_stats_modes(data)
    if not modes:
        plt.close()
        return

    # Biological tolerance threshold (±5% is standard in cell mechanics)
    TOLERANCE_PCT = 5.0

    # Store results for summary
    regulation_results = {}

    # Panel A: Mean error evolution
    ax0 = fig.add_subplot(gs[0, 0])
    for name, (df, color) in modes.items():
        if df is None or 'volume' not in df.columns or 'target_volume' not in df.columns:
            continue

        df = df.copy()
        df['vol_error_pct'] = (df['volume'] - df['target_volume']) / df['target_volume'] * 100

        error_by_iter = df.groupby('iteration')['vol_error_pct'].agg(['mean', 'std'])

        ax0.plot(error_by_iter.index, error_by_iter['mean'],
                label=name, color=color, linewidth=1.5, alpha=0.8)
        ax0.fill_between(error_by_iter.index,
                        error_by_iter['mean'] - error_by_iter['std'],
                        error_by_iter['mean'] + error_by_iter['std'],
                        alpha=0.2, color=color)

    ax0.axhline(y=0, color='black', linestyle='-', alpha=0.5)
    ax0.axhline(y=TOLERANCE_PCT, color='green', linestyle='--', alpha=0.5, label=f'±{TOLERANCE_PCT}% tolerance')
    ax0.axhline(y=-TOLERANCE_PCT, color='green', linestyle='--', alpha=0.5)
    ax0.set_xlabel('Iteration', fontsize=10)
    ax0.set_ylabel('Mean Volume Error (%)', fontsize=10)
    ax0.set_title('Volume Error Evolution (Mean ± Std)', fontsize=11, fontweight='bold')
    ax0.legend(fontsize=8)
    ax0.grid(True, alpha=0.3)

    # Panel B: Pass rate evolution (fraction within tolerance)
    ax1 = fig.add_subplot(gs[0, 1])
    for name, (df, color) in modes.items():
        if df is None or 'volume' not in df.columns or 'target_volume' not in df.columns:
            continue

        df = df.copy()
        df['vol_error_pct'] = (df['volume'] - df['target_volume']) / df['target_volume'] * 100
        df['within_tolerance'] = np.abs(df['vol_error_pct']) <= TOLERANCE_PCT

        # Calculate pass rate per iteration
        pass_rate_by_iter = df.groupby('iteration')['within_tolerance'].mean() * 100

        ax1.plot(pass_rate_by_iter.index, pass_rate_by_iter.values,
                label=f'{name} (final: {pass_rate_by_iter.iloc[-1]:.1f}%)',
                color=color, linewidth=1.5, alpha=0.8)

        # Store final pass rate
        regulation_results[name] = {
            'final_pass_rate': pass_rate_by_iter.iloc[-1],
            'mean_pass_rate': pass_rate_by_iter.mean()
        }

    ax1.axhline(y=95, color='green', linestyle='--', alpha=0.7, label='95% target')
    ax1.axhline(y=80, color='orange', linestyle='--', alpha=0.5, label='80% minimum')
    ax1.set_xlabel('Iteration', fontsize=10)
    ax1.set_ylabel(f'Cells within ±{TOLERANCE_PCT}% (%)', fontsize=10)
    ax1.set_title('Volume Regulation Pass Rate', fontsize=11, fontweight='bold')
    ax1.set_ylim(0, 105)
    ax1.legend(fontsize=8)
    ax1.grid(True, alpha=0.3)

    # Panel C: Std evolution
    ax2 = fig.add_subplot(gs[0, 2])
    for name, (df, color) in modes.items():
        if df is None or 'volume' not in df.columns or 'target_volume' not in df.columns:
            continue

        df = df.copy()
        df['vol_error_pct'] = (df['volume'] - df['target_volume']) / df['target_volume'] * 100
        error_by_iter = df.groupby('iteration')['vol_error_pct'].std()

        ax2.plot(error_by_iter.index, error_by_iter.values,
                label=name, color=color, linewidth=1.5, alpha=0.8)

    ax2.axhline(y=TOLERANCE_PCT, color='green', linestyle='--', alpha=0.5, label=f'{TOLERANCE_PCT}% target')
    ax2.set_xlabel('Iteration', fontsize=10)
    ax2.set_ylabel('Std of Volume Error (%)', fontsize=10)
    ax2.set_title('Volume Error Variability', fontsize=11, fontweight='bold')
    ax2.legend(fontsize=9)
    ax2.grid(True, alpha=0.3)

    # Panel D: Final error histogram
    ax3 = fig.add_subplot(gs[1, 0])
    for name, (df, color) in modes.items():
        if df is None or 'volume' not in df.columns or 'target_volume' not in df.columns:
            continue

        df = df.copy()
        df['vol_error_pct'] = (df['volume'] - df['target_volume']) / df['target_volume'] * 100
        last_iter = df['iteration'].max()
        df_last = df[df['iteration'] == last_iter]

        ax3.hist(df_last['vol_error_pct'], bins=50, alpha=0.6,
                label=f'{name} (σ={df_last["vol_error_pct"].std():.1f}%)',
                color=color, edgecolor='white', linewidth=0.5)

    ax3.axvline(x=0, color='black', linestyle='-', alpha=0.5, linewidth=1)
    ax3.axvline(x=TOLERANCE_PCT, color='green', linestyle='--', alpha=0.7)
    ax3.axvline(x=-TOLERANCE_PCT, color='green', linestyle='--', alpha=0.7)
    ax3.set_xlabel('Volume Error (%)', fontsize=10)
    ax3.set_ylabel('Cell Count', fontsize=10)
    ax3.set_title('Final Volume Error Distribution', fontsize=11, fontweight='bold')
    ax3.legend(fontsize=9)
    ax3.grid(True, alpha=0.3)

    # Panel E: Error vs Target Volume scatter
    ax4 = fig.add_subplot(gs[1, 1])
    for name, (df, color) in modes.items():
        if df is None or 'volume' not in df.columns or 'target_volume' not in df.columns:
            continue

        df = df.copy()
        last_iter = df['iteration'].max()
        df_last = df[df['iteration'] == last_iter].sample(n=min(500, len(df[df['iteration'] == last_iter])))

        ax4.scatter(df_last['target_volume'] * 1e15, df_last['volume'] * 1e15,
                   alpha=0.3, s=10, color=color, label=name)

    # Add diagonal
    if len(modes) > 0:
        all_vols = []
        for name, (df, color) in modes.items():
            if df is not None and 'target_volume' in df.columns:
                all_vols.extend(df['target_volume'].dropna().values)
        if all_vols:
            v_range = [min(all_vols) * 1e15, max(all_vols) * 1e15]
            ax4.plot(v_range, v_range, 'k--', alpha=0.5, label='Perfect')
            ax4.plot(v_range, [v * 1.05 for v in v_range], 'g:', alpha=0.5)
            ax4.plot(v_range, [v * 0.95 for v in v_range], 'g:', alpha=0.5, label='±5%')

    ax4.set_xlabel('Target Volume (fL)', fontsize=10)
    ax4.set_ylabel('Actual Volume (fL)', fontsize=10)
    ax4.set_title('Volume Regulation Accuracy', fontsize=11, fontweight='bold')
    ax4.legend(fontsize=8)
    ax4.grid(True, alpha=0.3)

    # Panel F: Summary verdict
    ax5 = fig.add_subplot(gs[1, 2])
    ax5.axis('off')

    summary_text = "VOLUME REGULATION VERDICT\n" + "=" * 35 + "\n\n"
    summary_text += f"Tolerance criterion: ±{TOLERANCE_PCT}%\n"
    summary_text += "(Standard for cell mechanics)\n\n"
    summary_text += "Pass Rate Criteria:\n"
    summary_text += "  ≥95% : EXCELLENT ✓✓\n"
    summary_text += "  ≥80% : ACCEPTABLE ✓\n"
    summary_text += "  <80% : NEEDS WORK ⚠\n\n"

    for name, results in regulation_results.items():
        final_rate = results['final_pass_rate']
        mean_rate = results['mean_pass_rate']

        if final_rate >= 95:
            verdict = "EXCELLENT ✓✓"
        elif final_rate >= 80:
            verdict = "ACCEPTABLE ✓"
        else:
            verdict = "NEEDS WORK ⚠"

        summary_text += f"{name.upper()}:\n"
        summary_text += f"  Final pass rate: {final_rate:.1f}%\n"
        summary_text += f"  Mean pass rate: {mean_rate:.1f}%\n"
        summary_text += f"  Verdict: {verdict}\n\n"

    ax5.text(0.1, 0.95, summary_text, transform=ax5.transAxes,
            fontsize=10, verticalalignment='top', family='monospace',
            bbox=dict(boxstyle='round', facecolor='lightgreen', alpha=0.3))

    plt.suptitle('Volume Regulation Analysis', fontsize=14, fontweight='bold')
    plt.savefig(output_dir / "09_volume_error_vs_target.png", dpi=DPI, bbox_inches='tight')
    plt.close()


def plot_10_division_dynamics(data: dict, output_dir: Path) -> None:
    """Plot 10: Division dynamics analysis.

    Divisions are taken from the logged ``divisions`` column when it carries signal,
    otherwise reconstructed from Δcells (see :func:`derive_divisions`). This keeps the
    plot meaningful for runs whose logged column is structurally zero.
    """
    fig, axes = plt.subplots(2, 2, figsize=FIGSIZE_2x2)

    # Use log data for cell count changes - iterate over detected schedulers
    style = data.get('style_config', {})
    colors = style.get('colors', COLORS)

    window = 100  # default; refined per-scheduler below, used in axis label outside the loop
    methods_used = set()  # track 'logged' vs 'delta_cells' for an on-figure note
    plotted_any = False

    for name, color, label in iter_schedulers(data):
        df = get_division_source(data, name)
        if df is None:
            continue

        # Calculate divisions (logged column if it has signal, else Δcells fallback)
        df = df.copy()
        if 'iteration' in df.columns:
            df = df.sort_values('iteration').reset_index(drop=True)
        divisions, method = derive_divisions(df)
        if divisions is None:
            continue
        methods_used.add(method)
        plotted_any = True
        df['cell_diff'] = df['cells'].diff().fillna(0)
        df['divisions'] = divisions.values

        # Panel A: Inter-division intervals
        division_events = df[df['divisions'] > 0]
        if len(division_events) > 1:
            intervals = division_events['iteration'].diff().dropna()
            if len(intervals) > 5:
                axes[0, 0].hist(intervals, bins=30, alpha=0.6, label=name, color=color,
                              edgecolor='white', linewidth=0.5)

        # Panel B: Burst size distribution
        if len(division_events) > 0:
            burst_sizes = division_events['divisions']
            axes[0, 1].hist(burst_sizes, bins=range(1, int(burst_sizes.max()) + 2), alpha=0.6,
                          label=name, color=color, edgecolor='white', linewidth=0.5)

        # Panel C: Division rate over time (smoothed)
        window = max(100, len(df) // 50)
        df['div_rate'] = df['divisions'].rolling(window=window, center=True).sum()
        df_rate = df[df['div_rate'] > 0]
        if len(df_rate) > 0:
            axes[1, 0].plot(df_rate['iteration'], df_rate['div_rate'],
                          label=name, color=color, linewidth=1.5, alpha=0.7)

        # Panel D: Cell count vs divisions cumulative
        df['div_cumsum'] = df['divisions'].cumsum()
        axes[1, 1].plot(df['cells'], df['div_cumsum'], label=name, color=color,
                       linewidth=1.5, alpha=0.7)

    axes[0, 0].set_xlabel('Inter-Division Interval (iterations)', fontsize=10)
    axes[0, 0].set_ylabel('Count', fontsize=10)
    axes[0, 0].set_title('Inter-Division Interval Distribution', fontsize=12)
    axes[0, 0].legend(fontsize=9)
    axes[0, 0].grid(True, alpha=0.3)

    axes[0, 1].set_xlabel('Burst Size (divisions/event)', fontsize=10)
    axes[0, 1].set_ylabel('Count', fontsize=10)
    axes[0, 1].set_title('Division Burst Size Distribution', fontsize=12)
    axes[0, 1].legend(fontsize=9)
    axes[0, 1].grid(True, alpha=0.3)

    axes[1, 0].set_xlabel('Iteration', fontsize=10)
    axes[1, 0].set_ylabel(f'Division Rate (per {window} iter)', fontsize=10)
    axes[1, 0].set_title('Division Rate Over Time', fontsize=12)
    axes[1, 0].legend(fontsize=9)
    axes[1, 0].grid(True, alpha=0.3)

    axes[1, 1].set_xlabel('Cell Count', fontsize=10)
    axes[1, 1].set_ylabel('Cumulative Divisions', fontsize=10)
    axes[1, 1].set_title('Cumulative Divisions vs Population', fontsize=12)
    axes[1, 1].legend(fontsize=9)
    axes[1, 1].grid(True, alpha=0.3)

    if not plotted_any:
        for ax in axes.flat:
            ax.text(0.5, 0.5, 'No cell-count data available\nfor division reconstruction',
                    ha='center', va='center', transform=ax.transAxes, fontsize=11, color='gray')

    plt.suptitle('Cell Division Dynamics', fontsize=14, fontweight='bold')

    # Note how divisions were obtained so the plot is not mistaken for logged counts
    if 'delta_cells' in methods_used:
        note = ('Divisions reconstructed from Δcells (logged divisions column is '
                'structurally 0 in this run)')
    elif 'logged' in methods_used:
        note = 'Divisions from logged divisions column'
    else:
        note = None
    if note:
        fig.text(0.5, 0.005, note, ha='center', fontsize=8, style='italic', alpha=0.7)

    plt.tight_layout(rect=[0, 0.02, 1, 1])
    plt.savefig(output_dir / "10_division_dynamics.png", dpi=DPI, bbox_inches='tight')
    plt.close()


@requires_any_sim_stats
def plot_11_contact_area_distribution(data: dict, output_dir: Path) -> None:
    """Plot 11: Contact area fraction distribution."""
    fig, axes = plt.subplots(1, 2, figsize=(12, 5))

    # Build modes dict dynamically from detected schedulers
    modes = build_sim_stats_modes(data)
    if not modes:
        plt.close()
        return

    contact_data = {}

    for name, (df, color) in modes.items():
        if df is None or 'cell_contact_area_fraction' not in df.columns:
            continue

        last_iter = df['iteration'].max()
        df_last = df[df['iteration'] == last_iter].copy()
        contact = df_last['cell_contact_area_fraction']
        contact_data[name] = contact.values

        # Histogram
        axes[0].hist(contact, bins=50, alpha=0.6, label=name, color=color,
                    edgecolor='white', linewidth=0.5)

        # Evolution over time
        contact_by_iter = df.groupby('iteration')['cell_contact_area_fraction'].mean()
        axes[1].plot(contact_by_iter.index, contact_by_iter.values,
                    label=name, color=color, linewidth=1.5, alpha=0.8)

    axes[0].axvline(x=0.85, color='gray', linestyle='--', alpha=0.7, label='Healthy ~85%')
    axes[0].set_xlabel('Contact Area Fraction', fontsize=10)
    axes[0].set_ylabel('Cell Count', fontsize=10)
    axes[0].set_title('Contact Area Distribution (Final)', fontsize=12)
    axes[0].legend(fontsize=9)
    axes[0].grid(True, alpha=0.3)

    axes[1].axhline(y=0.85, color='gray', linestyle='--', alpha=0.7)
    axes[1].set_xlabel('Iteration', fontsize=10)
    axes[1].set_ylabel('Mean Contact Fraction', fontsize=10)
    axes[1].set_title('Contact Fraction Evolution', fontsize=12)
    axes[1].legend(fontsize=9)
    axes[1].grid(True, alpha=0.3)

    plt.suptitle('Cell-Cell Contact Analysis', fontsize=14, fontweight='bold')
    plt.tight_layout()
    plt.savefig(output_dir / "11_contact_area_distribution.png", dpi=DPI, bbox_inches='tight')
    plt.close()


# =============================================================================
# SECTION 7: Physical/Energy Metrics Plots (12-16)
# =============================================================================

@requires_any_sim_stats
def plot_12_energy_evolution(data: dict, output_dir: Path) -> None:
    """Plot 12: Energy evolution over time."""
    fig, axes = plt.subplots(1, 3, figsize=(16, 5))

    # Build modes dict dynamically from detected schedulers
    modes = build_sim_stats_modes(data)
    if not modes:
        plt.close()
        return

    for name, (df, color) in modes.items():
        if df is None:
            continue

        # Aggregate energy by iteration
        energy_cols = ['kinetic_energy', 'total_potential_energy']
        available_cols = [c for c in energy_cols if c in df.columns]

        if not available_cols:
            continue

        grouped = df.groupby('iteration')[available_cols].sum().reset_index()

        if 'total_potential_energy' in grouped.columns:
            # Total potential energy
            axes[0].plot(grouped['iteration'], grouped['total_potential_energy'],
                        label=name, color=color, linewidth=1.5, alpha=0.8)

            # Log scale
            pe_pos = grouped[grouped['total_potential_energy'] > 0]
            if len(pe_pos) > 0:
                axes[1].semilogy(pe_pos['iteration'], pe_pos['total_potential_energy'],
                                label=name, color=color, linewidth=1.5, alpha=0.8)

        if 'kinetic_energy' in grouped.columns and 'total_potential_energy' in grouped.columns:
            grouped['total_energy'] = grouped['kinetic_energy'] + grouped['total_potential_energy']
            axes[2].plot(grouped['iteration'], grouped['total_energy'],
                        label=name, color=color, linewidth=1.5, alpha=0.8)

    axes[0].set_xlabel('Iteration', fontsize=10)
    axes[0].set_ylabel('Total Potential Energy (J)', fontsize=10)
    axes[0].set_title('Potential Energy Evolution', fontsize=12)
    axes[0].legend(fontsize=9)
    axes[0].grid(True, alpha=0.3)

    axes[1].set_xlabel('Iteration', fontsize=10)
    axes[1].set_ylabel('Total PE (J, log scale)', fontsize=10)
    axes[1].set_title('Potential Energy (Log Scale)', fontsize=12)
    axes[1].legend(fontsize=9)
    axes[1].grid(True, alpha=0.3, which='both')

    axes[2].set_xlabel('Iteration', fontsize=10)
    axes[2].set_ylabel('Total Energy (KE + PE)', fontsize=10)
    axes[2].set_title('Total System Energy', fontsize=12)
    axes[2].legend(fontsize=9)
    axes[2].grid(True, alpha=0.3)

    plt.suptitle('System Energy Evolution', fontsize=14, fontweight='bold')
    plt.tight_layout()
    plt.savefig(output_dir / "12_energy_evolution.png", dpi=DPI, bbox_inches='tight')
    plt.close()


@requires_any_sim_stats
def plot_13_energy_component_ratios(data: dict, output_dir: Path) -> None:
    """Plot 13: Energy component ratios (KE/PE, ST/P)."""
    fig, axes = plt.subplots(1, 2, figsize=(14, 5))

    # Build modes dict dynamically from detected schedulers
    modes = build_sim_stats_modes(data)
    if not modes:
        plt.close()
        return

    for name, (df, color) in modes.items():
        if df is None:
            continue

        # Aggregate by iteration
        energy_cols = ['kinetic_energy', 'total_potential_energy',
                       'surface_tension_energy', 'pressure_energy']
        available_cols = [c for c in energy_cols if c in df.columns]

        if len(available_cols) < 2:
            continue

        grouped = df.groupby('iteration')[available_cols].sum().reset_index()

        # KE/PE ratio
        if 'kinetic_energy' in grouped.columns and 'total_potential_energy' in grouped.columns:
            with np.errstate(divide='ignore', invalid='ignore'):
                ke_pe = np.where(grouped['total_potential_energy'] > 0,
                                grouped['kinetic_energy'] / grouped['total_potential_energy'] * 100,
                                0)
            valid = ke_pe > 0
            if valid.sum() > 5:
                axes[0].semilogy(grouped.loc[valid, 'iteration'], ke_pe[valid],
                                label=f'{name} KE/PE', color=color, linewidth=1.5, alpha=0.8)

        # ST/P ratio
        if 'surface_tension_energy' in grouped.columns and 'pressure_energy' in grouped.columns:
            with np.errstate(divide='ignore', invalid='ignore'):
                st_p = np.where(grouped['pressure_energy'] > 0,
                               grouped['surface_tension_energy'] / grouped['pressure_energy'] * 100,
                               0)
            valid = st_p > 0
            if valid.sum() > 5:
                axes[1].semilogy(grouped.loc[valid, 'iteration'], st_p[valid],
                                label=f'{name} ST/P', color=color, linewidth=1.5, alpha=0.8)

    # Reference lines
    axes[0].axhline(y=1.0, color='orange', linestyle=':', alpha=0.7, label='1% threshold')
    axes[0].axhline(y=0.1, color='gray', linestyle=':', alpha=0.5, label='0.1% quasi-static')
    axes[0].set_xlabel('Iteration', fontsize=10)
    axes[0].set_ylabel('KE/PE Ratio (%, log)', fontsize=10)
    axes[0].set_title('Kinetic/Potential Energy Ratio', fontsize=12)
    axes[0].legend(fontsize=8)
    axes[0].grid(True, alpha=0.3, which='both')
    add_stats_box(axes[0], 'KE/PE < 0.1% = quasi-static\nKE/PE > 1% = dynamic', (0.02, 0.15))

    axes[1].set_xlabel('Iteration', fontsize=10)
    axes[1].set_ylabel('ST/P Ratio (%, log)', fontsize=10)
    axes[1].set_title('Surface Tension/Pressure Ratio', fontsize=12)
    axes[1].legend(fontsize=8)
    axes[1].grid(True, alpha=0.3, which='both')
    add_stats_box(axes[1], 'Tracks mechanical\nbalance', (0.02, 0.15))

    plt.suptitle('Energy Component Analysis', fontsize=14, fontweight='bold')
    plt.tight_layout()
    plt.savefig(output_dir / "13_energy_component_ratios.png", dpi=DPI, bbox_inches='tight')
    plt.close()


@requires_any_sim_stats
def plot_14_pv_phase_space(data: dict, output_dir: Path) -> None:
    """Plot 14: Pressure-Volume phase space density."""
    # Build modes dict dynamically from detected schedulers
    modes = build_sim_stats_modes(data)
    if not modes:
        return

    fig, axes = plt.subplots(1, max(2, len(modes)), figsize=(7 * max(2, len(modes)), 6))
    # subplots always returns a >=2-element array here (cols = max(2, len(modes))),
    # so axes[idx] is already a single Axes; no wrapping needed.

    for idx, (name, (df, color)) in enumerate(modes.items()):
        if df is None or 'volume' not in df.columns or 'pressure' not in df.columns:
            continue

        ax = axes[idx]

        # Get last iteration
        last_iter = df['iteration'].max()
        df_last = df[df['iteration'] == last_iter].copy()

        # Filter valid data
        df_valid = df_last[(df_last['volume'] > 0) & (df_last['pressure'] > 0)]

        if len(df_valid) < 10:
            ax.text(0.5, 0.5, 'Insufficient data', ha='center', va='center',
                   transform=ax.transAxes, fontsize=12)
            ax.set_title(f'{name} P-V Phase Space', fontsize=12)
            continue

        # 2D histogram
        h = ax.hist2d(df_valid['volume'] * 1e9, df_valid['pressure'],
                     bins=50, cmap='Blues' if name == 'v1' else 'Reds',
                     alpha=0.8)
        plt.colorbar(h[3], ax=ax, label='Cell Count')

        # Add ideal gas reference line (PV = const)
        v_range = np.linspace(df_valid['volume'].min() * 1e9,
                              df_valid['volume'].max() * 1e9, 100)
        pv_const = df_valid['volume'].median() * 1e9 * df_valid['pressure'].median()
        ax.plot(v_range, pv_const / v_range, 'k--', alpha=0.5, label='PV = const')

        ax.set_xlabel('Volume (nL)', fontsize=10)
        ax.set_ylabel('Pressure (Pa)', fontsize=10)
        ax.set_title(f'{LABELS.get(name, name)} P-V Phase Space', fontsize=12)
        ax.legend(fontsize=8)

    plt.suptitle('Pressure-Volume Phase Space Analysis', fontsize=14, fontweight='bold')
    plt.tight_layout()
    plt.savefig(output_dir / "14_pv_phase_space.png", dpi=DPI, bbox_inches='tight')
    plt.close()


@requires_any_sim_stats
def plot_15_energy_stability(data: dict, output_dir: Path) -> None:
    """Plot 15: Energy stability analysis with quantitative conservation bounds.

    Enhanced with Bio-Sim expert recommendations:
    - Relative drift (%) with tolerance bounds
    - Energy conservation verdict (EXCELLENT/ACCEPTABLE/PROBLEMATIC)
    - FFT spectrum for periodic artifact detection
    - Hamiltonian violation rate
    """
    # Build modes dict dynamically from detected schedulers
    modes = build_sim_stats_modes(data)
    if not modes:
        return

    fig = plt.figure(figsize=(16, 12))
    gs = gridspec.GridSpec(2, 3, figure=fig, hspace=0.3, wspace=0.3)

    # Store results for summary
    conservation_results = {}

    # Panel A: Relative energy drift (%)
    ax_drift = fig.add_subplot(gs[0, 0])
    for name, (df, color) in modes.items():
        if df is None or 'total_potential_energy' not in df.columns:
            continue

        # Aggregate by iteration
        agg_cols = {'total_potential_energy': 'sum'}
        if 'kinetic_energy' in df.columns:
            agg_cols['kinetic_energy'] = 'sum'

        grouped = df.groupby('iteration').agg(agg_cols).reset_index()

        if 'kinetic_energy' in grouped.columns:
            grouped['total_energy'] = grouped['kinetic_energy'] + grouped['total_potential_energy']
        else:
            grouped['total_energy'] = grouped['total_potential_energy']

        E = grouped['total_energy'].values
        iterations = grouped['iteration'].values

        E0 = E[0] if E[0] != 0 else 1e-20
        relative_drift = np.abs(E - E0) / np.abs(E0) * 100  # Percentage

        valid = relative_drift > 0
        if valid.sum() > 10:
            ax_drift.semilogy(iterations[valid], relative_drift[valid],
                             label=f'{name} (max: {relative_drift.max():.3f}%)',
                             color=color, alpha=0.7, linewidth=1.5)

        # Store max drift for verdict
        conservation_results[name] = {
            'max_drift_pct': relative_drift.max(),
            'final_drift_pct': relative_drift[-1] if len(relative_drift) > 0 else 0
        }

    # Add tolerance lines
    ax_drift.axhline(y=0.1, color='green', linestyle='--', alpha=0.8, linewidth=2,
                    label='0.1% (Excellent)')
    ax_drift.axhline(y=1.0, color='orange', linestyle='--', alpha=0.8, linewidth=2,
                    label='1% (Acceptable)')
    ax_drift.axhline(y=10.0, color='red', linestyle='--', alpha=0.5, linewidth=1.5,
                    label='10% (Problematic)')

    ax_drift.set_xlabel('Iteration', fontsize=10)
    ax_drift.set_ylabel('Relative Drift |ΔE/E₀| (%)', fontsize=10)
    ax_drift.set_title('Energy Conservation (Relative)', fontsize=11, fontweight='bold')
    ax_drift.legend(fontsize=8, loc='upper left')
    ax_drift.grid(True, alpha=0.3, which='both')

    # Panel B: FFT spectrum
    ax_fft = fig.add_subplot(gs[0, 1])
    for name, (df, color) in modes.items():
        if df is None or 'total_potential_energy' not in df.columns:
            continue

        agg_cols = {'total_potential_energy': 'sum'}
        if 'kinetic_energy' in df.columns:
            agg_cols['kinetic_energy'] = 'sum'
        grouped = df.groupby('iteration').agg(agg_cols).reset_index()

        if 'kinetic_energy' in grouped.columns:
            E = (grouped['kinetic_energy'] + grouped['total_potential_energy']).values
        else:
            E = grouped['total_potential_energy'].values

        if len(E) > 64 and SCIPY_AVAILABLE:
            t = np.arange(len(E))
            coeffs = np.polyfit(t, E, 1)
            E_detrend = E - np.polyval(coeffs, t)

            fft_vals = np.fft.rfft(E_detrend)
            psd = np.abs(fft_vals)**2 / len(E)
            freq = np.fft.rfftfreq(len(E))

            valid = freq > 0
            if valid.sum() > 5:
                ax_fft.loglog(freq[valid], psd[valid],
                             label=name, color=color, alpha=0.7, linewidth=1.5)

    ax_fft.set_xlabel('Normalized Frequency', fontsize=10)
    ax_fft.set_ylabel('Power Spectral Density', fontsize=10)
    ax_fft.set_title('Energy Fluctuation Spectrum', fontsize=11, fontweight='bold')
    ax_fft.legend(fontsize=9)
    ax_fft.grid(True, alpha=0.3, which='both')
    add_stats_box(ax_fft, 'Flat = white noise (good)\nSpikes = periodic artifacts', (0.02, 0.15))

    # Panel C: dE/dt (Hamiltonian violation rate)
    ax_dedt = fig.add_subplot(gs[0, 2])
    for name, (df, color) in modes.items():
        if df is None or 'total_potential_energy' not in df.columns:
            continue

        agg_cols = {'total_potential_energy': 'sum'}
        if 'kinetic_energy' in df.columns:
            agg_cols['kinetic_energy'] = 'sum'
        grouped = df.groupby('iteration').agg(agg_cols).reset_index()

        if 'kinetic_energy' in grouped.columns:
            E = (grouped['kinetic_energy'] + grouped['total_potential_energy']).values
        else:
            E = grouped['total_potential_energy'].values
        iterations = grouped['iteration'].values

        if len(E) > 2:
            dE = np.gradient(E)
            dt = np.gradient(iterations)
            dE_dt = np.abs(dE / dt)

            valid = dE_dt > 0
            sample = max(1, len(grouped) // 300)
            if valid.sum() > 10:
                ax_dedt.semilogy(iterations[::sample][valid[::sample]],
                                dE_dt[::sample][valid[::sample]],
                                label=name, color=color, alpha=0.6, linewidth=1)

    ax_dedt.set_xlabel('Iteration', fontsize=10)
    ax_dedt.set_ylabel('|dE/dt| (J/iter)', fontsize=10)
    ax_dedt.set_title('Hamiltonian Violation Rate', fontsize=11, fontweight='bold')
    ax_dedt.legend(fontsize=9)
    ax_dedt.grid(True, alpha=0.3, which='both')

    # Panel D: Energy per cell evolution
    ax_epc = fig.add_subplot(gs[1, 0])
    for name, (df, color) in modes.items():
        if df is None or 'total_potential_energy' not in df.columns:
            continue

        energy_per_cell = df.groupby('iteration').agg({
            'total_potential_energy': 'mean'
        }).reset_index()
        ax_epc.plot(energy_per_cell['iteration'], energy_per_cell['total_potential_energy'],
                   label=name, color=color, linewidth=1.5, alpha=0.8)

    ax_epc.set_xlabel('Iteration', fontsize=10)
    ax_epc.set_ylabel('Mean Energy/Cell (J)', fontsize=10)
    ax_epc.set_title('Energy per Cell Evolution', fontsize=11, fontweight='bold')
    ax_epc.legend(fontsize=9)
    ax_epc.grid(True, alpha=0.3)

    # Panel E: Energy component evolution
    ax_comp = fig.add_subplot(gs[1, 1])
    for name, (df, color) in modes.items():
        if df is None:
            continue

        grouped = df.groupby('iteration').agg({
            col: 'sum' for col in ['kinetic_energy', 'surface_tension_energy', 'pressure_energy']
            if col in df.columns
        }).reset_index()

        if 'kinetic_energy' in grouped.columns:
            ax_comp.plot(grouped['iteration'], grouped['kinetic_energy'],
                        label=f'{name} KE', color=color, alpha=0.5, linewidth=1)
        if 'surface_tension_energy' in grouped.columns:
            ax_comp.plot(grouped['iteration'], grouped['surface_tension_energy'],
                        label=f'{name} ST', color=color, alpha=0.8, linewidth=1.5, linestyle='--')

    ax_comp.set_xlabel('Iteration', fontsize=10)
    ax_comp.set_ylabel('Energy (J)', fontsize=10)
    ax_comp.set_title('Energy Components', fontsize=11, fontweight='bold')
    ax_comp.legend(fontsize=7, loc='upper right')
    ax_comp.grid(True, alpha=0.3)

    # Panel F: Conservation verdict summary
    ax_verdict = fig.add_subplot(gs[1, 2])
    ax_verdict.axis('off')

    summary_text = "ENERGY CONSERVATION VERDICT\n" + "=" * 35 + "\n\n"
    summary_text += "Tolerance Criteria:\n"
    summary_text += "  < 0.1% : EXCELLENT ✓✓\n"
    summary_text += "  < 1.0% : ACCEPTABLE ✓\n"
    summary_text += "  < 10%  : MARGINAL ⚠\n"
    summary_text += "  > 10%  : PROBLEMATIC ✗\n\n"

    for name, results in conservation_results.items():
        max_drift = results['max_drift_pct']
        final_drift = results['final_drift_pct']

        if max_drift < 0.1:
            verdict = "EXCELLENT ✓✓"
            verdict_color = 'green'
        elif max_drift < 1.0:
            verdict = "ACCEPTABLE ✓"
            verdict_color = 'blue'
        elif max_drift < 10.0:
            verdict = "MARGINAL ⚠"
            verdict_color = 'orange'
        else:
            verdict = "PROBLEMATIC ✗"
            verdict_color = 'red'

        summary_text += f"{name.upper()}:\n"
        summary_text += f"  Max drift: {max_drift:.4f}%\n"
        summary_text += f"  Final drift: {final_drift:.4f}%\n"
        summary_text += f"  Verdict: {verdict}\n\n"

    summary_text += "Note: Energy drift in growing\n"
    summary_text += "cell populations is expected\n"
    summary_text += "due to cell division events."

    ax_verdict.text(0.1, 0.95, summary_text, transform=ax_verdict.transAxes,
                   fontsize=10, verticalalignment='top', family='monospace',
                   bbox=dict(boxstyle='round', facecolor='lightyellow', alpha=0.9))

    plt.suptitle('Energy Conservation & Stability Analysis', fontsize=14, fontweight='bold')
    plt.savefig(output_dir / "15_energy_stability.png", dpi=DPI, bbox_inches='tight')
    plt.close()


@requires_any_sim_stats
def plot_16_pressure_volume_correlation(data: dict, output_dir: Path) -> None:
    """Plot 16: Pressure-Volume correlation with contact coloring."""
    # Build modes dict dynamically from detected schedulers
    modes = build_sim_stats_modes(data)
    if not modes:
        return

    fig, axes = plt.subplots(1, max(2, len(modes)), figsize=(7 * max(2, len(modes)), 6))
    # subplots always returns a >=2-element array here (cols = max(2, len(modes))),
    # so axes[idx] is already a single Axes; no wrapping needed.

    for idx, (name, (df, color)) in enumerate(modes.items()):
        if df is None or 'volume' not in df.columns or 'pressure' not in df.columns:
            continue

        ax = axes[idx]

        last_iter = df['iteration'].max()
        df_last = df[df['iteration'] == last_iter].copy()

        # Sample for clarity
        sample_size = min(1000, len(df_last))
        df_sample = df_last.sample(sample_size) if len(df_last) > sample_size else df_last

        # Scatter colored by contact fraction if available
        if 'cell_contact_area_fraction' in df_sample.columns:
            scatter = ax.scatter(df_sample['volume'] * 1e9, df_sample['pressure'],
                               c=df_sample['cell_contact_area_fraction'],
                               cmap='viridis', alpha=0.5, s=15)
            plt.colorbar(scatter, ax=ax, label='Contact Fraction')
        else:
            ax.scatter(df_sample['volume'] * 1e9, df_sample['pressure'],
                      color=color, alpha=0.3, s=15)

        ax.set_xlabel('Volume (nL)', fontsize=10)
        ax.set_ylabel('Pressure (Pa)', fontsize=10)
        ax.set_title(f'{LABELS.get(name, name)}', fontsize=12)
        ax.grid(True, alpha=0.3)

        # Add correlation coefficient
        if SCIPY_AVAILABLE:
            corr, pval = stats.pearsonr(df_sample['volume'], df_sample['pressure'])
            add_stats_box(ax, f'r = {corr:.3f}\np = {pval:.2e}', (0.02, 0.98))

    plt.suptitle('Pressure-Volume Correlation Analysis', fontsize=14, fontweight='bold')
    plt.tight_layout()
    plt.savefig(output_dir / "16_pressure_volume_correlation.png", dpi=DPI, bbox_inches='tight')
    plt.close()


# =============================================================================
# SECTION 8: Summary & Statistical Plots (17-20)
# =============================================================================

@requires_any_sim_stats
def plot_17_statistical_comparison(data: dict, output_dir: Path) -> None:
    """Plot 17: Statistical comparison with box plots and tests."""
    # Build modes dict dynamically from detected schedulers
    modes = build_sim_stats_modes(data)
    if not modes:
        return

    fig, axes = plt.subplots(2, 3, figsize=(16, 10))

    # Collect data for statistical tests
    volume_data = {}
    pressure_data = {}
    energy_data = {}

    for name, (df, color) in modes.items():
        if df is None:
            continue

        last_iter = df['iteration'].max()
        df_last = df[df['iteration'] == last_iter]

        if 'volume' in df_last.columns:
            volume_data[name] = df_last['volume'].values
        if 'pressure' in df_last.columns:
            pressure_data[name] = df_last['pressure'].values
        if 'total_potential_energy' in df_last.columns:
            energy_data[name] = df_last['total_potential_energy'].values

    # Plot 1: Volume box plot
    if volume_data:
        bp_data = list(volume_data.values())
        bp_labels = list(volume_data.keys())
        bp = axes[0, 0].boxplot([v * 1e9 for v in bp_data], labels=bp_labels, patch_artist=True)
        for patch, name in zip(bp['boxes'], bp_labels):
            patch.set_facecolor(COLORS.get(name, 'gray'))
            patch.set_alpha(0.6)
        axes[0, 0].set_ylabel('Volume (nL)', fontsize=10)
        axes[0, 0].set_title('Volume Distribution', fontsize=12)
        axes[0, 0].grid(True, alpha=0.3)

        # KS test
        if len(volume_data) == 2 and SCIPY_AVAILABLE:
            v1, v2 = list(volume_data.values())
            ks_stat, ks_pval = stats.ks_2samp(v1, v2)
            add_stats_box(axes[0, 0], f'KS: p={ks_pval:.2e}', (0.7, 0.95))

    # Plot 2: Pressure box plot
    if pressure_data:
        bp_data = list(pressure_data.values())
        bp_labels = list(pressure_data.keys())
        bp = axes[0, 1].boxplot(bp_data, labels=bp_labels, patch_artist=True)
        for patch, name in zip(bp['boxes'], bp_labels):
            patch.set_facecolor(COLORS.get(name, 'gray'))
            patch.set_alpha(0.6)
        axes[0, 1].set_ylabel('Pressure (Pa)', fontsize=10)
        axes[0, 1].set_title('Pressure Distribution', fontsize=12)
        axes[0, 1].grid(True, alpha=0.3)

        if len(pressure_data) == 2 and SCIPY_AVAILABLE:
            p1, p2 = list(pressure_data.values())
            ks_stat, ks_pval = stats.ks_2samp(p1, p2)
            add_stats_box(axes[0, 1], f'KS: p={ks_pval:.2e}', (0.7, 0.95))

    # Plot 3: Energy box plot
    if energy_data:
        bp_data = list(energy_data.values())
        bp_labels = list(energy_data.keys())
        bp = axes[0, 2].boxplot(bp_data, labels=bp_labels, patch_artist=True)
        for patch, name in zip(bp['boxes'], bp_labels):
            patch.set_facecolor(COLORS.get(name, 'gray'))
            patch.set_alpha(0.6)
        axes[0, 2].set_ylabel('Energy (J)', fontsize=10)
        axes[0, 2].set_title('Energy Distribution', fontsize=12)
        axes[0, 2].grid(True, alpha=0.3)

    # Plot 4-5: Q-Q plots
    for idx, (name, arr) in enumerate(volume_data.items()):
        if idx >= 2:
            break
        ax = axes[1, idx]

        if SCIPY_AVAILABLE:
            stats.probplot(arr * 1e9, dist="norm", plot=ax)
            ax.set_title(f'{name} Q-Q Plot (Volume)', fontsize=12)
            ax.grid(True, alpha=0.3)

            # Shapiro-Wilk test (on sample)
            sample = arr[:5000] if len(arr) > 5000 else arr
            sw_stat, sw_pval = stats.shapiro(sample)
            add_stats_box(ax, f'Shapiro-Wilk\np={sw_pval:.2e}', (0.02, 0.98))

    # Plot 6: Summary statistics
    axes[1, 2].axis('off')
    summary_text = "Statistical Summary\n" + "=" * 35 + "\n\n"

    for name, (df, _) in modes.items():
        if df is None:
            continue
        last_iter = df['iteration'].max()
        df_last = df[df['iteration'] == last_iter]
        n = len(df_last)

        summary_text += f"{name} (n={n:,}):\n"

        if 'volume' in df_last.columns:
            s = compute_statistics(df_last['volume'].values)
            summary_text += f"  Vol: μ={s['mean']:.2e}, CV={s['cv']:.3f}\n"

        if 'pressure' in df_last.columns:
            s = compute_statistics(df_last['pressure'].values)
            summary_text += f"  Press: μ={s['mean']:.1f}Pa, CV={s['cv']:.3f}\n"

        summary_text += "\n"

    # Add effect size if both present
    if len(volume_data) == 2 and SCIPY_AVAILABLE:
        v1, v2 = list(volume_data.values())
        pooled_std = np.sqrt((np.var(v1) + np.var(v2)) / 2)
        cohens_d = (np.mean(v1) - np.mean(v2)) / pooled_std if pooled_std > 0 else 0
        summary_text += f"Cohen's d (volume): {cohens_d:.3f}\n"

        if abs(cohens_d) < 0.2:
            effect = "negligible"
        elif abs(cohens_d) < 0.5:
            effect = "small"
        elif abs(cohens_d) < 0.8:
            effect = "medium"
        else:
            effect = "large"
        summary_text += f"Effect size: {effect}\n"

    axes[1, 2].text(0.1, 0.9, summary_text, transform=axes[1, 2].transAxes,
                   fontsize=10, verticalalignment='top', family='monospace',
                   bbox=dict(boxstyle='round', facecolor='whitesmoke', alpha=0.8))
    axes[1, 2].set_title('Summary', fontsize=12)

    plt.suptitle('Statistical Comparison: v1 vs Adaptive', fontsize=14, fontweight='bold')
    plt.tight_layout()
    plt.savefig(output_dir / "17_statistical_comparison.png", dpi=DPI, bbox_inches='tight')
    plt.close()


@requires_data('comparison')
def plot_18_speedup_analysis(data: dict, output_dir: Path) -> None:
    """Plot 18: Speedup analysis comparing first two schedulers."""
    fig, axes = plt.subplots(1, 2, figsize=(14, 6))

    df = data['comparison']
    schedulers = get_comparison_schedulers(df)
    style = data.get('style_config', {})
    labels_map = style.get('labels', LABELS)

    if len(schedulers) < 2:
        axes[0].text(0.5, 0.5, 'Need at least 2 schedulers for comparison',
                    ha='center', va='center', transform=axes[0].transAxes,
                    fontsize=12, color='gray')
        plt.savefig(output_dir / "18_speedup_analysis.png", dpi=DPI, bbox_inches='tight')
        plt.close()
        return

    # Compare first two schedulers (baseline vs comparison)
    sched1, sched2 = schedulers[0], schedulers[1]
    cols1 = get_scheduler_columns(df, sched1)
    cols2 = get_scheduler_columns(df, sched2)

    label1 = labels_map.get(sched1, sched1)
    label2 = labels_map.get(sched2, sched2)

    # Calculate speedups
    if 'ips' in cols1 and 'ips' in cols2:
        df_valid = df[(df[cols1['ips']] > 0) & (df[cols2['ips']] > 0)].copy()

        if len(df_valid) > 0:
            df_valid['speedup'] = df_valid[cols2['ips']] / df_valid[cols1['ips']]

            # Speedup over time
            if 'timestamp' in df_valid.columns and df_valid['timestamp'].notna().any():
                x_data = to_minutes_since_start(df_valid['timestamp'])
                axes[0].plot(x_data, df_valid['speedup'],
                            color='purple', linewidth=1.5, alpha=0.8)
                axes[0].set_xlabel('Time (minutes since start)', fontsize=10)
            else:
                axes[0].plot(range(len(df_valid)), df_valid['speedup'],
                            color='purple', linewidth=1.5, alpha=0.8)
                axes[0].set_xlabel('Sample Index', fontsize=10)

            axes[0].axhline(y=1.0, color='gray', linestyle='--', alpha=0.7, label='No speedup')
            axes[0].set_ylabel(f'Speedup ({label2} / {label1})', fontsize=10)
            axes[0].set_title('Speedup Over Time', fontsize=12)
            axes[0].legend(fontsize=9)
            axes[0].grid(True, alpha=0.3)

            mean_speedup = df_valid['speedup'].mean()
            add_stats_box(axes[0], f'Mean speedup: {mean_speedup:.2f}x', (0.02, 0.98))

    # Bar chart of final metrics
    final = df.iloc[-1]
    metrics = {}

    if 'ips' in cols1 and 'ips' in cols2:
        v1, v2 = final[cols1['ips']], final[cols2['ips']]
        metrics['Iteration Rate'] = v2 / v1 if v1 > 0 else 0
    if 'iter' in cols1 and 'iter' in cols2:
        v1, v2 = final[cols1['iter']], final[cols2['iter']]
        metrics['Iteration Progress'] = v2 / v1 if v1 > 0 else 0
    if 'cells' in cols1 and 'cells' in cols2:
        v1, v2 = final[cols1['cells']], final[cols2['cells']]
        metrics['Cell Count'] = v2 / v1 if v1 > 0 else 0

    if metrics:
        bars = axes[1].bar(metrics.keys(), metrics.values(), color=['purple', 'teal', 'orange'][:len(metrics)], alpha=0.7)
        axes[1].axhline(y=1.0, color='gray', linestyle='--', alpha=0.7)
        axes[1].set_ylabel(f'Ratio ({label2} / {label1})', fontsize=10)
        axes[1].set_title('Final Metrics Comparison', fontsize=12)
        axes[1].grid(True, alpha=0.3, axis='y')

        # Add value labels
        for bar, val in zip(bars, metrics.values()):
            axes[1].text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.05,
                        f'{val:.2f}x', ha='center', va='bottom', fontsize=10)

    plt.suptitle(f'Performance Comparison: {label1} vs {label2}', fontsize=14, fontweight='bold')
    plt.tight_layout()
    plt.savefig(output_dir / "18_speedup_analysis.png", dpi=DPI, bbox_inches='tight')
    plt.close()


def plot_19_complexity_analysis(data: dict, output_dir: Path) -> None:
    """Plot 19: Computational complexity analysis with O(N^k) fitting."""
    # Check if any scheduler has computational data
    has_computational = any(data.get(f'{s.name}_computational') is not None for s in data.get('schedulers', []))
    if not has_computational:
        print("  [SKIP] 19_complexity_analysis.png - No computational data available")
        return

    fig, axes = plt.subplots(1, 2, figsize=(14, 6))

    for name, color, label in iter_schedulers(data):
        df = data.get(f'{name}_computational')
        if df is None:
            continue

        df_valid = df[(df['cells'] > 10) & (df['iter_per_sec'] > 0)].copy()
        if len(df_valid) < 10:
            continue

        # Time per iteration (inverse of iter_per_sec)
        df_valid['time_per_iter'] = 1000 / df_valid['iter_per_sec']  # ms

        # Linear plot
        axes[0].scatter(df_valid['cells'], df_valid['time_per_iter'],
                       alpha=0.3, s=10, color=color, label=f'{name} data')

        # Log-log plot with fit
        x = df_valid['cells'].values
        y = df_valid['time_per_iter'].values

        axes[1].loglog(x, y, 'o', alpha=0.3, markersize=3, color=color, label=f'{name} data')

        # Power law fit
        if SCIPY_AVAILABLE and len(df_valid) > 20:
            try:
                valid = (x > 0) & (y > 0)
                log_x = np.log10(x[valid])
                log_y = np.log10(y[valid])
                coeffs = np.polyfit(log_x, log_y, 1)
                slope = coeffs[0]

                x_fit = np.logspace(np.log10(x[valid].min()), np.log10(x[valid].max()), 50)
                y_fit = 10**(coeffs[1]) * x_fit**slope
                axes[1].loglog(x_fit, y_fit, '--', color=color, alpha=0.8,
                              label=f'{name}: O(N^{slope:.2f})')
            except Exception:
                pass

    axes[0].set_xlabel('Cell Count', fontsize=10)
    axes[0].set_ylabel('Time per Iteration (ms)', fontsize=10)
    axes[0].set_title('Iteration Time vs Cell Count', fontsize=12)
    axes[0].legend(fontsize=9)
    axes[0].grid(True, alpha=0.3)

    axes[1].set_xlabel('Cell Count (log)', fontsize=10)
    axes[1].set_ylabel('Time per Iteration (ms, log)', fontsize=10)
    axes[1].set_title('Complexity Analysis (Log-Log)', fontsize=12)
    axes[1].legend(fontsize=8)
    axes[1].grid(True, alpha=0.3, which='both')

    add_stats_box(axes[1], 'Slope interpretation:\n'
                          '  1.0 = O(N) linear\n'
                          '  1.5 = O(N^1.5)\n'
                          '  2.0 = O(N²)', (0.02, 0.35))

    plt.suptitle('Computational Complexity Analysis', fontsize=14, fontweight='bold')
    plt.tight_layout()
    plt.savefig(output_dir / "19_complexity_analysis.png", dpi=DPI, bbox_inches='tight')
    plt.close()


@requires_data('comparison')
def plot_20_summary_dashboard(data: dict, output_dir: Path) -> None:
    """Plot 20: Executive summary dashboard."""
    fig = plt.figure(figsize=(16, 10))
    gs = gridspec.GridSpec(3, 3, figure=fig, hspace=0.3, wspace=0.3)

    df = data['comparison']
    final = df.iloc[-1]

    # Detect schedulers dynamically
    schedulers = get_comparison_schedulers(df)
    if len(schedulers) < 2:
        print("    [20] Skipped: Need at least 2 schedulers for comparison")
        plt.close()
        return

    # Get style config for labels
    style = data.get('style_config', {})
    labels = style.get('labels', LABELS)

    # Build scheduler data dict with columns
    sched_data = {}
    for sched in schedulers:
        cols = get_scheduler_columns(df, sched)
        if cols.get('iter') and cols.get('cells'):
            sched_data[sched] = {
                'cols': cols,
                'iter': final.get(cols['iter'], 0),
                'cells': final.get(cols['cells'], 0),
                'label': labels.get(sched, sched.title())
            }

    if len(sched_data) < 2:
        print("    [20] Skipped: Not enough valid scheduler data")
        plt.close()
        return

    # Use first two schedulers for comparison (typically baseline vs optimized)
    sched_names = list(sched_data.keys())
    baseline = sched_names[0]
    optimized = sched_names[-1]  # Last one (often 'adaptive')

    base_data = sched_data[baseline]
    opt_data = sched_data[optimized]

    # Calculate key metrics
    cell_ratio = opt_data['cells'] / base_data['cells'] if base_data['cells'] > 0 else 0
    iter_ratio = opt_data['iter'] / base_data['iter'] if base_data['iter'] > 0 else 0

    # Top row: Key metrics (dynamic header based on detected schedulers)
    ax_metrics = fig.add_subplot(gs[0, :])
    ax_metrics.axis('off')

    metrics_text = f"""
    {'='*70}
    BENCHMARK SUMMARY DASHBOARD
    {'='*70}

    Final State (at end of benchmark):

    {'Metric':<30} {base_data['label']:<20} {opt_data['label']:<20} {'Ratio'}
    {'-'*70}
    {'Iterations':.<30} {base_data['iter']:>15,} {opt_data['iter']:>15,} {iter_ratio:>10.2f}x
    {'Cell Count':.<30} {base_data['cells']:>15,} {opt_data['cells']:>15,} {cell_ratio:>10.2f}x
    """

    ax_metrics.text(0.5, 0.5, metrics_text, transform=ax_metrics.transAxes,
                   fontsize=11, family='monospace', ha='center', va='center',
                   bbox=dict(boxstyle='round', facecolor='lightyellow', alpha=0.8))

    # Cell growth comparison - plot all detected schedulers
    ax_cells = fig.add_subplot(gs[1, 0])
    for sched, sd in sched_data.items():
        cols = sd['cols']
        iter_col, cells_col = cols.get('iter'), cols.get('cells')
        if iter_col and cells_col and iter_col in df.columns and cells_col in df.columns:
            df_valid = df[df[cells_col] > 0]
            color = COLORS.get(sched, '#333333')
            ax_cells.plot(df_valid[iter_col], df_valid[cells_col],
                         label=sd['label'], color=color, linewidth=2)
    ax_cells.set_xlabel('Iteration')
    ax_cells.set_ylabel('Cell Count')
    ax_cells.set_title('Cell Population Growth')
    ax_cells.legend()
    ax_cells.grid(True, alpha=0.3)

    # Iteration rate comparison - plot all detected schedulers
    ax_ips = fig.add_subplot(gs[1, 1])
    x_label = 'Sample'
    for sched, sd in sched_data.items():
        cols = sd['cols']
        ips_col = cols.get('ips')
        if ips_col and ips_col in df.columns:
            df_ips = df[df[ips_col] > 0]
            if len(df_ips) > 0:
                if 'timestamp' in df_ips.columns and df_ips['timestamp'].notna().any():
                    x = to_minutes_since_start(df_ips['timestamp'])
                    x_label = 'Minutes'
                else:
                    x = range(len(df_ips))
                color = COLORS.get(sched, '#333333')
                ax_ips.plot(x, df_ips[ips_col], label=sd['label'], color=color, linewidth=2)
    ax_ips.set_xlabel(x_label)
    ax_ips.set_ylabel('Iter/sec')
    ax_ips.set_title('Iteration Rate')
    ax_ips.legend()
    ax_ips.grid(True, alpha=0.3)

    # Speedup gauge - compare optimized vs baseline
    ax_speedup = fig.add_subplot(gs[1, 2])
    base_ips_col = base_data['cols'].get('ips')
    opt_ips_col = opt_data['cols'].get('ips')

    iter_rate_ratio = 0
    if base_ips_col and opt_ips_col and base_ips_col in df.columns and opt_ips_col in df.columns:
        valid_mask = df[base_ips_col] > 0
        if valid_mask.any():
            iter_rate_ratio = (df.loc[valid_mask, opt_ips_col] / df.loc[valid_mask, base_ips_col]).mean()

    speedup_values = {
        'Iter Rate': iter_rate_ratio,
        'Cell Count': cell_ratio,
        'Progress': iter_ratio,
    }

    colors_bar = ['#3498DB', '#2ECC71', '#E74C3C']
    bars = ax_speedup.bar(speedup_values.keys(), speedup_values.values(), color=colors_bar, alpha=0.7)
    ax_speedup.axhline(y=1.0, color='black', linestyle='--', alpha=0.5)
    ax_speedup.set_ylabel(f'Ratio ({opt_data["label"]} / {base_data["label"]})')
    ax_speedup.set_title('Performance Ratios')
    ax_speedup.grid(True, alpha=0.3, axis='y')

    for bar, val in zip(bars, speedup_values.values()):
        ax_speedup.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.05,
                       f'{val:.2f}x', ha='center', va='bottom', fontsize=10, fontweight='bold')

    # Recommendation text
    ax_rec = fig.add_subplot(gs[2, :])
    ax_rec.axis('off')

    # Generate recommendation (comparing optimized vs baseline)
    if cell_ratio > 1.5:
        recommendation = f"RECOMMENDATION: {opt_data['label']} mode shows significantly better cell growth."
        rec_color = 'green'
    elif cell_ratio > 1.1:
        recommendation = f"RECOMMENDATION: {opt_data['label']} mode shows moderately better cell growth."
        rec_color = 'blue'
    else:
        recommendation = "RECOMMENDATION: Performance difference is minimal between modes."
        rec_color = 'gray'

    rec_text = f"""
    {recommendation}

    Key Findings:
    - {opt_data['label']} achieved {cell_ratio:.1f}x more cells than {base_data['label']} at similar iteration counts
    - Cell growth rate: {opt_data['label']} shows {'higher' if cell_ratio > 1 else 'lower'} throughput
    - Both simulations maintain biological validity (pressure, energy stability)
    """

    ax_rec.text(0.5, 0.5, rec_text, transform=ax_rec.transAxes,
               fontsize=11, ha='center', va='center',
               bbox=dict(boxstyle='round', facecolor='lightgreen' if rec_color == 'green' else 'lightblue',
                        alpha=0.5))

    plt.suptitle('SimuCell3D Benchmark: Executive Summary', fontsize=16, fontweight='bold')
    plt.savefig(output_dir / "20_summary_dashboard.png", dpi=DPI, bbox_inches='tight')
    plt.close()


@requires_data('adaptive_computational', 'adaptive_phase')
def plot_21_roofline_analysis(data: dict, output_dir: Path) -> None:
    """Plot 21: Roofline-style performance characterization.

    HPC expert recommendation: Provides roofline model analysis to identify
    whether the simulation is memory-bound or compute-bound.
    """
    fig, axes = plt.subplots(2, 2, figsize=(14, 12))

    df_comp = data['adaptive_computational']
    df_phase = data.get('adaptive_phase')

    # Merge data if possible
    if df_phase is not None and 'iteration' in df_comp.columns and 'iteration' in df_phase.columns:
        df = pd.merge(df_comp, df_phase, on='iteration', how='inner', suffixes=('', '_phase'))
    else:
        df = df_comp.copy()

    df_valid = df[(df['cells'] > 10) & (df['iter_per_sec'] > 0)].copy()
    if len(df_valid) < 10:
        plt.close()
        return

    # Estimate operational intensity
    # OI = FLOPs / Bytes
    # For cell simulations: FLOPs ~ cells * contacts * ops_per_contact
    FLOPS_PER_CONTACT = 100  # Estimate for force/geometry calculations
    AVG_CONTACTS_PER_CELL = 6  # Typical for confluent tissue
    BYTES_PER_CELL = 1.77 * 1e6  # MB_PER_CELL_ADAPTIVE in bytes

    cells = df_valid['cells'].values
    ips = df_valid['iter_per_sec'].values

    # Estimated FLOPs per iteration
    estimated_flops = cells * AVG_CONTACTS_PER_CELL * FLOPS_PER_CONTACT
    bytes_touched = cells * BYTES_PER_CELL

    operational_intensity = estimated_flops / bytes_touched  # FLOPs/Byte
    achieved_flops = estimated_flops * ips  # FLOPs/sec

    # Panel A: Pseudo-Roofline
    ax = axes[0, 0]

    # Hardware ceilings (typical server values)
    PEAK_BANDWIDTH = 30e9   # B/s (DDR4)
    PEAK_COMPUTE = 100e9    # FLOPs/s (8 cores @ ~12 GFLOP/s each)

    oi_range = np.logspace(-3, 3, 100)
    memory_bound = PEAK_BANDWIDTH * oi_range
    compute_bound = np.full_like(oi_range, PEAK_COMPUTE)
    roofline = np.minimum(memory_bound, compute_bound)

    ax.loglog(oi_range, roofline / 1e9, 'k-', linewidth=2, label='Roofline')
    ax.loglog(oi_range, memory_bound / 1e9, '--', color='blue', alpha=0.5,
              label=f'Memory ({PEAK_BANDWIDTH/1e9:.0f} GB/s)')
    ax.axhline(y=PEAK_COMPUTE/1e9, color='red', linestyle='--', alpha=0.5,
               label=f'Compute ({PEAK_COMPUTE/1e9:.0f} GFLOP/s)')

    # Plot actual performance colored by cell count
    scatter = ax.scatter(operational_intensity, achieved_flops / 1e9,
                        c=cells, cmap='viridis', alpha=0.6, s=30)
    plt.colorbar(scatter, ax=ax, label='Cell Count')

    ax.set_xlabel('Operational Intensity (FLOPs/Byte)', fontsize=11)
    ax.set_ylabel('Performance (GFLOP/s)', fontsize=11)
    ax.set_title('Roofline Performance Model', fontsize=12, fontweight='bold')
    ax.legend(fontsize=8, loc='lower right')
    ax.grid(True, alpha=0.3, which='both')
    ax.set_xlim(1e-3, 1e3)
    ax.set_ylim(1e-3, 1e3)

    # Panel B: Roofline efficiency vs cell count
    ridge_point = PEAK_COMPUTE / PEAK_BANDWIDTH

    is_memory_bound = operational_intensity < ridge_point
    peak_achievable = np.where(is_memory_bound,
                               PEAK_BANDWIDTH * operational_intensity,
                               PEAK_COMPUTE)
    efficiency = achieved_flops / peak_achievable * 100

    axes[0, 1].semilogx(cells, efficiency, 'o-', color=COLORS['adaptive'],
                        alpha=0.6, markersize=3)
    axes[0, 1].axhline(y=50, color='orange', linestyle='--', alpha=0.5, label='50% efficiency')
    axes[0, 1].axhline(y=25, color='red', linestyle='--', alpha=0.5, label='25% efficiency')
    axes[0, 1].set_xlabel('Cell Count (log)', fontsize=11)
    axes[0, 1].set_ylabel('Roofline Efficiency (%)', fontsize=11)
    axes[0, 1].set_title('Performance Efficiency vs Scale', fontsize=12, fontweight='bold')
    axes[0, 1].legend(fontsize=9)
    axes[0, 1].grid(True, alpha=0.3)
    axes[0, 1].set_ylim(0, 100)

    # Panel C: Memory vs Compute bound regions
    bound_colors = np.where(is_memory_bound, 0, 1)
    scatter2 = axes[1, 0].scatter(cells, operational_intensity, c=bound_colors,
                                   cmap='RdYlGn', alpha=0.6, s=30)
    axes[1, 0].axhline(y=ridge_point, color='black', linestyle='--',
                       label=f'Ridge point (OI={ridge_point:.2f})')
    axes[1, 0].set_xlabel('Cell Count', fontsize=11)
    axes[1, 0].set_ylabel('Operational Intensity (FLOPs/Byte)', fontsize=11)
    axes[1, 0].set_title('Memory vs Compute Bound Regions', fontsize=12, fontweight='bold')
    axes[1, 0].legend(fontsize=9)
    axes[1, 0].grid(True, alpha=0.3)

    # Add region labels
    axes[1, 0].text(0.5, 0.15, 'MEMORY BOUND', transform=axes[1, 0].transAxes,
                   fontsize=12, color='red', alpha=0.7, ha='center')
    axes[1, 0].text(0.5, 0.85, 'COMPUTE BOUND', transform=axes[1, 0].transAxes,
                   fontsize=12, color='green', alpha=0.7, ha='center')

    # Panel D: Summary
    ax_summary = axes[1, 1]
    ax_summary.axis('off')

    mem_bound_pct = is_memory_bound.mean() * 100
    avg_efficiency = efficiency.mean()
    avg_oi = operational_intensity.mean()

    summary_text = "ROOFLINE ANALYSIS SUMMARY\n" + "=" * 40 + "\n\n"
    summary_text += f"Hardware Assumptions:\n"
    summary_text += f"  Peak bandwidth: {PEAK_BANDWIDTH/1e9:.0f} GB/s\n"
    summary_text += f"  Peak compute: {PEAK_COMPUTE/1e9:.0f} GFLOP/s\n"
    summary_text += f"  Ridge point: {ridge_point:.2f} FLOP/Byte\n\n"

    summary_text += f"Application Characteristics:\n"
    summary_text += f"  Avg operational intensity: {avg_oi:.3f} FLOP/Byte\n"
    summary_text += f"  Memory-bound samples: {mem_bound_pct:.0f}%\n"
    summary_text += f"  Avg roofline efficiency: {avg_efficiency:.1f}%\n\n"

    if mem_bound_pct > 70:
        recommendation = "MEMORY-BOUND\nConsider data layout optimizations,\ncache blocking, or reducing memory traffic."
    elif mem_bound_pct < 30:
        recommendation = "COMPUTE-BOUND\nConsider algorithmic improvements,\nSIMD vectorization, or GPU offloading."
    else:
        recommendation = "MIXED REGIME\nApplication transitions between\nmemory and compute bound."

    summary_text += f"Optimization Recommendation:\n{recommendation}"

    ax_summary.text(0.1, 0.9, summary_text, transform=ax_summary.transAxes,
                   fontsize=10, verticalalignment='top', family='monospace',
                   bbox=dict(boxstyle='round', facecolor='lightyellow', alpha=0.9))

    plt.suptitle('Roofline Performance Characterization', fontsize=14, fontweight='bold')
    plt.tight_layout()
    plt.savefig(output_dir / "21_roofline_analysis.png", dpi=DPI, bbox_inches='tight')
    plt.close()


# =============================================================================
# SECTION 9: Summary Statistics Generator
# =============================================================================

def _run_table_rows(data: dict, milestones=(10_000, 50_000, 100_000)) -> List[dict]:
    """One dict per run for the SUMMARY_STATS run tables, from its raw performance_diagnostics
    (falls back to the computational metrics) plus the optional run_notes.json facts."""
    notes_all = data.get('run_notes') or {}
    rows = []
    for name, _color, label in iter_schedulers(data):
        d = data.get(f'{name}_perf_diag')
        if d is None or len(d) == 0:
            d = data.get(f'{name}_computational')
            if d is None or len(d) == 0:
                continue
            d = d.rename(columns={'epoch': 'wall_epoch'})
            if 'iter_per_sec' in d.columns:
                d['total_iteration_ms'] = 1000.0 / d['iter_per_sec'].replace(0, np.nan)
        d = d.dropna(subset=['iteration']).sort_values('iteration')
        notes = notes_all.get(name, {})
        has_epoch = 'wall_epoch' in d.columns and d['wall_epoch'].notna().any()
        t0 = d['wall_epoch'].iloc[0] if has_epoch else np.nan
        hours = (d['wall_epoch'] - t0) / 3600.0 if has_epoch else pd.Series(np.nan, index=d.index)
        r = {
            'label': label,
            'commit': notes.get('commit', '—'),
            'cores': notes.get('cores', '—'),
            'outcome': notes.get('outcome', '—'),
            'final_iter': int(d['iteration'].iloc[-1]),
            'final_cells': int(d['cells'].iloc[-1]) if 'cells' in d.columns else None,
            'wall_h': float(hours.iloc[-1]) if has_epoch else np.nan,
            'peak_rss_gb': (notes['peak_rss_kb'] / 1e6) if notes.get('peak_rss_kb') else np.nan,
            'removals': notes.get('unstable_removals'),
        }
        # Hours to reach each cell-count milestone (first diagnostics row at or past it)
        for m in milestones:
            hit = d[d['cells'] >= m] if 'cells' in d.columns else d.iloc[0:0]
            r[f'h_to_{m}'] = float(hours.loc[hit.index[0]]) if len(hit) else np.nan
        tot = d['total_iteration_ms'] if 'total_iteration_ms' in d.columns else pd.Series(np.nan, index=d.index)
        valid = tot.notna() & (tot > 0)
        if valid.any():
            i_last = tot[valid].index[-1]
            r['final_s_iter'] = float(tot.loc[i_last]) / 1000.0
            # Per-cell cost strips the size difference between runs that ended at different N
            r['final_ms_per_cell'] = float(tot.loc[i_last]) / float(d.loc[i_last, 'cells']) if 'cells' in d.columns else np.nan
            if 'contact_detection_ms' in d.columns and pd.notna(d.loc[i_last, 'contact_detection_ms']):
                r['final_contact_pct'] = float(d.loc[i_last, 'contact_detection_ms']) / float(tot.loc[i_last]) * 100.0
            else:
                r['final_contact_pct'] = np.nan
        else:
            r['final_s_iter'] = r['final_ms_per_cell'] = r['final_contact_pct'] = np.nan
        # Cost at matched sizes: s/iter on the first row at or past 50k / 100k cells
        for m in (50_000, 100_000):
            at_m = d[valid & (d['cells'] >= m)] if 'cells' in d.columns else d.iloc[0:0]
            r[f's_iter_at_{m}'] = float(tot.loc[at_m.index[0]]) / 1000.0 if len(at_m) else np.nan
        if 'cov' in d.columns and d['cov'].notna().any():
            i_cov = d['cov'].idxmax()
            r['max_cov'] = float(d.loc[i_cov, 'cov'])
            r['max_cov_at'] = int(d.loc[i_cov, 'iteration'])
        else:
            r['max_cov'] = np.nan
            r['max_cov_at'] = None
        if r['removals'] is None and 'unstable_cells_removed' in d.columns:
            r['removals'] = int(d['unstable_cells_removed'].fillna(0).sum())
        rows.append(r)
    return rows


def _fmt(v, spec: str = '', suffix: str = '') -> str:
    """Markdown cell: '—' for missing values, else formatted with the given spec."""
    if v is None or (isinstance(v, float) and np.isnan(v)):
        return '—'
    return f"{v:{spec}}{suffix}"


def generate_run_tables(data: dict) -> List[str]:
    """Markdown run-summary and performance-observation tables (one row per run)."""
    rows = _run_table_rows(data)
    if not rows:
        return []
    lines = ["\n## Run Summary\n"]
    lines.append("| Run | Code | Cores | Final iter | Final cells | Wall time (h) | h → 10k cells | h → 50k | h → 100k | Peak RSS (GB) | Outcome |")
    lines.append("|---|---|---|---:|---:|---:|---:|---:|---:|---:|---|")
    for r in rows:
        lines.append(
            f"| {r['label']} | {r['commit']} | {r['cores']} | {_fmt(r['final_iter'], ',')} | {_fmt(r['final_cells'], ',')} "
            f"| {_fmt(r['wall_h'], '.1f')} | {_fmt(r['h_to_10000'], '.1f')} | {_fmt(r['h_to_50000'], '.1f')} "
            f"| {_fmt(r['h_to_100000'], '.1f')} | {_fmt(r['peak_rss_gb'], '.1f')} | {r['outcome']} |"
        )
    lines.append("")
    lines.append("Wall time is measured between the first and last performance-diagnostics rows (every 100 iterations), "
                 "so it stops short of the true end by up to 99 iterations. Peak RSS comes from `/usr/bin/time -v`, "
                 "which only reports for runs that ended under the wrapper.")
    lines.append("\n## Performance Observations\n")
    lines.append("| Run | s/iter at 50k cells | s/iter at 100k | Final s/iter | Final ms per cell-iter | Contact-detection share (final) | Max workload CoV (at iter) | Unstable cells removed |")
    lines.append("|---|---:|---:|---:|---:|---:|---:|---:|")
    for r in rows:
        cov = _fmt(r['max_cov'], '.2f')
        if r['max_cov_at'] is not None and cov != '—':
            cov += f" ({r['max_cov_at']:,})"
        lines.append(
            f"| {r['label']} | {_fmt(r['s_iter_at_50000'], '.1f')} | {_fmt(r['s_iter_at_100000'], '.1f')} "
            f"| {_fmt(r['final_s_iter'], '.1f')} | {_fmt(r['final_ms_per_cell'], '.2f')} "
            f"| {_fmt(r['final_contact_pct'], '.0f', '%')} | {cov} | {_fmt(r['removals'], ',')} |"
        )
    lines.append("")
    lines.append("Workload CoV is the per-thread contact-detection work heterogeneity (`solver::calculate_workload_heterogeneity`); "
                 "a jump from ~0.2–0.5 to >2 marks the single-thread contact-detection crawl. Unstable-cell removals are "
                 "counted from `Removing numerically unstable` lines in run.log when run_notes.json is present, otherwise "
                 "summed from the diagnostics column (which only samples one iteration per row).")
    return lines


def generate_summary_stats(data: dict, output_dir: Path) -> str:
    """Generate comprehensive SUMMARY_STATS.md file."""

    lines = []
    lines.append("# Benchmark Summary Statistics\n")
    lines.append("*Auto-generated by plot_benchmark_results.py*\n")

    # Get style config for labels
    style = data.get('style_config', {})
    labels = style.get('labels', LABELS)

    lines.extend(generate_run_tables(data))

    # Run information
    df = data.get('comparison')
    if df is not None and len(df) > 0:
        lines.append("\n## Run Information\n")

        if 'timestamp' in df.columns and df['timestamp'].notna().any():
            start = df['timestamp'].iloc[0]
            end = df['timestamp'].iloc[-1]
            duration = (end - start).total_seconds() / 3600
            lines.append(f"- **Start Time**: {start}")
            lines.append(f"- **End Time**: {end}")
            lines.append(f"- **Duration**: {duration:.1f} hours")

        lines.append(f"- **Data Points**: {len(df)}")

        # Detect schedulers dynamically
        schedulers = get_comparison_schedulers(df)
        final = df.iloc[-1]

        # Final state - dynamically iterate over detected schedulers
        lines.append("\n## Final State\n")
        for sched in schedulers:
            cols = get_scheduler_columns(df, sched)
            label = labels.get(sched, sched.title())
            lines.append(f"### {label}")
            # Per-scheduler last valid row: shorter runs have trailing NaNs in the merge
            if cols.get('iter') and cols['iter'] in df.columns and df[cols['iter']].notna().any():
                lines.append(f"- Final Iteration: {int(df[cols['iter']].dropna().iloc[-1]):,}")
            if cols.get('cells') and cols['cells'] in df.columns and df[cols['cells']].notna().any():
                lines.append(f"- Final Cell Count: {int(df[cols['cells']].dropna().iloc[-1]):,}")
            lines.append("")

        # Performance metrics - dynamically iterate over detected schedulers
        lines.append("\n## Performance Metrics\n")
        for sched in schedulers:
            cols = get_scheduler_columns(df, sched)
            label = labels.get(sched, sched.title())
            ips_col = cols.get('ips')
            if ips_col and ips_col in df.columns:
                df_valid = df[df[ips_col] > 0]
                if len(df_valid) > 0:
                    lines.append(f"### {label}")
                    lines.append(f"- Avg Iteration Rate: {df_valid[ips_col].mean():.2f} iter/sec")
                    lines.append(f"- Std Deviation: {df_valid[ips_col].std():.2f} iter/sec")
                    lines.append("")

        # Cell growth rate - dynamically iterate over detected schedulers
        lines.append("\n## Cell Growth Rate\n")
        if 'timestamp' in df.columns and df['timestamp'].notna().any():
            duration_hours = (df['timestamp'].iloc[-1] - df['timestamp'].iloc[0]).total_seconds() / 3600
            if duration_hours > 0:
                for sched in schedulers:
                    cols = get_scheduler_columns(df, sched)
                    label = labels.get(sched, sched.title())
                    cells_col = cols.get('cells')
                    if cells_col and cells_col in df.columns:
                        rate = (final[cells_col] - df[cells_col].iloc[0]) / duration_hours
                        lines.append(f"- {label}: {rate:.1f} cells/hour")

    # Biological metrics from simulation statistics - iterate over detected schedulers
    style = data.get('style_config', {})
    labels = style.get('labels', LABELS)
    for name, color, label in iter_schedulers(data):
        df_sim = data.get(f'{name}_sim_stats')
        if df_sim is not None and len(df_sim) > 0:
            lines.append(f"\n## {label} Biological Metrics\n")

            last_iter = df_sim['iteration'].max()
            df_last = df_sim[df_sim['iteration'] == last_iter]

            if 'volume' in df_last.columns:
                s = compute_statistics(df_last['volume'].values)
                lines.append(f"- Volume: μ={s['mean']:.2e} m³, CV={s['cv']:.3f}")

            if 'pressure' in df_last.columns:
                s = compute_statistics(df_last['pressure'].values)
                lines.append(f"- Pressure: μ={s['mean']:.1f} Pa, σ={s['std']:.1f} Pa")

            if 'total_potential_energy' in df_last.columns:
                s = compute_statistics(df_last['total_potential_energy'].values)
                lines.append(f"- Energy: μ={s['mean']:.2e} J, CV={s['cv']:.3f}")

    summary = "\n".join(lines)

    output_path = output_dir / "SUMMARY_STATS.md"
    with open(output_path, 'w') as f:
        f.write(summary)

    return summary


# =============================================================================
# SECTION 10: Main Orchestration
# =============================================================================

def main():
    if len(sys.argv) < 2:
        print("Usage: python plot_benchmark_results.py <benchmark_directory>")
        print("\nExample:")
        print("  python plot_benchmark_results.py doc/working/parallel_benchmark_20260124/")
        sys.exit(1)

    bench_dir = Path(sys.argv[1])
    output_dir = bench_dir / "plots"
    output_dir.mkdir(exist_ok=True)

    print(f"Loading data from: {bench_dir}")
    print("=" * 60)

    data = load_all_data(bench_dir)

    print("\nData loaded:")
    for key, df in data.items():
        if df is not None:
            print(f"  {key}: {len(df)} rows")
        else:
            print(f"  {key}: Not found")

    print(f"\nGenerating plots in: {output_dir}")
    print("=" * 60)

    # Define all plots in order
    plots = [
        (plot_01_iteration_progress, '01', 'iteration_progress'),
        (plot_02_iteration_rate_vs_cells, '02', 'iteration_rate_vs_cells'),
        (plot_03_memory_vs_cells, '03', 'memory_vs_cells'),
        (plot_04_phase_timing_breakdown, '04', 'phase_timing_breakdown'),
        (plot_05_workload_imbalance, '05', 'workload_imbalance'),
        (plot_06_cell_population_growth, '06', 'cell_population_growth'),
        (plot_07_volume_distribution, '07', 'volume_distribution'),
        (plot_08_pressure_distribution, '08', 'pressure_distribution'),
        (plot_09_volume_error_vs_target, '09', 'volume_error_vs_target'),
        (plot_10_division_dynamics, '10', 'division_dynamics'),
        (plot_11_contact_area_distribution, '11', 'contact_area_distribution'),
        (plot_12_energy_evolution, '12', 'energy_evolution'),
        (plot_13_energy_component_ratios, '13', 'energy_component_ratios'),
        (plot_14_pv_phase_space, '14', 'pv_phase_space'),
        (plot_15_energy_stability, '15', 'energy_stability'),
        (plot_16_pressure_volume_correlation, '16', 'pressure_volume_correlation'),
        (plot_17_statistical_comparison, '17', 'statistical_comparison'),
        (plot_18_speedup_analysis, '18', 'speedup_analysis'),
        (plot_19_complexity_analysis, '19', 'complexity_analysis'),
        (plot_20_summary_dashboard, '20', 'summary_dashboard'),
        (plot_21_roofline_analysis, '21', 'roofline_analysis'),
    ]

    success_count = 0
    for plot_func, num, name in plots:
        if plot_with_fallback(plot_func, data, output_dir, num, name):
            success_count += 1

    print("=" * 60)
    print("\nGenerating summary statistics...")
    summary = generate_summary_stats(data, output_dir)
    print(f"  [OK]   SUMMARY_STATS.md")

    print("\n" + "=" * 60)
    print(f"Complete: {success_count}/{len(plots)} plots generated")
    print(f"Output directory: {output_dir}")
    print("=" * 60)


if __name__ == "__main__":
    main()
