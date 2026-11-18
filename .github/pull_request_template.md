<!-- Describe the change in a few sentences: what it changes and why. Link the issue it fixes. -->

## Checklist

- [ ] The change is described in the top `## [X.Y.Z] - unreleased` section of `CHANGELOG.md`
      (user-visible changes only), or this pull request does not need an entry.
- [ ] Tests: new or changed behaviour is covered by a ctest test (`unit`, `integration`, or
      `long` label; see `test/test_labels.cmake`), or the reason there is none is given below.
- [ ] `ctest -L "unit|integration"` passes locally in a Release build.
- [ ] Documentation under `doc/` is updated if a parameter, command-line flag, output file or
      behaviour visible to users changed.
- [ ] For changes to `src/` or `include/` that affect results or stability: a maintainer has
      considered adding the label `run-long-tests`.

## Results

<!-- For changes that alter simulation results or performance: what was run (parameter file,
threads, machine) and what changed. Performance numbers from perf.yml are informational. -->
