(about)=
# About SimuCell3D

SimuCell3D was created by Steve Runser in the Iber Lab (ETH Zurich). The model and its
algorithms were developed by Steve Runser and Roman Vetter, and the program was implemented by
Steve Runser, in the group of Dagmar Iber at the Department of Biosystems Science and
Engineering (D-BSSE) of ETH Zurich and the Swiss Institute of Bioinformatics (SIB) in Basel. The
software and its applications are described in Runser, Vetter & Iber (2024), *Nature
Computational Science* ({doc}`citing`). The project was called DCM before release 2.0.

## Releases

Release 1.0 is the code published with the article, originally distributed from the
[Iber Lab repository](https://git.bsse.ethz.ch/iber/Publications/2024_runser_simucell3d) and
archived on Zenodo ([doi:10.5281/zenodo.10796908](https://doi.org/10.5281/zenodo.10796908)).
Releases 2.0 and 2.1 are maintenance releases of SimuCell3D, developed at
<https://github.com/nilesh-patil/simucell3d>. They fix bugs in the 1.0 code, add safeguards for
long simulations and improve parallel performance; they do not change the model. Because the
fixes change simulation results, the published figures should be compared with release 1.0
({doc}`whats_new/index`).

## Funding

The article states: "This work was partially funded by SNF Sinergia grant CRSII5_170930."

## Licence

SimuCell3D is distributed under the BSD 3-Clause License, copyright (c) 2024, ETH Zurich (Steve
Runser and the Iber Lab). The full text is in the
[`LICENSE`](https://github.com/nilesh-patil/simucell3d/blob/main/LICENSE) file of the repository.
The libraries in `lib/` (tinyxml2, delaunator, pybind11) keep their own licences.

## Contact

Questions and bug reports are handled through the
[issue tracker](https://github.com/nilesh-patil/simucell3d/issues) of the repository;
{doc}`/developers/contributing` describes how to contribute.

2.x maintenance: Nilesh Patil.
