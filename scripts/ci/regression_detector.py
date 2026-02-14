#!/usr/bin/env python3
"""
Regression detector for SimuCell3D benchmark results.

Uses bootstrap resampling to determine if the latest benchmark run shows
statistically significant performance regression compared to baseline runs.

Detection criteria (BOTH must be true):
  1. p-value < 0.05 (bootstrap permutation test)
  2. Performance change > 10% (practical significance threshold)

Usage:
    python -m ci.regression_detector --db metrics.db --scheduler adaptive
    python -m ci.regression_detector --db metrics.db --scheduler adaptive --output report.md
"""

import argparse
import sqlite3
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import List, Optional

import numpy as np


@dataclass
class RegressionResult:
    """Result of regression detection analysis."""
    is_regression: bool
    p_value: float
    pct_change: float  # negative = slower
    baseline_mean_ips: float
    current_mean_ips: float
    n_baseline_runs: int
    n_baseline_samples: int
    n_current_samples: int
    scheduler: str
    phase_changes: dict = field(default_factory=dict)

    def to_markdown(self) -> str:
        """Generate a Markdown report suitable for GitHub PR comments."""
        if self.is_regression:
            status = "REGRESSION DETECTED"
            emoji = "warning"
        elif self.pct_change > 5:
            status = "IMPROVEMENT DETECTED"
            emoji = "rocket"
        else:
            status = "No significant change"
            emoji = "white_check_mark"

        lines = [
            f"## Benchmark Results: {status}",
            "",
            f"| Metric | Value |",
            f"|--------|-------|",
            f"| Status | :{emoji}: {status} |",
            f"| Scheduler | `{self.scheduler}` |",
            f"| Baseline IPS (mean) | {self.baseline_mean_ips:.2f} |",
            f"| Current IPS (mean) | {self.current_mean_ips:.2f} |",
            f"| Change | {self.pct_change:+.1f}% |",
            f"| P-value | {self.p_value:.4f} |",
            f"| Baseline runs | {self.n_baseline_runs} |",
            f"| Baseline samples | {self.n_baseline_samples} |",
            f"| Current samples | {self.n_current_samples} |",
        ]

        if self.phase_changes:
            lines.extend([
                "",
                "### Phase Timing Changes",
                "",
                "| Phase | Change |",
                "|-------|--------|",
            ])
            for phase, change in sorted(self.phase_changes.items()):
                flag = " :warning:" if abs(change) > 10 else ""
                lines.append(f"| {phase} | {change:+.1f}%{flag} |")

        if self.is_regression:
            lines.extend([
                "",
                "> **Action required:** This PR introduces a statistically significant "
                "performance regression. Please investigate before merging.",
            ])

        return "\n".join(lines)


def _bootstrap_test(baseline: np.ndarray, current: np.ndarray,
                    n_bootstrap: int = 10000, seed: int = 42) -> float:
    """Bootstrap permutation test for difference in means.

    Returns p-value for the hypothesis that current is slower than baseline.
    """
    rng = np.random.RandomState(seed)

    observed_diff = np.mean(current) - np.mean(baseline)

    pooled = np.concatenate([baseline, current])
    n_baseline = len(baseline)

    count_more_extreme = 0
    for _ in range(n_bootstrap):
        rng.shuffle(pooled)
        perm_diff = np.mean(pooled[:n_baseline]) - np.mean(pooled[n_baseline:])
        # One-sided: is current significantly SLOWER (lower IPS)?
        if perm_diff >= abs(observed_diff):
            count_more_extreme += 1

    return count_more_extreme / n_bootstrap


def detect_regression(
    conn: sqlite3.Connection,
    scheduler: str = "adaptive",
    n_baseline_runs: int = 5,
    significance: float = 0.05,
    change_threshold: float = 10.0,
    n_bootstrap: int = 10000
) -> RegressionResult:
    """Detect performance regression by comparing latest run against baseline.

    Args:
        conn: SQLite connection to metrics database
        scheduler: Scheduler to analyze
        n_baseline_runs: Number of previous runs to use as baseline
        significance: P-value threshold (default 0.05)
        change_threshold: Minimum % change to flag (default 10%)
        n_bootstrap: Number of bootstrap iterations

    Returns:
        RegressionResult with detection verdict and statistics
    """
    conn.row_factory = sqlite3.Row
    cursor = conn.execute(
        """SELECT id FROM benchmark_runs
           WHERE scheduler = ?
           ORDER BY id ASC""",
        (scheduler,)
    )
    run_ids = [row['id'] for row in cursor.fetchall()]
    conn.row_factory = None

    if len(run_ids) < 2:
        return RegressionResult(
            is_regression=False, p_value=1.0, pct_change=0.0,
            baseline_mean_ips=0.0, current_mean_ips=0.0,
            n_baseline_runs=0, n_baseline_samples=0, n_current_samples=0,
            scheduler=scheduler
        )

    current_run_id = run_ids[-1]
    baseline_run_ids = run_ids[-(n_baseline_runs + 1):-1]

    def get_ips(run_ids_list):
        placeholders = ",".join("?" * len(run_ids_list))
        cursor = conn.execute(
            f"""SELECT iter_per_sec FROM computational_metrics
                WHERE run_id IN ({placeholders}) AND iter_per_sec > 0""",
            run_ids_list
        )
        return np.array([row[0] for row in cursor.fetchall()])

    baseline_ips = get_ips(baseline_run_ids)
    current_ips = get_ips([current_run_id])

    if len(baseline_ips) < 2 or len(current_ips) < 2:
        return RegressionResult(
            is_regression=False, p_value=1.0, pct_change=0.0,
            baseline_mean_ips=np.mean(baseline_ips) if len(baseline_ips) > 0 else 0.0,
            current_mean_ips=np.mean(current_ips) if len(current_ips) > 0 else 0.0,
            n_baseline_runs=len(baseline_run_ids),
            n_baseline_samples=len(baseline_ips),
            n_current_samples=len(current_ips),
            scheduler=scheduler
        )

    baseline_mean = np.mean(baseline_ips)
    current_mean = np.mean(current_ips)
    pct_change = ((current_mean - baseline_mean) / baseline_mean) * 100

    p_value = _bootstrap_test(baseline_ips, current_ips, n_bootstrap=n_bootstrap)

    # Regression = statistically significant AND practically significant slowdown
    is_regression = (p_value < significance) and (pct_change < -change_threshold)

    # Phase timing analysis
    phase_changes = {}
    for phase in ['contact_detection_ms', 'mesh_refinement_ms', 'time_integration_ms',
                  'polarization_ms', 'total_iteration_ms']:
        baseline_placeholders = ",".join("?" * len(baseline_run_ids))
        cursor = conn.execute(
            f"""SELECT AVG({phase}) FROM phase_timings
                WHERE run_id IN ({baseline_placeholders}) AND {phase} IS NOT NULL""",
            baseline_run_ids
        )
        bl_avg = cursor.fetchone()[0]

        cursor = conn.execute(
            f"""SELECT AVG({phase}) FROM phase_timings
                WHERE run_id = ? AND {phase} IS NOT NULL""",
            (current_run_id,)
        )
        cur_avg = cursor.fetchone()[0]

        if bl_avg and cur_avg and bl_avg > 0:
            phase_changes[phase.replace('_ms', '')] = ((cur_avg - bl_avg) / bl_avg) * 100

    return RegressionResult(
        is_regression=is_regression,
        p_value=p_value,
        pct_change=pct_change,
        baseline_mean_ips=baseline_mean,
        current_mean_ips=current_mean,
        n_baseline_runs=len(baseline_run_ids),
        n_baseline_samples=len(baseline_ips),
        n_current_samples=len(current_ips),
        scheduler=scheduler,
        phase_changes=phase_changes
    )


def main():
    parser = argparse.ArgumentParser(
        description='Detect performance regressions in benchmark results'
    )
    parser.add_argument('--db', type=Path, required=True,
                        help='Path to SQLite metrics database')
    parser.add_argument('--scheduler', type=str, default='adaptive',
                        help='Scheduler to analyze')
    parser.add_argument('--output', type=Path, default=None,
                        help='Write markdown report to file')
    parser.add_argument('--n-baseline', type=int, default=5,
                        help='Number of baseline runs to compare against')
    parser.add_argument('--threshold', type=float, default=10.0,
                        help='Minimum % change to flag as regression')

    args = parser.parse_args()

    if not args.db.exists():
        print(f"Error: database not found: {args.db}")
        return 1

    conn = sqlite3.connect(str(args.db))
    result = detect_regression(
        conn,
        scheduler=args.scheduler,
        n_baseline_runs=args.n_baseline,
        change_threshold=args.threshold
    )
    conn.close()

    report = result.to_markdown()
    print(report)

    if args.output:
        args.output.write_text(report)
        print(f"\nReport saved to: {args.output}")

    return 1 if result.is_regression else 0


if __name__ == "__main__":
    sys.exit(main())
