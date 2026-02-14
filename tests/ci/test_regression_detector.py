"""Tests for benchmark regression detection."""
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
