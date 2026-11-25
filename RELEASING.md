# Releasing SimuCell3D

This file is the release procedure for SimuCell3D 2.1 and later. A copy of the checklist
is the issue template `.github/ISSUE_TEMPLATE/release.md`; open one issue per release and
tick it off there.

## Version numbers and tags

- Versions follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html): `X.Y.Z`.
  A minor release (`2.2.0`) can change behaviour; a patch release (`2.1.1`) only fixes bugs.
- The single source of the version is `project(SimuCell3D VERSION X.Y.Z)` in the top-level
  `CMakeLists.txt`. It reaches the binary through `include/version.hpp.in`
  (`simucell3d --version` prints `SimuCell3D X.Y.Z (<sha>)`, checked by the
  `cli_version_flag` test). `CHANGELOG.md` and `CITATION.cff` repeat the number and are
  checked against it by `scripts/ci/check_version.py` in every CI run.
- Tags are `vX.Y.Z` (`v2.1.0`), and `vX.Y.Z-rcN` for release candidates. The older tags
  `v1.0` and `v2.0` are kept as they are.
- Patch releases of an older minor version are made from a `release/X.Y` branch.

## What blocks a release

| Gate | How it is checked |
|---|---|
| CI green on the release commit | `ci.yml`, run again by `release.yml` on the tag |
| Long tier green | `long.yml` (`ctest -L long` in Release and with ASan + UBSan), run again by `release.yml` |
| Growth run to 30,000 cells (minor releases) | by hand, results recorded in the release issue (below) |
| Version consistent | `check_version.py --tag vX.Y.Z`, the first job of `release.yml` |
| No open issue labelled `release-blocker` | by hand |

Performance comparisons (`perf.yml`, micro-benchmarks) are informational and never block
a release. A patch release may skip the 30,000-cell run if it changes no code under
`src/` or `include/`; say so in the release issue.

## 1. Before the release pull request

1. Open an issue from the **Release** template, titled `Release X.Y.Z`.
2. Check that the last CI run on `main` (or on `release/X.Y`) is green.
3. Run the long tier on the release candidate commit: push it to a branch named
   `release/X.Y.Z-prep` (the push starts `long.yml`), or start **Long tests** by hand from
   the Actions tab with the commit as `ref`. Link the run in the issue.
4. Growth run to 30,000 cells on the reference machine type (8 cores; the 2.1
   verification used Google Cloud 8-core VMs). Build Release from the candidate commit and
   run `parameters/longrun_fast_growth.xml`:

   ```bash
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build -j"$(nproc)"
   scripts/runs/launch_longrun_20k.sh     # writes simulation_results/longrun_20k_fast_growth/
   ```

   The launcher runs until it is stopped. Stop it once `nb cells` in its log
   (`simulation_results/longrun_20k_fast_growth_logs/run.log`) passes 30,000, with
   `pkill -f longrun_20k_fast_growth`. (`sim.pid` holds the pid of the `/usr/bin/time`
   wrapper, so killing only that pid can leave `simucell3d` running.) Record in the
   release issue: commit, machine type and core count, cells reached, simulated time, wall
   time, exit status or abort message, the number of `Removing numerically unstable cell`
   lines (cells removed by the safeguard) and of `Cell division failed for cell` lines
   (refused or failed divisions, retried later), for example with
   `grep -c 'Removing numerically unstable cell' run.log`. Compare with the same numbers
   for the previous release on the same machine type.
5. Build the documentation without warnings (`make -C doc html-strict`, see
   `doc/developers/writing_documentation.md`) and read the release-history page for the
   new version.

## 2. Release pull request

On a branch `release/X.Y.Z-prep`, in one commit:

1. `CMakeLists.txt`: `project(SimuCell3D VERSION X.Y.Z)`.
2. `CHANGELOG.md`: change `## [X.Y.Z] - unreleased` to `## [X.Y.Z] - YYYY-MM-DD` (the
   planned tag date), and change its link at the bottom of the file from
   `.../compare/vPREV...HEAD` to `.../compare/vPREV...vX.Y.Z`.
3. `CITATION.cff`: `version: X.Y.Z` and `date-released: 'YYYY-MM-DD'` (same date).
4. Check locally, then open the pull request:

   ```bash
   python3 scripts/ci/check_version.py --tag vX.Y.Z
   python3 scripts/ci/changelog_section.py X.Y.Z    # the text of the release notes
   ```

Merge when CI is green. The release notes are the `## [X.Y.Z]` section of `CHANGELOG.md`
and nothing else; edit that section, not the GitHub release page.

## 3. Release candidate (optional for minor releases)

```bash
git checkout main && git pull
git tag -a vX.Y.Z-rc1 -m "SimuCell3D X.Y.Z-rc1"
git push origin vX.Y.Z-rc1
```

`release.yml` creates a GitHub pre-release with the tarball. A candidate may still have
`unreleased` as its CHANGELOG date. Build the tarball on a clean machine
(`tar xzf`, `cmake`, `ctest -L "unit|integration"`) before tagging the final release.

## 4. Tag

On the merge commit of the release pull request:

```bash
git checkout main && git pull
git tag -a vX.Y.Z -m "SimuCell3D X.Y.Z"      # use -s instead of -a to sign the tag
git push origin vX.Y.Z
```

Pushing the tag starts two workflows:

- `release.yml`: `verify` (version check), `ci` and `long` (the full test tiers on the
  tagged commit), `dist` (`simucell3d-X.Y.Z.tar.gz` made with `git archive`, plus
  `SHA256SUMS`, then built and tested from the tarball), and `publish` (the GitHub
  release, with notes from `scripts/ci/changelog_section.py`).
- `docs.yml`: builds the documentation and deploys it to `/X.Y/` and, for the newest
  release, `/stable/` on <https://www.nilesh42.science/simucell3d/>, and updates the
  version switcher.

Watch both runs until they finish.

## 5. After the release

1. Check the release page: title, notes, `simucell3d-X.Y.Z.tar.gz` and `SHA256SUMS`;
   download the tarball and run `sha256sum -c SHA256SUMS`.
2. Check the documentation at `/X.Y/` and `/stable/`, and the version switcher.
3. Open a pull request that starts the next version: `project(SimuCell3D VERSION X.Y+1.0)`,
   a new `## [X.Y+1.0] - unreleased` section at the top of `CHANGELOG.md`, and its link
   `[X.Y+1.0]: .../compare/vX.Y.Z...HEAD`. Leave `CITATION.cff` at the released version.
4. Close the release issue.

## If something goes wrong

- A failed `release.yml` before `publish` leaves no release. Fix the problem on `main`,
  then release the next patch version; do not move or re-push a published tag.
- If the tag itself is wrong and nothing was published, delete the tag locally and on the
  remote, fix, and tag again. Once a release is published, a tag is never moved.
- A release with a bad tarball or bad notes: edit the notes from `CHANGELOG.md` or upload a
  corrected asset only if the source is unchanged; otherwise release `X.Y.Z+1`.

## Notes

- The tarball is a `git archive` of the tagged commit. It has no `.git` directory, so a
  binary built from it prints `unknown` as its commit hash.
- Release notes are never generated from `git log`: commit messages are not curated.
- The label `run-long-tests` must exist in the repository for the long tier to be
  requested on a pull request, and the label `perf: slower` for `perf.yml`.

## Repository settings (maintainers)

Branch protection for `main` and `release/*`: require a pull request, require the check
**CI result** (`ci-ok` in `ci.yml`) and, for documentation changes, the docs build;
require branches to be up to date; no force pushes or deletions; apply the rules to
administrators as well.
