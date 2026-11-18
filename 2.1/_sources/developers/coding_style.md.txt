(dev-coding-style)=
# Coding style

These are the conventions the C++ code follows today. Follow them in new code, and keep a
functional change free of unrelated reformatting so that reviews and `git blame` stay useful.

## Formatting

Formatting is defined by `.clang-format` at the repository root: Google style with 4-space
indentation, no tabs, a 120-column limit, attached braces, left-aligned pointers
(`double* p`) and sorted includes within blocks.

Much of the existing code predates this file and is not formatted with it. Format only the
changed lines:

```console
$ git clang-format --diff origin/main
$ git clang-format origin/main
```

The first command prints what would change; the second applies it to the changed lines.
Reformatting a whole file belongs in a separate commit with no functional change. The CI
`lint` job runs the same check on the changed lines of a pull request; it is advisory (a warning
and a diff in the job summary), because the existing code predates `.clang-format`
({ref}`dev-ci`).

## Naming

| Entity | Convention | Example |
|---|---|---|
| classes and structs | `snake_case` | `local_mesh_refiner`, `cell_type_parameters` |
| functions and methods | `snake_case` | `refine_meshes()`, `get_local_id()` |
| data members | `snake_case_` with trailing underscore | `cell_lst_`, `node_lst_` |
| local variables, parameters | `snake_case` | `nb_cells`, `min_edge_len` |
| macros | `UPPER_SNAKE_CASE` | `CONTACT_MODEL_INDEX` |
| header guards | `DEF_<NAME>` | `DEF_SOLVER` |
| exceptions | `<module>_exception`, derived from `std::exception`, all defined in `custom_exception.hpp` | `mesh_reader_exception` |

Common abbreviations: `nb_` (number of), `_lst` (list or vector), `_ptr` (smart pointer),
`c`, `n`, `f` for a cell, node or face in short loops. `PerformanceMonitor` is the one class in
CamelCase; do not rename it in passing.

The project does not use a namespace for its own classes. Free helper functions used by one
`.cpp` file are `static` or in an anonymous namespace.

## Files

- One class per header in `include/<module>/` and its implementation in `src/<module>/`, with
  the same base name.
- Headers start with an include guard `#ifndef DEF_<NAME>` / `#define DEF_<NAME>`. (The vendored
  `math_modules/eigen_solver.hpp` uses `#pragma once`; leave it as it is.)
- Include standard headers before project headers. Project headers are included by file name
  (`#include "cell.hpp"`); the include directories are set per target in the module's
  `CMakeLists.txt`.
- List new source files explicitly in the module's `CMakeLists.txt`; there is no globbing.

## C++ usage

- C++17 (`set(CMAKE_CXX_STANDARD 17)`). The oldest supported compiler is GCC 10; do not use
  library features it lacks.
- Ownership: cells are held by `std::shared_ptr` (`cell_ptr`); modules owned by the solver are
  `std::unique_ptr`. Nodes and faces are values inside their cell's vectors; refer to them by
  index, or by reference only within a scope in which the vectors cannot grow.
- Declare exception behaviour explicitly, as most existing member functions do: `noexcept` when
  the function cannot throw, `noexcept(false)` when it can. An exception thrown inside an OpenMP parallel
  region terminates the program, so functions called from parallel loops must be `noexcept` or
  catch internally and report the failure to the caller (as
  `local_mesh_refiner::refine_meshes()` returns its `refinement_failure` list).
- Use the exception classes of `include/custom_exception.hpp` for errors that end the run, and
  `unstable_simulation_exception` when the simulation has diverged. (The initialisation
  exception is spelled `intialization_exception` in the code.)
- Use `assert` for internal invariants. Assertions are compiled out in Release builds, so do
  not put side effects in them and do not rely on them to validate input.
- Physical quantities are `double` in SI units. State the unit in comments where it is not
  obvious.
- Code that depends on a compile-time switch from `global_configuration.hpp` uses
  `#if MACRO == value`; keep every documented combination compiling.
- Parallel loops over cells use `#pragma omp parallel for schedule(runtime)` unless there is a
  reason to fix the schedule; see {ref}`dev-performance-internals` for the rules on shared data
  and atomics.

## Static analysis

`.clang-tidy` enables a conservative set of checks (`bugprone-*`, `clang-analyzer-*`,
`performance-*` and a few `modernize-` and `readability-` checks), advisory only. It needs the
`compile_commands.json` that CMake writes into the build directory:

```console
$ clang-tidy -p build src/solver.cpp
```

## Comments and Doxygen

- Public classes and member functions in `include/` are documented with Javadoc-style Doxygen
  comments (`/** ... */`, `///<` for members); the API reference is generated from them. The
  first sentence is the brief description. Use `@param` (with the unit for physical
  quantities), `@return`, `@throws` and `@note` (thread safety, `noexcept` caveats).
- Each header has `@file` and `@ingroup <group>`; the groups are defined in
  `doc/api/groups.dox` ({ref}`dev-writing-docs`).
- Comments describe the code as it is. Do not write development history into comments
  ("Phase 1", "FIX:", "NEW:", issue or bug numbers); that belongs in the commit message and
  `CHANGELOG.md`.

## Python and shell scripts

- Python scripts under `scripts/` start with a module docstring that states the purpose and the
  usage line, and use `argparse` for their options.
- Shell scripts start with `#!/usr/bin/env bash` or `#!/bin/bash`, use `set -euo pipefail`, and
  print usage with `--help`.
- Do not hard-code machine-specific paths, host names or dates.
