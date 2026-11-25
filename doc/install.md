(install)=
# Installing SimuCell3D

SimuCell3D is distributed as source code and built with CMake. The build produces one
command-line program, `simucell3d`, and optionally a Python module that drives the same solver.
There are two ways to install it:

- {ref}`install-source`: compile on Linux, or on Windows through the Windows Subsystem for
  Linux (WSL).
- {ref}`install-docker`: build a container image that contains the compiled program.

## Requirements

| Requirement | Version | Notes |
|---|---|---|
| Operating system | Linux | On Windows, use [WSL](https://learn.microsoft.com/en-us/windows/wsl/install). Other platforms are not tested. |
| C++ compiler | C++17, with OpenMP | GCC 10 or newer; GCC 10, 13 and 14 are tested. |
| CMake | 3.10 or newer; 3.12 or newer to run the end-to-end tests | CMake 4 is supported. |
| OpenMP | | Required by every module (`find_package(OpenMP REQUIRED)`). GCC ships it as `libgomp`. |
| git | | To obtain the source. |
| Python 3 | | Only for the optional Python module and the analysis scripts in `scripts/`. |

The other C++ dependencies (tinyxml2 for the parameter file, delaunator for the triangulation of
division planes, and pybind11 for the Python module) are included in the `lib/` directory of the
repository.

On Debian or Ubuntu the build tools are installed with:

```console
$ sudo apt-get install build-essential cmake git
```

For the optional Python module, add the Python headers, and for the analysis scripts a way to
create a virtual environment:

```console
$ sudo apt-get install python3-dev python3-venv
```

On Debian 12 and 13 and on recent Ubuntu releases, the system Python refuses `pip install`
outside a virtual environment (`error: externally-managed-environment`). Install the Python
packages that the scripts need into a virtual environment:

```console
$ python3 -m venv ~/.venvs/simucell3d
$ . ~/.venvs/simucell3d/bin/activate
$ pip install pandas matplotlib numpy tqdm
```

The `pip install` commands elsewhere in this documentation assume such an environment is
active. Without the `python3-venv` package, `python3 -m venv` fails with a message about
`ensurepip`.

These instructions were checked for 2.1.0 on Debian 13 with GCC 14.2 and CMake 3.31. The
continuous-integration builds use GCC 10 (Debian 11) and GCC 13 (Ubuntu 24.04).

(install-source)=
## Building from source

Clone the repository and build in Release mode in a `build/` directory:

```console
$ git clone https://github.com/nilesh-patil/simucell3d.git
$ cd simucell3d
$ mkdir build
$ cd build
$ cmake -DCMAKE_BUILD_TYPE=Release ..
$ make -j4
```

`-j4` compiles four files in parallel; use a higher number on a machine with more cores, or a
lower one if the compiler runs out of memory. A Release build is important: a Debug build of the
solver is much slower.

The executable is built as `build/src/app/simucell3d`. After every build, CMake also creates the
symbolic link `build/simucell3d` to it; the documentation runs the program through that link,
from the `build/` directory.

:::{note}
The path of the source directory is compiled into the program (`PROJECT_SOURCE_DIR`). It is used
to find input meshes given by a relative path such as `data/input_meshes/fig_3_vesicle.vtk`
({doc}`/user_guide/11_parameter_file`). If the repository is moved after the build, run `cmake`
again.
:::

### Verifying the build

From the `build/` directory, print the version:

```console
$ ./simucell3d --version
SimuCell3D 2.1.0 (<commit>)
```

The text in parentheses is the git commit the build was configured from; it is `unknown` when the
source is not a git checkout.

The test suite is registered with CTest and is built together with the program. To run all
tests, or only those whose name starts with a prefix (here the input/output tests):

```console
$ ctest
$ ctest -R io_
```

The complete suite takes a while; the {doc}`/developers/testing` page describes its parts.

### Compile-time configuration

The contact model, the dynamic model and the polarization mode are selected when the program is
compiled, by three macros in `include/global_configuration.hpp`:

```cpp
#define POLARIZATION_MODE_INDEX 1
#define CONTACT_MODEL_INDEX 2
#define DYNAMIC_MODEL_INDEX 0
```

These are the defaults, and the configuration that most shipped parameter files assume. To use
another configuration, edit the header and rebuild. The meaning of each value, and which
parameter file needs which configuration, is described in
{doc}`/user_guide/10_compile_time_configuration`.

### Python module (optional)

The Python module `simucell3d_python_wrapper` is built when CMake is configured with
`-DENABLE_PYTHON_BINDINGS=TRUE`:

```console
$ cmake -DENABLE_PYTHON_BINDINGS=TRUE -DPYTHON_EXECUTABLE=$(which python3) -DCMAKE_BUILD_TYPE=Release ..
$ make -j4
```

The compiled module is written to `build/bin/python_bindings/`. Its use is described in
{doc}`/user_guide/16_python_interface`.

(install-docker)=
## Docker

The `Dockerfile` in the repository root builds SimuCell3D (Release, with the Python module) in a
container based on the `python:3.10` image. It requires [Docker](https://docs.docker.com/get-docker/).

```console
$ git clone https://github.com/nilesh-patil/simucell3d.git
$ cd simucell3d
$ docker build -t simucell3d .
```

The source tree is copied to `/app` in the image and built in `/app/container_build`, which is
also the working directory of the container. The entry point is the executable
`/app/container_build/src/app/simucell3d`, so the arguments of `docker run` are passed to it.
Run a simulation from the repository root with:

```console
$ mkdir -p simulation_results
$ docker run --rm -v "$(pwd)/simulation_results":/app/simulation_results \
    simucell3d ../parameters/core/parameters_vesicle_quick.xml
```

- The parameter file path is relative to `/app/container_build`. Only files that were in the
  repository when the image was built are available; after adding or changing a parameter file
  or a mesh, build the image again.
- The shipped parameter files write to `../simulation_results/<name>`, that is to
  `/app/simulation_results/<name>` in the container. The `-v` option mounts that directory on
  `simulation_results/` in the current directory, so the results remain after the container
  exits.
- Options such as `--threads=N` are appended after the parameter file, as with the native
  executable ({doc}`/user_guide/13_running_simulations`).

## Troubleshooting

CMake reports `Could NOT find OpenMP_CXX`
: The compiler has no OpenMP support. With GCC, OpenMP is part of the compiler packages
  (`build-essential` on Debian and Ubuntu). With another compiler, install its OpenMP runtime, or
  select GCC with `cmake -DCMAKE_CXX_COMPILER=g++ ..` in an empty build directory.

The compiler is killed or the machine runs out of memory during `make`
: Reduce the number of parallel jobs, for example `make -j2`.

`./simucell3d: No such file or directory`
: The command is not run from the `build/` directory, or the build failed before the link step.
  Check the end of the `make` output.

The program cannot find an input mesh
: See {doc}`/faq`.

:::{seealso}
- {doc}`getting_started`: a first simulation with the installed program.
- {doc}`/user_guide/13_running_simulations`: command-line options and environment variables.
:::
