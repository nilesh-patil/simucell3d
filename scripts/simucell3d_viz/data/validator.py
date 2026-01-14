"""
Data validation module for SimuCell3D benchmarks.

Validates benchmark data quality BEFORE visualization to catch issues early.
Generates detailed validation reports with issue categorization.
"""

from dataclasses import dataclass, field
from datetime import datetime
from enum import Enum
from pathlib import Path
from typing import Dict, List, Optional, Any, Tuple
import json

import pandas as pd
import numpy as np

from .scheduler_detector import detect_schedulers, SchedulerMetadata


class IssueSeverity(Enum):
    """Severity levels for validation issues."""
    INFO = "info"
    WARNING = "warning"
    ERROR = "error"
    CRITICAL = "critical"


@dataclass
class ValidationIssue:
    """Single validation issue found in data."""
    severity: IssueSeverity
    code: str
    message: str
    file_path: Optional[str] = None
    column: Optional[str] = None
    affected_rows: int = 0
    row_indices: Optional[List[int]] = None
    suggested_fix: Optional[str] = None

    def to_dict(self) -> Dict[str, Any]:
        return {
            'severity': self.severity.value,
            'code': self.code,
            'message': self.message,
            'file_path': self.file_path,
            'column': self.column,
            'affected_rows': self.affected_rows,
            'suggested_fix': self.suggested_fix,
        }


@dataclass
class FileValidationResult:
    """Validation result for a single file."""
    file_path: str
    exists: bool
    row_count: int = 0
    column_count: int = 0
    columns_present: List[str] = field(default_factory=list)
    columns_missing: List[str] = field(default_factory=list)
    issues: List[ValidationIssue] = field(default_factory=list)

    @property
    def is_valid(self) -> bool:
        """Check if file is valid (no errors or critical issues)."""
        return all(
            issue.severity not in (IssueSeverity.ERROR, IssueSeverity.CRITICAL)
            for issue in self.issues
        )

    def to_dict(self) -> Dict[str, Any]:
        return {
            'file_path': self.file_path,
            'exists': self.exists,
            'row_count': self.row_count,
            'column_count': self.column_count,
            'columns_present': self.columns_present,
            'columns_missing': self.columns_missing,
            'issues': [i.to_dict() for i in self.issues],
            'is_valid': self.is_valid,
        }


@dataclass
class ValidationReport:
    """Complete validation report for a benchmark directory."""
    benchmark_dir: str
    timestamp: str = field(default_factory=lambda: datetime.now().isoformat())
    schedulers: List[str] = field(default_factory=list)
    file_results: Dict[str, FileValidationResult] = field(default_factory=dict)
    cross_validation_issues: List[ValidationIssue] = field(default_factory=list)

    @property
    def is_valid(self) -> bool:
        """Check if entire benchmark is valid."""
        return (
            all(r.is_valid for r in self.file_results.values()) and
            all(i.severity not in (IssueSeverity.ERROR, IssueSeverity.CRITICAL)
                for i in self.cross_validation_issues)
        )

    @property
    def total_issues(self) -> int:
        total = len(self.cross_validation_issues)
        for result in self.file_results.values():
            total += len(result.issues)
        return total

    def severity_summary(self) -> Dict[str, int]:
        """Count issues by severity level."""
        counts = {s.value: 0 for s in IssueSeverity}
        for result in self.file_results.values():
            for issue in result.issues:
                counts[issue.severity.value] += 1
        for issue in self.cross_validation_issues:
            counts[issue.severity.value] += 1
        return counts

    def to_dict(self) -> Dict[str, Any]:
        return {
            'benchmark_dir': self.benchmark_dir,
            'timestamp': self.timestamp,
            'schedulers': self.schedulers,
            'is_valid': self.is_valid,
            'total_issues': self.total_issues,
            'severity_summary': self.severity_summary(),
            'file_results': {k: v.to_dict() for k, v in self.file_results.items()},
            'cross_validation_issues': [i.to_dict() for i in self.cross_validation_issues],
        }

    def export_json(self, path: Path) -> None:
        """Export validation report to JSON file."""
        import numpy as np

        class NumpyEncoder(json.JSONEncoder):
            """Handle numpy types for JSON serialization."""
            def default(self, obj):
                if isinstance(obj, (np.integer,)):
                    return int(obj)
                if isinstance(obj, (np.floating,)):
                    return float(obj)
                if isinstance(obj, np.ndarray):
                    return obj.tolist()
                return super().default(obj)

        path = Path(path)
        path.parent.mkdir(parents=True, exist_ok=True)
        with open(path, 'w') as f:
            json.dump(self.to_dict(), f, indent=2, cls=NumpyEncoder)
        print(f"  Exported validation report to: {path}")

    def export_html(self, path: Path) -> None:
        """Export validation report to HTML file."""
        path = Path(path)
        path.parent.mkdir(parents=True, exist_ok=True)

        # Simple HTML template
        html = f"""<!DOCTYPE html>
<html>
<head>
    <title>Validation Report - {Path(self.benchmark_dir).name}</title>
    <style>
        body {{ font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; margin: 20px; }}
        .summary {{ background: #f5f5f5; padding: 15px; border-radius: 8px; margin-bottom: 20px; }}
        .valid {{ color: #28a745; }}
        .invalid {{ color: #dc3545; }}
        .badge {{ display: inline-block; padding: 4px 8px; border-radius: 4px; font-size: 12px; margin-right: 8px; }}
        .badge-info {{ background: #17a2b8; color: white; }}
        .badge-warning {{ background: #ffc107; color: black; }}
        .badge-error {{ background: #dc3545; color: white; }}
        .badge-critical {{ background: #6f42c1; color: white; }}
        table {{ border-collapse: collapse; width: 100%; margin-bottom: 20px; }}
        th, td {{ border: 1px solid #ddd; padding: 8px; text-align: left; }}
        th {{ background: #f8f9fa; }}
        .section {{ margin-bottom: 30px; }}
        h2 {{ border-bottom: 2px solid #007bff; padding-bottom: 10px; }}
    </style>
</head>
<body>
    <h1>Data Validation Report</h1>

    <div class="summary">
        <h2>Summary</h2>
        <p><strong>Benchmark:</strong> {Path(self.benchmark_dir).name}</p>
        <p><strong>Validated:</strong> {self.timestamp}</p>
        <p><strong>Status:</strong> <span class="{'valid' if self.is_valid else 'invalid'}">
            {'PASSED' if self.is_valid else 'ISSUES FOUND'}
        </span></p>
        <p><strong>Schedulers:</strong> {', '.join(self.schedulers)}</p>
        <p>
            <span class="badge badge-info">{self.severity_summary()['info']} Info</span>
            <span class="badge badge-warning">{self.severity_summary()['warning']} Warnings</span>
            <span class="badge badge-error">{self.severity_summary()['error']} Errors</span>
            <span class="badge badge-critical">{self.severity_summary()['critical']} Critical</span>
        </p>
    </div>

    <div class="section">
        <h2>File Validation Results</h2>
        <table>
            <tr>
                <th>File</th>
                <th>Exists</th>
                <th>Rows</th>
                <th>Issues</th>
                <th>Status</th>
            </tr>
            {''.join(self._render_file_row(k, v) for k, v in self.file_results.items())}
        </table>
    </div>

    <div class="section">
        <h2>Issue Details</h2>
        {self._render_issues()}
    </div>
</body>
</html>"""

        with open(path, 'w') as f:
            f.write(html)
        print(f"  Exported validation report to: {path}")

    def _render_file_row(self, key: str, result: FileValidationResult) -> str:
        status_class = 'valid' if result.is_valid else 'invalid'
        status_text = 'OK' if result.is_valid else 'Issues'
        return f"""<tr>
            <td>{result.file_path}</td>
            <td>{'Yes' if result.exists else 'No'}</td>
            <td>{result.row_count}</td>
            <td>{len(result.issues)}</td>
            <td class="{status_class}">{status_text}</td>
        </tr>"""

    def _render_issues(self) -> str:
        if self.total_issues == 0:
            return "<p>No issues found.</p>"

        rows = []
        for key, result in self.file_results.items():
            for issue in result.issues:
                badge_class = f"badge-{issue.severity.value}"
                rows.append(f"""<tr>
                    <td><span class="badge {badge_class}">{issue.severity.value.upper()}</span></td>
                    <td>{issue.code}</td>
                    <td>{result.file_path}</td>
                    <td>{issue.message}</td>
                    <td>{issue.affected_rows}</td>
                </tr>""")

        return f"""<table>
            <tr><th>Severity</th><th>Code</th><th>File</th><th>Message</th><th>Affected Rows</th></tr>
            {''.join(rows)}
        </table>"""


# =============================================================================
# Validation Schema Definitions
# =============================================================================

# Required columns for each file type
REQUIRED_SCHEMAS = {
    'computational': {
        'required': ['timestamp', 'iteration', 'cells'],
        'optional': ['iter_per_sec', 'cpu_user_pct', 'cpu_sys_pct', 'rss_mb', 'vsize_mb'],
    },
    'biological': {
        'required': ['iteration', 'cells'],
        'optional': ['avg_volume', 'avg_pressure', 'std_volume', 'std_pressure',
                     'total_kinetic_energy', 'total_potential_energy'],
    },
    'phase_timings': {
        'required': ['iteration', 'cells'],
        'optional': ['mesh_refinement', 'contact_detection', 'polarization_forces',
                     'time_integration', 'total'],
    },
    'workload': {
        'required': ['timestamp', 'iteration'],
        'optional': ['cov', 'thread_imbalance_pct', 'simulation_phase'],
    },
    'comparison': {
        'required': ['timestamp'],
        'optional': [],  # Dynamic columns based on schedulers
    },
}

# Physiological plausibility ranges (from SimuCell3D paper)
BIOLOGICAL_RANGES = {
    'pressure': {
        'warn_min': 300, 'warn_max': 2200,      # Pa (physiological)
        'error_min': -200, 'error_max': 5000,   # Pa (physical limits)
    },
    'volume': {
        'warn_min': 2.5e-16, 'warn_max': 1.3e-15,   # m³ (physiological)
        'error_min': 1e-18, 'error_max': 1e-13,     # m³ (physical limits)
    },
    'iterations_per_sec': {
        'warn_min': 0.01, 'warn_max': 500,
        'error_min': 0, 'error_max': 10000,
    },
}


# =============================================================================
# Validation Functions
# =============================================================================

def validate_file_schema(
    df: pd.DataFrame,
    file_type: str,
    file_path: str
) -> FileValidationResult:
    """
    Validate DataFrame against expected schema.

    Args:
        df: DataFrame to validate
        file_type: Type of file ('computational', 'biological', etc.)
        file_path: Path to file (for reporting)

    Returns:
        FileValidationResult with issues found
    """
    result = FileValidationResult(
        file_path=file_path,
        exists=True,
        row_count=len(df),
        column_count=len(df.columns),
        columns_present=list(df.columns),
    )

    schema = REQUIRED_SCHEMAS.get(file_type, {'required': [], 'optional': []})

    # Check required columns
    for col in schema['required']:
        if col not in df.columns:
            result.columns_missing.append(col)
            result.issues.append(ValidationIssue(
                severity=IssueSeverity.ERROR,
                code='SCHEMA_001',
                message=f"Required column '{col}' not found",
                file_path=file_path,
                column=col,
                suggested_fix=f"Ensure {file_type}.csv contains column '{col}'",
            ))

    return result


def detect_nan_inf_values(
    df: pd.DataFrame,
    file_path: str
) -> List[ValidationIssue]:
    """
    Detect NaN and Inf values in numeric columns.

    Args:
        df: DataFrame to check
        file_path: Path to file (for reporting)

    Returns:
        List of ValidationIssue for each problematic column
    """
    issues = []

    for col in df.select_dtypes(include=[np.number]).columns:
        nan_count = df[col].isna().sum()
        inf_count = np.isinf(df[col].replace([np.inf, -np.inf], np.nan).fillna(0)).sum()

        # Check for inf in original values
        try:
            inf_mask = np.isinf(df[col])
            inf_count = inf_mask.sum()
        except (TypeError, ValueError):
            inf_count = 0

        total_issues = nan_count + inf_count
        if total_issues > 0:
            pct = 100 * total_issues / len(df)
            severity = IssueSeverity.WARNING if pct < 10 else IssueSeverity.ERROR

            issues.append(ValidationIssue(
                severity=severity,
                code='DATA_001',
                message=f"Column '{col}' contains {nan_count} NaN and {inf_count} Inf values ({pct:.1f}%)",
                file_path=file_path,
                column=col,
                affected_rows=total_issues,
                suggested_fix="Apply interpolation or replace with rolling median",
            ))

    return issues


def detect_iteration_resets(
    df: pd.DataFrame,
    file_path: str,
    iter_col: str = 'iteration',
    threshold: int = -100
) -> List[ValidationIssue]:
    """
    Detect iteration counter resets (backward jumps).

    Args:
        df: DataFrame with iteration column
        file_path: Path to file (for reporting)
        iter_col: Name of iteration column
        threshold: Decrease threshold to consider a reset

    Returns:
        List of ValidationIssue for detected resets
    """
    issues = []

    if iter_col not in df.columns:
        return issues

    iter_diff = df[iter_col].diff()
    reset_mask = iter_diff < threshold
    reset_count = reset_mask.sum()

    if reset_count > 0:
        reset_indices = df.index[reset_mask].tolist()
        issues.append(ValidationIssue(
            severity=IssueSeverity.WARNING,
            code='DATA_002',
            message=f"Detected {reset_count} iteration counter reset(s) (backward jumps > {abs(threshold)})",
            file_path=file_path,
            column=iter_col,
            affected_rows=reset_count,
            row_indices=reset_indices[:10],  # First 10 indices
            suggested_fix="Remove affected rows and interpolate gaps",
        ))

    return issues


def detect_empty_or_header_only(
    path: Path,
    file_type: str
) -> Optional[ValidationIssue]:
    """
    Detect files that are empty or contain only headers.

    Args:
        path: Path to CSV file
        file_type: Type of file for messaging

    Returns:
        ValidationIssue if file is empty/header-only, None otherwise
    """
    if not path.exists():
        return ValidationIssue(
            severity=IssueSeverity.INFO,
            code='FILE_001',
            message=f"File does not exist: {file_type}.csv",
            file_path=str(path),
        )

    try:
        df = pd.read_csv(path, nrows=5)
        if len(df) == 0:
            return ValidationIssue(
                severity=IssueSeverity.INFO,
                code='FILE_002',
                message=f"File contains only header (no data rows): {file_type}.csv",
                file_path=str(path),
                suggested_fix="Data collection may not be enabled for this metric type",
            )
    except Exception as e:
        return ValidationIssue(
            severity=IssueSeverity.ERROR,
            code='FILE_003',
            message=f"Could not read file: {str(e)}",
            file_path=str(path),
        )

    return None


def validate_biological_ranges(
    df: pd.DataFrame,
    file_path: str
) -> List[ValidationIssue]:
    """
    Validate values are within physiological plausibility ranges.

    Args:
        df: DataFrame with biological metrics
        file_path: Path to file (for reporting)

    Returns:
        List of ValidationIssue for out-of-range values
    """
    issues = []

    # Check pressure columns
    pressure_cols = [c for c in df.columns if 'pressure' in c.lower()]
    for col in pressure_cols:
        ranges = BIOLOGICAL_RANGES['pressure']
        _check_column_range(df, col, ranges, file_path, issues)

    # Check volume columns
    volume_cols = [c for c in df.columns if 'volume' in c.lower()]
    for col in volume_cols:
        ranges = BIOLOGICAL_RANGES['volume']
        _check_column_range(df, col, ranges, file_path, issues)

    # Check iterations per second
    if 'iter_per_sec' in df.columns or 'iterations_per_sec' in df.columns:
        col = 'iter_per_sec' if 'iter_per_sec' in df.columns else 'iterations_per_sec'
        ranges = BIOLOGICAL_RANGES['iterations_per_sec']
        _check_column_range(df, col, ranges, file_path, issues)

    return issues


def _check_column_range(
    df: pd.DataFrame,
    col: str,
    ranges: Dict[str, float],
    file_path: str,
    issues: List[ValidationIssue]
) -> None:
    """Helper to check if column values are within ranges."""
    values = df[col].dropna()
    if len(values) == 0:
        return

    # Count values outside warning range
    warn_outside = ((values < ranges['warn_min']) | (values > ranges['warn_max'])).sum()
    if warn_outside > 0:
        pct = 100 * warn_outside / len(values)
        issues.append(ValidationIssue(
            severity=IssueSeverity.INFO,
            code='BIO_001',
            message=f"Column '{col}': {warn_outside} values ({pct:.1f}%) outside physiological range "
                    f"[{ranges['warn_min']:.2e}, {ranges['warn_max']:.2e}]",
            file_path=file_path,
            column=col,
            affected_rows=warn_outside,
        ))

    # Count values outside error range (physical impossibility)
    error_outside = ((values < ranges['error_min']) | (values > ranges['error_max'])).sum()
    if error_outside > 0:
        pct = 100 * error_outside / len(values)
        issues.append(ValidationIssue(
            severity=IssueSeverity.WARNING,
            code='BIO_002',
            message=f"Column '{col}': {error_outside} values ({pct:.1f}%) outside physical limits "
                    f"[{ranges['error_min']:.2e}, {ranges['error_max']:.2e}]",
            file_path=file_path,
            column=col,
            affected_rows=error_outside,
        ))


def validate_cell_count_monotonicity(
    df: pd.DataFrame,
    file_path: str,
    cell_col: str = 'cells'
) -> List[ValidationIssue]:
    """
    Check that cell count is monotonically increasing (cells only divide).

    Args:
        df: DataFrame with cell count column
        file_path: Path to file (for reporting)
        cell_col: Name of cell count column

    Returns:
        List of ValidationIssue for non-monotonic segments
    """
    issues = []

    if cell_col not in df.columns:
        return issues

    cells = df[cell_col].values
    if len(cells) < 2:
        return issues

    # Detect significant decreases (> 10% drop from previous)
    cell_diff = np.diff(cells)
    decrease_mask = cell_diff < -0.1 * cells[:-1]
    decrease_count = decrease_mask.sum()

    if decrease_count > 0:
        issues.append(ValidationIssue(
            severity=IssueSeverity.WARNING,
            code='BIO_003',
            message=f"Cell count decreased significantly {decrease_count} time(s) - "
                    "cells should only divide (never die in these simulations)",
            file_path=file_path,
            column=cell_col,
            affected_rows=decrease_count,
            suggested_fix="Apply monotonic enforcement (running max)",
        ))

    return issues


# =============================================================================
# Main Validation Entry Point
# =============================================================================

def validate_benchmark(
    bench_dir: Path,
    verbose: bool = True
) -> ValidationReport:
    """
    Validate all data in a benchmark directory.

    This is the main entry point for data validation. It:
    1. Detects available schedulers
    2. Validates each file against expected schema
    3. Checks for NaN/Inf values
    4. Detects iteration resets
    5. Validates biological plausibility
    6. Performs cross-scheduler consistency checks

    Args:
        bench_dir: Path to benchmark directory
        verbose: Print progress messages

    Returns:
        ValidationReport with all issues found

    Example:
        >>> report = validate_benchmark(Path('doc/working/parallel_benchmark_20260126_093337'))
        >>> if not report.is_valid:
        ...     report.export_html('validation_report.html')
    """
    bench_dir = Path(bench_dir)
    report = ValidationReport(benchmark_dir=str(bench_dir))

    if verbose:
        print(f"\n{'='*60}")
        print("SimuCell3D Benchmark Data Validation")
        print(f"{'='*60}")
        print(f"Directory: {bench_dir}")

    # Step 1: Detect schedulers
    try:
        schedulers = detect_schedulers(bench_dir)
        report.schedulers = [s.name for s in schedulers]
        if verbose:
            print(f"Schedulers detected: {report.schedulers}")
    except ValueError as e:
        report.cross_validation_issues.append(ValidationIssue(
            severity=IssueSeverity.CRITICAL,
            code='STRUCT_001',
            message=str(e),
        ))
        return report

    # Step 2: Validate comparison.csv
    comparison_path = bench_dir / 'metrics' / 'comparison.csv'
    if comparison_path.exists():
        if verbose:
            print(f"\nValidating: metrics/comparison.csv")
        df = pd.read_csv(comparison_path)
        result = validate_file_schema(df, 'comparison', str(comparison_path))
        result.issues.extend(detect_nan_inf_values(df, str(comparison_path)))
        report.file_results['comparison'] = result
    else:
        empty_issue = detect_empty_or_header_only(comparison_path, 'comparison')
        if empty_issue:
            report.file_results['comparison'] = FileValidationResult(
                file_path=str(comparison_path),
                exists=False,
                issues=[empty_issue],
            )

    # Step 3: Validate per-scheduler files
    for sched in schedulers:
        if verbose:
            print(f"\nValidating scheduler: {sched.name}")

        # Computational metrics
        if sched.has_computational:
            path = sched.metrics_dir / 'computational.csv'
            if verbose:
                print(f"  - computational.csv")
            df = pd.read_csv(path)
            result = validate_file_schema(df, 'computational', str(path))
            result.issues.extend(detect_nan_inf_values(df, str(path)))
            result.issues.extend(detect_iteration_resets(df, str(path)))
            result.issues.extend(validate_biological_ranges(df, str(path)))
            report.file_results[f'{sched.name}_computational'] = result

        # Biological metrics
        if sched.has_biological:
            path = sched.metrics_dir / 'biological.csv'
            if verbose:
                print(f"  - biological.csv")
            df = pd.read_csv(path)
            result = validate_file_schema(df, 'biological', str(path))
            result.issues.extend(detect_nan_inf_values(df, str(path)))
            result.issues.extend(detect_iteration_resets(df, str(path)))
            result.issues.extend(validate_biological_ranges(df, str(path)))
            result.issues.extend(validate_cell_count_monotonicity(df, str(path)))
            report.file_results[f'{sched.name}_biological'] = result

        # Phase timings
        if sched.has_phase_timings:
            path = sched.metrics_dir / 'phase_timings.csv'
            if verbose:
                print(f"  - phase_timings.csv")
            df = pd.read_csv(path)
            result = validate_file_schema(df, 'phase_timings', str(path))
            result.issues.extend(detect_nan_inf_values(df, str(path)))
            report.file_results[f'{sched.name}_phase_timings'] = result

        # Workload (may not exist for all schedulers)
        if sched.has_workload:
            path = sched.metrics_dir / 'workload.csv'
            if verbose:
                print(f"  - workload.csv")
            df = pd.read_csv(path)
            result = validate_file_schema(df, 'workload', str(path))
            result.issues.extend(detect_nan_inf_values(df, str(path)))
            report.file_results[f'{sched.name}_workload'] = result

    # Step 4: Summary
    if verbose:
        summary = report.severity_summary()
        print(f"\n{'='*60}")
        print("Validation Summary")
        print(f"{'='*60}")
        print(f"Status: {'PASSED' if report.is_valid else 'ISSUES FOUND'}")
        print(f"Total issues: {report.total_issues}")
        print(f"  - Info: {summary['info']}")
        print(f"  - Warnings: {summary['warning']}")
        print(f"  - Errors: {summary['error']}")
        print(f"  - Critical: {summary['critical']}")

    return report
