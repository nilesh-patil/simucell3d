# Phase 4: CI/CD Integration & Engineering Quality Implementation Plan

**Goal:** Automate regression detection, historical metrics tracking, and dashboard generation for the SimuCell3D benchmarking system.

**Architecture:** Three GitHub Actions workflows (PR/nightly/weekly) trigger benchmark runs and feed results into a SQLite database via an ingestion script. A regression detector uses bootstrap resampling to flag performance regressions. Extended visualization (5 new plots) and a static HTML dashboard provide stakeholder-facing reporting.

**Tech Stack:** Python 3.10+, SQLite3, GitHub Actions, matplotlib, scipy, numpy, pandas, Jinja2 (HTML dashboard)

---

### Task 1: SQLite Metrics Database Schema & Ingestion Script

**Files:**
- Create: `scripts/ci/aggregate_results.py`
- Create: `scripts/ci/__init__.py`

**Step 1: Write the failing test**

Create a test that verifies the database schema creation and CSV ingestion.

```python
# tests/ci/test_aggregate_results.py
"""Tests for benchmark metrics aggregation into SQLite."""
import os
import sys
import sqlite3
import tempfile
import shutil
from pathlib import Path

# Add scripts to path
sys.path.insert(0, str(Path(__file__).resolve().parent.parent.parent / 'scripts'))


def create_mock_benchmark_dir(base_dir):
    """Create a mock benchmark directory with CSV files."""
    bench_dir = Path(base_dir) / "benchmark_20260215_120000"
    metrics_dir = bench_dir / "metrics" / "adaptive"
    metrics_dir.mkdir(parents=True)

    # Create computational.csv
    comp_csv = metrics_dir / "computational.csv"
    comp_csv.write_text(
        "timestamp,epoch,iteration,cells,iter_per_sec,rss_mb\n"
        "2026-02-15T12:00:00,1739620800,1,13,5.2,122.1\n"
        "2026-02-15T12:00:01,1739620801,2,13,5.1,122.1\n"
        "2026-02-15T12:00:02,1739620802,3,14,4.9,123.8\n"
    )

    # Create biological.csv
    bio_csv = metrics_dir / "biological.csv"
    bio_csv.write_text(
        "iteration,mean_pressure,mean_volume,cell_count\n"
        "1,1500.0,4.2e-16,13\n"
        "2,1520.0,4.3e-16,13\n"
        "3,1480.0,4.1e-16,14\n"
    )

    # Create phase_timings.csv
    phase_csv = metrics_dir / "phase_timings.csv"
    phase_csv.write_text(
        "timestamp,iteration,mesh_refinement_ms,contact_detection_ms,polarization_ms,time_integration_ms,total_iteration_ms\n"
        "2026-02-15T12:00:00,1,10.5,120.3,15.2,8.1,192.5\n"
        "2026-02-15T12:00:01,2,11.0,125.1,14.8,8.3,196.2\n"
        "2026-02-15T12:00:02,3,12.1,130.5,16.0,9.0,204.0\n"
    )

    return bench_dir


def test_create_schema():
    """Test that create_schema builds the expected tables."""
    from ci.aggregate_results import create_schema

    with tempfile.NamedTemporaryFile(suffix='.db', delete=False) as f:
        db_path = f.name

    try:
        conn = sqlite3.connect(db_path)
        create_schema(conn)

        # Check tables exist
        cursor = conn.execute(
            "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name"
        )
        tables = [row[0] for row in cursor.fetchall()]
        assert 'benchmark_runs' in tables, f"Missing benchmark_runs table, got {tables}"
        assert 'computational_metrics' in tables, f"Missing computational_metrics, got {tables}"
        assert 'biological_metrics' in tables, f"Missing biological_metrics, got {tables}"
        assert 'phase_timings' in tables, f"Missing phase_timings, got {tables}"

        # Check benchmark_runs columns
        cursor = conn.execute("PRAGMA table_info(benchmark_runs)")
        cols = [row[1] for row in cursor.fetchall()]
        assert 'git_commit' in cols
        assert 'git_branch' in cols
        assert 'scheduler' in cols
        assert 'run_timestamp' in cols

        conn.close()
    finally:
        os.unlink(db_path)

    print("PASS: test_create_schema")


def test_ingest_benchmark():
    """Test ingesting a benchmark directory into SQLite."""
    from ci.aggregate_results import create_schema, ingest_benchmark

    with tempfile.TemporaryDirectory() as tmpdir:
        bench_dir = create_mock_benchmark_dir(tmpdir)
        db_path = Path(tmpdir) / "test.db"

        conn = sqlite3.connect(str(db_path))
        create_schema(conn)

        run_id = ingest_benchmark(
            conn,
            bench_dir=bench_dir,
            git_commit="abc1234",
            git_branch="feature/test",
            scheduler="adaptive"
        )

        assert run_id is not None, "ingest_benchmark should return a run_id"
        assert run_id > 0

        # Verify data was inserted
        cursor = conn.execute("SELECT COUNT(*) FROM computational_metrics WHERE run_id = ?", (run_id,))
        comp_count = cursor.fetchone()[0]
        assert comp_count == 3, f"Expected 3 computational rows, got {comp_count}"

        cursor = conn.execute("SELECT COUNT(*) FROM biological_metrics WHERE run_id = ?", (run_id,))
        bio_count = cursor.fetchone()[0]
        assert bio_count == 3, f"Expected 3 biological rows, got {bio_count}"

        cursor = conn.execute("SELECT COUNT(*) FROM phase_timings WHERE run_id = ?", (run_id,))
        phase_count = cursor.fetchone()[0]
        assert phase_count == 3, f"Expected 3 phase_timing rows, got {phase_count}"

        conn.close()

    print("PASS: test_ingest_benchmark")


def test_query_latest_runs():
    """Test querying the latest benchmark runs."""
    from ci.aggregate_results import create_schema, ingest_benchmark, query_latest_runs

    with tempfile.TemporaryDirectory() as tmpdir:
        bench_dir = create_mock_benchmark_dir(tmpdir)
        db_path = Path(tmpdir) / "test.db"

        conn = sqlite3.connect(str(db_path))
        create_schema(conn)

        # Ingest twice to have 2 runs
        ingest_benchmark(conn, bench_dir, "abc1234", "main", "adaptive")
        ingest_benchmark(conn, bench_dir, "def5678", "main", "adaptive")

        runs = query_latest_runs(conn, scheduler="adaptive", limit=5)
        assert len(runs) == 2, f"Expected 2 runs, got {len(runs)}"
        # Most recent should be first
        assert runs[0]['git_commit'] == "def5678"

        conn.close()

    print("PASS: test_query_latest_runs")


if __name__ == "__main__":
    test_name = sys.argv[1] if len(sys.argv) > 1 else None
    tests = {
        'test_create_schema': test_create_schema,
        'test_ingest_benchmark': test_ingest_benchmark,
        'test_query_latest_runs': test_query_latest_runs,
    }

    if test_name:
        tests[test_name]()
    else:
        for name, fn in tests.items():
            fn()
        print(f"\nAll {len(tests)} tests passed.")
```

**Step 2: Run test to verify it fails**

Run: `cd /home/nilesh-patil/projects/version-cpp-next/.worktrees/benchmarking-system && python tests/ci/test_aggregate_results.py`
Expected: FAIL with `ModuleNotFoundError: No module named 'ci.aggregate_results'`

**Step 3: Write minimal implementation**

```python
# scripts/ci/__init__.py
"""CI/CD integration scripts for SimuCell3D benchmarking."""

# scripts/ci/aggregate_results.py
#!/usr/bin/env python3
"""
Aggregate benchmark CSV results into SQLite for historical tracking.

Usage:
    python -m ci.aggregate_results --db metrics.db --benchmark-dir <dir> --commit <sha> --branch <branch>

Reads metrics CSV files produced by preprocess_benchmark_metrics.py and inserts
them into a SQLite database linked to Git commit metadata.
"""

import argparse
import sqlite3
import sys
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional

import pandas as pd


def create_schema(conn: sqlite3.Connection) -> None:
    """Create the benchmark metrics database schema."""
    conn.executescript("""
        CREATE TABLE IF NOT EXISTS benchmark_runs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            git_commit TEXT NOT NULL,
            git_branch TEXT NOT NULL,
            scheduler TEXT NOT NULL,
            run_timestamp TEXT NOT NULL DEFAULT (datetime('now')),
            benchmark_dir TEXT,
            hostname TEXT,
            omp_threads INTEGER,
            notes TEXT
        );

        CREATE TABLE IF NOT EXISTS computational_metrics (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            run_id INTEGER NOT NULL REFERENCES benchmark_runs(id),
            iteration INTEGER NOT NULL,
            cells INTEGER,
            iter_per_sec REAL,
            rss_mb REAL,
            epoch REAL,
            UNIQUE(run_id, iteration)
        );

        CREATE TABLE IF NOT EXISTS biological_metrics (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            run_id INTEGER NOT NULL REFERENCES benchmark_runs(id),
            iteration INTEGER NOT NULL,
            mean_pressure REAL,
            mean_volume REAL,
            cell_count INTEGER,
            mean_kinetic_energy REAL,
            mean_potential_energy REAL,
            UNIQUE(run_id, iteration)
        );

        CREATE TABLE IF NOT EXISTS phase_timings (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            run_id INTEGER NOT NULL REFERENCES benchmark_runs(id),
            iteration INTEGER NOT NULL,
            mesh_refinement_ms REAL,
            contact_detection_ms REAL,
            polarization_ms REAL,
            time_integration_ms REAL,
            total_iteration_ms REAL,
            UNIQUE(run_id, iteration)
        );

        CREATE INDEX IF NOT EXISTS idx_comp_run ON computational_metrics(run_id);
        CREATE INDEX IF NOT EXISTS idx_bio_run ON biological_metrics(run_id);
        CREATE INDEX IF NOT EXISTS idx_phase_run ON phase_timings(run_id);
        CREATE INDEX IF NOT EXISTS idx_runs_commit ON benchmark_runs(git_commit);
        CREATE INDEX IF NOT EXISTS idx_runs_branch ON benchmark_runs(git_branch);
    """)
    conn.commit()


def ingest_benchmark(
    conn: sqlite3.Connection,
    bench_dir: Path,
    git_commit: str,
    git_branch: str,
    scheduler: str,
    hostname: str = "",
    omp_threads: int = 0,
    notes: str = ""
) -> int:
    """Ingest benchmark CSV files into the database.

    Args:
        conn: SQLite connection
        bench_dir: Path to benchmark directory (contains metrics/<scheduler>/*.csv)
        git_commit: Git commit SHA
        git_branch: Git branch name
        scheduler: Scheduler name (e.g., 'adaptive')
        hostname: Machine hostname
        omp_threads: Number of OpenMP threads used
        notes: Optional notes about the run

    Returns:
        The run_id of the inserted benchmark run
    """
    # Insert benchmark run record
    cursor = conn.execute(
        """INSERT INTO benchmark_runs
           (git_commit, git_branch, scheduler, benchmark_dir, hostname, omp_threads, notes)
           VALUES (?, ?, ?, ?, ?, ?, ?)""",
        (git_commit, git_branch, scheduler, str(bench_dir), hostname, omp_threads, notes)
    )
    run_id = cursor.lastrowid

    metrics_dir = bench_dir / "metrics" / scheduler

    # Ingest computational metrics
    comp_path = metrics_dir / "computational.csv"
    if comp_path.exists():
        df = pd.read_csv(comp_path)
        for _, row in df.iterrows():
            conn.execute(
                """INSERT OR IGNORE INTO computational_metrics
                   (run_id, iteration, cells, iter_per_sec, rss_mb, epoch)
                   VALUES (?, ?, ?, ?, ?, ?)""",
                (run_id, int(row.get('iteration', 0)),
                 int(row.get('cells', 0)),
                 float(row.get('iter_per_sec', 0)),
                 float(row.get('rss_mb', 0)),
                 float(row.get('epoch', 0)))
            )

    # Ingest biological metrics
    bio_path = metrics_dir / "biological.csv"
    if bio_path.exists():
        df = pd.read_csv(bio_path)
        for _, row in df.iterrows():
            conn.execute(
                """INSERT OR IGNORE INTO biological_metrics
                   (run_id, iteration, mean_pressure, mean_volume, cell_count,
                    mean_kinetic_energy, mean_potential_energy)
                   VALUES (?, ?, ?, ?, ?, ?, ?)""",
                (run_id, int(row.get('iteration', 0)),
                 float(row.get('mean_pressure', 0)) if pd.notna(row.get('mean_pressure')) else None,
                 float(row.get('mean_volume', 0)) if pd.notna(row.get('mean_volume')) else None,
                 int(row.get('cell_count', 0)) if pd.notna(row.get('cell_count')) else None,
                 float(row.get('mean_kinetic_energy', 0)) if pd.notna(row.get('mean_kinetic_energy')) else None,
                 float(row.get('mean_potential_energy', 0)) if pd.notna(row.get('mean_potential_energy')) else None)
            )

    # Ingest phase timings
    phase_path = metrics_dir / "phase_timings.csv"
    if phase_path.exists():
        df = pd.read_csv(phase_path)
        for _, row in df.iterrows():
            conn.execute(
                """INSERT OR IGNORE INTO phase_timings
                   (run_id, iteration, mesh_refinement_ms, contact_detection_ms,
                    polarization_ms, time_integration_ms, total_iteration_ms)
                   VALUES (?, ?, ?, ?, ?, ?, ?)""",
                (run_id, int(row.get('iteration', 0)),
                 float(row.get('mesh_refinement_ms', 0)) if pd.notna(row.get('mesh_refinement_ms')) else None,
                 float(row.get('contact_detection_ms', 0)) if pd.notna(row.get('contact_detection_ms')) else None,
                 float(row.get('polarization_ms', 0)) if pd.notna(row.get('polarization_ms')) else None,
                 float(row.get('time_integration_ms', 0)) if pd.notna(row.get('time_integration_ms')) else None,
                 float(row.get('total_iteration_ms', 0)) if pd.notna(row.get('total_iteration_ms')) else None)
            )

    conn.commit()
    return run_id


def query_latest_runs(
    conn: sqlite3.Connection,
    scheduler: str = None,
    branch: str = None,
    limit: int = 10
) -> List[Dict]:
    """Query the most recent benchmark runs.

    Args:
        conn: SQLite connection
        scheduler: Filter by scheduler name
        branch: Filter by branch name
        limit: Maximum number of runs to return

    Returns:
        List of dicts with run metadata, most recent first
    """
    query = "SELECT * FROM benchmark_runs WHERE 1=1"
    params = []

    if scheduler:
        query += " AND scheduler = ?"
        params.append(scheduler)
    if branch:
        query += " AND git_branch = ?"
        params.append(branch)

    query += " ORDER BY id DESC LIMIT ?"
    params.append(limit)

    conn.row_factory = sqlite3.Row
    cursor = conn.execute(query, params)
    rows = cursor.fetchall()
    conn.row_factory = None

    return [dict(row) for row in rows]


def query_run_summary(conn: sqlite3.Connection, run_id: int) -> Dict:
    """Get summary statistics for a benchmark run."""
    summary = {'run_id': run_id}

    # Computational summary
    cursor = conn.execute("""
        SELECT
            AVG(iter_per_sec) as avg_ips,
            MIN(iter_per_sec) as min_ips,
            MAX(iter_per_sec) as max_ips,
            MAX(cells) as max_cells,
            AVG(rss_mb) as avg_rss_mb,
            COUNT(*) as n_iterations
        FROM computational_metrics WHERE run_id = ?
    """, (run_id,))
    row = cursor.fetchone()
    if row:
        summary['avg_ips'] = row[0]
        summary['min_ips'] = row[1]
        summary['max_ips'] = row[2]
        summary['max_cells'] = row[3]
        summary['avg_rss_mb'] = row[4]
        summary['n_iterations'] = row[5]

    # Phase timing summary
    cursor = conn.execute("""
        SELECT
            AVG(total_iteration_ms) as avg_iter_ms,
            AVG(contact_detection_ms) as avg_contact_ms,
            AVG(mesh_refinement_ms) as avg_mesh_ms
        FROM phase_timings WHERE run_id = ?
    """, (run_id,))
    row = cursor.fetchone()
    if row:
        summary['avg_iter_ms'] = row[0]
        summary['avg_contact_ms'] = row[1]
        summary['avg_mesh_ms'] = row[2]

    return summary


def main():
    parser = argparse.ArgumentParser(
        description='Aggregate benchmark results into SQLite database'
    )
    parser.add_argument('--db', type=Path, required=True,
                        help='Path to SQLite database file')
    parser.add_argument('--benchmark-dir', type=Path, required=True,
                        help='Path to benchmark directory with metrics/')
    parser.add_argument('--commit', type=str, required=True,
                        help='Git commit SHA')
    parser.add_argument('--branch', type=str, required=True,
                        help='Git branch name')
    parser.add_argument('--scheduler', type=str, default='adaptive',
                        help='Scheduler name (default: adaptive)')
    parser.add_argument('--hostname', type=str, default='',
                        help='Machine hostname')
    parser.add_argument('--omp-threads', type=int, default=0,
                        help='Number of OpenMP threads')
    parser.add_argument('--init-only', action='store_true',
                        help='Only create schema, do not ingest')

    args = parser.parse_args()

    conn = sqlite3.connect(str(args.db))
    create_schema(conn)

    if args.init_only:
        print(f"Database initialized: {args.db}")
        conn.close()
        return 0

    if not args.benchmark_dir.exists():
        print(f"Error: benchmark directory not found: {args.benchmark_dir}")
        return 1

    run_id = ingest_benchmark(
        conn,
        bench_dir=args.benchmark_dir,
        git_commit=args.commit,
        git_branch=args.branch,
        scheduler=args.scheduler,
        hostname=args.hostname,
        omp_threads=args.omp_threads
    )

    summary = query_run_summary(conn, run_id)
    print(f"Ingested run #{run_id}: commit={args.commit[:8]}, "
          f"scheduler={args.scheduler}, "
          f"iterations={summary.get('n_iterations', 0)}, "
          f"avg_ips={summary.get('avg_ips', 0):.2f}")

    conn.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

**Step 4: Run test to verify it passes**

Run: `cd /home/nilesh-patil/projects/version-cpp-next/.worktrees/benchmarking-system && python tests/ci/test_aggregate_results.py`
Expected: PASS (all 3 tests)

**Step 5: Commit**

```bash
git add scripts/ci/__init__.py scripts/ci/aggregate_results.py tests/ci/test_aggregate_results.py
git commit -m "feat(ci): add SQLite metrics database and CSV ingestion script"
```

---

### Task 2: Regression Detector with Bootstrap Resampling

**Files:**
- Create: `scripts/ci/regression_detector.py`

**Step 1: Write the failing test**

```python
# tests/ci/test_regression_detector.py
"""Tests for benchmark regression detection."""
import os
import sys
import sqlite3
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent.parent / 'scripts'))


def create_db_with_runs(db_path, ips_values_per_run):
    """Create a test DB with multiple runs having specified IPS values."""
    from ci.aggregate_results import create_schema

    conn = sqlite3.connect(str(db_path))
    create_schema(conn)

    for i, ips_values in enumerate(ips_values_per_run):
        commit = f"commit{i:04d}"
        cursor = conn.execute(
            """INSERT INTO benchmark_runs
               (git_commit, git_branch, scheduler)
               VALUES (?, 'main', 'adaptive')""",
            (commit,)
        )
        run_id = cursor.lastrowid

        for j, ips in enumerate(ips_values):
            conn.execute(
                """INSERT INTO computational_metrics
                   (run_id, iteration, cells, iter_per_sec, rss_mb, epoch)
                   VALUES (?, ?, 13, ?, 120.0, ?)""",
                (run_id, j + 1, ips, 1000000 + j)
            )

    conn.commit()
    return conn


def test_no_regression():
    """Test that stable performance is NOT flagged as regression."""
    from ci.regression_detector import detect_regression

    with tempfile.TemporaryDirectory() as tmpdir:
        db_path = Path(tmpdir) / "test.db"
        # 5 baseline runs with ~5.0 IPS, then a new run also ~5.0 IPS
        baseline_runs = [[5.0, 5.1, 4.9, 5.0, 5.2] for _ in range(5)]
        current_run = [5.0, 5.1, 4.9, 5.0, 5.2]
        conn = create_db_with_runs(db_path, baseline_runs + [current_run])

        result = detect_regression(conn, scheduler='adaptive')

        assert not result.is_regression, \
            f"Stable performance should NOT be flagged. Got: change={result.pct_change:.1f}%, p={result.p_value:.4f}"

        conn.close()

    print("PASS: test_no_regression")


def test_regression_detected():
    """Test that a >10% slowdown IS flagged as regression."""
    from ci.regression_detector import detect_regression

    with tempfile.TemporaryDirectory() as tmpdir:
        db_path = Path(tmpdir) / "test.db"
        # 5 baseline runs with ~5.0 IPS, then a new run with ~4.0 IPS (20% slower)
        baseline_runs = [[5.0, 5.1, 4.9, 5.0, 5.2] for _ in range(5)]
        current_run = [4.0, 3.9, 4.1, 4.0, 3.8]
        conn = create_db_with_runs(db_path, baseline_runs + [current_run])

        result = detect_regression(conn, scheduler='adaptive')

        assert result.is_regression, \
            f"20% slowdown should be flagged. Got: change={result.pct_change:.1f}%, p={result.p_value:.4f}"
        assert result.pct_change < -10, \
            f"Expected >10% regression, got {result.pct_change:.1f}%"

        conn.close()

    print("PASS: test_regression_detected")


def test_improvement_not_flagged():
    """Test that performance improvement is NOT flagged as regression."""
    from ci.regression_detector import detect_regression

    with tempfile.TemporaryDirectory() as tmpdir:
        db_path = Path(tmpdir) / "test.db"
        # 5 baseline runs with ~5.0 IPS, then a new run with ~6.0 IPS (20% faster)
        baseline_runs = [[5.0, 5.1, 4.9, 5.0, 5.2] for _ in range(5)]
        current_run = [6.0, 6.1, 5.9, 6.0, 6.2]
        conn = create_db_with_runs(db_path, baseline_runs + [current_run])

        result = detect_regression(conn, scheduler='adaptive')

        assert not result.is_regression, \
            f"Improvement should NOT be flagged as regression. Got: change={result.pct_change:.1f}%"

        conn.close()

    print("PASS: test_improvement_not_flagged")


def test_markdown_report():
    """Test that the markdown report is generated correctly."""
    from ci.regression_detector import detect_regression

    with tempfile.TemporaryDirectory() as tmpdir:
        db_path = Path(tmpdir) / "test.db"
        baseline_runs = [[5.0, 5.1, 4.9, 5.0, 5.2] for _ in range(5)]
        current_run = [4.0, 3.9, 4.1, 4.0, 3.8]
        conn = create_db_with_runs(db_path, baseline_runs + [current_run])

        result = detect_regression(conn, scheduler='adaptive')
        report = result.to_markdown()

        assert "Regression" in report or "regression" in report, \
            f"Report should mention regression: {report[:200]}"
        assert "%" in report, f"Report should include percentage: {report[:200]}"

        conn.close()

    print("PASS: test_markdown_report")


if __name__ == "__main__":
    test_name = sys.argv[1] if len(sys.argv) > 1 else None
    tests = {
        'test_no_regression': test_no_regression,
        'test_regression_detected': test_regression_detected,
        'test_improvement_not_flagged': test_improvement_not_flagged,
        'test_markdown_report': test_markdown_report,
    }

    if test_name:
        tests[test_name]()
    else:
        for name, fn in tests.items():
            fn()
        print(f"\nAll {len(tests)} tests passed.")
```

**Step 2: Run test to verify it fails**

Run: `cd /home/nilesh-patil/projects/version-cpp-next/.worktrees/benchmarking-system && python tests/ci/test_regression_detector.py`
Expected: FAIL with `ModuleNotFoundError: No module named 'ci.regression_detector'`

**Step 3: Write minimal implementation**

```python
# scripts/ci/regression_detector.py
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
    phase_changes: dict = field(default_factory=dict)  # phase_name -> pct_change

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
    # Get all runs for this scheduler, ordered by ID (most recent last)
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

    # Get IPS values
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

    # Bootstrap permutation test
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
```

**Step 4: Run test to verify it passes**

Run: `cd /home/nilesh-patil/projects/version-cpp-next/.worktrees/benchmarking-system && python tests/ci/test_regression_detector.py`
Expected: PASS (all 4 tests)

**Step 5: Commit**

```bash
git add scripts/ci/regression_detector.py tests/ci/test_regression_detector.py
git commit -m "feat(ci): add regression detector with bootstrap resampling"
```

---

### Task 3: GitHub Actions Workflow - PR Quick Benchmark

**Files:**
- Create: `.github/workflows/benchmark-pr.yml`

**Step 1: Write the workflow file**

```yaml
# .github/workflows/benchmark-pr.yml
name: PR Benchmark

on:
  pull_request:
    branches: [main, version-cpp-next]
    paths:
      - 'src/**'
      - 'include/**'
      - 'CMakeLists.txt'

concurrency:
  group: benchmark-pr-${{ github.event.pull_request.number }}
  cancel-in-progress: true

jobs:
  quick-benchmark:
    name: Quick Performance Check
    runs-on: ubuntu-latest
    timeout-minutes: 5

    steps:
      - uses: actions/checkout@v4
        with:
          fetch-depth: 0  # Full history for git metadata

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y libomp-dev python3-pip
          pip3 install pandas numpy scipy matplotlib

      - name: Build (Release)
        run: cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)

      - name: Run quick benchmark (vesicle, ~30s)
        run: |
          export OMP_NUM_THREADS=$(nproc)
          export OMP_PROC_BIND=close
          export OMP_PLACES=cores

          BENCH_DIR="benchmark_pr_$(date +%Y%m%d_%H%M%S)"
          mkdir -p "$BENCH_DIR/sim_adaptive"

          # Run short vesicle simulation with diagnostics
          timeout 120 ./build/simucell3d \
            parameters/core/parameters_vesicle.xml \
            --schedule=adaptive \
            --diagnostics-csv="$BENCH_DIR/sim_adaptive/performance_diagnostics.csv" \
            || true

          echo "BENCH_DIR=$BENCH_DIR" >> "$GITHUB_ENV"

      - name: Preprocess metrics
        run: |
          python3 scripts/preprocess_benchmark_metrics.py "$BENCH_DIR" || true

      - name: Check for regression
        id: regression
        run: |
          DB_PATH="doc/working/benchmark_metrics.db"

          # Initialize DB if it doesn't exist
          if [ ! -f "$DB_PATH" ]; then
            python3 -c "
          import sqlite3, sys
          sys.path.insert(0, 'scripts')
          from ci.aggregate_results import create_schema
          conn = sqlite3.connect('$DB_PATH')
          create_schema(conn)
          conn.close()
          "
          fi

          # Ingest current results
          python3 -m scripts.ci.aggregate_results \
            --db "$DB_PATH" \
            --benchmark-dir "$BENCH_DIR" \
            --commit "${{ github.event.pull_request.head.sha }}" \
            --branch "${{ github.head_ref }}" \
            --scheduler adaptive \
            --omp-threads $(nproc) \
            --hostname "github-actions" || true

          # Run regression detector
          python3 -m scripts.ci.regression_detector \
            --db "$DB_PATH" \
            --scheduler adaptive \
            --output regression_report.md || true

          if [ -f regression_report.md ]; then
            echo "report_exists=true" >> "$GITHUB_OUTPUT"
          fi

      - name: Post PR comment
        if: steps.regression.outputs.report_exists == 'true'
        uses: actions/github-script@v7
        with:
          script: |
            const fs = require('fs');
            const report = fs.readFileSync('regression_report.md', 'utf8');

            // Find existing bot comment
            const comments = await github.rest.issues.listComments({
              owner: context.repo.owner,
              repo: context.repo.repo,
              issue_number: context.issue.number,
            });

            const botComment = comments.data.find(c =>
              c.user.type === 'Bot' && c.body.includes('Benchmark Results')
            );

            const body = report + '\n\n---\n*Generated by benchmark-pr workflow*';

            if (botComment) {
              await github.rest.issues.updateComment({
                owner: context.repo.owner,
                repo: context.repo.repo,
                comment_id: botComment.id,
                body: body,
              });
            } else {
              await github.rest.issues.createComment({
                owner: context.repo.owner,
                repo: context.repo.repo,
                issue_number: context.issue.number,
                body: body,
              });
            }

      - name: Fail on regression
        if: steps.regression.outputs.report_exists == 'true'
        run: |
          if grep -q "REGRESSION DETECTED" regression_report.md; then
            echo "::error::Performance regression detected. See PR comment for details."
            exit 1
          fi
```

**Step 2: Validate YAML syntax**

Run: `python3 -c "import yaml; yaml.safe_load(open('.github/workflows/benchmark-pr.yml'))" 2>&1 || echo "Install PyYAML: pip install pyyaml"`
Expected: No errors (valid YAML)

**Step 3: Commit**

```bash
git add .github/workflows/benchmark-pr.yml
git commit -m "ci: add PR benchmark workflow with regression detection"
```

---

### Task 4: GitHub Actions Workflow - Nightly Comprehensive Suite

**Files:**
- Create: `.github/workflows/benchmark-nightly.yml`

**Step 1: Write the workflow file**

```yaml
# .github/workflows/benchmark-nightly.yml
name: Nightly Benchmark

on:
  schedule:
    - cron: '0 2 * * *'  # 2 AM UTC daily
  workflow_dispatch:
    inputs:
      scheduler:
        description: 'Scheduler to test'
        default: 'all'
        type: choice
        options: [all, adaptive, static, dynamic, guided]

jobs:
  nightly-benchmark:
    name: Nightly ${{ matrix.scheduler }}
    runs-on: ubuntu-latest
    timeout-minutes: 120
    strategy:
      fail-fast: false
      matrix:
        scheduler: [adaptive, static, dynamic]

    steps:
      - uses: actions/checkout@v4
        with:
          fetch-depth: 0
          lfs: true

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y libomp-dev python3-pip
          pip3 install pandas numpy scipy matplotlib jinja2

      - name: Build (Release)
        run: cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)

      - name: Run comprehensive benchmark
        run: |
          export OMP_NUM_THREADS=$(nproc)
          export OMP_PROC_BIND=close
          export OMP_PLACES=cores
          export OMP_WAIT_POLICY=passive
          export OMP_DYNAMIC=false

          BENCH_DIR="benchmark_nightly_$(date +%Y%m%d)"
          mkdir -p "$BENCH_DIR/sim_${{ matrix.scheduler }}"

          # Run default_dynamic simulation (longer than vesicle)
          timeout 6000 ./build/simucell3d \
            parameters/core/parameters_default_dynamic.xml \
            --schedule=${{ matrix.scheduler }} \
            --diagnostics-csv="$BENCH_DIR/sim_${{ matrix.scheduler }}/performance_diagnostics.csv" \
            || true

          echo "BENCH_DIR=$BENCH_DIR" >> "$GITHUB_ENV"

      - name: Preprocess & ingest metrics
        run: |
          python3 scripts/preprocess_benchmark_metrics.py "$BENCH_DIR" || true

          DB_PATH="doc/working/benchmark_metrics.db"
          python3 -m scripts.ci.aggregate_results \
            --db "$DB_PATH" \
            --benchmark-dir "$BENCH_DIR" \
            --commit "$(git rev-parse HEAD)" \
            --branch "$(git branch --show-current)" \
            --scheduler "${{ matrix.scheduler }}" \
            --omp-threads $(nproc) \
            --hostname "github-actions-nightly" || true

      - name: Generate plots
        run: |
          python3 scripts/plot_benchmark_unified.py "$BENCH_DIR" \
            --quality draft --no-latex || true

      - name: Run regression detector
        run: |
          python3 -m scripts.ci.regression_detector \
            --db "doc/working/benchmark_metrics.db" \
            --scheduler "${{ matrix.scheduler }}" \
            --output "regression_${{ matrix.scheduler }}.md" || true

      - name: Upload artifacts
        uses: actions/upload-artifact@v4
        with:
          name: benchmark-${{ matrix.scheduler }}-${{ github.run_number }}
          path: |
            ${{ env.BENCH_DIR }}/plots-unified/
            ${{ env.BENCH_DIR }}/metrics/
            regression_${{ matrix.scheduler }}.md
          retention-days: 30

      - name: Upload database
        uses: actions/upload-artifact@v4
        with:
          name: metrics-db-${{ matrix.scheduler }}-${{ github.run_number }}
          path: doc/working/benchmark_metrics.db
          retention-days: 90
```

**Step 2: Validate YAML syntax**

Run: `python3 -c "import yaml; yaml.safe_load(open('.github/workflows/benchmark-nightly.yml'))"`
Expected: No errors

**Step 3: Commit**

```bash
git add .github/workflows/benchmark-nightly.yml
git commit -m "ci: add nightly comprehensive benchmark workflow"
```

---

### Task 5: GitHub Actions Workflow - Weekly Stress Tests

**Files:**
- Create: `.github/workflows/benchmark-weekly.yml`

**Step 1: Write the workflow file**

```yaml
# .github/workflows/benchmark-weekly.yml
name: Weekly Stress Test

on:
  schedule:
    - cron: '0 0 * * 0'  # Midnight UTC every Sunday
  workflow_dispatch:

jobs:
  stress-test:
    name: Weekly Stress (${{ matrix.test }})
    runs-on: ubuntu-latest
    timeout-minutes: 1440  # 24 hours
    strategy:
      fail-fast: false
      matrix:
        test:
          - name: scaling-large
            param_file: parameters/core/parameters_default_dynamic.xml
            scheduler: adaptive
            description: "Large-scale scaling test"
          - name: all-schedulers
            param_file: parameters/core/parameters_default_dynamic.xml
            scheduler: guided
            description: "All scheduler comparison"

    steps:
      - uses: actions/checkout@v4
        with:
          fetch-depth: 0
          lfs: true

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y libomp-dev python3-pip
          pip3 install pandas numpy scipy matplotlib jinja2

      - name: Build (Release)
        run: cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)

      - name: Run stress test
        run: |
          export OMP_NUM_THREADS=$(nproc)
          export OMP_PROC_BIND=close
          export OMP_PLACES=cores
          export OMP_WAIT_POLICY=passive
          export OMP_DYNAMIC=false

          BENCH_DIR="benchmark_weekly_$(date +%Y%m%d)"
          SCHED="${{ matrix.test.scheduler }}"
          mkdir -p "$BENCH_DIR/sim_${SCHED}"

          timeout 82800 ./build/simucell3d \
            ${{ matrix.test.param_file }} \
            --schedule=${SCHED} \
            --diagnostics-csv="$BENCH_DIR/sim_${SCHED}/performance_diagnostics.csv" \
            || true

          echo "BENCH_DIR=$BENCH_DIR" >> "$GITHUB_ENV"

      - name: Process results
        run: |
          python3 scripts/preprocess_benchmark_metrics.py "$BENCH_DIR" || true

          python3 -m scripts.ci.aggregate_results \
            --db "doc/working/benchmark_metrics.db" \
            --benchmark-dir "$BENCH_DIR" \
            --commit "$(git rev-parse HEAD)" \
            --branch "$(git branch --show-current)" \
            --scheduler "${{ matrix.test.scheduler }}" \
            --omp-threads $(nproc) \
            --hostname "github-actions-weekly" || true

      - name: Generate full plot suite
        run: |
          python3 scripts/plot_benchmark_unified.py "$BENCH_DIR" \
            --quality publication --no-latex || true

      - name: Upload artifacts
        uses: actions/upload-artifact@v4
        with:
          name: weekly-${{ matrix.test.name }}-${{ github.run_number }}
          path: |
            ${{ env.BENCH_DIR }}/plots-unified/
            ${{ env.BENCH_DIR }}/metrics/
          retention-days: 90
```

**Step 2: Commit**

```bash
git add .github/workflows/benchmark-weekly.yml
git commit -m "ci: add weekly stress test benchmark workflow"
```

---

### Task 6: Extend plot_benchmark_unified.py with 5 New Plots

**Files:**
- Modify: `scripts/plot_benchmark_unified.py` (add 5 new registered plots after existing 12)

The 5 new plots are:
1. **Cache efficiency heatmap** - L1/L2/L3 miss rates vs cell count (id='13')
2. **Energy conservation timeline** - energy drift across runs (id='14')
3. **Roofline trajectory** - operational intensity evolution (id='15')
4. **Contact angle distribution** - biological validation (id='16')
5. **IPS regression timeline** - historical performance tracking (id='17')

**Step 1: Write the failing test**

```python
# tests/ci/test_new_plots.py
"""Tests for the 5 new visualization plots."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent.parent / 'scripts'))


def test_new_plots_registered():
    """Test that all 5 new plots are registered in PLOT_REGISTRY."""
    from plot_benchmark_unified import PLOT_REGISTRY

    expected_new_ids = ['13', '14', '15', '16', '17']
    for plot_id in expected_new_ids:
        assert plot_id in PLOT_REGISTRY, \
            f"Plot '{plot_id}' not found in PLOT_REGISTRY. Available: {list(PLOT_REGISTRY.keys())}"

    print("PASS: test_new_plots_registered")


def test_total_plot_count():
    """Test that we have at least 17 plots registered (12 original + 5 new)."""
    from plot_benchmark_unified import PLOT_REGISTRY

    assert len(PLOT_REGISTRY) >= 17, \
        f"Expected >= 17 plots, got {len(PLOT_REGISTRY)}"

    print("PASS: test_total_plot_count")


def test_plot_functions_callable():
    """Test that all new plot functions are callable."""
    from plot_benchmark_unified import PLOT_REGISTRY

    for plot_id in ['13', '14', '15', '16', '17']:
        plot_info = PLOT_REGISTRY[plot_id]
        assert callable(plot_info.function), \
            f"Plot {plot_id} function is not callable"

    print("PASS: test_plot_functions_callable")


if __name__ == "__main__":
    test_name = sys.argv[1] if len(sys.argv) > 1 else None
    tests = {
        'test_new_plots_registered': test_new_plots_registered,
        'test_total_plot_count': test_total_plot_count,
        'test_plot_functions_callable': test_plot_functions_callable,
    }

    if test_name:
        tests[test_name]()
    else:
        for name, fn in tests.items():
            fn()
        print(f"\nAll {len(tests)} tests passed.")
```

**Step 2: Run test to verify it fails**

Run: `cd /home/nilesh-patil/projects/version-cpp-next/.worktrees/benchmarking-system && python tests/ci/test_new_plots.py`
Expected: FAIL (plot IDs 13-17 not in registry)

**Step 3: Add 5 new plot functions to plot_benchmark_unified.py**

Insert BEFORE the `detect_ongoing_simulation` function (around line 1828). Add these 5 plot functions:

```python
# === NEW PLOTS FOR CI/CD DASHBOARD (Phase 4) ===

@register_plot(
    id='13',
    name='cache_efficiency',
    narrative='computational',
    requires=['computational', 'phase'],
    description='Cache efficiency proxy: time-per-cell vs cell count heatmap'
)
def plot_cache_efficiency(data: Dict, style: Dict, output_dir: Path,
                          use_latex: bool = True) -> bool:
    """Plot cache efficiency proxy: time per cell vs cell count as heatmap."""
    print("  Plotting: 13_cache_efficiency")
    fig, ax = plt.subplots(figsize=FIGSIZE_DOUBLE)

    has_data = False
    for sched in data.get('schedulers', []):
        accessor = SchedulerDataAccessor(data, sched.name, 'computational')
        if not accessor.is_available():
            continue

        df = accessor.get_raw_dataframe()
        if df is None or len(df) < 5:
            continue

        cells_col = accessor.get_column(['cells', 'num_cells'])
        ips_col = accessor.get_column(['iter_per_sec', 'ips'])

        if not cells_col or not ips_col:
            continue

        cells = df[cells_col].values
        ips = df[ips_col].values

        valid = (cells > 0) & (ips > 0) & np.isfinite(cells) & np.isfinite(ips)
        if np.sum(valid) < 5:
            continue

        cells_v = cells[valid]
        ips_v = ips[valid]
        time_per_cell = 1000.0 / (ips_v * cells_v)  # ms per cell per iteration

        color = style['colors'].get(sched.name, cm.viridis(0.5))
        label = style['labels'].get(sched.name, sched.name)

        scatter = ax.scatter(cells_v, time_per_cell, c=np.arange(len(cells_v)),
                           cmap='viridis', s=10, alpha=0.6, label=label)
        has_data = True

    if not has_data:
        plt.close(fig)
        print("    Skipped: No computational data")
        return False

    ax.set_xlabel('Cell Count')
    ax.set_ylabel('Time per Cell (ms)')
    ax.set_title('Cache Efficiency: Time per Cell vs Population Size')
    ax.legend(loc='upper left', frameon=False)
    cbar = plt.colorbar(scatter, ax=ax, label='Iteration Index')
    apply_tufte_style(ax, grid=True)

    save_figure(fig, output_dir / '13_cache_efficiency')
    return True


@register_plot(
    id='14',
    name='energy_conservation_timeline',
    narrative='biological',
    requires=['biological'],
    description='Energy conservation analysis with drift detection across schedulers'
)
def plot_energy_conservation_timeline(data: Dict, style: Dict, output_dir: Path,
                                      use_latex: bool = True) -> bool:
    """Plot energy conservation timeline with statistical drift analysis."""
    print("  Plotting: 14_energy_conservation_timeline")
    fig, axes = plt.subplots(1, 2, figsize=FIGSIZE_WIDE)

    has_data = False
    drift_rates = []

    for sched in data.get('schedulers', []):
        accessor = SchedulerDataAccessor(data, sched.name, 'biological')
        if not accessor.is_available():
            continue

        bio_df = accessor.get_raw_dataframe()
        if bio_df is None:
            continue

        ke_col = accessor.get_column(['total_kinetic_energy', 'kinetic_energy', 'mean_kinetic_energy'])
        pe_col = accessor.get_column(['total_potential_energy', 'potential_energy', 'mean_potential_energy'])

        if not ke_col or not pe_col:
            continue

        t = compute_time_seconds(bio_df)
        E_total = bio_df[ke_col].values + bio_df[pe_col].values
        valid = np.isfinite(E_total) & np.isfinite(t)

        if np.sum(valid) < 10:
            continue

        t_v = t[valid]
        E_v = E_total[valid]

        color = style['colors'].get(sched.name, cm.viridis(0.5))
        label = style['labels'].get(sched.name, sched.name)

        # Left panel: normalized energy over time
        E_norm = E_v / E_v[0] if E_v[0] != 0 else E_v
        axes[0].plot(t_v, E_norm, color=color, linewidth=1.0, label=label)

        # Compute drift rate
        try:
            result = test_energy_conservation(t_v, E_v)
            drift_rates.append({
                'scheduler': sched.name,
                'label': label,
                'drift_rate': result.rel_drift_rate,
                'conserved': result.is_conserved,
                'color': color,
            })
        except (ValueError, Exception):
            pass

        has_data = True

    if not has_data:
        plt.close(fig)
        print("    Skipped: No energy data")
        return False

    axes[0].axhline(y=1.0, color='gray', linestyle='--', linewidth=0.5, alpha=0.5)
    axes[0].set_xlabel('Time (s)')
    axes[0].set_ylabel('E(t) / E(0)')
    axes[0].set_title('A. Normalized Total Energy')
    axes[0].legend(loc='best', frameon=False, fontsize=7)
    apply_tufte_style(axes[0], grid=True, integer_time_axis=True)

    # Right panel: drift rate bar chart
    if drift_rates:
        names = [d['label'] for d in drift_rates]
        rates = [abs(d['drift_rate']) for d in drift_rates]
        colors_bar = [d['color'] for d in drift_rates]
        conserved = [d['conserved'] for d in drift_rates]

        bars = axes[1].barh(names, rates, color=colors_bar, alpha=0.7)
        for i, (bar, cons) in enumerate(zip(bars, conserved)):
            marker = 'PASS' if cons else 'FAIL'
            axes[1].text(bar.get_width() * 1.05, bar.get_y() + bar.get_height()/2,
                        marker, va='center', fontsize=7,
                        color='green' if cons else 'red')

        axes[1].axvline(x=1e-6, color='red', linestyle='--', linewidth=0.8, label='Threshold')
        axes[1].set_xlabel('|Relative Drift Rate| (1/s)')
        axes[1].set_xscale('log')
        axes[1].set_title('B. Conservation Test')
        axes[1].legend(loc='best', frameon=False, fontsize=7)
        apply_tufte_style(axes[1], grid=True)

    plt.tight_layout()
    save_figure(fig, output_dir / '14_energy_conservation_timeline')
    return True


@register_plot(
    id='15',
    name='roofline_trajectory',
    narrative='computational',
    requires=['computational', 'phase'],
    description='Operational intensity trajectory over simulation time'
)
def plot_roofline_trajectory(data: Dict, style: Dict, output_dir: Path,
                             use_latex: bool = True) -> bool:
    """Plot roofline trajectory: operational intensity over time."""
    print("  Plotting: 15_roofline_trajectory")
    fig, ax = plt.subplots(figsize=FIGSIZE_DOUBLE)

    has_data = False
    for sched in data.get('schedulers', []):
        accessor = SchedulerDataAccessor(data, sched.name, 'computational')
        phase_accessor = SchedulerDataAccessor(data, sched.name, 'phase')

        if not accessor.is_available() or not phase_accessor.is_available():
            continue

        comp_df = accessor.get_raw_dataframe()
        phase_df = phase_accessor.get_raw_dataframe()

        if comp_df is None or phase_df is None:
            continue

        cells_col = accessor.get_column(['cells'])
        total_col = phase_accessor.get_column(['total_iteration_ms'])
        contact_col = phase_accessor.get_column(['contact_detection_ms'])

        if not cells_col or not total_col:
            continue

        # Align by iteration if possible
        cells = comp_df[cells_col].values
        total_ms = phase_df[total_col].values
        min_len = min(len(cells), len(total_ms))

        if min_len < 5:
            continue

        cells = cells[:min_len]
        total_ms = total_ms[:min_len]

        # Operational intensity proxy: cells * interactions / time
        # Use cells^(4/3) as interaction proxy (from N^4/3 complexity)
        flops_proxy = cells ** (4.0/3.0)
        # Bytes proxy: cells * sizeof(node) ~ cells * 200 bytes
        bytes_proxy = cells * 200.0
        operational_intensity = flops_proxy / bytes_proxy
        throughput = flops_proxy / (total_ms / 1000.0)  # interactions per second

        valid = np.isfinite(operational_intensity) & np.isfinite(throughput) & (throughput > 0)
        if np.sum(valid) < 5:
            continue

        color = style['colors'].get(sched.name, cm.viridis(0.5))
        label = style['labels'].get(sched.name, sched.name)

        oi = operational_intensity[valid]
        tp = throughput[valid]
        ax.scatter(oi, tp, c=np.arange(np.sum(valid)), cmap='viridis',
                  s=10, alpha=0.6, label=label)

        # Draw trajectory arrow
        if len(oi) > 2:
            ax.annotate('', xy=(oi[-1], tp[-1]), xytext=(oi[0], tp[0]),
                       arrowprops=dict(arrowstyle='->', color=color, lw=1.5))

        has_data = True

    if not has_data:
        plt.close(fig)
        print("    Skipped: Insufficient data")
        return False

    ax.set_xlabel('Operational Intensity (FLOP/byte)')
    ax.set_ylabel('Throughput (interactions/s)')
    ax.set_xscale('log')
    ax.set_yscale('log')
    ax.set_title('Roofline Trajectory Over Simulation Time')
    ax.legend(loc='upper left', frameon=False)
    apply_tufte_style(ax, grid=True)

    save_figure(fig, output_dir / '15_roofline_trajectory')
    return True


@register_plot(
    id='16',
    name='contact_angle_distribution',
    narrative='biological',
    requires=['biological'],
    description='Contact fraction distribution for biological validation'
)
def plot_contact_angle_distribution(data: Dict, style: Dict, output_dir: Path,
                                     use_latex: bool = True) -> bool:
    """Plot contact angle/fraction distribution across schedulers."""
    print("  Plotting: 16_contact_angle_distribution")
    fig, axes = plt.subplots(1, 2, figsize=FIGSIZE_WIDE)

    has_data = False
    for sched in data.get('schedulers', []):
        accessor = SchedulerDataAccessor(data, sched.name, 'biological')
        if not accessor.is_available():
            continue

        bio_df = accessor.get_raw_dataframe()
        if bio_df is None or len(bio_df) < 5:
            continue

        color = style['colors'].get(sched.name, cm.viridis(0.5))
        label = style['labels'].get(sched.name, sched.name)

        # Contact fraction time series
        cf_col = accessor.get_column(['mean_contact_fraction', 'contact_fraction',
                                       'cell_contact_area_fraction'])
        if cf_col:
            t = compute_time_seconds(bio_df)
            cf = bio_df[cf_col].values
            valid = np.isfinite(cf) & np.isfinite(t)
            if np.sum(valid) > 5:
                axes[0].plot(t[valid], cf[valid], color=color, linewidth=1.0, label=label)
                has_data = True

                # Distribution of contact fractions (steady-state last 50%)
                steady = cf[valid][len(cf[valid])//2:]
                if len(steady) > 5:
                    axes[1].hist(steady, bins=20, color=color, alpha=0.5,
                                edgecolor='black', linewidth=0.3, label=label)

    if not has_data:
        plt.close(fig)
        print("    Skipped: No contact fraction data")
        return False

    axes[0].set_xlabel('Time (s)')
    axes[0].set_ylabel('Mean Contact Fraction')
    axes[0].set_title('A. Contact Fraction Evolution')
    axes[0].legend(loc='best', frameon=False, fontsize=7)
    apply_tufte_style(axes[0], grid=True, integer_time_axis=True)

    axes[1].set_xlabel('Contact Fraction')
    axes[1].set_ylabel('Frequency')
    axes[1].set_title('B. Steady-State Distribution')
    axes[1].legend(loc='best', frameon=False, fontsize=7)
    apply_tufte_style(axes[1], grid=True)

    plt.tight_layout()
    save_figure(fig, output_dir / '16_contact_angle_distribution')
    return True


@register_plot(
    id='17',
    name='ips_regression_timeline',
    narrative='computational',
    requires=['computational'],
    description='Historical IPS performance across benchmark runs for regression tracking'
)
def plot_ips_regression_timeline(data: Dict, style: Dict, output_dir: Path,
                                 use_latex: bool = True) -> bool:
    """Plot IPS regression timeline from SQLite database if available."""
    print("  Plotting: 17_ips_regression_timeline")

    # Try to load from SQLite database
    db_path = Path('doc/working/benchmark_metrics.db')
    if not db_path.exists():
        # Fallback: use current run data only
        fig, ax = plt.subplots(figsize=FIGSIZE_DOUBLE)

        has_data = False
        for sched in data.get('schedulers', []):
            accessor = SchedulerDataAccessor(data, sched.name, 'computational')
            if not accessor.is_available():
                continue

            ts = accessor.get_time_series(
                value_cols=['iter_per_sec', 'ips'],
                min_points=5
            )
            if ts is None:
                continue

            color = style['colors'].get(sched.name, cm.viridis(0.5))
            label = style['labels'].get(sched.name, sched.name)

            # Compute rolling average IPS
            window = max(5, len(ts.values) // 20)
            rolling_ips = pd.Series(ts.values).rolling(window=window, min_periods=1).mean().values

            ax.plot(ts.time, rolling_ips, color=color, linewidth=1.2, label=label)
            ax.fill_between(ts.time,
                           pd.Series(ts.values).rolling(window=window).quantile(0.25).values,
                           pd.Series(ts.values).rolling(window=window).quantile(0.75).values,
                           color=color, alpha=0.15)
            has_data = True

        if not has_data:
            plt.close(fig)
            print("    Skipped: No IPS data")
            return False

        ax.set_xlabel('Simulation Time (s)')
        ax.set_ylabel('Iterations per Second')
        ax.set_title('IPS Performance (Current Run)')
        ax.legend(loc='best', frameon=False)
        apply_tufte_style(ax, grid=True, integer_time_axis=True)

        save_figure(fig, output_dir / '17_ips_regression_timeline')
        return True

    # Load historical data from SQLite
    import sqlite3
    conn = sqlite3.connect(str(db_path))
    conn.row_factory = sqlite3.Row

    fig, ax = plt.subplots(figsize=FIGSIZE_DOUBLE)

    cursor = conn.execute("""
        SELECT r.id, r.git_commit, r.run_timestamp, r.scheduler,
               AVG(c.iter_per_sec) as avg_ips,
               MIN(c.iter_per_sec) as min_ips,
               MAX(c.iter_per_sec) as max_ips
        FROM benchmark_runs r
        JOIN computational_metrics c ON r.id = c.run_id
        WHERE c.iter_per_sec > 0
        GROUP BY r.id
        ORDER BY r.id ASC
    """)
    rows = cursor.fetchall()
    conn.close()

    if not rows:
        plt.close(fig)
        print("    Skipped: No historical data in database")
        return False

    # Group by scheduler
    sched_data = {}
    for row in rows:
        sched = row['scheduler']
        if sched not in sched_data:
            sched_data[sched] = {'runs': [], 'avg_ips': [], 'min_ips': [], 'max_ips': []}
        sched_data[sched]['runs'].append(row['id'])
        sched_data[sched]['avg_ips'].append(row['avg_ips'])
        sched_data[sched]['min_ips'].append(row['min_ips'])
        sched_data[sched]['max_ips'].append(row['max_ips'])

    for sched, sd in sched_data.items():
        x = range(len(sd['runs']))
        color = style['colors'].get(sched, cm.viridis(0.5))
        label = style['labels'].get(sched, sched)

        ax.plot(x, sd['avg_ips'], 'o-', color=color, linewidth=1.2,
               markersize=4, label=label)
        ax.fill_between(x, sd['min_ips'], sd['max_ips'],
                        color=color, alpha=0.15)

    ax.set_xlabel('Benchmark Run #')
    ax.set_ylabel('Average IPS')
    ax.set_title('Historical IPS Performance')
    ax.legend(loc='best', frameon=False)
    apply_tufte_style(ax, grid=True)

    save_figure(fig, output_dir / '17_ips_regression_timeline')
    return True
```

**Step 4: Run test to verify it passes**

Run: `cd /home/nilesh-patil/projects/version-cpp-next/.worktrees/benchmarking-system && python tests/ci/test_new_plots.py`
Expected: PASS (all 3 tests)

**Step 5: Commit**

```bash
git add scripts/plot_benchmark_unified.py tests/ci/test_new_plots.py
git commit -m "feat(viz): add 5 new CI/CD dashboard plots (cache, energy, roofline, contact, IPS)"
```

---

### Task 7: Static HTML Dashboard Generator

**Files:**
- Create: `scripts/ci/generate_dashboard.py`

**Step 1: Write the failing test**

```python
# tests/ci/test_generate_dashboard.py
"""Tests for static HTML dashboard generation."""
import os
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent.parent / 'scripts'))


def test_generate_minimal_dashboard():
    """Test generating a minimal dashboard HTML file."""
    from ci.generate_dashboard import generate_dashboard

    with tempfile.TemporaryDirectory() as tmpdir:
        output_path = Path(tmpdir) / "index.html"

        generate_dashboard(
            output_path=output_path,
            title="SimuCell3D Benchmarks",
            plots_dir=None,  # No plots, just structure
            regression_report=None,
            run_metadata={'commit': 'abc1234', 'branch': 'main', 'scheduler': 'adaptive'}
        )

        assert output_path.exists(), "Dashboard HTML not created"
        html = output_path.read_text()
        assert "<html" in html, "Not valid HTML"
        assert "SimuCell3D" in html, "Title not in HTML"
        assert "abc1234" in html, "Commit not in HTML"

    print("PASS: test_generate_minimal_dashboard")


def test_dashboard_with_plots():
    """Test dashboard with mock plot images."""
    from ci.generate_dashboard import generate_dashboard

    with tempfile.TemporaryDirectory() as tmpdir:
        # Create mock plot files
        plots_dir = Path(tmpdir) / "plots"
        plots_dir.mkdir()
        for i in range(3):
            (plots_dir / f"0{i+1}_test_plot.png").write_bytes(b'\x89PNG\r\n\x1a\n' + b'\x00' * 100)

        output_path = Path(tmpdir) / "index.html"

        generate_dashboard(
            output_path=output_path,
            title="SimuCell3D Benchmarks",
            plots_dir=plots_dir,
            regression_report="## No regressions detected",
            run_metadata={'commit': 'abc1234', 'branch': 'main'}
        )

        html = output_path.read_text()
        assert "01_test_plot.png" in html, "Plot reference not in HTML"

    print("PASS: test_dashboard_with_plots")


if __name__ == "__main__":
    test_name = sys.argv[1] if len(sys.argv) > 1 else None
    tests = {
        'test_generate_minimal_dashboard': test_generate_minimal_dashboard,
        'test_dashboard_with_plots': test_dashboard_with_plots,
    }

    if test_name:
        tests[test_name]()
    else:
        for name, fn in tests.items():
            fn()
        print(f"\nAll {len(tests)} tests passed.")
```

**Step 2: Run test to verify it fails**

Run: `cd /home/nilesh-patil/projects/version-cpp-next/.worktrees/benchmarking-system && python tests/ci/test_generate_dashboard.py`
Expected: FAIL with `ModuleNotFoundError: No module named 'ci.generate_dashboard'`

**Step 3: Write minimal implementation**

```python
# scripts/ci/generate_dashboard.py
#!/usr/bin/env python3
"""
Generate static HTML dashboard from benchmark results.

Creates a self-contained HTML file with embedded plots and metrics summary.

Usage:
    python -m ci.generate_dashboard --output docs/benchmarks/index.html \
        --plots-dir <benchmark>/plots-unified/ \
        --regression-report regression.md \
        --commit abc1234 --branch main
"""

import argparse
import base64
import sys
from datetime import datetime
from pathlib import Path
from typing import Dict, Optional


DASHBOARD_TEMPLATE = """<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>{title}</title>
    <style>
        :root {{
            --bg: #fafafa;
            --card-bg: #ffffff;
            --text: #333;
            --border: #e0e0e0;
            --accent: #1a73e8;
            --success: #0d904f;
            --warning: #e8a817;
            --danger: #d93025;
        }}
        * {{ margin: 0; padding: 0; box-sizing: border-box; }}
        body {{
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: var(--bg);
            color: var(--text);
            line-height: 1.6;
        }}
        .header {{
            background: var(--card-bg);
            border-bottom: 1px solid var(--border);
            padding: 1.5rem 2rem;
        }}
        .header h1 {{ font-size: 1.5rem; font-weight: 600; }}
        .header .meta {{ color: #666; font-size: 0.85rem; margin-top: 0.3rem; }}
        .container {{ max-width: 1200px; margin: 0 auto; padding: 1.5rem; }}
        .card {{
            background: var(--card-bg);
            border: 1px solid var(--border);
            border-radius: 8px;
            padding: 1.5rem;
            margin-bottom: 1.5rem;
        }}
        .card h2 {{
            font-size: 1.1rem;
            margin-bottom: 1rem;
            padding-bottom: 0.5rem;
            border-bottom: 1px solid var(--border);
        }}
        .grid {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(500px, 1fr)); gap: 1.5rem; }}
        .plot-img {{ width: 100%; height: auto; border-radius: 4px; }}
        .regression-report {{
            font-family: 'SFMono-Regular', Consolas, monospace;
            font-size: 0.85rem;
            white-space: pre-wrap;
            background: #f5f5f5;
            padding: 1rem;
            border-radius: 4px;
            overflow-x: auto;
        }}
        .badge {{
            display: inline-block;
            padding: 0.2rem 0.6rem;
            border-radius: 12px;
            font-size: 0.75rem;
            font-weight: 600;
        }}
        .badge-success {{ background: #e6f4ea; color: var(--success); }}
        .badge-warning {{ background: #fef7e0; color: var(--warning); }}
        .badge-danger {{ background: #fce8e6; color: var(--danger); }}
        table {{ width: 100%; border-collapse: collapse; }}
        th, td {{ text-align: left; padding: 0.5rem; border-bottom: 1px solid var(--border); }}
        th {{ font-weight: 600; font-size: 0.85rem; color: #666; }}
    </style>
</head>
<body>
    <div class="header">
        <h1>{title}</h1>
        <div class="meta">
            Generated: {timestamp} | Commit: <code>{commit}</code> | Branch: <code>{branch}</code>
            {extra_meta}
        </div>
    </div>

    <div class="container">
        {regression_section}
        {plots_section}
    </div>
</body>
</html>"""


def _make_regression_section(report: Optional[str]) -> str:
    if not report:
        return ""
    return f"""
    <div class="card">
        <h2>Regression Detection</h2>
        <div class="regression-report">{report}</div>
    </div>"""


def _make_plots_section(plots_dir: Optional[Path]) -> str:
    if not plots_dir or not plots_dir.exists():
        return '<div class="card"><h2>Plots</h2><p>No plots available.</p></div>'

    plot_files = sorted(plots_dir.glob("*.png"))
    if not plot_files:
        return '<div class="card"><h2>Plots</h2><p>No plot images found.</p></div>'

    items = []
    for pf in plot_files:
        name = pf.stem.replace('_', ' ').title()
        # Use relative path reference (not embedding)
        items.append(f"""
        <div class="card">
            <h2>{name}</h2>
            <img class="plot-img" src="{pf.name}" alt="{name}" loading="lazy">
        </div>""")

    return f'<div class="grid">{"".join(items)}</div>'


def generate_dashboard(
    output_path: Path,
    title: str = "SimuCell3D Benchmark Dashboard",
    plots_dir: Optional[Path] = None,
    regression_report: Optional[str] = None,
    run_metadata: Optional[Dict] = None,
) -> None:
    """Generate a static HTML dashboard.

    Args:
        output_path: Path to write the HTML file
        title: Dashboard title
        plots_dir: Directory containing plot PNG files
        regression_report: Markdown/text regression report
        run_metadata: Dict with 'commit', 'branch', etc.
    """
    meta = run_metadata or {}

    html = DASHBOARD_TEMPLATE.format(
        title=title,
        timestamp=datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        commit=meta.get('commit', 'unknown')[:8],
        branch=meta.get('branch', 'unknown'),
        extra_meta=f"| Scheduler: <code>{meta.get('scheduler', 'N/A')}</code>" if 'scheduler' in meta else "",
        regression_section=_make_regression_section(regression_report),
        plots_section=_make_plots_section(plots_dir),
    )

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(html)


def main():
    parser = argparse.ArgumentParser(description='Generate static HTML benchmark dashboard')
    parser.add_argument('--output', type=Path, default=Path('docs/benchmarks/index.html'),
                        help='Output HTML file path')
    parser.add_argument('--plots-dir', type=Path, default=None,
                        help='Directory containing plot PNG files')
    parser.add_argument('--regression-report', type=Path, default=None,
                        help='Path to regression report markdown')
    parser.add_argument('--title', type=str, default='SimuCell3D Benchmark Dashboard',
                        help='Dashboard title')
    parser.add_argument('--commit', type=str, default='unknown')
    parser.add_argument('--branch', type=str, default='unknown')
    parser.add_argument('--scheduler', type=str, default='adaptive')

    args = parser.parse_args()

    regression_text = None
    if args.regression_report and args.regression_report.exists():
        regression_text = args.regression_report.read_text()

    generate_dashboard(
        output_path=args.output,
        title=args.title,
        plots_dir=args.plots_dir,
        regression_report=regression_text,
        run_metadata={
            'commit': args.commit,
            'branch': args.branch,
            'scheduler': args.scheduler,
        }
    )

    print(f"Dashboard generated: {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

**Step 4: Run test to verify it passes**

Run: `cd /home/nilesh-patil/projects/version-cpp-next/.worktrees/benchmarking-system && python tests/ci/test_generate_dashboard.py`
Expected: PASS (both tests)

**Step 5: Commit**

```bash
git add scripts/ci/generate_dashboard.py tests/ci/test_generate_dashboard.py
git commit -m "feat(ci): add static HTML dashboard generator"
```

---

### Task 8: CI/CD Documentation

**Files:**
- Create: `doc/benchmarking/ci-integration.md`
- Create: `doc/benchmarking/unified-dashboard.md`

**Step 1: Write CI integration guide**

```markdown
# doc/benchmarking/ci-integration.md
# CI/CD Benchmark Integration Guide

## Overview

SimuCell3D uses three automated benchmark workflows to track performance:

| Workflow | Trigger | Duration | Purpose |
|----------|---------|----------|---------|
| `benchmark-pr.yml` | Pull requests to main | <5 min | Quick regression check |
| `benchmark-nightly.yml` | Daily at 2 AM UTC | <2 hr | Comprehensive multi-scheduler |
| `benchmark-weekly.yml` | Weekly (Sunday midnight) | <24 hr | Stress testing |

## Architecture

```
PR/Nightly/Weekly Trigger
    │
    ├─ Build SimuCell3D (Release)
    ├─ Run simulation with --diagnostics-csv
    ├─ Preprocess metrics (preprocess_benchmark_metrics.py)
    ├─ Ingest into SQLite (ci/aggregate_results.py)
    ├─ Detect regressions (ci/regression_detector.py)
    ├─ Generate plots (plot_benchmark_unified.py)
    └─ Generate dashboard (ci/generate_dashboard.py)
```

## Regression Detection

The regression detector uses **bootstrap permutation testing** with dual thresholds:

1. **Statistical significance**: p-value < 0.05 (10,000 bootstrap iterations)
2. **Practical significance**: >10% performance change

Both conditions must be met to flag a regression. This prevents false alarms from
natural performance variance while catching meaningful slowdowns.

### How it works

1. Latest benchmark IPS values are compared against the previous 5 runs (baseline)
2. A permutation test shuffles combined samples 10,000 times to build a null distribution
3. The p-value represents how likely the observed difference arose by chance
4. Phase-level timing breakdowns identify which simulation phase regressed

### PR workflow behavior

- Regression detected: PR check **fails**, comment posted with details
- No regression: PR check **passes**, comment updated with results
- Insufficient baseline: Check passes with warning (needs 5+ historical runs)

## SQLite Metrics Database

Historical benchmark data is stored in `doc/working/benchmark_metrics.db`.

### Schema

- `benchmark_runs` - Run metadata (commit, branch, scheduler, timestamp)
- `computational_metrics` - Per-iteration IPS, cell count, memory
- `biological_metrics` - Pressure, volume, cell count per iteration
- `phase_timings` - Per-phase timing breakdown per iteration

### Manual ingestion

```bash
python -m scripts.ci.aggregate_results \
    --db doc/working/benchmark_metrics.db \
    --benchmark-dir <benchmark_directory> \
    --commit $(git rev-parse HEAD) \
    --branch $(git branch --show-current) \
    --scheduler adaptive
```

## Manual workflow dispatch

All workflows support manual triggering via GitHub Actions UI or CLI:

```bash
# Trigger nightly benchmark
gh workflow run benchmark-nightly.yml

# Trigger weekly stress test
gh workflow run benchmark-weekly.yml
```

## Extending the system

### Adding a new metric

1. Add column to relevant table in `scripts/ci/aggregate_results.py`
2. Add ingestion logic in `ingest_benchmark()`
3. Add plot in `scripts/plot_benchmark_unified.py` using `@register_plot`
4. Update regression detector if metric should trigger alerts

### Adding a new scheduler

No changes needed - the system auto-detects schedulers from `sim_*` directories.
```

**Step 2: Write dashboard documentation**

```markdown
# doc/benchmarking/unified-dashboard.md
# SimuCell3D Benchmark Dashboard

## Overview

The benchmark dashboard provides a unified view of simulation performance,
biological accuracy, and regression tracking. It is generated automatically
by CI workflows and can also be produced locally.

## Generating the dashboard locally

```bash
# 1. Run a benchmark
./build/simucell3d parameters/core/parameters_vesicle.xml \
    --schedule=adaptive \
    --diagnostics-csv=benchmark/sim_adaptive/performance_diagnostics.csv

# 2. Preprocess metrics
python scripts/preprocess_benchmark_metrics.py benchmark/

# 3. Generate plots (17 original + 5 new = 22 total)
python scripts/plot_benchmark_unified.py benchmark/ --quality draft --no-latex

# 4. Generate HTML dashboard
python -m scripts.ci.generate_dashboard \
    --output docs/benchmarks/index.html \
    --plots-dir benchmark/plots-unified/ \
    --commit $(git rev-parse HEAD) \
    --branch $(git branch --show-current)
```

## Plot inventory (22 plots)

### Biological Narrative (7 plots)
| ID | Name | Description |
|----|------|-------------|
| 01 | Pressure Evolution | Pressure homeostasis with 95% CI |
| 02 | Energy Landscape | Total energy with conservation testing |
| 02b | Cell Heterogeneity | Per-cell metric distributions |
| 03 | Biological Dashboard | 6-panel biological summary |
| 11 | Population Dynamics | Growth and division rates |
| 14 | Energy Conservation Timeline | Drift detection across runs |
| 16 | Contact Angle Distribution | Contact fraction validation |

### Computational Narrative (10 plots)
| ID | Name | Description |
|----|------|-------------|
| 04 | Scaling Analysis | O(N^4/3) power law regression |
| 05 | Phase Timing | Stacked phase timing breakdown |
| 06 | Roofline Model | Memory vs compute bound analysis |
| 07 | Load Balance | Thread workload distribution |
| 08 | Scheduler Comparison | Time series IPS comparison |
| 09 | Performance Ratio | Bootstrap significance testing |
| 10 | Scheduler Radar | Multi-dimensional scheduler comparison |
| 13 | Cache Efficiency | Time-per-cell vs population size |
| 15 | Roofline Trajectory | Operational intensity over time |
| 17 | IPS Regression Timeline | Historical performance tracking |
```

**Step 3: Commit**

```bash
git add doc/benchmarking/ci-integration.md doc/benchmarking/unified-dashboard.md
git commit -m "docs: add CI/CD integration and dashboard documentation"
```

---

### Task 9: Integration Test & Final Verification

**Step 1: Create test/__init__.py files and run all tests**

Run:
```bash
cd /home/nilesh-patil/projects/version-cpp-next/.worktrees/benchmarking-system
touch tests/ci/__init__.py
python tests/ci/test_aggregate_results.py
python tests/ci/test_regression_detector.py
python tests/ci/test_new_plots.py
python tests/ci/test_generate_dashboard.py
```
Expected: All tests pass

**Step 2: Validate all YAML workflows**

Run:
```bash
python3 -c "
import yaml
for f in ['.github/workflows/benchmark-pr.yml',
          '.github/workflows/benchmark-nightly.yml',
          '.github/workflows/benchmark-weekly.yml']:
    yaml.safe_load(open(f))
    print(f'  OK: {f}')
print('All workflows valid')
"
```
Expected: All 3 workflows pass YAML validation

**Step 3: Verify file count and structure**

Run:
```bash
echo "=== New CI scripts ==="
ls -la scripts/ci/
echo ""
echo "=== New workflows ==="
ls -la .github/workflows/benchmark-*.yml
echo ""
echo "=== New tests ==="
ls -la tests/ci/
echo ""
echo "=== New docs ==="
ls -la doc/benchmarking/
```
Expected: All deliverables present

**Step 4: Final commit with integration test marker**

```bash
git add tests/ci/__init__.py
git commit -m "test: add CI test infrastructure init files"
```
