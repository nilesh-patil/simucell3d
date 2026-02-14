#!/usr/bin/env python3
"""
Roofline Analysis Tool for SimuCell3D

Generates roofline model plots from perf counter data to identify whether
computational kernels are compute-bound or memory-bound.

The roofline model plots achievable performance (FLOP/s) against
operational intensity (FLOP/byte). A kernel below the roofline is
bottlenecked by one of:
  - Memory bandwidth (below the sloped line)
  - Compute throughput (below the horizontal line)

Usage:
    python3 roofline_analysis.py --json results/*.json --output roofline.png
    python3 roofline_analysis.py --peak-flops 50 --peak-bandwidth 25 --json data/
    python3 roofline_analysis.py --detect-hardware --json results/

Dependencies:
    - matplotlib (optional, for plotting; falls back to ASCII art)
    - numpy (optional, for numerical operations)
    - json (standard library)
"""

import argparse
import json
import os
import sys
import glob
from pathlib import Path


def load_json_results(paths):
    """Load benchmark results from JSON files or directories."""
    results = []
    for path in paths:
        if os.path.isdir(path):
            json_files = glob.glob(os.path.join(path, "*.json"))
            for jf in json_files:
                try:
                    with open(jf) as f:
                        results.append(json.load(f))
                except (json.JSONDecodeError, IOError) as e:
                    print(f"Warning: Could not load {jf}: {e}", file=sys.stderr)
        elif os.path.isfile(path):
            try:
                with open(path) as f:
                    results.append(json.load(f))
            except (json.JSONDecodeError, IOError) as e:
                print(f"Warning: Could not load {path}: {e}", file=sys.stderr)
    return results


def detect_hardware():
    """Detect hardware capabilities for roofline ceiling."""
    peak_flops_gflops = 50.0   # Default: conservative estimate
    peak_bw_gbs = 25.0         # Default: DDR4 dual-channel

    try:
        # Try to detect CPU frequency
        with open("/proc/cpuinfo") as f:
            for line in f:
                if "cpu MHz" in line:
                    freq_mhz = float(line.split(":")[1].strip())
                    # Assume 2 FP ops per cycle (FMA) * cores
                    import multiprocessing
                    cores = multiprocessing.cpu_count()
                    peak_flops_gflops = (freq_mhz / 1000.0) * 2 * cores
                    break
    except (IOError, ValueError):
        pass

    try:
        # Try to detect memory bandwidth from DMI or meminfo
        with open("/proc/meminfo") as f:
            for line in f:
                if "MemTotal" in line:
                    mem_kb = int(line.split(":")[1].strip().split()[0])
                    # Rough estimate: DDR4 bandwidth ~25 GB/s for typical configs
                    if mem_kb > 32 * 1024 * 1024:  # >32GB
                        peak_bw_gbs = 50.0  # Server-class
                    elif mem_kb > 8 * 1024 * 1024:  # >8GB
                        peak_bw_gbs = 25.0  # Desktop
                    else:
                        peak_bw_gbs = 15.0  # Laptop
                    break
    except (IOError, ValueError):
        pass

    return peak_flops_gflops, peak_bw_gbs


def estimate_operational_intensity(result):
    """Estimate operational intensity (FLOP/byte) from perf counters.

    For contact detection:
    - FLOPs: ~10 per AABB overlap test (6 comparisons + 4 logic)
    - Bytes: ~48 per AABB read (6 doubles) + ~24 per vec3 query
    """
    counters = result.get("counters", {})
    instructions = counters.get("instructions", 0)
    l1_loads = counters.get("L1-dcache-loads", 0)

    if l1_loads == 0 or instructions == 0:
        return None

    # Approximate: 30% of instructions are FP ops (from profiling)
    estimated_flops = instructions * 0.30

    # Each L1 load is 8 bytes (double)
    estimated_bytes = l1_loads * 8

    if estimated_bytes == 0:
        return None

    return estimated_flops / estimated_bytes


def estimate_gflops(result):
    """Estimate achieved GFLOP/s from perf counters."""
    counters = result.get("counters", {})
    instructions = counters.get("instructions", 0)
    duration = result.get("duration_seconds", 0)

    if duration == 0 or instructions == 0:
        return None

    # Approximate: 30% of instructions are FP ops
    estimated_flops = instructions * 0.30
    return (estimated_flops / duration) / 1e9


def generate_roofline_plot(results, peak_flops, peak_bw, output_file):
    """Generate roofline plot. Falls back to ASCII if matplotlib unavailable."""
    # Extract data points
    points = []
    for r in results:
        oi = estimate_operational_intensity(r)
        gflops = estimate_gflops(r)
        name = r.get("benchmark", "unknown")

        if oi is not None and gflops is not None and oi > 0 and gflops > 0:
            points.append((oi, gflops, name))

    if not points:
        print("Warning: No valid data points for roofline plot.", file=sys.stderr)
        print("This may happen when perf hardware counters are not available.")
        print("Run benchmarks with perf_wrapper.sh to collect counter data.")
        _generate_ascii_placeholder(peak_flops, peak_bw)
        return

    try:
        import matplotlib
        matplotlib.use("Agg")  # Non-interactive backend
        import matplotlib.pyplot as plt
        import numpy as np

        _generate_matplotlib_plot(points, peak_flops, peak_bw, output_file)
    except ImportError:
        print("matplotlib not available. Generating ASCII roofline.", file=sys.stderr)
        _generate_ascii_roofline(points, peak_flops, peak_bw, output_file)


def _generate_matplotlib_plot(points, peak_flops, peak_bw, output_file):
    """Generate roofline plot with matplotlib."""
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    import numpy as np

    fig, ax = plt.subplots(1, 1, figsize=(10, 7))

    # Roofline model
    oi_range = np.logspace(-2, 3, 500)
    roofline = np.minimum(peak_bw * oi_range, peak_flops)
    ax.loglog(oi_range, roofline, "k-", linewidth=2, label="Roofline")

    # Memory-bound region
    ridge_point = peak_flops / peak_bw
    ax.axvline(x=ridge_point, color="gray", linestyle="--", alpha=0.5, label=f"Ridge point (OI={ridge_point:.1f})")

    # Plot benchmark points
    oi_vals = [p[0] for p in points]
    gflops_vals = [p[1] for p in points]
    names = [p[2] for p in points]

    # Color by category
    colors = []
    for name in names:
        if "contact" in name or "uspg" in name or "sap" in name:
            colors.append("red")
        elif "atomic" in name or "mutex" in name:
            colors.append("blue")
        elif "bandwidth" in name or "memory" in name or "soa" in name or "aos" in name:
            colors.append("green")
        else:
            colors.append("orange")

    ax.scatter(oi_vals, gflops_vals, c=colors, s=80, zorder=5, edgecolors="black")

    # Label points
    for oi, gf, name in points:
        short_name = name.replace("test_", "").replace("benchmark_", "")
        ax.annotate(short_name, (oi, gf), textcoords="offset points",
                    xytext=(5, 5), fontsize=7, alpha=0.8)

    ax.set_xlabel("Operational Intensity (FLOP/byte)", fontsize=12)
    ax.set_ylabel("Performance (GFLOP/s)", fontsize=12)
    ax.set_title("SimuCell3D Roofline Analysis", fontsize=14)
    ax.legend(loc="lower right")
    ax.grid(True, alpha=0.3, which="both")
    ax.set_xlim(0.01, 100)
    ax.set_ylim(0.01, peak_flops * 2)

    plt.tight_layout()
    plt.savefig(output_file, dpi=150, bbox_inches="tight")
    print(f"Roofline plot saved to: {output_file}")


def _generate_ascii_roofline(points, peak_flops, peak_bw, output_file):
    """Generate ASCII art roofline when matplotlib is unavailable."""
    width = 70
    height = 20

    print("\n" + "=" * width)
    print("  SimuCell3D Roofline Analysis (ASCII)")
    print("=" * width)
    print(f"  Peak Compute:  {peak_flops:.1f} GFLOP/s")
    print(f"  Peak Bandwidth: {peak_bw:.1f} GB/s")
    print(f"  Ridge Point:   {peak_flops / peak_bw:.2f} FLOP/byte")
    print("-" * width)

    print("\n  Benchmark Results:")
    print(f"  {'Name':<40} {'OI':>8} {'GFLOP/s':>10} {'Bound':>12}")
    print(f"  {'-'*40} {'-'*8} {'-'*10} {'-'*12}")

    ridge = peak_flops / peak_bw
    for oi, gflops, name in sorted(points, key=lambda x: x[0]):
        short_name = name.replace("test_", "")[:38]
        if oi < ridge:
            bound = "MEM-BOUND"
        else:
            bound = "CPU-BOUND"
        achievable = min(peak_bw * oi, peak_flops)
        utilization = (gflops / achievable * 100) if achievable > 0 else 0
        print(f"  {short_name:<40} {oi:>8.2f} {gflops:>10.2f} {bound:>12}")

    # Save text report
    if output_file:
        txt_file = output_file.replace(".png", ".txt").replace(".pdf", ".txt")
        with open(txt_file, "w") as f:
            f.write("SimuCell3D Roofline Analysis\n")
            f.write(f"Peak Compute:  {peak_flops:.1f} GFLOP/s\n")
            f.write(f"Peak Bandwidth: {peak_bw:.1f} GB/s\n\n")
            for oi, gflops, name in points:
                f.write(f"{name}: OI={oi:.2f}, GFLOP/s={gflops:.2f}\n")
        print(f"\nText report saved to: {txt_file}")


def _generate_ascii_placeholder(peak_flops, peak_bw):
    """Generate placeholder when no data is available."""
    ridge = peak_flops / peak_bw
    print("\n  Roofline Model (no benchmark data available)")
    print(f"  Peak Compute:   {peak_flops:.1f} GFLOP/s")
    print(f"  Peak Bandwidth: {peak_bw:.1f} GB/s")
    print(f"  Ridge Point:    {ridge:.2f} FLOP/byte")
    print()
    print("  Run benchmarks with perf_wrapper.sh first:")
    print("    ./scripts/benchmarking/latency/perf_wrapper.sh --all")
    print("  Then re-run this script with the JSON output.")


def print_metrics_summary(results):
    """Print a summary table of key metrics from all results."""
    print("\n" + "=" * 90)
    print("  SimuCell3D Latency Profiling Summary")
    print("=" * 90)
    print(f"  {'Benchmark':<40} {'Cache Miss%':>11} {'IPC':>6} {'Branch Miss%':>12} {'Status':>8}")
    print(f"  {'-'*40} {'-'*11} {'-'*6} {'-'*12} {'-'*8}")

    for r in sorted(results, key=lambda x: x.get("benchmark", "")):
        name = r.get("benchmark", "?")[:38]
        derived = r.get("derived", {})
        cache_miss = derived.get("cache_miss_rate", 0) * 100
        ipc = derived.get("ipc", 0)
        branch_miss = derived.get("branch_miss_rate", 0) * 100
        status = "PASS" if r.get("exit_code", 1) == 0 else "FAIL"

        print(f"  {name:<40} {cache_miss:>10.2f}% {ipc:>6.2f} {branch_miss:>11.2f}% {status:>8}")

    print("=" * 90)


def main():
    parser = argparse.ArgumentParser(
        description="Roofline analysis for SimuCell3D benchmarks",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Generate roofline from perf data
  python3 roofline_analysis.py --json results/*.json --output roofline.png

  # Auto-detect hardware and analyze
  python3 roofline_analysis.py --detect-hardware --json results/

  # Print metrics summary
  python3 roofline_analysis.py --json results/ --summary-only
        """,
    )
    parser.add_argument("--json", nargs="+", required=True,
                        help="JSON result files or directories")
    parser.add_argument("--output", default="roofline.png",
                        help="Output plot file (default: roofline.png)")
    parser.add_argument("--peak-flops", type=float, default=None,
                        help="Peak GFLOP/s (auto-detected if not specified)")
    parser.add_argument("--peak-bandwidth", type=float, default=None,
                        help="Peak memory bandwidth in GB/s (auto-detected if not specified)")
    parser.add_argument("--detect-hardware", action="store_true",
                        help="Auto-detect hardware capabilities")
    parser.add_argument("--summary-only", action="store_true",
                        help="Only print metrics summary, skip roofline plot")

    args = parser.parse_args()

    # Load results
    results = load_json_results(args.json)
    if not results:
        print("Error: No valid JSON results found.", file=sys.stderr)
        sys.exit(1)

    print(f"Loaded {len(results)} benchmark results.")

    # Detect or use provided hardware parameters
    if args.detect_hardware or (args.peak_flops is None and args.peak_bandwidth is None):
        detected_flops, detected_bw = detect_hardware()
        peak_flops = args.peak_flops or detected_flops
        peak_bw = args.peak_bandwidth or detected_bw
        print(f"Hardware: Peak={peak_flops:.1f} GFLOP/s, BW={peak_bw:.1f} GB/s")
    else:
        peak_flops = args.peak_flops or 50.0
        peak_bw = args.peak_bandwidth or 25.0

    # Print summary
    print_metrics_summary(results)

    # Generate roofline
    if not args.summary_only:
        generate_roofline_plot(results, peak_flops, peak_bw, args.output)


if __name__ == "__main__":
    main()
