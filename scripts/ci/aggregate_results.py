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
