# Reference outputs for the paper figures

This directory is reserved for reference simulation outputs ("golden masters") used to check
that a build still reproduces selected figures of Runser, Vetter & Iber (2024),
*Nature Computational Science*, <https://doi.org/10.1038/s43588-024-00620-9>.

It contains no reference outputs yet. VTK files added here are stored with Git LFS
(`.gitattributes`).

The parameter files for the Fig. 3e monolayer and multilayer cases are
`parameters/benchmarks/fig3e_monolayer.xml` and `parameters/benchmarks/fig3e_multilayer.xml`;
`scripts/benchmarking/scientific/reproduce_fig3e.sh` runs both. Results of 2.x differ from 1.0,
the version used for the paper (see `CHANGELOG.md`), so a reference output must name the version
and commit that produced it.

See the testing and benchmarking pages of the developer guide (`doc/developers/testing.md`,
`doc/developers/benchmarking.md`).
