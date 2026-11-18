---
html_theme.sidebar_secondary.remove: true
---

(index)=
# SimuCell3D

SimuCell3D simulates the mechanics of three-dimensional tissues at subcellular resolution.
Each cell is a {term}`deformable cell model (DCM)`: a closed, triangulated surface whose shape
follows from its volume, surface tension, area elasticity and bending energy, and from
adhesion and repulsion at contacts with neighbouring cells. Faces of a cell carry a
{term}`face type` (for example apical, lateral or basal), so that cell polarization can be
modelled with face-dependent mechanical properties. Besides epithelial cells, a tissue can
contain nuclei, extracellular matrix (ECM), lumens and static obstacles. Cells grow, divide
and are removed during a run. The simulator is a C++17 command-line program parallelized with
OpenMP; an optional Python module drives the same solver.

SimuCell3D was created by Steve Runser in the Iber Lab (D-BSSE, ETH Zurich, and the Swiss
Institute of Bioinformatics) and is described in Runser, Vetter & Iber (2024), *Nature
Computational Science* ({doc}`citing`); the concept is by Roman Vetter and Dagmar Iber
({doc}`about`). This documentation covers the 2.x maintenance releases, maintained by Nilesh
Patil and not reviewed or endorsed by the authors of the article, the Iber Lab or ETH Zurich.

:::{important}
Results differ from 1.0. A simulation run with SimuCell3D 2.1 does not reproduce the numbers
produced by 1.0, the code published with the paper. Several 1.0 bugs changed the physics, and
fixing them changes trajectories; 2.1 also adds safeguards that remove or repair cells that
1.0 would have kept; a run can lose many cells this way and still exit with status 0
({doc}`user_guide/09_safeguards`). To compare with the published figures, run release 1.0; note
that the tag `v1.0` and the Zenodo archive cited by the paper differ in their default contact
model ({doc}`whats_new/v1.0`). See {doc}`whats_new/v2.1`.
:::

:::{figure} /images/tissue_topologies.png
:alt: Rendered SimuCell3D tissues of several topologies, with individual cells shown as
  closed triangulated surfaces.
:width: 80%

Tissue topologies simulated with SimuCell3D.
:::

::::{grid} 1 2 2 2
:gutter: 3

:::{grid-item-card} Install
:link: install
:link-type: doc

Build `simucell3d` from source with CMake, or run it in Docker. Optional Python bindings.
:::

:::{grid-item-card} Getting started
:link: getting_started
:link-type: doc

Run a first simulation, open its output in ParaView and change a parameter.
:::

:::{grid-item-card} User guide
:link: user_guide/index
:link-type: doc

The model, its parameters and compile-time options, input meshes, output files and tools.
:::

:::{grid-item-card} API reference
:link: api/index
:link-type: doc

The C++ classes of the solver and the Python module.
:::
::::

## Citing

If SimuCell3D contributes to a publication, cite the paper:

> Runser, S., Vetter, R. & Iber, D. SimuCell3D: three-dimensional simulation of tissue
> mechanics with cell polarization. *Nat. Comput. Sci.* **4**, 299-309 (2024).
> <https://doi.org/10.1038/s43588-024-00620-9>

BibTeX and the software citation are on the {doc}`citing` page.

```{toctree}
:hidden:
:maxdepth: 2

install
getting_started
user_guide/index
examples/index
api/index
developers/index
whats_new/index
glossary
faq
citing
about
```
