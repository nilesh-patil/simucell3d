"""Tests for benchmark metrics aggregation into SQLite."""
import os
import sys
import sqlite3
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent.parent / 'scripts'))


def create_mock_benchmark_dir(base_dir):
    """Create a mock benchmark directory with CSV files."""
    bench_dir = Path(base_dir) / "benchmark_20260215_120000"
    metrics_dir = bench_dir / "metrics" / "adaptive"
    metrics_dir.mkdir(parents=True)

    comp_csv = metrics_dir / "computational.csv"
    comp_csv.write_text(
        "timestamp,epoch,iteration,cells,iter_per_sec,rss_mb\n"
        "2026-02-15T12:00:00,1739620800,1,13,5.2,122.1\n"
        "2026-02-15T12:00:01,1739620801,2,13,5.1,122.1\n"
        "2026-02-15T12:00:02,1739620802,3,14,4.9,123.8\n"
    )

    bio_csv = metrics_dir / "biological.csv"
    bio_csv.write_text(
        "iteration,mean_pressure,mean_volume,cell_count\n"
        "1,1500.0,4.2e-16,13\n"
        "2,1520.0,4.3e-16,13\n"
        "3,1480.0,4.1e-16,14\n"
    )

    phase_csv = metrics_dir / "phase_timings.csv"
    phase_csv.write_text(
        "timestamp,iteration,mesh_refinement_ms,contact_detection_ms,"
        "polarization_ms,time_integration_ms,total_iteration_ms\n"
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

        cursor = conn.execute(
            "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name"
        )
        tables = [row[0] for row in cursor.fetchall()]
        assert 'benchmark_runs' in tables, f"Missing benchmark_runs table, got {tables}"
        assert 'computational_metrics' in tables, f"Missing computational_metrics, got {tables}"
        assert 'biological_metrics' in tables, f"Missing biological_metrics, got {tables}"
        assert 'phase_timings' in tables, f"Missing phase_timings, got {tables}"

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

        ingest_benchmark(conn, bench_dir, "abc1234", "main", "adaptive")
        ingest_benchmark(conn, bench_dir, "def5678", "main", "adaptive")

        runs = query_latest_runs(conn, scheduler="adaptive", limit=5)
        assert len(runs) == 2, f"Expected 2 runs, got {len(runs)}"
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
