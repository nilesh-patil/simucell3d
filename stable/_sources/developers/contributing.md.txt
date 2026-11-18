(dev-contributing)=
# Contributing

Contributions are welcome: bug reports, fixes, tests, documentation, new model features and
performance work. This page describes how to report a problem, how a change travels from a
branch to a release, and what a pull request needs before it can be merged.

## Reporting a problem

Open an issue on GitHub (<https://github.com/nilesh-patil/simucell3d/issues>). A useful bug
report contains:

1. The version: the output of `simucell3d --version` (version and git commit).
2. The operating system, compiler and version, build type, and any non-default values in
   `include/global_configuration.hpp`.
3. The command line and the parameter file. If the input mesh is not one of the shipped meshes,
   attach it or say how it was made.
4. What happened and what was expected, with the last lines of the output (standard output and
   standard error) in a code block. For crashes, a backtrace ({ref}`dev-debugging`).
5. For results that look wrong: the iteration or simulation time at which they appear, and how
   they compare with 1.0 if known. Note that results of 2.x differ from 1.0 by design
   ({doc}`/whats_new/v2.1`).

Questions about using SimuCell3D can also be asked as issues. Search the existing issues first.

## Workflow

1. **Fork and branch.** Create a branch from `main` with a descriptive name and a prefix that
   matches the change: `fix/`, `feat/`, `perf/`, `docs/`, `test/`, `refactor/`, `ci/`.
2. **Build and test locally** ({ref}`dev-setup`, {ref}`dev-testing`). Add a test that fails
   without the change for every bug fix, and tests for every new function or behaviour.
3. **Commit** in small, self-contained steps (see below).
4. **Open a pull request** against `main`. CI runs on every pull request
   ({ref}`dev-ci`); a maintainer reviews the change.
5. **Merge.** Pull requests are merged when the required CI checks are green and the review is
   complete. Fixes for a released version may also be backported to its release branch.

Large changes (a new contact model, a change to the equations, a new file format) are easier to
review when they are discussed in an issue first.

## Commit messages

Commit messages follow the Conventional Commits format:

```text
type(scope): short summary in the imperative, lower case, no final period

Body: what was wrong or missing, and why this change is the right fix. Wrap at
about 100 columns. Mention measurements with their conditions, and the tests added.

Refs: #123
```

| Type | Use for |
|---|---|
| `feat` | new behaviour visible to users (a parameter, a flag, an output column) |
| `fix` | a bug fix |
| `perf` | a change that only affects speed or memory |
| `refactor` | code restructuring without behaviour change |
| `test` | tests only |
| `docs` | documentation only (including `CHANGELOG.md` and code comments) |
| `build`, `ci` | CMake and CI changes |
| `chore` | tooling and maintenance |

The scope is the module or area, for example `divider`, `contact`, `mesh`, `solver`, `io`,
`changelog`. Examples from the history:

```text
fix(divider): refuse a division whose cut face Delaunay cannot cover, instead of ear clipping it
test(divider): refuse divisions whose cut face Delaunay cannot cover (fails until the fix)
docs(changelog): divisions the Delaunay interface cannot cover are refused and retried
```

A failing test committed before its fix, as in the second example, documents the bug and
proves the fix.

## Pull request checklist

Before asking for review, check the items below. The pull request template
(`.github/pull_request_template.md`) repeats the essential ones.

- [ ] The change builds without new warnings in Release and Debug.
- [ ] `ctest -L "unit|integration"` passes, also in a sanitizer build if the change touches
      memory handling or parallel code ({ref}`dev-testing`).
- [ ] New and changed lines are formatted with `git clang-format` ({ref}`dev-coding-style`).
- [ ] Bug fixes come with a regression test; new behaviour comes with tests.
- [ ] Changes that alter simulation results are stated as such in the pull request and in
      `CHANGELOG.md`, and the reason is explained.
- [ ] Performance changes state the measurement and its conditions ({ref}`dev-benchmarking`).
- [ ] Public headers have Doxygen comments for new or changed classes and functions, and the
      documentation builds without warnings if `doc/` or `include/` changed
      ({ref}`dev-writing-docs`).
- [ ] `CHANGELOG.md` has an entry under the top (unreleased) section, unless the change is
      internal only (tests, CI, refactoring without behaviour change).

Changes to the code under `src/` or `include/` can additionally be run through the long test
tier by a maintainer, by adding the label `run-long-tests` to the pull request ({ref}`dev-ci`).

## Changelog entries

`CHANGELOG.md` follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). The top
section is the next release, headed `## [X.Y.Z] - unreleased`. Add each entry to the subsection
that fits: changed behaviour (results differ), added safeguards, fixed, performance,
engineering, known issues. Write for users: say what changed in the behaviour or the results,
not how the code was edited. Give numbers only with their conditions.

## Licence

SimuCell3D is distributed under the BSD 3-Clause licence (`LICENSE`). A contribution
submitted to the project is distributed under the same licence. Do not add code that
cannot be distributed under it; vendored third-party code goes in `lib/` with its own licence
file.
