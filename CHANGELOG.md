# Changelog

All notable changes to SimuCell3D are recorded in this file.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and version
numbers follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.1.0] - unreleased

> **Results differ from 1.0.** A simulation run with 2.1.0 will not reproduce the numbers
> produced by 1.0.0, the code published with Runser et al., *Nature Computational Science*
> (2024). Several 1.0 bugs changed the physics, and fixing them changes trajectories. 2.1.0
> also adds safeguards that delete or repair cells that 1.0 would have kept, or crashed on.
> To compare with the published figures, re-run the comparison with 1.0.0 rather than mixing
> results from the two versions.

These notes compare 2.1.0 with 1.0.0, because that is the version used in the paper. Items
marked *(2.0.0)* were already in the 2.0.0 release.

### Changed behaviour (results differ from 1.0)

- **Contact distance to a face interior.** When a node projected onto the inside of a
  triangle, 1.0 computed the closest point as `a + ab*v + a + ac*w`, with one vertex `a` too
  many. The distance was measured to the wrong point, so repulsion missed contacts in face
  interiors. The closest point is now `a + ab*v + ac*w`. *(2.0.0)*
- **Semi-implicit Euler for coupled nodes.** With the full equations of motion
  (`DYNAMIC_MODEL_INDEX 0`) and face-face coupling, 1.0 moved nodes coupled across a cell-cell
  contact with their old momentum. That is an explicit (forward) Euler step, although the scheme
  is documented as semi-implicit. The momentum is now updated first, and the new averaged
  momentum moves the coupled nodes. *(2.0.0)*
- **Coupling partner lookup ([#113](https://github.com/nilesh-patil/simucell3d/issues/113)).**
  The face-face coupling model looked up a node's current partner by the other cell's global id,
  but the table is indexed by local id. Once cells had divided, the two ids differ, the lookup
  always missed, and the partner became whichever candidate a thread visited last instead of
  the nearest node. The partner is now the nearest node.
- **Face orientation after an edge split.** When the mesh refiner split an edge, it oriented the
  new triangles using the normals stored on the old faces. Those normals can be out of date, so
  some new triangles came out inside-out. The cell then carried an inverted patch for good: its
  volume was mis-measured, pressure stopped resisting compression, and the cell swelled (up to
  3-7 times its target volume) or stretched into a long thin cell that slowed contact detection.
  In one long run, 8% of cells carried such a patch at 26k cells. The new triangles now copy the
  orientation of the face they replace. 1.0 has the same bug.
- **Division when the cutting plane crosses the surface more than once.** If the division plane
  also cut through a thin protrusion of the cell, 1.0 followed only one of the intersection loops,
  the division failed ("not properly triangulated"), and the cell retried every iteration. Now
  every loop is traced, the cell is cut along the longest one, the two daughters are separated by
  a flood fill that does not cross the cut, and the interface between them falls back to
  ear-clipping triangulation when the Delaunay triangulation leaves a gap. In a single-threaded
  test run, failed divisions fell from 1,014 to 10.
- **Bending force on sliver triangles.** The bending force contains 1/area and cot(angle) terms.
  On a nearly flat triangle it could become more than a million times larger than normal and
  move a node by centimetres in one time step. A hinge is now skipped when one of its two triangles has
  an area below 1e-4 times the square of its longest edge, and |cot(angle)| is capped at 1000.
  Well-shaped triangles give exactly the same force as before.

### Added safeguards (not in the published model)

These keep long runs alive. They are not part of the model described in the paper, and each one
is reported so that you can see how often it acted.

- **Unstable cells are deleted.** A cell is removed if any node coordinate is not a finite number,
  or if its bounding box is wider than 10 times the cube root of its target volume. Each removal
  is logged to stderr and counted in the `unstable_cells_removed` column of the diagnostics CSV.
- **Inside-out and self-intersecting cells are deleted too.** A cell is also removed if its signed
  volume is zero or negative (the whole cell is inside out), or if its volume exceeds the volume
  of its bounding box (its surface passes through itself). The log line names the reason
  (`negative volume`, `volume exceeds bounding box`). Static cells are exempt.
- **The mesh refiner gives up on an oscillating cell.** When refinement keeps undoing itself at
  the same place on a cell, the refiner stops working on that cell for the current step, logs it,
  and lets the run continue. Some edges of that cell may stay outside `[min_edge_length,
  max_edge_length]` until the next step. 1.0 aborted the whole run with "The refinement of the
  mesh of cell X failed." *(2.0.0; the log message is new in 2.1.0)*
- **Face orientation self-check.** After every mesh refinement, each cell checks that all its
  faces are oriented consistently. A cell that fails is logged and its faces are re-oriented.
  The check costs about 0.25% of a time step.
- **The contact grid refuses a diverged domain.** If node coordinates are infinite or so large
  that the spatial grid cannot hold them, the run stops with an error instead of hanging.
- **A cell whose mesh refinement fails is deleted.** If refining one cell's surface mesh throws an
  error (for example an edge lookup that finds nothing in a damaged mesh, or the refiner's
  operation cap), that cell is removed right after refinement, before contact detection, and the
  run continues. Each removal is logged
  (`Removing cell <id> at iteration <n>: mesh refinement failed: <error>`) and counted in
  `unstable_cells_removed`. The run still stops on a diverged domain or when out of memory, and
  also when more than max(10, 1% of the cells) fail in one iteration, or all of them do, since
  that points to a bug in the refiner rather than to a few damaged cells. Lookups inside
  functions declared `noexcept` cannot be caught this way and still abort the program.

### Fixed

- Crash after a cell was removed: the remaining cells kept stale local ids, which the contact
  model uses as list indices (segfault at about 2.7k cells in the paper configuration).
- Race condition in cell division: daughters were appended to the cell list from inside the
  parallel loop that iterates over it.
- Heap use-after-free in the mesh refiner's edge split. *(2.0.0)*
- Division-by-zero guards in matrix inversion and vector angle calculations. *(2.0.0)*
- Integer overflow in the spatial grid (USPG): in voxel indexing *(2.0.0)*, and in the voxel
  count of a very large domain, which caused a 20-hour stall.
- The membrane elasticity energy in the output was always 0.
- The GROWTH phase in the diagnostics could never be detected.
- Face-face coupling under contention: threads read a node's coupling map while others wrote
  to it, and a farther partner could overwrite a closer one. The closest-partner rule is now
  applied under both nodes' locks, and both directions of a coupling are stored together.
- Contact forces added to the nodes of neighbouring cells were lost under contention. The
  `omp atomic` pragmas in `vec3` had no effect, because the math module is compiled without
  OpenMP; these forces now use an atomic add. ThreadSanitizer reported 43 races in contact
  resolution before this fix and none after.
- An error raised during contact detection (for example a diverged domain) now reaches the
  error handler instead of aborting the program with SIGABRT.
- Undefined behaviour when the solver was destroyed: the contact model base class had no virtual
  destructor, so the derived contact model was deleted without its destructor and its face grid
  was never freed. AddressSanitizer reported it as a new-delete type mismatch.
- One damaged cell ended the whole run: an error thrown while refining one cell's mesh left the
  refiner and reached the error handler, which stopped the simulation. An 8-core
  paper-configuration run ended this way at 79,635 cells with an uncaught
  `std::bad_optional_access`. The same code is in 1.0. The cell is now removed instead (see
  Added safeguards).

### Performance

- One OpenMP thread is pinned to each physical core by default. Previously threads could end up
  in pairs on hyperthreads of the same core while other cores sat idle. `--threads=N` overrides
  the thread count.
- Adaptive OpenMP scheduling, selected with `--schedule` (default `adaptive`). *(2.0.0)*
- The spatial grid (USPG) stores voxels in vectors and orders them along a Morton curve.
  *(2.0.0)*
- Optional Sweep-and-Prune contact detection (`<contact_detection_algorithm>sweep_and_prune`
  in the parameter file; the spatial grid remains the default). *(2.0.0)*
- Face-face contact uses a compact face grid and splits the work of very large cells across
  threads. This halves the contact step (2.6k cells, 4 threads: 1,059 to 500 ms per iteration).
- In 8-core runs of the paper-configuration growth simulation, 2.1.0 needs 44-46% less time per
  iteration at 30k cells than the previous development builds, and reaches 30k cells in about
  4 hours. 1.0 with only the local-id crash fix needed 28.5 hours. Because the physics also
  differs (see above), this last comparison is not like-for-like.

### Engineering

- The project was renamed from DCM to SimuCell3D, and the source layout and build were
  reorganised. *(2.0.0)*
- Per-iteration diagnostics CSV, written with `--diagnostics-csv=PATH`. *(2.0.0)*
- Four GitHub Actions workflows: CI (Release, Debug, AddressSanitizer, UndefinedBehaviorSanitizer,
  latency profiling, format check) and pull-request, nightly and weekly benchmarks. *(2.0.0)*
- The test suite grew from 126 registered ctest targets in 1.0 to 327.
- Run launchers live in `scripts/runs/`, and the long-run growth configuration is
  `parameters/longrun_fast_growth.xml`.
- `simucell3d --version` prints the version and the git commit it was configured from.
- CMake 3.10 or newer is required, and CMake 4 is supported.

### Known issues

- **Cells interpenetrate**, in every version including 1.0. The likely cause is that, in the
  paper configuration, the contact cutoff (2e-7 m) is five times smaller than the minimum edge
  length (1e-6 m).
- **Newborn daughters removed as unstable.** In an 8-core paper-configuration run of this code
  without the winding self-check and the new volume checks, 194 cells had been removed as
  unstable by 79,635 cells, and 190 of them were daughters under 200 iterations old (most under
  50), including all 16 cells that had become inside-out and tangled. The cause has been traced to
  the divider's fallback for a cut face that the Delaunay triangulation leaves incomplete (about 3%
  of divisions): it rebuilds the face from its outline points only, the mesh refiner cannot
  repair the resulting daughters, and their slivers produce very large bending forces. 2.1.0
  removes inside-out cells as soon as their volume turns negative or impossible; a fix of the
  fallback itself is in progress.
- **Unchecked lookups in `noexcept` functions.** A few edge and face lookups on the mesh
  refiner's path (for example `edge::f1()`/`f2()`, the triangle score) still end the program with
  SIGABRT if a mesh is damaged, because they are declared `noexcept`.
- **Stale coupling links.** When a node switches to a closer coupling partner, its old partner
  keeps a link back to it until the coupling maps are rebuilt. This has been present since 1.0.
- Contact models 0 and 1 still add contact forces without an atomic add. The paper
  configuration uses model 2, which is fixed.
- **Multi-threaded runs are not bit-reproducible.** Two runs with the same input and more than
  one thread can give slightly different results.

## [2.0.0] - 2026-03-02

First release of this fork.

- Renamed the project from DCM to SimuCell3D and reorganised the source layout and build.
- Adaptive OpenMP scheduling (`--schedule`), a faster spatial grid (vector storage, Morton
  order), optional Sweep-and-Prune contact detection, and a per-iteration diagnostics CSV.
- First correctness fixes, which already change results compared with 1.0: the contact
  closest-point error and the semi-implicit Euler step for coupled nodes (see 2.1.0).
- Memory-safety and robustness fixes (heap use-after-free in edge splits, division-by-zero
  guards, spatial-grid integer overflow) and an oscillation bail-out in the mesh refiner.
- GitHub Actions CI with sanitizer builds and benchmark workflows; the test suite grew to 294
  registered ctest targets.

## [1.0.0] - 2024-06-26

The code published with Runser et al., "SimuCell3D: three-dimensional simulation of tissue
mechanics with cell polarization", *Nature Computational Science* (2024).

[2.1.0]: https://github.com/nilesh-patil/simucell3d/compare/v2.0...HEAD
[2.0.0]: https://github.com/nilesh-patil/simucell3d/compare/v1.0...v2.0
[1.0.0]: https://github.com/nilesh-patil/simucell3d/releases/tag/v1.0
