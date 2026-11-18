(dev-ci)=
# Continuous integration

SimuCell3D uses GitHub Actions; the workflows are in `.github/workflows/`. They follow three
rules: a red required check means the code is broken (never the runner or benchmark noise), every
pull request runs the simulator end to end, and nothing runs on a schedule. Expensive checks run
when a release is prepared or when a maintainer asks for them.

## Overview

| Workflow | Runs on | Purpose |
|---|---|---|
| `ci.yml` (CI) | every pull request; pushes to `main` and `release/**`; by hand; called by `release.yml` | the required checks |
| `long.yml` (Long tests) | pull requests touching `src/` or `include/` with the label `run-long-tests`; pushes to `release/**`; called by `release.yml`; by hand | the `long` test tier and the micro-benchmarks |
| `perf.yml` (Performance A/B) | pull requests touching `src/`, `include/`, `lib/`, `CMakeLists.txt` or `parameters/`; by hand | timing comparison with the base branch; informational |
| `docker.yml` (Docker) | pull requests and pushes to `main` that change the `Dockerfile`, `.dockerignore`, the top-level `CMakeLists.txt`, `src/`, `include/` or `lib/` | builds the Docker image and runs `simucell3d --version`; informational |
| `docs.yml` (Docs) | pull requests touching `doc/`, `include/`, `CHANGELOG.md`, `README.md` or `CMakeLists.txt`; pushes to `main`; tags `vX.Y.Z`; by hand | builds and deploys the documentation |
| `release.yml` (Release) | tags `vX.Y.Z` and `vX.Y.Z-rcN` | publishes a release |

There is no nightly or weekly workflow.

## `ci.yml`

| Job | Configuration | What it checks |
|---|---|---|
| `lint` | Ubuntu 24.04 | `actionlint` on the workflow files; `ruff --select E9,F63,F7,F82` (syntax errors and undefined names) on `scripts/` and `tests/`; `clang-format` on the changed lines, **advisory**: differences are reported as a warning and a diff in the job summary, but do not fail the job |
| `build-oldest` | GCC 10 in a `debian:bullseye` container, Release | build, `ctest -L "unit\|integration"` on the oldest supported toolchain |
| `build-test` | GCC 13 Release; GCC 13 Debug; `ubuntu-latest` Release (canary, allowed to fail) | build, `ctest -L "unit\|integration"`; the canary shows a runner-image upgrade without blocking merges |
| `sanitizers` | Debug, `-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer` | `ctest -L "unit\|integration"` with `ASAN_OPTIONS=detect_leaks=0` |
| `python` | Python 3.10 and 3.12, packages from `scripts/ci/requirements.txt` | `python -m pytest tests` |
| `bindings` | Release with `-DENABLE_PYTHON_BINDINGS=TRUE` | builds `simucell3d_python_wrapper` and imports it |
| `version-check` | | `python3 scripts/ci/check_version.py` ({ref}`dev-release`) |
| `ci-ok` (CI result) | | succeeds only if all jobs above succeeded; the single check to require in branch protection |

The integration tier includes end-to-end runs of the `simucell3d` executable (a growth run with
one division, 20 iterations of the 434-cell vesicle, and error-path tests), so every
pull request runs the simulator ({ref}`dev-testing`).

When a build job fails, its CTest logs (`Testing/`) are uploaded as an artifact. A new push to a
pull request cancels the run of the previous push. `ci.yml` can be started by hand for any
branch, tag or commit (Actions tab, "CI", "Run workflow", field `ref`).

## The long tier: `long.yml`

The workflow runs `ctest -L long` in two builds, Release and RelWithDebInfo with
AddressSanitizer and UndefinedBehaviorSanitizer, and `ctest -L benchmark` (report only) in the
Release build. The `long` tier holds the stability and scientific-validation suites and the growth
run `e2e_growth_long` ({ref}`dev-testing`).

It runs only:

- on a pull request that touches `src/` or `include/` and carries the label `run-long-tests`.
  A maintainer adds the label; the run starts when it is added and repeats on every push while
  it stays;
- on every push to a `release/**` branch;
- from `release.yml`, for every tag;
- by hand (Actions tab, "Long tests", "Run workflow").

Use the label for changes to the physics, the mesh operations, the contact models or the
parallel code, where the short tiers can miss a regression that appears only after many
iterations.

## Performance comparison: `perf.yml`

The job builds the pull request and its base on the same runner and times both on the same fixed
amount of work with `scripts/ci/perf_ab.py`: `parameters/core/parameters_vesicle_quick.xml`
cut to $2 \times 10^{-6}$ s (20 iterations of the 434-cell vesicle, mesh written at the start and
the end only), one warm-up, then five repetitions per build, alternating between them. It posts (or updates) one pull-request comment with the medians and
ranges, and adds the label `perf: slower` when the head build's median is more than 10 % above
the base and the ranges do not overlap. It fails only if the pull-request build cannot run the
workload at all; a slowdown never fails it. Shared runners are too noisy for a hard threshold,
and timings from different runners are not comparable, so a maintainer reads the comment and,
for a real slowdown, asks for a measurement on a dedicated machine ({ref}`dev-benchmarking`).

## Documentation: `docs.yml`

The job installs Doxygen and the packages of `doc/requirements.txt`, writes the Doxygen XML
(`make doxygen`), and builds the site with warnings as errors (`make html-strict`). It sets
`SIMUCELL3D_REQUIRE_DOXYGEN=1`, so the build fails rather than publish API pages without the C++
reference. On a pull request the HTML is uploaded as the artifact `docs-html` (kept 14 days) for
review.

On a push to `main` the site is deployed to the `gh-pages` branch under `/dev/`. On a tag
`vX.Y.Z` it is deployed to `/X.Y/` and, if X.Y is the newest release, also to `/stable/`, and
`switcher.json` at the site root (the version menu) is updated. Other directories on `gh-pages`
are kept, so older versions stay online. The site root redirects to `/stable/` once a release
has been deployed, and to `/dev/` before that.

## Releases: `release.yml`

Pushing a tag `vX.Y.Z` (or `vX.Y.Z-rcN`) runs, in order: `verify`
(`check_version.py --tag`), `ci` (calls `ci.yml`), `long` (calls `long.yml`), `dist` (source
tarball `simucell3d-X.Y.Z.tar.gz` made with `git archive`, plus `SHA256SUMS`; the tarball is
built and tested on its own), and `publish` (GitHub release with notes from
`scripts/ci/changelog_section.py`, a pre-release for `-rcN`). The documentation for the tag is
deployed by `docs.yml`, which has its own tag trigger. The workflow never pushes commits or
moves tags. The procedure around it is in `RELEASING.md` and {ref}`dev-release`.

## Reproducing a CI job locally

```console
$ cmake -B build -DCMAKE_BUILD_TYPE=Release
$ cmake --build build -j
$ cd build && ctest -L "unit|integration" -j
```

For the sanitizer job, configure as in {ref}`dev-setup`; for the GCC 10 job, use the
`debian:bullseye` container shown there. For the lint job:

```console
$ ruff check --select E9,F63,F7,F82 scripts tests
$ git clang-format --diff origin/main
```

When a job fails only in CI, download the `Testing/` artifact from the run page;
`Testing/Temporary/LastTest.log` holds the output of every test.

## Branch protection

The recommended settings for `main` and the `release/*` branches are: changes through pull
requests only, the check "CI result" (`ci-ok`) required, no force pushes. Requiring only
`ci-ok` keeps the protection rule unchanged when jobs are added to `ci.yml`.

:::{seealso}
- {ref}`dev-testing` for the test tiers and labels.
- {ref}`dev-release` for the release procedure.
:::
