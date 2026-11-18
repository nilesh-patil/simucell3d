#!/usr/bin/env python3
"""Same-machine A/B timing of two simucell3d builds on a fixed amount of work.

Used by .github/workflows/perf.yml to compare a pull request (B, "head") with its merge
base (A, "base"). Both binaries run the same parameter file, which fixes the number of
iterations, on the same runner, interleaved A B A B ... so that slow drifts of the
machine affect both equally, after one unmeasured warm-up run of each. Mesh output goes to a temporary directory that is deleted
after each run.

The result is informational: the script exits 0 whenever the head build ran, even if it
is slower. It writes a Markdown table (``--markdown``) and the raw timings
(``--json``). The verdict is "slower" only when the median ratio head/base is above
``--threshold`` (default 1.10) and the two ranges of wall times do not overlap.

Usage::

    python3 scripts/ci/perf_ab.py --base build-base/simucell3d --head build-head/simucell3d \\
        --parameter-file parameters/core/parameters_vesicle_quick.xml \\
        --simulation-duration 2e-6 --sampling-period 2e-6 --source-dir . --reps 5 --threads 4 \\
        --markdown perf.md --json perf.json

Exit status: 0 when the head build completed every repetition, 1 otherwise.
"""

import argparse
import json
import os
import platform
import re
import shutil
import statistics
import subprocess
import sys
import tempfile
import time
from pathlib import Path


def cpu_model():
    try:
        for line in Path("/proc/cpuinfo").read_text().splitlines():
            if line.startswith("model name"):
                return line.split(":", 1)[1].strip()
    except OSError:
        pass
    return platform.processor() or "unknown"


def shortened_copy(parameter_file, duration, sampling, directory):
    """Write a copy of parameter_file with <simulation_duration> and <sampling_period> replaced."""
    text = Path(parameter_file).read_text(encoding="utf-8")
    for tag, value in (("simulation_duration", duration), ("sampling_period", sampling)):
        if value is None:
            continue
        text, count = re.subn(r"<{0}>[^<]*</{0}>".format(tag), "<{0}>{1}</{0}>".format(tag, value), text)
        if count == 0:
            raise SystemExit("error: {}: no <{}> element".format(parameter_file, tag))
    path = Path(directory) / Path(parameter_file).name
    path.write_text(text, encoding="utf-8")
    return path


def run_once(binary, parameter_file, source_dir, threads, timeout):
    """Run one simulation; return (wall seconds, exit status)."""
    out_dir = tempfile.mkdtemp(prefix="perf_ab_")
    cmd = [str(binary), str(parameter_file), "--output-dir=" + out_dir, "--threads={}".format(threads)]
    start = time.perf_counter()
    try:
        proc = subprocess.run(cmd, cwd=str(source_dir), stdout=subprocess.DEVNULL,
                              stderr=subprocess.PIPE, timeout=timeout, check=False)
        status = proc.returncode
        if status != 0:
            sys.stderr.write(proc.stderr.decode(errors="replace")[-2000:])
    except subprocess.TimeoutExpired:
        status = "timeout"
    wall = time.perf_counter() - start
    shutil.rmtree(out_dir, ignore_errors=True)
    return wall, status


def verdict(base, head, threshold):
    if not base or not head:
        return "not compared", None
    ratio = statistics.median(head) / statistics.median(base)
    overlap = min(head) <= max(base) and min(base) <= max(head)
    if ratio > threshold and not overlap:
        return "slower", ratio
    if ratio < 1.0 / threshold and not overlap:
        return "faster", ratio
    return "no significant change", ratio


def markdown(result):
    lines = ["### Performance A/B (informational)", ""]
    lines.append("Same runner, interleaved, {reps} repetitions each, {threads} threads, "
                 "`{param}`.".format(**result))
    lines.append("")
    lines.append("| Build | Median wall time (s) | Min (s) | Max (s) | Failed runs |")
    lines.append("|---|---|---|---|---|")
    for name in ("base", "head"):
        t = result[name]["times"]
        if t:
            lines.append("| {} (`{}`) | {:.2f} | {:.2f} | {:.2f} | {} |".format(
                name, result[name]["ref"], statistics.median(t), min(t), max(t), result[name]["failures"]))
        else:
            lines.append("| {} (`{}`) | - | - | - | {} |".format(name, result[name]["ref"], result[name]["failures"]))
    lines.append("")
    ratio = result["ratio"]
    lines.append("Verdict: **{}**{}. Threshold: median ratio above {:.2f} with non-overlapping "
                 "ranges.".format(result["verdict"],
                                  " (head/base = {:.3f})".format(ratio) if ratio else "",
                                  result["threshold"]))
    lines.append("")
    lines.append("Runner: {cpu}, {nproc} logical CPUs. Hosted runners are shared machines; treat "
                 "differences below about 10 % as noise.".format(**result))
    return "\n".join(lines) + "\n"


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--base", type=Path, required=True, help="simucell3d binary of the merge base (A)")
    parser.add_argument("--head", type=Path, required=True, help="simucell3d binary of the pull request (B)")
    parser.add_argument("--base-ref", default="base")
    parser.add_argument("--head-ref", default="head")
    parser.add_argument("--parameter-file", type=Path, required=True)
    parser.add_argument("--simulation-duration", help="override <simulation_duration> [s] (fixes the work)")
    parser.add_argument("--sampling-period", help="override <sampling_period> [s] (limits mesh output)")
    parser.add_argument("--source-dir", type=Path, default=Path.cwd(),
                        help="working directory of each run (relative mesh paths resolve against it)")
    parser.add_argument("--reps", type=int, default=5)
    parser.add_argument("--warmup", type=int, default=1,
                        help="unmeasured runs of each build first (file cache, CPU frequency)")
    parser.add_argument("--threads", type=int, default=os.cpu_count() or 1)
    parser.add_argument("--timeout", type=float, default=600.0, help="seconds per run")
    parser.add_argument("--threshold", type=float, default=1.10)
    parser.add_argument("--markdown", type=Path)
    parser.add_argument("--json", type=Path)
    args = parser.parse_args(argv)

    tmp = tempfile.mkdtemp(prefix="perf_ab_params_")
    parameter_file = shortened_copy(args.parameter_file, args.simulation_duration, args.sampling_period, tmp)

    builds = {"base": {"bin": args.base, "ref": args.base_ref, "times": [], "failures": 0},
              "head": {"bin": args.head, "ref": args.head_ref, "times": [], "failures": 0}}
    for _ in range(args.warmup):
        for name in ("base", "head"):
            run_once(builds[name]["bin"], parameter_file.resolve(), args.source_dir.resolve(),
                     args.threads, args.timeout)
    for rep in range(args.reps):
        for name in ("base", "head"):
            wall, status = run_once(builds[name]["bin"], parameter_file.resolve(),
                                    args.source_dir.resolve(), args.threads, args.timeout)
            print("rep {} {}: {:.2f} s, exit {}".format(rep + 1, name, wall, status), flush=True)
            if status == 0:
                builds[name]["times"].append(wall)
            else:
                builds[name]["failures"] += 1

    word, ratio = verdict(builds["base"]["times"], builds["head"]["times"], args.threshold)
    result = {
        "reps": args.reps, "threads": args.threads, "param": "{} (duration {} s)".format(args.parameter_file.name, args.simulation_duration or "as in file"),
        "threshold": args.threshold, "verdict": word, "ratio": ratio,
        "cpu": cpu_model(), "nproc": os.cpu_count(),
        "base": {k: v for k, v in builds["base"].items() if k != "bin"},
        "head": {k: v for k, v in builds["head"].items() if k != "bin"},
    }
    shutil.rmtree(tmp, ignore_errors=True)
    text = markdown(result)
    print(text)
    if args.markdown:
        args.markdown.write_text(text)
    if args.json:
        args.json.write_text(json.dumps(result, indent=2) + "\n")
    return 0 if builds["head"]["failures"] == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
