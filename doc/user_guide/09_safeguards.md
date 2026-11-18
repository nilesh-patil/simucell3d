(ug-safeguards)=
# Safeguards

A long simulation of a growing tissue performs billions of mesh operations and force
evaluations, and occasionally one cell's mesh becomes damaged or its nodes diverge. In 1.0 such
an event ended the run, sometimes with a crash, sometimes with a hang. SimuCell3D 2.1 adds
{term}`safeguards <safeguard>`: mechanisms that detect these events, remove or repair the affected
cell, log what they did, and let the run continue. They are not part of the model published by
Runser, Vetter & Iber (2024). This chapter lists each safeguard with its trigger, its action, its
log line and how to count how often it acted, followed by the conditions that still stop a run
and the known issues.

:::{important}
A safeguard changes the simulated tissue: a removed cell disappears with its volume, and its
neighbours relax into the gap. Count the safeguard events of every run and report them with the
results. A run with many events is not a faithful solution of the model.

The unstable-cell removal has no limit: a run whose tissue diverges can lose almost all of its
cells and still exit with status 0 (see {ref}`ug-safeguards-known-issues`). The exit status alone
does not show that a run succeeded.
:::

## Overview

| Safeguard | Trigger | Action | Logged | Counted in diagnostics CSV |
|---|---|---|---|---|
| Unstable-cell removal | non-finite node, excessive extent, inside-out or self-intersecting surface | cell removed | yes | `unstable_cells_removed` |
| Refinement-failure removal | error while refining one cell's mesh | cell removed | yes | `unstable_cells_removed` |
| Refiner oscillation bail-out | more than 50 operations at one place | refinement of that cell stopped for the iteration | rate-limited | no |
| Orientation self-check | inconsistently wound faces after refinement | faces re-oriented | rate-limited | no |
| Refused division | division interface not fully triangulated | division deferred | yes | no |
| Diverged domain | tissue bounding box not finite or too large | run stopped with an error | yes | no |
| Bending guards | sliver triangle or near-zero angle in a hinge | hinge skipped or $\cot$ capped | no | no |

All log lines go to standard error. The diagnostics CSV is `performance_diagnostics.csv` in the
output folder, or the file given with `--diagnostics-csv` ({doc}`14_output_files`); it has one row
per 100 iterations, and `unstable_cells_removed` counts the removals in that window. Removals in
the last, incomplete window of a run are not written, and a run shorter than 100 iterations
writes no diagnostics CSV at all; the run total is not printed. The standard error log is
therefore the complete record.

## Unstable-cell removal

**Trigger.** At the end of every iteration, after time integration, every cell is checked. A cell
is unstable if

- a node coordinate is not a finite number; or
- the largest side of its axis-aligned bounding box exceeds $10\,V_0^{1/3}$ (10 times the cube
  root of its target volume; the current volume is used if the target volume is not positive); or
- its signed volume ({doc}`02_cell_mechanics`) is zero or negative, so the surface is inside out;
  or
- its volume exceeds the volume of its bounding box by more than 0.1%, which is only possible if
  the surface intersects itself.

The two volume tests use the signed volume of the last force evaluation and are not applied to
ECM and static cells ({doc}`08_cell_classes`).

**Action.** The cell is removed in the same step as the cells below their minimum volume. There
is no limit on the number of cells removed in one iteration or in a run, unlike the
refinement-failure removal below.

**Log line.**

```text
Removing numerically unstable cell <id> at iteration <n>: extent <e> m (max <m> m), volume <V> m^3, target volume <V0> m^3, <k> non-finite nodes
```

When a volume test fires, the line continues with `, negative volume` and/or
`, volume exceeds bounding box`, followed by
`(signed volume <Vs> m^3, bounding box volume <Vb> m^3)`.

**Why.** Left in the tissue, a diverged cell grows to around $10^{19}$ m within a few dozen
iterations. Its faces then spoil the bounding box of the contact search, and every later contact
detection becomes extremely slow. Removing one cell keeps the run alive.

## Refinement-failure removal

**Trigger.** Refining one cell's mesh raises an error: for example an edge lookup that finds
nothing in a damaged mesh, or the refiner's operation cap of 500,000 operations
({doc}`05_mesh_refinement`).

**Action.** The cell is removed right after mesh refinement, before contact detection, so that its
damaged mesh never reaches the force computation. The other cells are refined normally.

**Log line.**

```text
Removing cell <id> at iteration <n>: mesh refinement failed: <error message>
```

**Limits.** The run is stopped instead, with nothing removed, if in one iteration

- more than $\max(10, \lfloor N/100 \rfloor)$ of the $N$ cells fail; or
- every cell fails.

Many simultaneous failures point to a bug in the refiner rather than to a few damaged cells. The
run then ends with

```text
Mesh refinement failed for <k> of <N> cells at iteration <n>; stopping
```

The run is also stopped, not contained, by an out-of-memory error during refinement and by a
diverged domain.

:::{versionchanged} 2.1
In 1.0 (and 2.0) any error while refining one cell ended the whole run. An 8-core run of the
reconstructed paper-like configuration (`parameters/longrun_fast_growth.xml`, not the lab's
configuration; {doc}`/examples/growth_from_one_cell`) ended this way at 79,635 cells.
:::

## Refiner oscillation bail-out

**Trigger.** Within one refinement call, more than 50 splits or collapses fall in the same cubic
bucket of edge $10^{-7}$ m, which indicates a split-collapse cycle that does not converge.

**Action.** The refiner stops working on that cell for this iteration and keeps the mesh as it is.
Some edges may stay outside $[l_{\min}, 3 l_{\min}]$ until the next iteration.

**Log line** (first 20 occurrences, then every 1000th):

```text
local_mesh_refiner: oscillation bail-out on cell <id> after <n> operations, <k> edges left unchecked (occurrence <m>)
```

:::{versionchanged} 2.1
The bail-out exists since 2.0; the log line is new in 2.1. 1.0 aborted the run with
"The refinement of the mesh of cell X failed."
:::

## Orientation self-check

**Trigger.** After every refinement, the faces of a cell are checked for consistent winding: the
two faces of each edge must traverse it in opposite directions. A fast checksum screens every cell;
only cells with a non-zero checksum are examined edge by edge.

**Action.** All faces of the cell are re-oriented from one seed face, as for a new cell. A surface
that is no longer manifold cannot be repaired and is only logged.

**Log line** (first 20 occurrences, then every 1000th):

```text
local_mesh_refiner: inconsistent face winding on cell <id> (<k> edges), faces re-oriented (occurrence <m>)
```

or `not repaired: the surface is not manifold` instead of `faces re-oriented`. The check costs
about 0.25% of a time step. With the edge-split orientation fix of 2.1
({doc}`05_mesh_refinement`) the check should rarely fire; a regression in the refiner would show
up here.

## Refused divisions

A division whose interface the Delaunay triangulation does not fully cover is refused, and the
mother divides at a later division check ({doc}`06_growth_division_removal`). The log line is

```text
Cell division failed for cell <id>: Interface outline not covered by the Delaunay triangulation (<k> of <n> sides missing); division refused
```

Refusals delay divisions but do not remove cells. In an 8-core run of the reconstructed
paper-like configuration from 1 to 150,498 cells, 14% of division attempts were refused.

A division is not refused when the cutting plane crosses the cell in a loop nested inside the
longest loop, as in a cup-shaped cell. The division then goes ahead with daughters that
interpenetrate, and no safeguard detects it (see {ref}`ug-safeguards-known-issues`).

## Diverged domain

**Trigger.** During contact detection, the bounding box of the tissue is not finite, or the
contact grid would need more voxels or entries than it can index. This happens only when nodes
have diverged faster than the unstable-cell removal could act.

**Action.** The run stops with an error message that starts with "The bounding box of the
simulation", "The contact grid would need more than 1e10 voxels", "Too many face/voxel entries
in the contact grid" or "The mesh is too large", and `simucell3d` exits with status 1.

:::{versionchanged} 2.1
In earlier versions an overflowing grid index made contact detection degenerate into a search that
never returned (a 20-hour stall was observed), and an error raised during contact detection ended
the program with SIGABRT instead of an error message.
:::

## Bending guards

The bending force skips hinges with a sliver triangle and caps $\lvert\cot\alpha\rvert$ at 1000
({doc}`02_cell_mechanics`). Strictly this is a change of the force, listed in the CHANGELOG
under changed behaviour, not a safeguard. It is silent: no log line, no counter. Well-shaped
hinges are not affected.

(ug-safeguards-counting)=
## Counting safeguard events

Capture standard error to a file when running a simulation. From the `build/` directory:

```console
$ ./simucell3d ../parameters/core/parameters_vesicle_quick.xml 2> run.err
$ grep -c "Removing numerically unstable cell" run.err
$ grep -c "mesh refinement failed" run.err
$ grep -c "division refused" run.err
$ grep "oscillation bail-out" run.err | tail -n 1
$ grep "inconsistent face winding" run.err | tail -n 1
```

The first three commands count events. The bail-out and winding messages are rate-limited, so for
them read the `occurrence` number of the last line, which bounds the number of events from below.
The column `unstable_cells_removed` of the diagnostics CSV gives the number of removals per
100 iterations, which shows when in the run they happened, but it misses the last incomplete
window. Compare the number of removals with the number of cells: the program does not stop or
change its exit status however many cells it removes, and
`scripts/parameter_screening/collect_screening_data.py` treats every run with exit status 0 as a
success ({doc}`17_parameter_screening`).

## Conditions that still stop a run

- More refinement failures in one iteration than the limit above, or all cells failing.
- A diverged domain.
- Out of memory.
- An error during initialisation (parameter file, input mesh, initial triangulation) or while
  writing output.
- Lookups inside functions declared `noexcept` on the refiner's path (for example `edge::f1()`,
  `edge::f2()` and the triangle score) cannot be caught; on a damaged mesh they end the program
  with SIGABRT.

(ug-safeguards-known-issues)=
## Known issues

The CHANGELOG of 2.1 lists these known issues; they concern the safeguards or the events they
handle.

- **No limit on unstable-cell removal; a diverged run exits with status 0.** In a review run of
  `parameters_vesicle_quick.xml` with the time step raised to $5\times10^{-7}$ s, all 432
  epithelial cells were removed within 8 iterations, and the run exited with status 0. Count the
  removals of every run as described above.
- **Nested cut loops in divisions.** The divider cuts along the longest intersection loop and does
  not check whether another loop lies inside it. For a cup-shaped or invaginated cell, the
  interface then spans the cavity: in a review test with a dimpled cylinder, 20 of 20 divisions
  went ahead this way, the daughters' total volume exceeded the mother's by 3.4%, and part of one
  daughter lay inside the other. Both daughters pass the unstable-cell checks, and nothing is
  logged. How often this happens in growth runs has not been measured.
- **Rare blow-ups of single nodes.** Occasionally a node is thrown far from its cell in one time
  step, sometimes to a non-finite position. The unstable-cell removal deletes the cell, but
  neighbouring cells can go with it: in a 150,000-cell run, 8 cells with one non-finite node each
  were removed in one iteration, and 4 cells with the same extent of $1.5\times10^{9}$ m in another.
  All three long 2.1 test runs had such groups. The cause is not yet known.
- **Deferred divisions** become more frequent as the tissue grows ({doc}`06_growth_division_removal`).
- **Cells interpenetrate** in long growth runs of the reconstructed paper-like configuration,
  with 2.1 and with 1.0 ({doc}`03_contact`).
- **Unchecked lookups in `noexcept` functions** (see above).
- **Stale coupling links** ({doc}`03_contact`).
- **Contact models 0 and 1 can lose contact forces with more than one thread**, because they add
  forces to other cells' nodes without an atomic add. They are the two models described in the
  paper. Run them with `--threads=1`, or use model 2 ({doc}`03_contact`).
- **Multi-threaded runs are not bit-reproducible** ({doc}`04_time_integration`).

:::{seealso}
- {doc}`/whats_new/v2.1` for the full list of changes in 2.1.
- {doc}`14_output_files` for the diagnostics CSV.
- {doc}`/developers/debugging` for reproducing a crash.
:::
