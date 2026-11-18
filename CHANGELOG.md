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
marked *(2.0.0)* were already in the 2.0.0 release. "1.0" means the tag `v1.0` (commit
`38af451`, 2024-06-26). The Zenodo archive cited by the paper
([doi:10.5281/zenodo.10796908](https://doi.org/10.5281/zenodo.10796908)) is the earlier commit
`5342bf8`; its source files are the same, but its default contact model is 1 instead of 2, and it
lacks the input files of Figs. 1 and 3.

Measurements below marked "reconstructed configuration" come from
`parameters/longrun_fast_growth.xml`, a growth configuration written for 2.x from Table 2 of the
paper. It is not the lab's configuration and differs from Table 2 (see Known issues).

### Changed behaviour (results differ from 1.0)

- **Contact distance to a face interior.** When a node projected onto the inside of a
  triangle, 1.0 computed the closest point as `a + ab*v + a + ac*w`, with one vertex `a` too
  many. The distance was measured to the wrong point, so repulsion missed contacts in face
  interiors. The closest point is now `a + ab*v + ac*w`. *(2.0.0)*
- **Semi-implicit Euler for all nodes.** With the full equations of motion
  (`DYNAMIC_MODEL_INDEX 0`) and face-face coupling (contact model 2, the default), 1.0 moved
  every node with its old momentum, coupled across a cell-cell contact or not. That is an
  explicit (forward) Euler step, although the paper's Methods describe a semi-implicit one; for
  a mode of stiffness k it acts like a damping coefficient of zeta - k*dt, so modes stiffer than
  zeta/dt were amplified. The momentum is now updated first, and the new momentum (averaged over
  a coupled group) moves every node, as in the Methods. Every cell surface relaxes differently,
  not only the contacts, so 2.1 does not reproduce the dynamics of results computed with 1.0 in
  this configuration. Contact models 0 and 1 were already semi-implicit in 1.0. *(2.0.0)*
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
  every loop is traced, the cell is cut along the longest one, and the two daughters are separated
  by a flood fill that does not cross the cut (but see Known issues: a loop nested inside the
longest one is not detected). When the Delaunay triangulation of the cut face does
  not cover its whole outline, the division is refused and the mother tries again at the next
  division check, with new random interior points; the log line names the cell. This happens more
  often as the tissue grows (see Known issues). In a single-threaded test run of an earlier version
  of this change, which patched such cut faces instead of refusing them, failed divisions fell
  from 1,014 to 10.
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
  model uses as list indices (segfault at about 2.7k cells in the reconstructed configuration).
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
  refiner and reached the error handler, which stopped the simulation. An 8-core run of the
  reconstructed configuration ended this way at 79,635 cells with an uncaught
  `std::bad_optional_access`. The same code is in 1.0. The cell is now removed instead (see
  Added safeguards).

- Six error checks never took effect: in the time integration (unsupported combinations of
  compile-time models) and in the automatic polarizer (an unknown region label), the error object
  was created but not thrown. These branches are not compiled in the default configuration.
- A parameter file that is not valid XML was reported as "could not be found ... must be an
  absolute path". The reader now reports a missing file, an unreadable file and an XML parse error
  (with its line) separately; relative paths are accepted, as they always were.
- `compute_IoU_euler.py` failed with `UnboundLocalError` when given explicit indexes.

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
- In 8-core runs of the reconstructed configuration, 2.1.0 needs 44-46% less time per
  iteration at 30k cells than the previous development builds, and reaches 30k cells in about
  4 hours. 1.0 with only the local-id crash fix needed 28.5 hours. Because the physics also
  differs (see above), this last comparison is not like-for-like.

### Engineering

- The project was renamed from DCM to SimuCell3D, and the source layout and build were
  reorganised. *(2.0.0)*
- Per-iteration diagnostics CSV, written with `--diagnostics-csv=PATH`. *(2.0.0)*
- New test and release process. CI (`ci.yml`) builds with GCC 10 and GCC 13 in Release and Debug,
  with AddressSanitizer and UndefinedBehaviorSanitizer, builds the Python bindings, runs the
  Python tests and checks that the version is consistent; one summary check, "CI result", gates
  merges. Tests carry ctest labels for four tiers: `unit`, `integration` (short end-to-end runs
  of shipped parameter files), `benchmark` (report only) and `long`. The long tier runs on
  labelled pull requests, release branches and release tags instead of on a schedule; the
  nightly and weekly benchmark workflows of 2.0.0 are removed. Releases are made from `vX.Y.Z`
  tags by `release.yml` (see `RELEASING.md`).
- The test suite grew from 126 registered ctest targets in 1.0 to 340.
- Documentation rewritten as a Sphinx site (user guide, examples, C++ and Python API reference,
  developer guide), published for each release at https://nilesh-patil.github.io/simucell3d/.
  `CITATION.cff` added.
- Every shipped parameter file states in its header the compile-time switches it needs.
  `parameters/core/parameters_vesicle_quick.xml` now runs at a stable time step (1e-7 s) for
  200 steps; at 5e-7 s the unstable-cell safeguard removed nearly all of its cells within a few
  iterations, and the run still exited with status 0 (see Known issues). The unused `<INMForce>`
  tag was removed from the shipped parameter files.
- Parameter screening: `collect_screening_data.py` writes `<screen>/summary_folder/`, the path the
  analysis scripts read, and detects crashed runs from the exit status that the launcher records,
  instead of looking for a `mesh_before_crash.vtk` file that is never written.
- Run launchers live in `scripts/runs/`, and the long-run growth configuration is
  `parameters/longrun_fast_growth.xml`, the reconstructed configuration described under Known
  issues.
- `NOTICE` added. `LICENSE` is again the licence text of 1.0, unchanged (the 2.0 file had added a
  copyright line that 1.0 does not have); the changes since 1.0 are copyright Nilesh Patil under
  the same licence, and 2.x is not reviewed or endorsed by the Iber Lab or ETH Zurich.
  `CITATION.cff` names the 2.x maintainer as software author, the article as the preferred
  citation and the 1.0 Zenodo archive as a reference.
- `simucell3d --version` prints the version and the git commit it was configured from.
- CMake 3.10 or newer is required, and CMake 4 is supported.

### Known issues

- **Unstable-cell removal has no limit, and a diverged run still exits with status 0.** Unlike
  refinement failures, unstable cells are removed without any limit per iteration or per run. In a
  review run of `parameters/core/parameters_vesicle_quick.xml` with the time step raised to 5e-7 s
  (its 2.0 value), all 432 epithelial cells were removed within 8 iterations, and the run exited
  with status 0. The total number of removals is not printed; the diagnostics CSV is written every
  100 iterations, so removals in the last partial window are missing and a run shorter than 100
  iterations writes no CSV; and `collect_screening_data.py` counts every run with exit status 0 as
  a success. Count the `Removing` lines in the standard error of every run (user guide,
  Safeguards) and treat a run with many removals as failed.
- **A division along a nested cut can give interpenetrating daughters.** When the division plane
  cuts the surface in several loops, the divider keeps the longest loop and does not check
  whether another loop lies inside it. For a cup-shaped or invaginated cell the interface then
  spans the cavity: in a review test (a cylinder with an axial dimple), 20 of 20 divisions went
  ahead with the interface across the cavity, the daughters' total volume was 3.4% larger than the
  mother's, and part of one daughter lay inside the other. Both daughters pass the unstable-cell
  checks and nothing is logged. How often this happens in growth runs has not been measured.
  Judging from its code, 1.0 failed such divisions instead.
- **Contact models 0 and 1 can lose contact forces with more than one thread.** They add forces
  to the nodes of neighbouring cells with a non-atomic add inside parallel loops, so with several
  threads some contact forces can be lost; only model 2 was fixed (see Fixed). Models 0 and 1 are
  the two models described in the paper, and `parameters_default_dynamic.xml`,
  `parameters_paper_exact.xml` (model 1) and `parameters_default_overdamped.xml` (model 0) need
  them. Run these models with `--threads=1`, or use model 2. No warning is printed.
- **The reconstructed configuration is not the lab's.** `parameters/longrun_fast_growth.xml`
  differs from Table 2 of the paper: minimum edge length 1e-6 m instead of 2e-7 m, lateral surface
  tension 8e-4 N/m instead of 1e-3 N/m, basal adhesion 0 and basal repulsion 5e8 Pa/m instead of
  1e9 Pa/m (both without effect with contact model 2), contact model 2, which the paper does not
  describe, and 10% standard deviations of growth rate and division volume, which Table 2 does
  not list. The lab's own benchmark file of 1.0, `parameters_benchmarck.xml` (contact model 1,
  polarization mode 0, minimum edge length 6e-7 m), was removed in 2.0.0 without a note. In
  November 2026 it was run once per setting on 8 cores with the matching compile-time switches. At
  its shipped time step of 1e-7 s, 2.1.0 removed blown-up cells in large numbers from about 9,000
  cells on (4,456 of 157,716 cells created) and was stopped at 28,600 cells; at 2.5e-8 s it grew
  to 18,300 cells without removals before it was stopped. 1.0 stopped with a mesh refinement error
  at about 170 cells at 1e-7 s.
- **Cells interpenetrate** in long growth runs of the reconstructed configuration, with 2.1 and
  with 1.0. In that file the contact cutoff (2e-7 m) is five times smaller than the minimum edge
  length (1e-6 m). Table 2 of the paper uses 2e-7 m for both, so this ratio belongs to the
  reconstruction, not to the published configuration; it may be one cause. Whether the lab's
  configurations show the same overlap has not been measured.
- **Some divisions are deferred, more often as the tissue grows.** A division is refused when the
  Delaunay triangulation of the cut face does not cover its outline, and the mother tries again at
  the next division check. In an 8-core run of the reconstructed configuration from 1 to 150,498
  cells, 14% of division attempts (refusals plus divisions) were refused: 5% below 30,000 cells,
  rising to 21% between 125,000 and 150,000. A mother that was refused was refused 1.75 times on average (median
  1, at most 31). At a fixed shape some are refused every time (1 of 5 test mothers was refused 100
  of 100 times) until the cell's shape changes. An earlier version of 2.1 patched these cut faces
  by ear clipping their outline instead; the mesh refiner could not repair the resulting daughters,
  and in an 8-core run of that code 190 of the 194 cells removed as unstable by 79,635 cells were
  such newborn daughters. In a 300-iteration restart test, refusing these divisions removed the
  newborn deletions (0 against 6). A better way to divide these cells is planned.
- **Rare blow-ups of single nodes.** Occasionally a node is thrown far from its cell in one time
  step, sometimes to a position that is no longer a finite number. The unstable-cell safeguard
  removes the cell and the run continues, but neighbouring cells can go with it: in the
  150,000-cell run above, 8 cells with one non-finite node each were removed in one iteration, and
  4 cells with the same extent of 1.5e9 m in another. All three long 2.1 test runs had such
  groups. The cause is not yet known.
- **Unchecked lookups in `noexcept` functions.** A few edge and face lookups on the mesh
  refiner's path (for example `edge::f1()`/`f2()`, the triangle score) still end the program with
  SIGABRT if a mesh is damaged, because they are declared `noexcept`.
- **Stale coupling links.** When a node switches to a closer coupling partner, its old partner
  keeps a link back to it until the coupling maps are rebuilt. This has been present since 1.0.
- **The bending force is not an exact gradient.** Two defects in the gradient of the hinge angle
  make the bending force deviate from the gradient of the bending energy: the cotangent terms of
  the second face use the normal of the first face (`src/mesh/cell.cpp`, `grad_x0_theta` and
  `grad_x1_theta`), and the edge direction follows the node ids rather than the winding of the
  first face. Compared with a finite-difference gradient on a test sphere, the force is on
  average about 1.5 times the gradient, with a relative residual of 0.26 to 0.40 that grows with
  the fold angle and depends on node numbering. With both defects fixed it would be exactly 1.5
  times the gradient. Both are inherited from 1.0 and left unchanged in 2.1, so that results stay
  comparable with the published ones.
- **Bending prefactor and `bending_energy`.** The bending force uses a prefactor of 3 where the
  paper's Methods give 2. The code is consistent with Eq. (1) of the paper: on regular,
  sphere-like meshes the Methods' discrete bending energy is about 2/3 of the Eq. (1) bending
  term, and the code's 3 restores it. The `bending_energy` written to the output uses the
  Methods' formula, so it is about 2/3 of the Eq. (1) term. Unchanged since 1.0. See the user
  guide chapter on cell mechanics.
- **Unknown command-line options are ignored.** A mistyped `--option` is silently ignored.
- **Python bindings are only smoke-tested.** CI builds the module and imports it, but no test runs a
  simulation through it.
- **Multi-threaded runs are not bit-reproducible.** Two runs with the same input and more than
  one thread can give slightly different results.

## [2.0.0] - 2026-03-02

First maintenance release after 1.0.

- Renamed the project from DCM to SimuCell3D and reorganised the source layout and build.
- Adaptive OpenMP scheduling (`--schedule`), a faster spatial grid (vector storage, Morton
  order), optional Sweep-and-Prune contact detection, and a per-iteration diagnostics CSV.
- First correctness fixes, which already change results compared with 1.0: the contact
  closest-point error and the semi-implicit Euler step for all nodes with contact model 2 (see 2.1.0).
- Memory-safety and robustness fixes (heap use-after-free in edge splits, division-by-zero
  guards, spatial-grid integer overflow) and an oscillation bail-out in the mesh refiner.
- The parameter file `parameters_benchmarck.xml` of 1.0, the lab's growth benchmark, was removed
  (not recorded at the time; see Known issues of 2.1.0).
- GitHub Actions CI with sanitizer builds and benchmark workflows; the test suite grew to 294
  registered ctest targets.

## [1.0.0] - 2024-06-26

The code published with Runser et al., "SimuCell3D: three-dimensional simulation of tissue
mechanics with cell polarization", *Nature Computational Science* (2024). The date is that of the
tag `v1.0` (commit `38af451`), a commit by the original author made after the Zenodo archive
cited by the paper ([doi:10.5281/zenodo.10796908](https://doi.org/10.5281/zenodo.10796908),
commit `5342bf8`, published 2024-03-08). The tag adds the input meshes and parameter files of
Figs. 1 and 3 and changes the default `CONTACT_MODEL_INDEX` from 1 to 2.

[2.1.0]: https://github.com/nilesh-patil/simucell3d/compare/v2.0...HEAD
[2.0.0]: https://github.com/nilesh-patil/simucell3d/compare/v1.0...v2.0
[1.0.0]: https://github.com/nilesh-patil/simucell3d/releases/tag/v1.0
