"""
Data handling for the SimuCell3D benchmark plots.

Helper package of ``scripts/plot_benchmark_unified.py``: validation of a benchmark
directory, data loading and cleaning with an audit trail, detection of the scheduling
modes present (``sim_<mode>/`` subdirectories), and the statistics used by the plots.
The plots themselves are drawn by ``plot_benchmark_unified.py``; this package has no
command-line entry point.

Usage:
    import sys; sys.path.insert(0, "scripts")
    from simucell3d_viz import validate_benchmark, load_and_clean_data

    report = validate_benchmark(bench_dir)
    data, audit = load_and_clean_data(bench_dir)
"""

__version__ = "2.0.0"
__author__ = "SimuCell3D Team"

from .data.validator import validate_benchmark, ValidationReport
from .data.cleaner import load_and_clean_data, AuditTrail
from .data.scheduler_detector import detect_schedulers, SchedulerMetadata

__all__ = [
    "validate_benchmark",
    "ValidationReport",
    "load_and_clean_data",
    "AuditTrail",
    "detect_schedulers",
    "SchedulerMetadata",
]
