# Contributing to SimuCell3D

Contributions are welcome: bug reports, fixes, tests, documentation and new model features.
The full guide is the developer guide of the documentation:
<https://nilesh-patil.github.io/simucell3d/stable/developers/index.html>
(source: [`doc/developers/`](doc/developers/)).

## Reporting a problem

Open an issue at <https://github.com/nilesh-patil/simucell3d/issues> with:

1. the output of `simucell3d --version`;
2. operating system, compiler and build type, and any changes to
   `include/global_configuration.hpp`;
3. the command line and the parameter file (and the input mesh, if it is not a shipped one);
4. what happened and what was expected, with the last lines of the output.

## Proposing a change

1. Branch from `main` (`fix/...`, `feat/...`, `perf/...`, `docs/...`, `test/...`).
2. Build and run the tests:

   ```console
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build -j
   cd build && ctest -L "unit|integration" --output-on-failure
   ```

3. Add a regression test for every bug fix and tests for new behaviour.
4. Format the changed lines with `git clang-format origin/main`.
5. Write commit messages in the Conventional Commits form, for example
   `fix(divider): refuse a division whose cut face cannot be triangulated`.
6. Add an entry to the top section of `CHANGELOG.md` for changes users can see, and say
   explicitly if simulation results change.
7. Open a pull request against `main`. CI must pass before merging.

Details: [contributing](doc/developers/contributing.md),
[testing](doc/developers/testing.md),
[coding style](doc/developers/coding_style.md),
[architecture](doc/developers/architecture.md).

## Licence

SimuCell3D is distributed under the BSD 3-Clause licence ([LICENSE](LICENSE)). Contributions are
accepted under the same licence.
