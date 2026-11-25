#!/usr/bin/env python3
"""Draw the benchmark figures of doc/benchmarks/ as static SVG, one light and one dark variant each.

Reads only the small tables in doc/benchmarks/data/ and writes doc/benchmarks/figures/<name>-{light,dark}.svg.
Standard library only. Run from any directory:

    python3 doc/benchmarks/make_figures.py

The pages embed each pair with the PyData theme classes ``only-light`` and ``only-dark``.
Colours (docs2 design defaults): teal marks 2.1.0, purple 2.0, amber 1.0 with two crash fixes, dark grey
1.0 as published, light grey the intermediate builds.
Hue is never the only cue: every series is also labelled directly and differs in marker or line style.
"""
import csv
import math
from pathlib import Path

HERE = Path(__file__).resolve().parent
DATA = HERE / "data"
FIG = HERE / "figures"

# Build codes in data/: B0 = 1.0, B1 = 1.0 with two crash fixes, B2 = 2.0, B3-B6 = intermediate builds
# (9acfe36, ba74eed, f08df16, a331169), B7 = 2.1.0. Case codes and columns: data/README.txt.
THEMES = {
    # Palette fixed by docs2/DESIGN_DEFAULTS.md. Figures have transparent backgrounds; the paper
    # values are the PyData page backgrounds the colours were checked against.
    "light": {
        "paper": "#ffffff",
        "ink": "#222832",     # theme text colour; text, axes, ticks
        "muted": "#222832",   # same as ink (DESIGN_DEFAULTS: all text in the theme text colour)
        "v21": "#0b6b7c",     # 2.1.0
        "v20": "#6f4bb8",     # 2.0
        "v1": "#9a6700",      # 1.0 with two crash fixes
        "v0": "#6b7280",      # 1.0 as published
        "mid": "#8b929c",     # intermediate builds: thin marks only, 3.1:1; words about them use ink
        "emph": "#a8432b",    # emphasis / failure marker
        "rule": "#222832",    # axis lines
        "band": "#e9ecef",    # range fill, decorative
    },
    "dark": {
        "paper": "#14181e",
        "ink": "#ced6dd",
        "muted": "#ced6dd",
        "v21": "#62c7d8",
        "v20": "#b79cf0",
        "v1": "#e3b341",
        "v0": "#9aa3ad",
        "mid": "#68707c",     # marks only (3:1 floor)
        "emph": "#f09a7a",
        "rule": "#ced6dd",
        "band": "#232a33",    # range fill, decorative
    },
}
TEXT_KEYS = ("ink", "muted", "v21", "v20", "v1", "v0", "emph")   # used for text: need 4.5:1
MARK_KEYS = ("mid",)                                             # marks only: need 3:1
W = 720
# Cumulative wall-time exponent of 2.1.0, fitted on the full per-iteration logs (see performance.md, Scaling).
VERIFIED_T_EXPONENT = "1.333 ± 0.010"
FONT = "system-ui, -apple-system, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif"


# ------------------------------------------------------------------ contrast check (WCAG 2.x)
def _lum(hexc):
    v = [int(hexc[i:i + 2], 16) / 255 for i in (1, 3, 5)]
    v = [c / 12.92 if c <= 0.03928 else ((c + 0.055) / 1.055) ** 2.4 for c in v]
    return 0.2126 * v[0] + 0.7152 * v[1] + 0.0722 * v[2]


def contrast(a, b):
    la, lb = sorted((_lum(a), _lum(b)), reverse=True)
    return (la + 0.05) / (lb + 0.05)


for _name, _t in THEMES.items():
    for _k in TEXT_KEYS:
        assert contrast(_t[_k], _t["paper"]) >= 4.5, (_name, _k, contrast(_t[_k], _t["paper"]))
    for _k in MARK_KEYS:
        assert contrast(_t[_k], _t["paper"]) >= 3.0, (_name, _k, contrast(_t[_k], _t["paper"]))


# ------------------------------------------------------------------ data
def read(name):
    with open(DATA / name) as f:
        return list(csv.DictReader(line for line in f if not line.startswith("#")))


# ------------------------------------------------------------------ svg helpers
def esc(s):
    return str(s).replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


class Svg:
    def __init__(self, fid, h, title, desc, t):
        self.t, self.o = t, []
        self.o.append(
            f'<svg xmlns="http://www.w3.org/2000/svg" role="img" aria-labelledby="{fid}-t {fid}-d" '
            f'viewBox="0 0 {W} {h}" width="{W}" height="{h}" font-family="{FONT}">'
            f'<!-- drawn for a transparent ground over the page background: {t["paper"]} -->'
            f'<title id="{fid}-t">{esc(title)}</title><desc id="{fid}-d">{esc(desc)}</desc>')

    def text(self, x, y, s, col="ink", size=12, anchor="start", weight="normal"):
        col = "ink" if col == "mid" else col   # the light grey of the intermediate builds is for marks only
        self.o.append(f'<text x="{x:.1f}" y="{y:.1f}" fill="{self.t[col]}" font-size="{size}" '
                      f'text-anchor="{anchor}" font-weight="{weight}">{esc(s)}</text>')

    def sup(self, x, y, base, exp, tail, size=11, anchor="start", col="ink"):
        """text with a superscript: base, raised exponent, tail (no keyboard notation in charts)"""
        self.o.append(f'<text x="{x:.1f}" y="{y:.1f}" fill="{self.t[col]}" font-size="{size}" '
                      f'text-anchor="{anchor}">{esc(base)}<tspan dy="-{size * 0.4:.1f}" '
                      f'font-size="{size * 0.75:.1f}">{esc(exp)}</tspan><tspan dy="{size * 0.4:.1f}">'
                      f'{esc(tail)}</tspan></text>')

    def line(self, x1, y1, x2, y2, col="rule", w=1.0, dash=None):
        d = f' stroke-dasharray="{dash}"' if dash else ""
        d += ' aria-hidden="true"' if col == "rule" else ""
        self.o.append(f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" '
                      f'stroke="{self.t[col]}" stroke-width="{w}"{d}/>')

    def poly(self, pts, col, w=1.6, dash=None, opacity=1.0):
        d = f' stroke-dasharray="{dash}"' if dash else ""
        p = " ".join(f"{x:.1f},{y:.1f}" for x, y in pts)
        self.o.append(f'<polyline points="{p}" fill="none" stroke="{self.t[col]}" stroke-width="{w}" '
                      f'stroke-linejoin="round" stroke-opacity="{opacity}"{d}/>')

    def rect(self, x, y, w, h, col):
        d = ' aria-hidden="true"' if col == "band" else ""
        self.o.append(f'<rect x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}" fill="{self.t[col]}"{d}/>')

    def dot(self, x, y, col, r=3.0, hollow=False):
        if hollow:
            self.o.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r}" fill="none" stroke="{self.t[col]}" stroke-width="1.4"/>')
        else:
            self.o.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r}" fill="{self.t[col]}"/>')

    def square(self, x, y, col, r=3.0):
        self.o.append(f'<rect x="{x - r:.1f}" y="{y - r:.1f}" width="{2 * r}" height="{2 * r}" fill="{self.t[col]}"/>')

    def cross(self, x, y, col, r=4.0, w=1.8):
        self.line(x - r, y - r, x + r, y + r, col, w)
        self.line(x - r, y + r, x + r, y - r, col, w)

    def save(self, name, theme):
        FIG.mkdir(exist_ok=True)
        (FIG / f"{name}-{theme}.svg").write_text("\n".join(self.o) + "\n</svg>\n")


def lin(d0, d1, r0, r1):
    return lambda v: r0 + (v - d0) / (d1 - d0) * (r1 - r0)


def logs(d0, d1, r0, r1):
    a, b = math.log10(d0), math.log10(d1)
    return lambda v: r0 + (math.log10(v) - a) / (b - a) * (r1 - r0)


def kfmt(n):
    if n >= 1000:
        v = n / 1000
        return f"{v:.0f}k" if abs(v - round(v)) < 1e-9 else f"{v:.1f}k"
    return f"{n:g}"


def yaxis(s, x, y0, y1, ticks, sy, fmt, label=None):
    s.line(x, sy(ticks[0]), x, sy(ticks[-1]), "rule")
    for t in ticks:
        s.line(x - 4, sy(t), x, sy(t), "rule")
        s.text(x - 7, sy(t) + 4, fmt(t), "muted", 11, "end")
    if label:
        s.text(x - 7, y1 - 12, label, "muted", 11, "start")


def xaxis(s, y, ticks, sx, fmt, label=None, lx=None):
    s.line(sx(ticks[0]), y, sx(ticks[-1]), y, "rule")
    for t in ticks:
        s.line(sx(t), y, sx(t), y + 4, "rule")
        s.text(sx(t), y + 17, fmt(t), "muted", 11, "middle")
    if label:
        s.text(lx if lx is not None else sx(ticks[0]), y + 34, label, "muted", 11)


# ------------------------------------------------------------------ 1. completion
CLEAN = {"clean", "clean-at-cap"}


def completion_counts():
    rows = read("completion.csv")
    assert len(rows) == 20
    n = {b: sum(r[b] in CLEAN for r in rows) for b in ("B0", "B1", "B2", "B3", "B4", "B5", "B6", "B7")}
    assert (n["B0"], n["B1"], n["B2"], n["B7"]) == (6, 8, 13, 17), n
    return n


def fig_completion(theme):
    n = completion_counts()
    mids = [n[b] for b in ("B3", "B4", "B5", "B6")]
    bars = [("1.0 as published", n["B0"], n["B0"], "v0"), ("1.0 with two crash fixes", n["B1"], n["B1"], "v1"),
            ("2.0", n["B2"], n["B2"], "v20"), ("four intermediate builds", min(mids), max(mids), "mid"),
            ("2.1.0", n["B7"], n["B7"], "v21")]
    h = 40 + 34 * len(bars) + 34
    s = Svg(f"fcomp-{theme}", h, "Benchmark cases completed, of 20, by version",
            f"1.0 as published completes {n['B0']} of the 20 common cases, 1.0 with two crash fixes {n['B1']}, 2.0 {n['B2']}, "
            f"the four intermediate builds {min(mids)} to {max(mids)}, and 2.1.0 {n['B7']}.", THEMES[theme])
    x0, x1 = 210, 640
    sx = lin(0, 20, x0, x1)
    y = 30
    for label, lo, hi, col in bars:
        s.text(x0 - 12, y + 4, label, "ink", 12, "end")
        s.rect(x0, y - 8, sx(lo) - x0, 16, col)
        if hi != lo:
            s.rect(sx(lo), y - 8, sx(hi) - sx(lo), 16, "band")
            s.line(sx(hi), y - 8, sx(hi), y + 8, "ink", 1.5)
            s.text(sx(hi) + 8, y + 4, f"{lo} to {hi}", "ink", 12)
        else:
            s.text(sx(lo) + 8, y + 4, f"{lo}", "ink", 12, weight="bold" if col == "v21" else "normal")
        y += 34
    ya = y - 10
    xaxis(s, ya, [0, 5, 10, 15, 20], sx, lambda t: f"{t}", "cases that complete with no cell removed, of 20", x0)
    s.save("completion", theme)


# ------------------------------------------------------------------ 2. growth against wall time
def runs_by(rows, key="run"):
    out = {}
    for r in rows:
        out.setdefault(r[key], []).append(r)
    return out


def time_to(pts, n):
    """wall hour at which the running cell count reaches n (pts sorted by wall), interpolated in log N
    between the two plotted points that bracket n. growth_wall.csv is thinned for plotting, so this is an
    approximation; the milestone tables on the pages come from the full per-iteration logs."""
    prev = None
    for w, c in pts:
        if c >= n:
            if prev is None or prev[1] == c:
                return w
            pw, pc = prev
            f = (math.log(n) - math.log(pc)) / (math.log(c) - math.log(pc))
            return pw + f * (w - pw)
        prev = (w, c)
    return None


def fit_T(pts, n_lo=10000, k=30):
    """OLS slope of log cumulative wall time against log N at k log-spaced N from n_lo to the run maximum."""
    nmax = max(c for _, c in pts)
    ns = [n_lo * (nmax / n_lo) ** (i / (k - 1)) for i in range(k)]
    xs, ys = [], []
    for n in ns:
        t = time_to(pts, n)
        if t:
            xs.append(math.log10(n)); ys.append(math.log10(t))
    mx, my = sum(xs) / len(xs), sum(ys) / len(ys)
    b = sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / sum((x - mx) ** 2 for x in xs)
    return b, my - b * mx


def growth_series():
    g = runs_by(read("growth_wall.csv"))
    ser = {}
    for run, rows in g.items():
        pts = sorted((float(r["wall_h"]), int(r["cells"])) for r in rows)
        run_max, mono = 0, []
        for w, c in pts:
            run_max = max(run_max, c)
            mono.append((w, run_max))
        ser[run] = (rows[0]["build"], mono)
    return ser


def fig_growth(theme):
    ser = growth_series()
    fits = {r: fit_T(p) for r, (b, p) in ser.items() if b == "B7"}
    h = 400
    left, right, top, bot = 70, 560, 24, 340
    sx = logs(0.01, 60, left, right)
    sy = logs(100, 2e5, bot, top)
    s = Svg(f"fgrow-{theme}", h, "Cells against wall-clock time, growth benchmark, time step 2.5e-8 s",
            "On log axes from 36 seconds (0.01 hours) to 48 hours, three runs of 2.1.0 grow from about 220 to "
            "240 cells to 142 to 149 thousand cells; "
            "three runs of 1.0 with two crash fixes follow a curve about twice as slow and stop with an error at "
            "23, 25 and 42 thousand cells. Above 10,000 cells the 2.1.0 curves follow a power law with exponent "
            f"{VERIFIED_T_EXPONENT}; the dashed line is the fit published with the paper, measured at a four times larger time step.",
            THEMES[theme])
    yaxis(s, left, bot, top, [100, 1000, 10000, 100000], sy, kfmt, "cells (log scale)")
    xaxis(s, bot, [0.01, 0.1, 1, 10, 48], sx, lambda t: f"{t:g}",
          "wall-clock hours from the first iteration, 8 threads (log scale)", left)
    # paper fit T = 0.013 s * N^1.33 (Fig. 1e of the paper; dt 1e-7 s, other hardware)
    pf = [(0.013 * n ** 1.33 / 3600, n) for n in [10 ** (e / 20) for e in range(40, 107)]]
    s.poly([(sx(w), sy(n)) for w, n in pf if 0.01 <= w <= 48], "mid", 1.2, "5 4")
    for run, (b, pts) in ser.items():
        col = "v1" if b == "B1" else "v21"
        pp = [(sx(w), sy(c)) for w, c in pts if w >= 0.01 and c >= 100]
        s.poly(pp, col, 1.6)
        ex, ey = pp[-1]
        if b == "B1":
            s.cross(ex, ey, col)
        else:
            s.dot(ex, ey, col, 3.2)
    # direct labels
    s.text(right + 8, sy(146000) + 4, "2.1.0, 3 runs:", "v21", 12, weight="bold")
    s.text(right + 8, sy(146000) + 19, "142k to 149k cells", "v21", 12)
    s.text(right + 8, sy(146000) + 34, "at the 48 h cap", "v21", 12)
    s.text(sx(17) + 8, sy(42335) + 30, "1.0 with two crash fixes,", "v1", 12, weight="bold")
    s.text(sx(17) + 8, sy(42335) + 45, "3 runs; each stops (x)", "v1", 12)
    s.text(sx(17) + 8, sy(42335) + 60, "at 23k, 25k or 42k cells", "v1", 12)
    s.text(sx(0.9), sy(30000) - 6, "fit published with the paper,", "muted", 11, "end")
    s.sup(sx(0.9), sy(30000) + 8, "T = 0.013 s · N", "1.33", ", time step 1e-7 s", 11, "end")
    s.save("growth_wall", theme)
    return fits


# ------------------------------------------------------------------ 3. memory
def fig_memory(theme):
    rows = read("memory.csv")
    h = 380
    left, right, top, bot = 70, 600, 24, 320
    sx = lin(0, 160000, left, right)
    sy = lin(0, 64, bot, top)
    b7 = [r for r in rows if r["build"] == "B7" and int(r["max_cells"]) > 100000]
    k = sum(float(r["maxrss_gib"]) for r in b7) / sum(int(r["max_cells"]) for r in b7) * 1000
    b1 = [r for r in rows if r["build"] == "B1" and r["run"].endswith(("full", "r2", "r3"))]
    k1 = sum(float(r["maxrss_gib"]) / int(r["max_cells"]) * 1000 for r in b1) / len(b1)
    s = Svg(f"fmem-{theme}", h, "Peak memory against peak cell count, growth benchmark",
            f"Peak resident memory grows linearly with cell count: about {k:.3f} GiB per 1,000 cells in 2.1.0, "
            f"18.6 to 19.2 GiB at 143 to 149 thousand cells, and about {k1:.2f} GiB per 1,000 cells in 1.0 with "
            "two crash fixes. Three intermediate builds leak memory and reach the machine's 61.6 GiB limit "
            "before 140 thousand cells.", THEMES[theme])
    yaxis(s, left, bot, top, [0, 16, 32, 48, 64], sy, lambda t: f"{t:g}", "peak memory, GiB")
    xaxis(s, bot, [0, 40000, 80000, 120000, 160000], sx, lambda t: kfmt(t) if t else "0",
          "peak cells in the run", left)
    s.line(sx(0), sy(61.6), sx(160000), sy(61.6), "mid", 1.0, "5 4")
    s.text(sx(2000), sy(61.6) - 6, "61.6 GiB: all the memory of the machine", "muted", 11)
    s.poly([(sx(0), sy(0)), (sx(155000), sy(155 * k))], "v21", 1.0, "2 3")
    for r in rows:
        n, m, b = int(r["max_cells"]), float(r["maxrss_gib"]), r["build"]
        if n < 1000:
            continue
        if b == "B7":
            s.dot(sx(n), sy(m), "v21", 3.6)
        elif b == "B1":
            s.square(sx(n), sy(m), "v1", 3.4)
        elif b in ("B3", "B4", "B5"):
            s.dot(sx(n), sy(m), "mid", 3.4, hollow=True)
        else:
            s.dot(sx(n), sy(m), "mid", 2.6)
    s.text(sx(150000), sy(19.0) - 30, "2.1.0:", "v21", 12, "end", "bold")
    s.text(sx(150000), sy(19.0) - 15, f"{k:.3f} GiB per 1,000 cells", "v21", 12, "end")
    s.text(sx(30000), sy(14), f"1.0 with two crash fixes (squares):", "v1", 12)
    s.text(sx(30000), sy(14) + 15, f"{k1:.2f} GiB per 1,000 cells", "v1", 12)
    s.text(sx(60500) + 10, sy(32) + 4, "three intermediate builds that leak", "mid", 12)
    s.text(sx(60500) + 10, sy(32) + 19, "(hollow): 30 to 35 GiB at 60k cells", "mid", 12)
    s.text(sx(64000), sy(3.2), "fourth intermediate build (small solid grey): on the 2.1.0 line", "mid", 12)
    s.save("memory", theme)
    return k, k1


# ------------------------------------------------------------------ 4. removals against time step
def fig_removals(theme):
    rows = read("growth_runs.csv")
    sweep = {(r["build"], float(r["dt"])): r for r in rows
             if r["run"] in {f"at1-dt{c}-b{b}" for c in ("12", "25", "50", "100") for b in "34567"}}
    dts = [1.25e-8, 2.5e-8, 5e-8, 1e-7]
    h = 370
    left, right, top, bot = 84, 520, 24, 300
    sx = lambda i: left + 40 + i * (right - left - 80) / 3
    zero_y = bot - 8
    sy = logs(100, 1e5, zero_y - 34, top)
    b7 = [int(sweep[("B7", d)]["removals"]) for d in dts]
    s = Svg(f"frem-{theme}", h, "Cells removed as unstable in a 3-hour growth run, by time step",
            "At time steps of 1.25e-8 and 2.5e-8 s neither 2.1.0 nor any intermediate build removes a cell in 3 hours. "
            "At 5e-8 s, 2.1.0 and two "
            "intermediate builds remove none and two intermediate builds remove about 10,000. At the published 1e-7 s every "
            f"build removes cells: 2.1.0 {b7[3]:,}, the intermediate builds 20,639 to 38,588.", THEMES[theme])
    for t in (100, 1000, 10000, 100000):
        s.line(left - 4, sy(t), left, sy(t), "rule")
        s.text(left - 7, sy(t) + 4, f"{t:,}", "muted", 11, "end")
    s.line(left, sy(1e5), left, sy(100), "rule")
    s.text(left - 7, zero_y + 4, "0", "muted", 11, "end")
    s.text(left - 7, top - 10, "cells removed in 3 h (log scale)", "muted", 11)
    s.line(left, bot, right, bot, "rule")
    for i, d in enumerate(dts):
        s.line(sx(i), bot, sx(i), bot + 4, "rule")
        s.text(sx(i), bot + 17, f"{d:g}", "muted", 11, "middle")
    s.text(left, bot + 34, "time step, s (1e-7 is the value published for this benchmark)", "muted", 11)

    def ypos(v):
        return zero_y if v == 0 else sy(v)

    for b in "3456":
        vals = [int(sweep[(f"B{b}", d)]["removals"]) for d in dts]
        s.poly([(sx(i), ypos(v)) for i, v in enumerate(vals)], "mid", 1.0, opacity=0.8)
        for i, v in enumerate(vals):
            s.dot(sx(i), ypos(v), "mid", 2.8, hollow=True)
    s.poly([(sx(i), ypos(v)) for i, v in enumerate(b7)], "v21", 2.0)
    for i, v in enumerate(b7):
        s.dot(sx(i), ypos(v), "v21", 3.8)
    s.text(sx(3) + 12, sy(b7[3]) + 4, f"2.1.0: {b7[3]:,}", "v21", 12, weight="bold")
    s.text(sx(3) + 12, sy(30000) - 6, "four intermediate", "mid", 12)
    s.text(sx(3) + 12, sy(30000) + 9, "builds: 20,639 to 38,588", "mid", 12)
    s.text(sx(2) - 12, sy(10500) - 12, "two intermediate builds,", "mid", 12, "end")
    s.text(sx(2) - 12, sy(10500) + 3, "about 10,000", "mid", 12, "end")
    s.text(sx(0), zero_y - 10, "no cell removed", "muted", 11)
    s.save("removals_dt", theme)


# ------------------------------------------------------------------ 5. superposition and core packing
def fig_packing(theme):
    rows = runs_by(read("packing.csv"))
    h = 380
    pw, gap, left, top, bot = 268, 92, 58, 40, 306
    s = Svg(f"fpack-{theme}", h, "Superposed cells and core packing against tissue size, growth benchmark",
            "Left: the share of cells that sit on top of another cell is zero or near zero up to about 4,000 cells and "
            "rises to 39 percent at 136,043 cells in 2.1.0; 1.0 with two crash fixes rises the same way while its runs last. "
            "Right: the summed cell volume in the core divided by the core volume passes 1, where cells must overlap, "
            "between about 2,000 and 13,000 cells depending on the run, and reaches 1.9.", THEMES[theme])
    panels = [("frac_superposed", lin(0, 0.42, bot, top), [0, 0.1, 0.2, 0.3, 0.4], lambda v: f"{v * 100:.0f}%",
               "Cells superposed on another cell", None),
              ("phi_core", lin(0.8, 2.0, bot, top), [0.8, 1.0, 1.2, 1.4, 1.6, 1.8, 2.0], lambda v: f"{v:.1f}",
               "Core packing ratio", 1.0)]
    for pi, (key, sy, yt, yf, title, ref) in enumerate(panels):
        px = left + pi * (pw + gap)
        sx = logs(300, 200000, px, px + pw)
        s.text(px - 40, 18, title, "ink", 13, weight="bold")
        yaxis(s, px, bot, top, yt, sy, yf)
        xaxis(s, bot, [1000, 10000, 100000], sx, kfmt, "cells in the snapshot (log scale)", px)
        if ref:
            s.line(px, sy(ref), px + pw, sy(ref), "mid", 1.0, "5 4")
            s.text(px + 8, sy(1.95), "dashed line at 1: cells fill the core;", "muted", 11)
            s.text(px + 8, sy(1.95) + 14, "above it they must overlap", "muted", 11)
        for run, rr in sorted(rows.items(), key=lambda kv: {"B6": 0, "B1": 1, "B7": 2}[kv[1][0]["build"]]):
            b = rr[0]["build"]
            col = {"B1": "v1", "B7": "v21", "B6": "mid"}[b]
            pts = [(sx(float(r["n_cells"])), sy(float(r[key]))) for r in rr]
            s.poly(pts, col, 1.1, opacity=0.9)
            for x, y in pts:
                if b == "B1":
                    s.square(x, y, col, 2.6)
                else:
                    s.dot(x, y, col, 2.6, hollow=(b == "B6"))
    sx0 = logs(300, 200000, left, left + pw)
    sy0 = panels[0][1]
    s.text(sx0(110000), sy0(0.39) - 10, "2.1.0, 3 runs", "v21", 12, "end", "bold")
    s.text(sx0(400), sy0(0.36), "1.0 with two crash fixes,", "v1", 12)
    s.text(sx0(400), sy0(0.36) + 15, "3 runs (squares)", "v1", 12)
    s.text(sx0(400), sy0(0.36) + 37, "one intermediate", "mid", 12)
    s.text(sx0(400), sy0(0.36) + 52, "build (hollow)", "mid", 12)
    s.save("packing", theme)


# ------------------------------------------------------------------ 6. vesicle readouts
LABEL = {"sphericity": "sphericity", "contact_area_frac": "contact-area fraction",
         "V_over_Vtarget": "volume / target volume", "P_mean": "mean cell pressure",
         "frac_touching_neither": "share of cells off both surfaces", "AR_ab": "apico-basal aspect ratio",
         "AR_obb": "bounding-box aspect ratio", "layer_N": "layer number", "neighbours": "contact neighbours"}


def fig_sensitivity(theme):
    rows = [r for r in read("vesicle_sensitivity.csv") if r["readout"] in LABEL]
    rows.sort(key=lambda r: -float(r["min_step_over_sd"]))
    h = 40 + 28 * len(rows) + 60
    x0, x1 = 250, 590
    sx = logs(0.1, 100, x0, x1)
    s = Svg(f"fsens-{theme}", h, "Vesicle readouts: parameter response against replicate noise, 2.1.0",
            "For each readout of the vesicle sweep in 2.1.0, the smallest change between two neighbouring epsilon values, "
            "divided by the replicate standard deviation. Sphericity (26) and contact-area fraction (13) are far above 2; "
            "mean pressure and volume over target volume are at about 2; the aspect ratios, the share of cells off both "
            "surfaces, layer number and neighbour count are below 2.", THEMES[theme])
    y = 34
    ya = y + 28 * len(rows) - 10
    s.line(sx(2), 22, sx(2), ya, "mid", 1.0, "5 4")
    s.text(sx(2) + 6, 18, "2 standard deviations", "muted", 11)
    s.text(x1 + 70, 18, "step", "muted", 11, "end")
    s.text(x1 + 124, 18, "range", "muted", 11, "end")
    for r in rows:
        ms, rg = float(r["min_step_over_sd"]), float(r["range_over_sd"])
        col = "v21" if ms >= 2 else "mid"
        s.text(x0 - 12, y + 4, LABEL[r["readout"]], "ink", 12, "end")
        s.line(x0, y, sx(ms), y, "rule", 1.0)
        s.dot(sx(ms), y, col, 4.5, hollow=(ms < 2))
        s.text(x1 + 70, y + 4, f"{ms:.2g}" if ms < 10 else f"{ms:.0f}", col, 12, "end")
        s.text(x1 + 124, y + 4, f"{rg:.0f}", "muted", 12, "end")
        y += 28
    xaxis(s, ya, [0.1, 1, 10, 100], sx, lambda t: f"{t:g}", None)
    s.text(x0, ya + 34, "smallest step between neighbouring epsilon values,", "muted", 11)
    s.text(x0, ya + 48, "in replicate standard deviations (log scale)", "muted", 11)
    s.save("vesicle_sensitivity", theme)


if __name__ == "__main__":
    for th in THEMES:
        fig_completion(th)
        fits = fig_growth(th)
        k = fig_memory(th)
        fig_removals(th)
        fig_packing(th)
        fig_sensitivity(th)
    # printed so that the numbers on the pages can be checked against the data files
    n = completion_counts()
    print("completed of 20:", n)
    ser = growth_series()
    for b in ("B1", "B7"):
        t10 = [time_to(p, 10000) for r, (bb, p) in ser.items() if bb == b]
        m = sum(t10) / 3
        sd = (sum((t - m) ** 2 for t in t10) / 2) ** 0.5
        print(f"{b} hours to 10k: {['%.3f' % t for t in t10]} mean {m:.3f} sd {sd:.3f}")
    e = [f[0] for f in fits.values()]
    m = sum(e) / 3
    print("2.1.0 T(N) exponent 10k-max per run:", ["%.3f" % x for x in e], f"mean {m:.4f} sd {(sum((x - m) ** 2 for x in e) / 2) ** 0.5:.4f}")
    e30 = [fit_T(p, 30000)[0] for r, (bb, p) in ser.items() if bb == "B7"]
    print("2.1.0 T(N) exponent 30k-max per run:", ["%.3f" % x for x in e30], f"mean {sum(e30) / 3:.3f}")
    print("memory GiB per 1k cells, 2.1.0 / 1.0+fixes: %.4f / %.4f" % k)
    for name, t in THEMES.items():
        print(name, {kk: round(contrast(t[kk], t["paper"]), 2) for kk in ("ink", "muted", "rule", "v21", "v1", "mid")})
