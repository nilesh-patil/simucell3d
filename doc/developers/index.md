(dev-index)=
# Developer guide

This guide is for people who change SimuCell3D: fix bugs, add model features, improve
performance, extend the tests or the documentation, or prepare a release. It assumes that the
reader can build and run the simulator ({doc}`/install`, {doc}`/getting_started`) and is familiar
with the model ({doc}`/user_guide/index`).

{ref}`dev-contributing`
: How to report a problem, branch, write commit messages, and what a pull request needs.

{ref}`dev-setup`
: Build configurations for development: Debug, sanitizers, the oldest supported compiler,
  Python bindings, editor support.

{ref}`dev-architecture`
: The source layout, the main simulation loop step by step, data ownership and the extension
  points for new cell classes, contact models and output columns.

{ref}`dev-testing`
: The test suite: how tests are written and registered, the test tiers, and how to add a test.

{ref}`dev-ci`
: The GitHub Actions workflows: what runs on a pull request, on `main`, on release branches and
  tags.

{ref}`dev-benchmarking`
: How to measure and compare performance, the benchmark driver and the plotting tools, and how
  the November 2026 cross-version benchmark was run.

{ref}`dev-performance-internals`
: Thread placement, loop scheduling, where the speed-ups since 1.0 came from, memory use,
  synchronisation, the performance monitor and profiling.

{ref}`dev-debugging`
: Reading the logs, the failure signatures seen in long runs and their causes, sanitizer builds,
  gdb, and debugging the Python bindings.

{ref}`dev-coding-style`
: Formatting, naming, C++ usage and comment conventions.

{ref}`dev-writing-docs`
: Building this site, the published versions, where content goes, theme-aware figures and the
  writing conventions.

{ref}`dev-release`
: The release checklist, from the version bump to the published release and documentation.

```{toctree}
:maxdepth: 1
:hidden:

contributing
development_setup
architecture
testing
continuous_integration
benchmarking
performance_internals
debugging
coding_style
writing_documentation
release_process
```
