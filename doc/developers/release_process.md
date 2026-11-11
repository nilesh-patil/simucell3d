(dev-release)=
# Release process

This page explains how SimuCell3D versions are numbered and checked, what must pass before a
release, and what the release automation does. The step-by-step checklist is
[`RELEASING.md`](https://github.com/nilesh-patil/simucell3d/blob/main/RELEASING.md) at the
repository root; it is the canonical procedure, and the issue template
`.github/ISSUE_TEMPLATE/release.md` is its copy for the release issue.

## Versions and tags

SimuCell3D follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html). Releases are
tagged `vX.Y.Z` (for example `v2.1.0`); release candidates are tagged `vX.Y.Z-rcN`. The older
tags `v1.0` and `v2.0` predate this scheme and stay as they are.

The version is defined in one place, the `project()` command of the top-level
`CMakeLists.txt`:

```cmake
project(SimuCell3D VERSION 2.1.0)
```

Everything else derives from it or is checked against it:

| Consumer | How it gets the version |
|---|---|
| `simucell3d --version` | `include/version.hpp.in`, configured by CMake into `generated/version.hpp`; checked by the `cli_version_flag` test |
| documentation | `doc/conf.py` reads it from `CMakeLists.txt` |
| `CHANGELOG.md` | the top heading `## [X.Y.Z] - unreleased` or `## [X.Y.Z] - YYYY-MM-DD`; checked |
| `CITATION.cff` | `version:` and `date-released:`; checked |
| git tag | `vX.Y.Z`; checked when the tag is pushed |

`scripts/ci/check_version.py` performs the checks; CI runs it on every pull request:

```console
$ python3 scripts/ci/check_version.py                 # branch mode
Version 2.1.0 is consistent.
$ python3 scripts/ci/check_version.py --tag v2.1.0    # release mode
$ python3 scripts/ci/check_version.py --print-version
2.1.0
```

In branch mode it checks that the top CHANGELOG heading names the CMake version and is dated
`unreleased` or `YYYY-MM-DD`, that the headings are in descending order and each has a compare
link at the bottom of the file, that `CITATION.cff` states the CMake version or the newest
released version, and that `version.hpp.in` takes the version from CMake. In release mode it
also checks that the tag names the CMake version, that the top heading is dated (a `-rcN` tag
may still be `unreleased`), that its compare link ends at the tag rather than at `HEAD`, and that
`CITATION.cff` states the released version and date.

## What gates a release

A release is published only when all of the following hold for the commit to be tagged:

1. **CI is green** ({ref}`dev-ci`): the check "CI result" (`ci-ok`) of `ci.yml`.
2. **The long test tier is green** (`long.yml`): it runs automatically on pushes to `release/**`
   branches and on the tag, and can be started by hand.
3. **A growth run to 30,000 cells completed** on the reference machine, with its results
   recorded in the release issue (below). It is required for minor releases and for patch
   releases that change `src/` or `include/`.
4. **No open issue is labelled `release-blocker`.**

The documentation is also expected to build without warnings (`docs.yml`).

Performance comparisons with the previous release are recorded for information; they do not
block a release.

## The growth run

The growth run exercises what the test suite cannot: thousands of divisions, mesh refinement
and contact detection over many hours, and the {term}`safeguards <safeguard>` under load. It
grows a tissue from one cell with the paper-configuration parameters
(`parameters/longrun_fast_growth.xml`). The criterion is 30,000 cells. For reference,
`CHANGELOG.md` records that 2.1.0 reaches 30,000 cells in about 4 hours on 8 cores, and that
5 % of division attempts were refused below 30,000 cells in an 8-core run.

1. Build the release candidate in Release mode on the reference machine (8 physical cores
   for 2.1), in `build/`.
2. Start the run:

   ```console
   $ bash scripts/runs/launch_longrun_20k.sh
   ```

   The script runs `simucell3d` in the background with `--schedule=adaptive`, writes the output
   to `simulation_results/longrun_20k_fast_growth/` and the log and process id to
   `simulation_results/longrun_20k_fast_growth_logs/`.
3. Follow the cell count: `nb cells` in the progress lines of
   `simulation_results/longrun_20k_fast_growth_logs/run.log`, or the `cells` column of
   `simulation_results/longrun_20k_fast_growth/performance_diagnostics.csv` (one row every 100
   iterations):

   ```console
   $ tail -n 1 simulation_results/longrun_20k_fast_growth_logs/run.log
   ```

4. When the cell count passes 30,000, stop the run, for example with
   `pkill -f longrun_20k_fast_growth`, which matches the `simucell3d` process and its wrapper.
5. Fill in the results table of the release issue (template
   `.github/ISSUE_TEMPLATE/release.md`), with the same rows for the previous release on the same
   machine type:

   | Row | Where to find it |
   |---|---|
   | Commit | `./build/simucell3d --version` |
   | Machine type, cores, threads | CPU model (`lscpu`); the `Thread count:` line at the start of `run.log` |
   | Cells reached, simulated time reached [s] | `cells` and `sim_time` of the last row of the diagnostics CSV |
   | Wall time [h] | `wall_epoch` of that row minus `wall_epoch` of the first row |
   | Exit status or abort message | the end of `run.log`: an exception message, a signal, or the process stopped by hand at 30,000 cells |
   | `Removing numerically unstable cell` lines | `grep -c "Removing numerically unstable cell" run.log` |
   | `Cell division failed for cell` lines | `grep -c "Cell division failed for cell" run.log` (refused divisions) |

The run fails the gate if it ends before 30,000 cells, if `run.log` ends with an exception or a
signal, or if the refusal or removal counts are far above those recorded for the previous
release; the last point is a judgement for the maintainer, documented in the issue.

## The release procedure

`RELEASING.md` lists the steps; in outline:

1. Open a release issue from the template `.github/ISSUE_TEMPLATE/release.md`.
2. Push the release candidate commit to a branch `release/X.Y.Z-prep`; the push runs CI and the
   long tier. Patch releases of an older minor version are made from a `release/X.Y` branch.
3. Finish `CHANGELOG.md` (every user-visible change, results-changing items under "Changed
   behaviour") and the matching `whats_new/` page, and run the growth run.
4. In one pull request from `release/X.Y.Z-prep`, date the CHANGELOG heading, point its compare link at the new tag, and
   set `version:` and `date-released:` in `CITATION.cff`. Before merging:

   ```console
   $ python3 scripts/ci/check_version.py --tag vX.Y.Z
   $ python3 scripts/ci/changelog_section.py X.Y.Z
   ```

   The second command prints the release notes as they will be published.
5. Push the tag `vX.Y.Z` (or `vX.Y.Z-rcN` for a candidate). Two workflows start:
   - `release.yml` checks the version, runs `ci.yml` and `long.yml` on the tagged commit,
     builds `simucell3d-X.Y.Z.tar.gz` with `git archive` and its `SHA256SUMS`, builds and tests
     the tarball, and creates the GitHub release with the notes from `CHANGELOG.md` (a
     pre-release for `-rcN`). Release notes never come from the commit log.
   - `docs.yml` deploys the documentation to `/X.Y/` and, for the newest release, to
     `/stable/`, and updates the version menu.
6. Check the release page, the tarball checksum and the documentation, then open the next
   development cycle on `main` with a new `## [X.Y.Z] - unreleased` CHANGELOG section.

A published tag is never moved or deleted; a broken release is fixed by the next patch
release. The release artifacts for 2.1 are the source tarball and `SHA256SUMS`.

:::{seealso}
- `RELEASING.md` at the repository root (the checklist).
- {ref}`dev-ci` for the workflows.
- {ref}`dev-testing` for the test tiers.
:::
