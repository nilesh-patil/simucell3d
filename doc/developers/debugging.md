(dev-debugging)=
# Debugging

This page collects the tools and procedures for finding the cause of a crash, a hang or a
diverging simulation. The build configurations themselves are described in
{ref}`dev-setup`.

## Read the log first

`simucell3d` writes progress to standard output and problems to standard error. When a run is
redirected to a file, run it with `stdbuf -oL -eL` so that the progress lines are written as
they happen and the last line before a crash is in the file.

Lines to look for:

| Line (standard error) | Meaning |
|---|---|
| `Removing numerically unstable cell <id> at iteration <i>: extent ... m (max ... m), volume ... m^3, target volume ... m^3, <k> non-finite nodes` | The stability check removed a cell whose mesh exploded or became non-finite; `, negative volume` or `, volume exceeds bounding box` is appended when those checks fired. |
| `Removing cell <id> at iteration <i>: mesh refinement failed: <reason>` | The refiner could not repair the cell's mesh; the cell was removed. |
| `Mesh refinement failed for <n> of <N> cells at iteration <i>; stopping` | Too many refinement failures in one iteration (more than max(10, 1 % of the cells)), or all cells failed; the run stops with an `unstable_simulation_exception`. |

The number of removals per 100 iterations is the `unstable_cells_removed` column of the
{term}`diagnostics CSV`. A few removals in a long run are expected (see
{doc}`/user_guide/09_safeguards`); a sudden increase points to a parameter or a code change that
destabilises the integration. An exception that reaches `main()` is printed to standard error
and the program exits with status 1.

Grep a log for the summary:

```console
$ grep -c "Removing numerically unstable cell" run.log
$ grep "mesh refinement failed" run.log | tail
```

## Make the failure smaller

Most failures depend on the state reached after many iterations, so start by making the
reproduction cheaper:

- **Thread count.** Re-run with `--threads=1`. If the failure disappears, suspect a data race
  (use ThreadSanitizer, below). Results differ between thread counts in any case, because the
  order of floating-point additions changes; a failure that moves to a different iteration with a
  different thread count is not by itself a race.
- **Output.** Point `--output-dir=` at scratch space and keep the last VTK frames: the cell id
  in a log line identifies the cell (the `cell_id` array) in `cell_data/result_<n>.vtk`.
- **A unit test.** When the failing cell's mesh is known, a test that loads that mesh and
  calls the failing function directly reproduces the problem in milliseconds. The cell divider
  tests in `test/test_triangulation_modules/test_cell_divider/` follow this pattern.

(dev-debugging-signatures)=
## Failure signatures from long runs

The defects below first showed up in 8-core growth runs of the reconstructed configuration
(`parameters/longrun_fast_growth.xml`) between September and November 2026, at thousands to tens
of thousands of cells. All are fixed in 2.1.0, and each has a regression test. This list is a
guide for a failure that looks similar: the signature, the cause that was found, the fix, and
the tests to start from.

### Crash soon after the first cell removal

Signature
: A segmentation fault a few iterations after the first cell was removed at its minimum volume (at 2,709 cells, in `epithelial_cell::special_polarization_update`).

Cause
: After a cell was erased from `cell_lst_`, the cells behind it kept their old local ids, so the contact model indexed the wrong cell, or one past the end.

Fix
: f61b620: local ids are reassigned after every removal.

Tests
: `solver_test_local_ids_resynced_after_cell_removal`.

### Mass removal of exploded cells

Signature
: Mass removal of cells from about 13,000 cells; removal lines with extents of metres or more, nodes moved far outside the tissue in one step.

Cause
: The bending force divides by the triangle area and was guarded only against an area of exactly zero; one sliver triangle gave a force about $2 \times 10^6$ times the normal scale.

Fix
: 8fd9e2b: the force is bounded on degenerate triangles.

Tests
: `stability_degenerate_hinge_force_bounded`, `stability_degenerate_hinge_force_saturates`, `stability_healthy_mesh_untouched_by_guard`.

### Inside-out patches on cells

Signature
: Inside-out patches on 8 % of the cells at 26,000 cells; swollen cells or long, thin cells.

Cause
: Edge splitting chose the winding of the new triangles from the normal stored on the old face, which can be stale.

Fix
: 3a0fd76: the winding is taken from the node cycle of the replaced face; 3287dd1 checks the winding after every refinement.

Tests
: `triangulation_modules_local_mesh_refiner_refine_mesh_keeps_winding_consistent`, `triangulation_modules_local_mesh_refiner_winding_self_check` (mesh `split_edge_winding_daughter.vtk`).

### Mother cells that never divide

Signature
: 12,700 to 149,000 failed-division lines per run ending in `is not properly triangulated.`; the same mother cells fail again at every division check.

Cause
: The division plane met the surface in several loops (a thin tendril) and the divider traced only one.

Fix
: fc0e29c: every loop is traced and the cell is cut along the longest; c949e4e: a cut face that the Delaunay triangulation does not cover is refused.

Tests
: `triangulation_modules_cell_divider_divide_cell_multi_loop_plane_test` (meshes `stuck_mother_45093.vtk`, `stuck_mother_60905.vtk` captured from the runs), `..._refuses_uncovered_interface_test`, `..._run_keeps_refused_mother_test`.

### Silent stall in contact detection

Signature
: No output for 20 hours at 15,800 cells; the process at full load inside contact detection.

Cause
: One cell's nodes diverged to about $10^{19}$ m; the voxel count of the spatial grid overflowed to 0, and contact detection degenerated into a scan of all nodes against all faces.

Fix
: 9acfe36: diverged cells are removed (`Removing numerically unstable cell`), and the grid refuses a non-finite or oversized domain with `unstable_simulation_exception`.

Tests
: `solver_test_unstable_cell_removed`, `solver_test_contact_model_throws_on_diverged_domain`, `uspg_4d_diverged_domain_test`, `uspg_3d_diverged_domain_test`, `uspg_3d_large_grid_index_overflow_test`.

### Run ended by one damaged cell

Signature
: The run ended at 79,635 cells with an uncaught `std::bad_optional_access`.

Cause
: An error in the refinement of one damaged cell left the parallel loop and reached `main()`.

Fix
: 60cacc8: the cell is removed (`Removing cell <id> at iteration <i>: mesh refinement failed: ...`).

Tests
: `solver_test_refinement_failure_end_to_end` (committed failing before the fix, e193669), `solver_test_refinement_failure_removed`, `solver_test_refinement_failure_limit`, `solver_test_refinement_failure_all_cells_stop`.

### Threads sharing physical cores

Signature
: On a 4-core, 8-thread machine, the 4 threads ran on two physical cores.

Cause
: With `OMP_PROC_BIND=close` and no `OMP_PLACES`, libgomp placed consecutive threads on sibling hyperthreads.

Fix
: 27cbf72: the solver pins one thread per physical core itself.

Tests
: `test_cpu_topology_placement_4c8t`, `solver_test_thread_placement`.

### How they were narrowed down

1. **Log lines and counts first.** The cell id, iteration and extent in the removal lines, and
   the count of `Cell division failed` lines, located the cell and the iteration.
2. **The last mesh frames.** The VTK frames before the failure showed the inverted patches and
   the tendrils; the mothers that never divided were extracted from a frame into the test
   meshes above.
3. **gdb for crashes, sampling for stalls.** A core dump located the stale local id (the last
   cell had `local_id_` equal to the size of the cell list). For the stall, backtraces of the
   running process placed it in contact detection, and the bounds of the grid's domain
   ($-4 \times 10^{19}$ to $2 \times 10^{19}$ m) pointed to a diverged cell.
4. **A failing test before the fix.** Each cause was reproduced in a unit test, on a captured
   mesh where one existed, and the test was committed before the fix.

The thread placement problem leaves no error at all; it shows in the start-up line
`Thread pinning: ...` and in the share of cores in use (`/usr/bin/time -v`, or `top`).

## Debug and sanitizer builds

A Debug build enables the `assert` statements, which catch many invariant violations close to
their cause; it is slow (no optimisation). For long runs, a Release build with debug symbols is
the usual compromise:

```console
$ cmake -B build-dbg -DCMAKE_BUILD_TYPE=RelWithDebInfo
$ cmake --build build-dbg -j
```

`RelWithDebInfo` compiles with `-O2 -g -DNDEBUG`: the assertions are off, the speed is close to
Release and backtraces have line numbers.

AddressSanitizer and UndefinedBehaviorSanitizer catch out-of-bounds accesses, use after free,
and undefined arithmetic:

```console
$ cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
$ cmake --build build-asan -j
$ cd build-asan
$ ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 UBSAN_OPTIONS=print_stacktrace=1 \
    ./simucell3d ../parameters/core/parameters_vesicle_quick.xml --output-dir=asan_run
```

`detect_leaks=0` is needed because several test fixtures do not free all their memory. For
runs that must reach many iterations, replace `Debug` by `RelWithDebInfo` in the first command;
the sanitizers still work, and the run is several times faster.

ThreadSanitizer finds data races; it cannot be combined with AddressSanitizer:

```console
$ cmake -B build-tsan -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_CXX_FLAGS="-fsanitize=thread" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"
```

GCC's libgomp is not instrumented for ThreadSanitizer, so reports that involve only OpenMP
runtime frames can be false positives; reports between two lines of SimuCell3D code are worth
investigating. ThreadSanitizer was used to find the contact-force races fixed in 2.1
(`CHANGELOG.md`).

## gdb

Run a crash under `gdb` with backtraces of all threads printed on the signal:

```console
$ cd build-dbg
$ gdb -batch -ex run -ex "thread apply all bt 25" \
    --args ./simucell3d --threads=4 --output-dir=gdb_run ../parameters/core/parameters_vesicle_quick.xml
```

For long runs, `scripts/runs/launch_crash_repro.sh [build-dir] [run-name]` starts the long-run
growth configuration (`parameters/longrun_fast_growth.xml`) in the background under `gdb`,
prints the backtrace of every thread on a crash, writes a core file into
`simulation_results/<run-name>_logs/`, and sets the sanitizer options above in case the build
directory holds a sanitizer build. Set `THREADS=N` to pass `--threads=N`.

To inspect a core file written by the system instead (enable with `ulimit -c unlimited`):

```console
$ gdb ./simucell3d core
(gdb) thread apply all bt
```

A hang can be inspected by attaching to the running process (`gdb -p <pid>`, then
`thread apply all bt`) or sampled with `perf top -p <pid>`. A run whose spatial grid has become
very large spends all its time in contact detection; the stability checks are meant to prevent
this, so a hang in `contact_model_abstract` or the USPG classes usually means a cell diverged
without being removed.

## Python bindings in VS Code

The Python module can be debugged at the C++ level by starting the Python interpreter under
`gdb`. Build the bindings in Debug mode:

```console
$ cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_PYTHON_BINDINGS=TRUE \
    -DPYTHON_EXECUTABLE=$(which python3)
$ cmake --build build -j
```

Then add a launch configuration to `.vscode/launch.json` that runs the Python file open in the
editor:

```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "python_binding_debug",
            "type": "cppdbg",
            "request": "launch",
            "program": "/usr/bin/python3",
            "args": ["${file}"],
            "cwd": "${workspaceFolder}/scripts/python_bindings",
            "stopAtEntry": false,
            "MIMode": "gdb",
            "miDebuggerPath": "/usr/bin/gdb",
            "setupCommands": [
                {
                    "description": "Enable pretty-printing for gdb",
                    "text": "-enable-pretty-printing",
                    "ignoreFailures": true
                }
            ]
        }
    ]
}
```

Set `program` to the interpreter of the virtual environment that the scripts use. Open the
script, set breakpoints in the C++ sources and start debugging with F5.

:::{seealso}
- {ref}`dev-testing` for adding a regression test once the cause is found.
- {doc}`/user_guide/09_safeguards` for what each removal mechanism checks.
:::
