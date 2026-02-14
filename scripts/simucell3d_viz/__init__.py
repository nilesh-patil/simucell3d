"""
SimuCell3D Benchmark Visualization Package

Publication-quality visualization suite for SimuCell3D benchmark analysis.
Supports automatic scheduler detection, data validation, and 17+ plot types.

Usage:
    from simucell3d_viz import validate_benchmark, load_and_clean_data, generate_plots

    # Validate data first
    report = validate_benchmark(bench_dir)

    # Load with automatic cleaning
    data = load_and_clean_data(bench_dir)

    # Generate all plots
    generate_plots(data, output_dir)

CLI Usage:
    python -m simucell3d_viz <benchmark_dir> [options]
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
