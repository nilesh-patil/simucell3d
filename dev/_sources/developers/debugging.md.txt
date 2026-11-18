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
