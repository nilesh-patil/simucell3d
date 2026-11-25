(benchmarks-completion)=
# Which cases complete

Each build ran the same 20 cases with the paper's parameters ({ref}`benchmarks-method`). A case
completes when every run of it reached its end, or its planned time cap, without the solver
removing a cell. 1.0 completes 6, 1.0 with two crash fixes 8, 2.0 13, the four intermediate builds
15 or 16, and 2.1.0 17.

## Every case in every build

Legend: ● completes; ◐ runs to its end but removes cells or loses the tissue; ✕ stops (abort with
an error, crash, or hang). A case is judged by its worst run. Where a case ran three times, one
failed run is enough to mark it.

| Case | 1.0 | 1.0 with two crash fixes | 2.0 | Intermediate builds (4) | 2.1.0 |
|---|---|---|---|---|---|
| **Growth benchmark (contact model 1)** | | | | | |
| `1e-7` s (published), 48 h | ✕ stops (error) | ✕ stops (error) | ✕ crashes | ✕ 0 of 4 complete (2 hang, 2 remove cells) | ◐ removes cells |
| `5e-8` s, 3 h | ✕ stops (error) | ✕ stops (error) | ✕ crashes | ◐ 2 of 4 complete (1 removes cells, 1 loses the tissue) | ● completes to its cap |
| `2.5e-8` s, 48 h | ✕ stops (error) | ✕ stops (error) | ✕ crashes | ● 4 of 4 complete | ● completes to its cap |
| `1.25e-8` s, 3 h | ✕ stops (error) | ● completes to its cap | ✕ crashes | ● 4 of 4 complete | ● completes to its cap |
| **Vesicle ε sweep, 100,000 iterations** | | | | | |
| ε = 1.0 | ✕ stops (error) | ✕ stops (error) | ● completes | ● 4 of 4 complete | ● completes |
| ε = 1.5 | ✕ stops (error) | ● completes | ● completes | ● 4 of 4 complete | ● completes |
| ε = 2.0 | ● completes | ● completes | ● completes | ● 4 of 4 complete | ● completes |
| ε = 3.0 | ✕ stops (error) | ✕ stops (error) | ● completes | ● 4 of 4 complete | ● completes |
| ε = 4.0 | ✕ stops (error) | ✕ stops (error) | ● completes | ● 4 of 4 complete | ● completes |
| **Mono- and multilayer vesicle** | | | | | |
| Monolayer, paper protocol | ● completes | ● completes | ● completes | ● 4 of 4 complete | ● completes |
| Multilayer, paper protocol | ● completes | ● completes | ● completes | ● 4 of 4 complete | ● completes |
| Multilayer, shipped file, 48 h | ● completes to its cap | ● completes to its cap | ● completes to its cap | ● 4 of 4 complete | ● completes to its cap |
| Monolayer, shipped file | ✕ stops (error) | ✕ stops (error) | ✕ crashes | ◐ 0 of 4 complete (4 lose the tissue) | ◐ loses the tissue |
| **Tube** | | | | | |
| Tube | ● completes | ● completes | ● completes | ● 4 of 4 complete | ● completes |
| **Analytic one- and two-cell cases** | | | | | |
| Single cell | ✕ stops (error) | ✕ stops (error) | ● completes | ● 4 of 4 complete | ● completes |
| Laplace pressure | ✕ stops (error) | ✕ stops (error) | ● completes | ● 4 of 4 complete | ● completes |
| Two cells, apart | ✕ stops (error) | ✕ stops (error) | ● completes | ● 4 of 4 complete | ● completes |
| Two cells, apart, finer mesh | ● completes | ● completes | ● completes | ● 4 of 4 complete | ● completes |
| Two cells, in contact | ✕ stops (error) | ✕ stops (error) | ✕ crashes | ◐ 1 of 4 complete (3 lose the tissue) | ● completes |
| Two cells, in contact, finer mesh | ✕ stops (error) | ✕ stops (error) | ✕ stops (error) | ◐ 0 of 4 complete (4 lose the tissue) | ◐ loses the tissue |

## The three cases 2.1.0 misses

Growth benchmark at the published time step, `1e-7` s
: 2.1.0 grows the aggregate to 150,000 cells but removes cells as unstable on the way: 93,310
  removals in the full run, the first at about 4,800 cells. No build completes this case.
  {ref}`benchmarks-time-step` gives the details and the step that works.

Monolayer vesicle, shipped file (`fig3e_monolayer.xml`, `1e-2` s)
: No build survives it. 1.0 stops with the refinement error, 2.0 crashes, and every later build
  removes most of its cells: 2.1.0 ends with 18 of 434 cells and exit status 0
  ({ref}`benchmarks-validity`). The same file run by the paper protocol (to `1.736e-4` s) completes
  in every build.

Two cells in contact, smaller minimum edge length
: Every build that can start this file loses a cell. In 2.1.0 the cell is removed at iteration
  204 for excessive extent, with no non-finite node. Whether this is the same defect
  as the node blow-ups at `1e-7` s is not established. The same file with the shipped edge length
  completes in 2.1.0.

## How 1.0 fails

All 14 cases that 1.0 does not complete end the same way, with its mesh-refinement error:

```text
The refinement of the mesh of cell N failed
```

On the growth benchmark this happens at every time step, at 126 to 165 cells, after about `1e-4` s
of simulated time. On the four shipped analytic files it happens at iteration 0, so 1.0 cannot
start them. With the smaller minimum edge length (`1.5e-7` m), 1.0 completes the two-cells-apart
case and stops at iteration 38 on the two-cells-in-contact case. The two crash fixes move the growth-benchmark failure later (to 22,719 to
42,335 cells at `2.5e-8` s) but do not remove it, and the size at which a run stops is not
reproducible.

2.0 no longer stops with the refinement error on the vesicle and analytic files, but it crashes on
the growth benchmark: every one of its growth runs
fails by 216 cells, a few iterations after its first cell removal. 2.0 lacks the resync of
local cell ids after a removal, which is one of the two crash fixes and is part of 2.1.0.

## Forward runs of the vesicle

The vesicle ε sweep is a fixed-length case (100,000 iterations) of the kind a parameter study
calls many times. Usable runs over runs attempted at each ε:

| Build | ε 1.0 | ε 1.5 | ε 2.0 | ε 3.0 | ε 4.0 | Hours per usable run | VM-hours spent per usable run |
|---|---|---|---|---|---|---|---|
| 1.0 | 0/1 | 0/1 | 1/1 | 0/1 | 0/1 | 16.86 | 26.7 |
| 1.0 with two crash fixes | 0/3 | 3/3 | 3/3 | 2/3 | 0/3 | 16.55 ± 0.62 | 19.4 |
| 2.0 | 1/1 | 1/1 | 1/1 | 1/1 | 1/1 | 9.49 ± 0.79 | 9.5 |
| 2.1.0 | 3/3 | 3/3 | 3/3 | 3/3 | 3/3 | 6.20 ± 0.95 | 6.2 |

The last column divides all machine hours a build spent on the sweep, failed runs included, by
its number of usable runs.

The failures of 1.0 depend on the parameter, not on chance: with its crash fixes, 1.0 never
completes ε = 1.0 or ε = 4.0 in three attempts each, so those values return no result at any
budget. Across every paper-parameter case that runs to a fixed end, 33% of the runs of 1.0 are
usable, 48% with its crash fixes, 80% in 2.0 and 92% in 2.1.0.

Usable is a low bar: it means the run finished cleanly, not that the tissue it produced is
biologically right. {ref}`benchmarks-validity` shows cases where it is not.
