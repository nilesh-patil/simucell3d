(about)=
# About SimuCell3D

SimuCell3D was created by Steve Runser in the Iber Lab (ETH Zurich). It was developed in the
group of Dagmar Iber at the Department of Biosystems Science and Engineering (D-BSSE) of ETH
Zurich and the Swiss Institute of Bioinformatics (SIB) in Basel. The article lists the author
contributions as: concept, Roman Vetter and Dagmar Iber; model and algorithm development, Steve
Runser and Roman Vetter; implementation, numerical simulations and figures, Steve Runser. The
software and its applications are described in Runser, Vetter & Iber (2024), *Nature
Computational Science* ({doc}`citing`). The project was called DCM before release 2.0.

## Releases

Release 1.0 is the code published with the article, distributed from the
[Iber Lab repository](https://git.bsse.ethz.ch/iber/Publications/2024_runser_simucell3d). Two
snapshots of it exist, and they differ:

- The Zenodo archive cited by the article,
  [doi:10.5281/zenodo.10796908](https://doi.org/10.5281/zenodo.10796908) (published 2024-03-08),
  contains commit `5342bf8`. Its default contact model is 1 (node-node coupling).
- The tag `v1.0` of this repository is commit `38af451` (2024-06-26), a later commit by the
  same author and the head of the Iber Lab repository. It adds the input meshes and parameter
  files of Figs. 1 and 3 and makes contact model 2 (face-face coupling) the default. The source
  files in `src/` are the same as in the archive.

Which contact model produced each published figure is not documented
({doc}`whats_new/v1.0`).

Releases 2.0 and 2.1 are maintenance releases of SimuCell3D, developed by Nilesh Patil at
<https://github.com/nilesh-patil/simucell3d>. They keep the published model but change its
implementation (bug fixes, bending-force guards), add safeguards that remove cells by criteria
that are not part of the paper, and improve parallel performance. Because these changes alter
simulation results, the published figures should be compared with release 1.0
({doc}`whats_new/index`). The 2.x releases have not been reviewed or endorsed by the authors of
the article, the Iber Lab or ETH Zurich.

## Funding

The article states: "This work was partially funded by SNF Sinergia grant CRSII5_170930."

## Licence

SimuCell3D is distributed under the BSD 3-Clause License. The
[`LICENSE`](https://github.com/nilesh-patil/simucell3d/blob/main/LICENSE) file of the repository
is the licence text of release 1.0, unchanged; it names no copyright holder. The changes made
since release 1.0 are copyright (c) 2026 Nilesh Patil and are released under the same licence,
as stated in the [`NOTICE`](https://github.com/nilesh-patil/simucell3d/blob/main/NOTICE) file.
The libraries in `lib/` (tinyxml2, delaunator, pybind11) keep their own licences.

## Contact

Questions and bug reports about the 2.x releases are handled through the
[issue tracker](https://github.com/nilesh-patil/simucell3d/issues) of the repository;
{doc}`/developers/contributing` describes how to contribute.

2.x maintenance: Nilesh Patil.
