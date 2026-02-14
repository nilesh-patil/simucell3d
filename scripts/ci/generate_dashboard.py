#!/usr/bin/env python3
"""
Generate static HTML dashboard from benchmark results.

Creates a self-contained HTML file with plot references and metrics summary.

Usage:
    python -m ci.generate_dashboard --output docs/benchmarks/index.html \
        --plots-dir <benchmark>/plots-unified/ \
        --regression-report regression.md \
        --commit abc1234 --branch main
"""

import argparse
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
