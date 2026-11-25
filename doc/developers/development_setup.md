(dev-setup)=
# Development setup

This page lists the build configurations used during development and how to reproduce the CI
builds locally. Installation for users is described in {doc}`/install`.

## Requirements

| Tool | Version |
|---|---|
| C++ compiler with OpenMP | C++17; GCC 10 is the oldest supported compiler, and CI also builds with a recent GCC |
| CMake | 3.10 or newer (CMake 4 is supported) |
| Python | 3.10 or newer, for the scripts, the Python tests and the documentation |
| git | the configure step records the commit for `simucell3d --version` |
| optional | `clang-format` and `clang-tidy` (style), `gdb`, `valgrind`, `perf`, `doxygen` |

On Debian or Ubuntu:

```console
$ sudo apt-get install build-essential cmake git python3 python3-venv clang-format gdb
```

GCC ships OpenMP (libgomp). With Clang, also install the OpenMP runtime (`libomp-dev`).

## Getting the source

```console
$ git clone https://github.com/nilesh-patil/simucell3d.git
$ cd simucell3d
```

All third-party C++ code is vendored in `lib/` (TinyXML-2, Delaunator, pybind11); there are no
submodules. `.gitattributes` stores VTK files under `baselines/` in Git LFS; the 2.1.0 tree holds
none (only `baselines/nature_paper/README.md`), and nothing in the build needs LFS files.

## Build configurations

Use one build directory per configuration. The tests locate their input files through the
source directory recorded at configure time, so a build directory can live anywhere; the
examples below put them at the repository root.

| Purpose | Configure |
|---|---|
| Release (runs, benchmarks) | `cmake -B build -DCMAKE_BUILD_TYPE=Release` |
| Debug (assertions on) | `cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug` |
| Release with symbols (long runs under gdb, profiling) | `cmake -B build-dbg -DCMAKE_BUILD_TYPE=RelWithDebInfo` |
| AddressSanitizer + UndefinedBehaviorSanitizer | `cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"` |
| ThreadSanitizer | `cmake -B build-tsan -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CXX_FLAGS="-fsanitize=thread" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"` |
| Python bindings | add `-DENABLE_PYTHON_BINDINGS=TRUE -DPYTHON_EXECUTABLE=$(which python3)` |

Then build and test:

```console
$ cmake --build build -j
$ cd build
$ ctest -L "unit|integration" --output-on-failure -j
```

Notes:

- Without `CMAKE_BUILD_TYPE`, CMake uses no optimisation flags. Always set a build type.
- The Debug build is slow; use it for unit tests and short runs. For runs of many iterations
  use Release or RelWithDebInfo.
- Sanitizer test runs need `ASAN_OPTIONS=detect_leaks=0`: several test fixtures do not free all
  their memory, which LeakSanitizer would report.
- Changing `include/global_configuration.hpp` recompiles every file that includes it; no
  reconfiguration is needed.
- `ccache` speeds up rebuilds: add `-DCMAKE_CXX_COMPILER_LAUNCHER=ccache` to the configure
  command.

The executable is `build/src/app/simucell3d`; a post-build step links it to `build/simucell3d`.
The module libraries are in `build/bin/<module>/`, the test executables in
`build/test/<module>/`, and the Python module in `build/bin/python_bindings/`.

## The oldest supported compiler

GCC 10 (Debian 11) is the oldest compiler CI builds with. To check a change against it
locally without installing it, build in a container. The command mirrors the `build-oldest` job
of `ci.yml`, including its first step: Debian 11 security updates moved to `archive.debian.org`,
so the package sources must be rewritten before `apt-get update`.

```console
$ docker run --rm -v "$PWD":/src -w /src debian:bullseye bash -c \
    "sed -i -E 's#http://(deb|security)\.debian\.org/debian-security#http://archive.debian.org/debian-security#' /etc/apt/sources.list && \
     apt-get update && apt-get install -y build-essential cmake git && \
     cmake -B build-gcc10 -DCMAKE_BUILD_TYPE=Release && cmake --build build-gcc10 -j && \
     cd build-gcc10 && ctest -L 'unit|integration' --output-on-failure"
```

Debian 11 ships CMake 3.18, which is above the minimum of 3.10.

## Editor support

The top-level `CMakeLists.txt` sets `CMAKE_EXPORT_COMPILE_COMMANDS`, so every build directory
contains `compile_commands.json`. Point `clangd` or the editor's C++ extension at it, for example
with a symbolic link at the repository root:

```console
$ ln -s build/compile_commands.json .
```

`clang-tidy -p build <file>` uses the same file ({ref}`dev-coding-style`).

## Python environment

The Python tools and tests have separate requirement files:

| Purpose | Install |
|---|---|
| tests in `tests/`, `ruff` | `python -m pip install -r scripts/ci/requirements.txt` (Python 3.10 or 3.12, as in CI) |
| documentation | `python -m pip install -r doc/requirements.txt` ({ref}`dev-writing-docs`) |
| Python bindings examples | `python -m pip install -r scripts/python_bindings/requirements.txt` |
| screening analysis | `python -m pip install -r scripts/screening_analysis/requirements.txt` |
| benchmark plots | `pandas`, `numpy`, `scipy`, `matplotlib` |

Use one virtual environment per purpose; the pinned versions in the requirement files differ.

## Running a simulation from a build directory

Parameter files name the input mesh relative to the repository root and the output folder
relative to the current directory, so the shipped examples are run from `build/`:

```console
$ cd build
$ ./simucell3d ../parameters/core/parameters_vesicle_quick.xml --threads=4 --output-dir=dev_run
```

`parameters_vesicle_quick.xml` runs 200 iterations of the 434-cell vesicle; it writes four mesh
frames and one diagnostics row (iteration 100). The output folder is deleted at the start of the
run: the one named in the parameter file (`../simulation_results/...`), or the one given with
`--output-dir=`, as here. Use a separate `--output-dir` for each development run so that no
other run's output is deleted.

:::{seealso}
- {ref}`dev-testing`
- {ref}`dev-debugging`
- {ref}`dev-ci` for the exact CI configurations.
:::
