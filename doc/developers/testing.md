(dev-testing)=
# Testing

The C++ tests live in `test/`, one subdirectory per module, and are registered with CTest.
Labels place each test in one or more tiers; CI selects tiers by label. Python tests for the
CI scripts live in `tests/`. This page explains how to run the tests, how a test is written and
registered, and what each tier contains.

## Running the tests

From a configured and built build directory:

```console
$ ctest -L "unit|integration" --output-on-failure -j
```

Useful variants:

| Command | Runs |
|---|---|
| `ctest -N` | lists the registered tests without running them |
| `ctest -L unit` | one tier, selected by label (a regular expression) |
| `ctest -L e2e` | the end-to-end runs of `simucell3d` |
| `ctest -R stability_` | tests whose name matches a regular expression |
| `ctest -R mesh_edge --output-on-failure -V` | verbose output of matching tests |
| `ctest --rerun-failed --output-on-failure` | only the tests that failed in the last run |

A bare `ctest` runs every registered test, including the long and benchmark tiers.

The Python tests use pytest, with the packages pinned in `scripts/ci/requirements.txt`
(the same versions as CI, which tests Python 3.10 and 3.12):

```console
$ python -m pip install -r scripts/ci/requirements.txt
$ python -m pytest tests/
```

## Tiers

| Tier | Label | Content | Runs |
|---|---|---|---|
| Unit | `unit` | Tests of single classes and functions in `test/test_*`: mesh, math, I/O, triangulation, contact models, spatial grids, solver steps, thread placement. The `stability_*` and `scientific_*` tests are also `unit`, because they finish in seconds. | every CI build ({ref}`dev-ci`) |
| Integration | `integration` | Tests across modules and end-to-end runs of the `simucell3d` executable: `cli_version_flag`, `test_contact_model_integration`, and the short tests in `test/test_e2e/`. | every CI build |
| Benchmark | `benchmark` | The micro-benchmarks in `test/test_benchmarks/` (`benchmark_*`). They report timings and never fail on speed; their numbers are meaningful only in a Release build ({ref}`dev-performance-internals`). | the long-tier workflow, Release only |
| Long | `long` | The `stability_*` and `scientific_*` tests and `e2e_growth_long`, a growth run from one cell to at least 100 cells (7000 iterations). | release branches, tags, labelled pull requests, by hand ({ref}`dev-ci`) |

Further labels group tests within the tiers: `e2e`, `stability`, `scientific`,
`contact_models`.

What CI runs, by tier:

```console
$ ctest -L "unit|integration"   # every CI run: Release, Debug, sanitizers, GCC 10
$ ctest -L long                 # the long-tier workflow
$ ctest -L benchmark            # the long-tier workflow, Release build only
```

In 2.1.0 the tiers hold 314 `unit`, 8 `integration`, 17 `benchmark` and 27 `long` tests, 340
registered tests in all (the `stability_*` and `scientific_*` tests count in both `unit` and
`long`). `ctest -N -L <label>` prints the current numbers.

One check sits outside CTest: a growth simulation to 30,000 cells, run before every release
({ref}`dev-release`).

Run `ctest -L "unit|integration"` in a Release build before opening a pull request, and also in
a sanitizer build when the change touches memory handling or parallel code ({ref}`dev-setup`).

### End-to-end tests

`test/test_e2e/CMakeLists.txt` registers tests that run the `simucell3d` executable. The run
tests are driven by `test/test_e2e/run_simulation.cmake` (a CMake script, run with `cmake -P`),
which copies a shipped parameter file with a shorter `simulation_duration` and
`sampling_period`, runs it with `--output-dir` in the build tree and a fixed `--threads`, and
checks that:

1. the program exits with status 0;
2. `cell_data/` and `face_data/` hold at least two VTK files with a valid header;
3. `simulation_statistics.csv` has the expected header, no `nan` or `inf`, and a last row at the
   final iteration;
4. the last statistics row lists at least a minimum number of cells.

| Test | Labels | Run |
|---|---|---|
| `e2e_growth_division` | `integration;e2e` | `parameters/longrun_fast_growth.xml` for $2 \times 10^{-4}$ s (2000 iterations, at least one division), 2 threads |
| `e2e_vesicle_quick` | `integration;e2e` | `parameters/core/parameters_vesicle_quick.xml` for $2 \times 10^{-6}$ s (20 iterations), 2 threads; all 434 cells must remain (about 22 s in Release, several minutes under ASan) |
| `e2e_growth_long` | `long;e2e` | `parameters/longrun_fast_growth.xml` for $7 \times 10^{-4}$ s (7000 iterations), 4 threads, at least 100 cells at the end |
| `e2e_missing_parameter_file`, `e2e_malformed_parameter_file`, `e2e_incomplete_parameter_file`, `e2e_invalid_threads_flag` | `integration;e2e` | invalid input must give exit status 1 and an error message (`expect_failure.cmake`) |

The duration and minimum cell count of `e2e_growth_long` can be overridden at configure time
with `-DE2E_LONG_DURATION=<s>` and `-DE2E_LONG_MIN_CELLS=<n>`. A new end-to-end run is added with
`simucell3d_add_e2e_run()` in the same file.

## How a test is written

The tests do not use a test framework. Each test executable is a C++ program whose `main()`
selects one test function by name from its first argument; each test function returns 0 on
success and a non-zero value on failure:

```cpp
int is_manifold_test() {
    // ... build a small mesh ...
    bool t1 = e.is_manifold();
    bool t2 = ...;
    return !(t1 && t2);  // 0 = pass
}

int main(int argc, char** argv) {
    assert(argc == 2);
    std::string test_name = argv[1];
    if (test_name == "is_manifold_test") return is_manifold_test();
    // ... one line per test ...
    std::cout << "TEST NAME :" << test_name << " DOES NOT EXIST" << std::endl;
    return 1;
}
```

The `CMakeLists.txt` of the test directory builds the executable, links the module libraries it
needs, and registers one CTest test per function:

```cmake
add_executable(test_edge test_edge.cpp)
target_link_libraries(test_edge PUBLIC src mesh)
target_include_directories(test_edge PUBLIC
    "${PROJECT_SOURCE_DIR}/include/"
    "${PROJECT_SOURCE_DIR}/include/math_modules"
    "${PROJECT_SOURCE_DIR}/include/mesh")

add_test(NAME mesh_edge_is_manifold_test COMMAND test_edge is_manifold_test)
```

Conventions:

- New test names follow `<module>_<submodule>_<function>`, for example
  `mesh_edge_is_manifold_test` or `solver_test_unstable_cell_removed`, so that
  `ctest -R <module>_` selects a module. A few older directories use other prefixes (table
  below).
- Check results with explicit comparisons and the return value, not with `assert`: assertions
  are compiled out in Release builds, where the check would silently pass.
- Print the values that are compared (`std::cout`), so that a failure in CI can be diagnosed
  from the log.
- Test input files are found through the `PROJECT_SOURCE_DIR` macro, which the test's
  `CMakeLists.txt` defines with `add_definitions(-DPROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")`;
  never through the current directory.
- A test that cannot run in the current configuration returns 77 and sets
  `SKIP_RETURN_CODE 77`, so that CTest reports it as skipped. Example:
  `solver_test_refinement_failure_end_to_end` skips in Debug builds, where an assertion in the
  refiner fires before the code path under test.
- Parallel code is tested with more than one thread: call `omp_set_num_threads()` or construct
  the solver with an explicit thread count.
- Tests that call private members use a `friend` tester class declared in the header (for
  example `local_mesh_refiner_tester`, `tester_contact_model_abstract`).

## Adding a test

1. Add the test function to the existing test file of the module, or create
   `test/test_<module>/test_<name>/` with a `.cpp` file and a `CMakeLists.txt`, and add the
   directory with `add_subdirectory()` to the parent `CMakeLists.txt`.
2. Add the dispatch line to `main()`.
3. Register it with `add_test(NAME <module>_<submodule>_<function> COMMAND <exe> <function>)`.
4. For a new directory, end its `CMakeLists.txt` with the `include()` of
   `test/test_labels.cmake`; set `LABELS` explicitly only for tests that are not fast unit tests
   (see below).
5. Build and run it alone: `ctest -R <name> --output-on-failure -V`.

For a bug fix, commit the test first, failing, and the fix in a separate commit (see
{ref}`dev-contributing`).

### Labels

Labels are assigned by `test/test_labels.cmake`, which every test `CMakeLists.txt` includes
after its `add_test` lines:

```cmake
include(${PROJECT_SOURCE_DIR}/test/test_labels.cmake)
```

It labels each test of the directory that does not set `LABELS` itself, from the test name:

| Name | Labels |
|---|---|
| `benchmark_*` | `benchmark` |
| `stability_*` | `unit;long;stability` |
| `scientific_*` | `unit;long;scientific` |
| anything else | `unit` |

A new test therefore needs no label of its own if it is a fast unit test. Set `LABELS`
explicitly (and a `TIMEOUT`) for anything else, before the `include()`:

```cmake
set_tests_properties(my_slow_test PROPERTIES LABELS "long" TIMEOUT 1800)
```

A new test directory must end with the `include()` line, otherwise its tests have no label and
are not selected by `ctest -L "unit|integration"`, so CI does not run them. List unlabelled
tests with:

```console
$ ctest -N -LE "unit|integration|benchmark|long"
```

The list should be empty.

## Test suites by directory

| Directory | Prefix | Tests |
|---|---|---|
| `test_math_modules/` | `math_` | `vec3`, `mat33`, `quaternion`, eigen solver |
| `test_mesh/` | `mesh_`, `cell_` | node, edge, face, cell, cell classes |
| `test_io/` | `io_` | parameter reader, mesh reader and writer, statistics writer, initializer |
| `test_triangulation_modules/` | `triangulation_` | local mesh refiner, cell divider, initial triangulation, ball pivoting, Poisson sampling |
| `test_contact_models/` | `contact_`, `sap_` | contact models, contact detection strategies, contact forces; `test_contact_model_integration` |
| `test_uspg/` | `uspg_`, `morton_` | spatial grids and Morton codes |
| `test_time_integration/` | `time_` | time integration schemes |
| `test_automatic_polarization/` | | automatic polarizer; the executable is built but registers no CTest test |
| `test_solver/` | `solver_` | solver construction, scheduling, cell removal, refinement failures, thread placement |
| `test_cpu_topology/` | `test_cpu_topology_` | CPU topology detection and placement order |
| `test_performance_monitor/` | `performance_` | performance monitor |
| `test_utils/` | `test_utils_` | utilities |
| `test_scientific_benchmarks/` | `scientific_` | physical checks: energy conservation and dissipation, pressure response and the Laplace law, volume preservation, mesh quality, contact reciprocity, force balance, momentum conservation |
| `test_stability/` | `stability_` | determinism, time-step convergence, extreme parameters, bending-force bounds on degenerate triangles |
| `test_benchmarks/` | `benchmark_` | micro-benchmarks |
| `test_e2e/` | `e2e_` | end-to-end runs of `simucell3d` |

`src/app/CMakeLists.txt` registers `cli_version_flag` (`simucell3d --version` must print
`SimuCell3D <version> (<commit>)` and exit with status 0); `test/test_e2e/` holds the other
tests that run the executable.

The physics-level suites:

- **Determinism** (`stability_determinism_*`): two single-threaded runs of 200 iterations of
  `test_scientific_benchmarks/params_single_sphere.xml` must give bitwise identical positions,
  volumes and energies (`std::memcmp`). The sphere does not divide; cell division samples
  points with a clock-seeded generator, so single-threaded runs with divisions are not
  repeatable and are not covered. Runs with more than one thread are not bitwise
  reproducible, because the order of floating-point additions depends on the schedule.
- **Time-step convergence** (`stability_timestep_*`): the same sphere with $\Delta t$ =
  $10^{-7}$, $\Delta t/2$, $\Delta t/4$ s for $5 \times 10^{-6}$ s of simulated time; the
  observed order $p = \log_2(e_{\Delta t} / e_{\Delta t/2})$ must lie in $[0.3, 1.7]$
  (semi-implicit Euler is first order). Volume errors below $10^{-17}$ are treated as
  floating-point noise.
- **Extreme parameters** (`stability_extreme_*`): zero bulk modulus, high pressure, edges at
  half the minimum length, contact cutoff equal to the minimum edge length; the run must
  complete without non-finite values.
- **Degenerate hinge** (`stability_degenerate_hinge_*`, `stability_healthy_mesh_untouched_by_guard`):
  the bending force on a near-degenerate triangle must stay bounded, and the guard must not
  change the force on a healthy mesh. These tests build their parameters in code.

These tests call `solver::run_iteration()` a fixed number of times rather than
`solver::run()`, so their cost is fixed.

## Python tests

`tests/ci/` holds pytest tests of the scripts in `scripts/ci/`: the version check, the CHANGELOG
section extraction, and the benchmark result aggregation and comparison tools. Their
dependencies are pinned in `scripts/ci/requirements.txt`; they run in CI ({ref}`dev-ci`).

## Long-running and memory checks

Outside CTest, `scripts/benchmarking/stability/` has three tools for long runs:

```console
$ scripts/benchmarking/stability/run_24hr_test.sh parameters/core/parameters_vesicle.xml \
    --duration-hours 24 --build-dir build
$ scripts/benchmarking/stability/oom_monitor.sh <pid> --threshold-mb 2048 --interval-sec 10 \
    --output memory.csv
$ scripts/benchmarking/stability/run_valgrind_check.sh --build-dir build --test-pattern stability
```

`run_24hr_test.sh` runs a simulation for the given number of hours while recording memory use
(`memory.csv`) and the log; `oom_monitor.sh` records the resident memory of a running process and
warns when it exceeds the threshold or doubles; `run_valgrind_check.sh` runs the matching CTest
tests under Valgrind memcheck with the suppressions in `valgrind_suppressions.supp`.

The release growth run is described in {ref}`dev-release`.

:::{seealso}
- {ref}`dev-ci` for which tiers run where.
- {ref}`dev-debugging` for sanitizer builds.
:::
