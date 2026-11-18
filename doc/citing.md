(citing)=
# Citing SimuCell3D

If SimuCell3D contributes to a publication, cite the article that describes it:

> Runser, S., Vetter, R. & Iber, D. SimuCell3D: three-dimensional simulation of tissue
> mechanics with cell polarization. *Nat. Comput. Sci.* **4**, 299-309 (2024).
> <https://doi.org/10.1038/s43588-024-00620-9>

```bibtex
@article{runser2024simucell3d,
  author  = {Runser, Steve and Vetter, Roman and Iber, Dagmar},
  title   = {{SimuCell3D}: three-dimensional simulation of tissue mechanics with cell polarization},
  journal = {Nature Computational Science},
  volume  = {4},
  pages   = {299--309},
  year    = {2024},
  doi     = {10.1038/s43588-024-00620-9}
}
```

The article is open access (CC BY 4.0). A publisher correction,
*Nat. Comput. Sci.* **4**, 379 (2024),
[doi:10.1038/s43588-024-00635-2](https://doi.org/10.1038/s43588-024-00635-2), corrects a label
in Fig. 3c ($\tilde\gamma = 0.16$, not 0.016).

## Software version

State the version used, because results differ between releases ({doc}`whats_new/index`). The
program prints it with `simucell3d --version`, together with the git commit it was built from.

- **Release 1.0**, the code published with the article, is archived on Zenodo:
  Runser, S. Source code of SimuCell3D. *Zenodo* (2024).
  [doi:10.5281/zenodo.10796908](https://doi.org/10.5281/zenodo.10796908).
  The archive contains commit `5342bf8`, whose default contact model is 1. The tag `v1.0` of this
  repository is a later commit (`38af451`) that adds the input files of Figs. 1 and 3 and makes
  contact model 2 the default ({doc}`whats_new/v1.0`). If you ran the tag rather than the
  archive, say so, together with the compile-time configuration.
- **Releases 2.x** are maintenance releases by Nilesh Patil. They are not reviewed or endorsed by
  the authors of the article, the Iber Lab or ETH Zurich. Cite the article above for the model,
  and give the 2.x version you used. The repository file
  [`CITATION.cff`](https://github.com/nilesh-patil/simucell3d/blob/main/CITATION.cff) states the
  version, lists the 2.x maintainer as software author, names the article as the preferred
  citation and lists the 1.0 archive as a reference. GitHub shows it as "Cite this repository".

## Data

The raw data generated for the article are archived on Zenodo:
Runser, S., Vetter, R. & Iber, D. Raw data generated for the article "SimuCell3D: 3D Simulation of
Tissue Mechanics with Cell Polarization". *Zenodo* (2024).
[doi:10.5281/zenodo.10797576](https://doi.org/10.5281/zenodo.10797576).
