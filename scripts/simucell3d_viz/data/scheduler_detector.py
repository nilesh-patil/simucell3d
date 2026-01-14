"""
Scheduler auto-detection for SimuCell3D benchmarks.

Automatically detects available scheduler modes from benchmark directory structure
by scanning metrics/{scheduler}/ and sim_{scheduler}/ patterns.
"""

from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Any
import numpy as np

# Optional matplotlib for color generation
try:
    import matplotlib.cm as cm
    MATPLOTLIB_AVAILABLE = True
except ImportError:
    MATPLOTLIB_AVAILABLE = False


@dataclass
class SchedulerMetadata:
    """
    Metadata for a detected scheduler mode.

    Attributes:
        name: Scheduler identifier (e.g., 'v1', 'adaptive', 'static')
        metrics_dir: Path to metrics subdirectory
        sim_dir: Path to simulation data directory (optional)
        has_biological: Whether biological.csv exists
        has_computational: Whether computational.csv exists
        has_phase_timings: Whether phase_timings.csv exists
        has_workload: Whether workload.csv exists
        has_simulation_stats: Whether simulation_statistics.csv exists
        row_counts: Dict mapping file type to row count
    """
    name: str
    metrics_dir: Optional[Path] = None
    sim_dir: Optional[Path] = None
    has_biological: bool = False
    has_computational: bool = False
    has_phase_timings: bool = False
    has_workload: bool = False
    has_simulation_stats: bool = False
    row_counts: Dict[str, int] = field(default_factory=dict)

    def get_file_path(self, file_type: str) -> Optional[Path]:
        """
        Get path to a specific file type.

        Args:
            file_type: One of 'biological', 'computational', 'phase_timings',
                      'workload', 'simulation_statistics'

        Returns:
            Path to file if it exists, None otherwise
        """
        if file_type == 'simulation_statistics' and self.sim_dir:
            path = self.sim_dir / 'simulation_statistics.csv'
            return path if path.exists() else None

        if self.metrics_dir:
            path = self.metrics_dir / f'{file_type}.csv'
            return path if path.exists() else None

        return None

    def summary(self) -> str:
        """Return a human-readable summary of this scheduler's data availability."""
        available = []
        if self.has_biological:
            available.append('biological')
        if self.has_computational:
            available.append('computational')
        if self.has_phase_timings:
            available.append('phase_timings')
        if self.has_workload:
            available.append('workload')
        if self.has_simulation_stats:
            rows = self.row_counts.get('simulation_statistics', '?')
            available.append(f'simulation_stats({rows} rows)')

        return f"{self.name}: {', '.join(available) if available else 'no data'}"


def detect_schedulers(bench_dir: Path) -> List[SchedulerMetadata]:
    """
    Auto-detect scheduler modes from benchmark directory structure.

    Scans two directory patterns:
    1. metrics/{scheduler_name}/ - Aggregated metrics
    2. sim_{scheduler_name}/ - Detailed simulation data

    Args:
        bench_dir: Root benchmark directory

    Returns:
        List of SchedulerMetadata objects sorted by name

    Raises:
        ValueError: If no scheduler data found

    Example:
        >>> schedulers = detect_schedulers(Path('/benchmark'))
        >>> print([s.name for s in schedulers])
        ['adaptive', 'static', 'v1']
    """
    bench_dir = Path(bench_dir)
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

    # Pattern 2: Scan sim_* directories for simulation_statistics.csv
    for sim_dir in bench_dir.glob("sim_*"):
        if sim_dir.is_dir():
            name = sim_dir.name.replace("sim_", "")
            if name not in schedulers:
                schedulers[name] = SchedulerMetadata(name=name)

            meta = schedulers[name]
            meta.sim_dir = sim_dir
            meta.has_simulation_stats = (sim_dir / "simulation_statistics.csv").exists()

            # Quick row count for simulation_statistics.csv
            sim_stats_path = sim_dir / "simulation_statistics.csv"
            if sim_stats_path.exists():
                try:
                    # Fast row count using line counting
                    with open(sim_stats_path, 'rb') as f:
                        meta.row_counts['simulation_statistics'] = sum(1 for _ in f) - 1
                except Exception:
                    pass

    if not schedulers:
        raise ValueError(
            f"No scheduler data found in {bench_dir}. "
            f"Expected metrics/{{scheduler}}/ or sim_{{scheduler}}/ directories."
        )

    # Sort by name for consistent ordering
    return sorted(schedulers.values(), key=lambda s: s.name)


def generate_style_config(schedulers: List[SchedulerMetadata]) -> Dict[str, Any]:
    """
    Generate dynamic color/label/marker mappings for N schedulers.

    Uses viridis colormap for perceptual uniformity and CVD-safety.
    Colors are evenly spaced across the colormap to maximize contrast.

    Args:
        schedulers: List of detected scheduler metadata

    Returns:
        Dict with 'colors', 'labels', 'markers', 'linestyles' sub-dicts

    Example:
        >>> style = generate_style_config(schedulers)
        >>> ax.plot(x, y, color=style['colors']['v1'], label=style['labels']['v1'])
    """
    n = len(schedulers)

    # Generate evenly-spaced colors (avoid colormap extremes for better visibility)
    if n == 1:
        color_positions = [0.5]
    elif n == 2:
        color_positions = [0.25, 0.75]  # Maximum contrast for 2 schedulers
    else:
        color_positions = list(np.linspace(0.15, 0.85, n))

    # Generate colors
    colors = {}
    if MATPLOTLIB_AVAILABLE:
        for meta, pos in zip(schedulers, color_positions):
            colors[meta.name] = cm.viridis(pos)
    else:
        # Fallback hex colors if matplotlib not available
        fallback_colors = ['#440154', '#21918c', '#fde725', '#5ec962', '#3b528b']
        for i, meta in enumerate(schedulers):
            colors[meta.name] = fallback_colors[i % len(fallback_colors)]

    # Generate human-readable labels
    label_map = {
        'v1': 'v1.0 Baseline',
        'adaptive': 'Adaptive Scheduler',
        'static': 'Static Scheduler',
        'guided': 'Guided Scheduler',
        'dynamic': 'Dynamic Scheduler',
    }
    labels = {meta.name: label_map.get(meta.name, meta.name.replace('_', ' ').title())
              for meta in schedulers}

    # Cycle through markers and linestyles
    marker_cycle = ['o', 's', '^', 'v', 'D', 'P', '*', 'X']
    linestyle_cycle = ['-', '--', '-.', ':']

    markers = {meta.name: marker_cycle[i % len(marker_cycle)]
               for i, meta in enumerate(schedulers)}
    linestyles = {meta.name: linestyle_cycle[i % len(linestyle_cycle)]
                  for i, meta in enumerate(schedulers)}

    return {
        'colors': colors,
        'labels': labels,
        'markers': markers,
        'linestyles': linestyles,
    }


# Phase colors (fixed - not scheduler-dependent)
def get_phase_colors() -> Dict[str, Any]:
    """Get colors for simulation phases."""
    if MATPLOTLIB_AVAILABLE:
        return {
            'mesh_refinement': cm.viridis(0.15),
            'contact_detection': cm.viridis(0.45),
            'polarization': cm.viridis(0.65),
            'time_integration': cm.viridis(0.85),
        }
    else:
        return {
            'mesh_refinement': '#440154',
            'contact_detection': '#31688e',
            'polarization': '#35b779',
            'time_integration': '#fde725',
        }


def print_scheduler_summary(schedulers: List[SchedulerMetadata]) -> None:
    """Print a formatted summary of detected schedulers."""
    print(f"\nDetected {len(schedulers)} scheduler(s):")
    print("-" * 50)
    for sched in schedulers:
        print(f"  {sched.summary()}")
    print("-" * 50)
