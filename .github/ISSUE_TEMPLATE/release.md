---
name: Release
about: Checklist for releasing a new version (maintainers). The procedure is in RELEASING.md.
title: "Release X.Y.Z"
labels: release
---

Procedure and commands: [RELEASING.md](https://github.com/nilesh-patil/simucell3d/blob/main/RELEASING.md).

## Before the release pull request

- [ ] CI on `main` (or `release/X.Y`) is green: <!-- link to the run -->
- [ ] No open issue labelled `release-blocker`.
- [ ] Long tier green on the candidate commit (`long.yml`): <!-- link to the run -->
- [ ] Growth run to 30,000 cells on the reference machine (minor releases; patch releases
      that change `src/` or `include/`). Results:

| Item | This release | Previous release |
|---|---|---|
| Commit | | |
| Machine type, cores, threads | | |
| Cells reached | | |
| Simulated time reached [s] | | |
| Wall time [h] | | |
| Exit status or abort message | | |
| `Removing numerically unstable cell` lines | | |
| `Cell division failed for cell` lines | | |

- [ ] Documentation builds without warnings (`make -C doc html-strict`); release-history
      page for X.Y.Z reviewed.

## Release pull request (`release/X.Y.Z-prep`)

- [ ] `CMakeLists.txt`: `project(SimuCell3D VERSION X.Y.Z)`.
- [ ] `CHANGELOG.md`: heading dated `## [X.Y.Z] - YYYY-MM-DD`; link `.../compare/vPREV...vX.Y.Z`.
- [ ] `CITATION.cff`: `version: X.Y.Z`, `date-released: 'YYYY-MM-DD'`.
- [ ] `python3 scripts/ci/check_version.py --tag vX.Y.Z` passes locally.
- [ ] Pull request merged with CI green: <!-- link -->

## Tag and publish

- [ ] (optional) `vX.Y.Z-rc1` tagged; pre-release tarball built and tested on a clean machine.
- [ ] `vX.Y.Z` tagged on the merge commit and pushed.
- [ ] `release.yml` finished: <!-- link to the run -->
- [ ] `docs.yml` finished: <!-- link to the run -->

## After the release

- [ ] Release page: notes, `simucell3d-X.Y.Z.tar.gz`, `SHA256SUMS` (`sha256sum -c SHA256SUMS` passes).
- [ ] Documentation at `/X.Y/` and `/stable/`; version switcher lists X.Y.
- [ ] `main` moved to the next version (`## [X.Y+1.0] - unreleased`, CMake version bumped).
