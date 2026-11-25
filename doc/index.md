---
html_theme.sidebar_secondary.remove: true
---

(index)=
# SimuCell3D

:::{rst-class} landing-lead
SimuCell3D simulates the mechanics of three-dimensional tissues at subcellular resolution, with
every cell a deformable, triangulated surface.
:::

<!-- Hero figure. To use a new render, change the path here and "og_image" in conf.py. -->
```{figure} /images/hero_tissues.png
---
alt: Three simulated tissues rendered in 3D. A hollow epithelial vesicle cut open, an
  epithelial tube cut open to show its packed cells, and a spherical aggregate of about two
  thousand cells.
class: dark-light
figclass: landing-hero
width: 100%
---

Tissues simulated with SimuCell3D 2.1.0: a vesicle around a lumen, an epithelial tube, and an
aggregate grown from one cell (2,155 cells). In the vesicle and the tube, orange faces are free
(apical) surfaces and blue faces are cell-cell contacts; the aggregate is unpolarized, so all its
faces are orange. Panels are not to the same scale.
```

## Quick start

1. Build `simucell3d` in Release mode on Linux ({doc}`install` lists the requirements and the
   Docker alternative).

   ```console
   $ git clone https://github.com/nilesh-patil/simucell3d.git
   $ cd simucell3d
   $ mkdir build
   $ cd build
   $ cmake -DCMAKE_BUILD_TYPE=Release ..
   $ make -j4
   ```

2. Run the short vesicle simulation from `build/` (200 iterations, a few minutes). Its output
   folder, `simulation_results/simulation_vesicle/`, is deleted at the start of every run.

   ```console
   $ ./simucell3d ../parameters/core/parameters_vesicle_quick.xml
   ```

3. Open the output in [ParaView](https://www.paraview.org/download/): *File > Open*, go to
   `simulation_results/simulation_vesicle/cell_data/`, select the group `result_..vtk`, click
   *OK* and then *Apply*. {doc}`getting_started` explains the output and changes a parameter.

## Documentation

::::{grid} 1 2 3 3
:gutter: 3
:class-container: landing-cards

:::{grid-item-card} {fas}`play` Get started
:link: getting_started
:link-type: doc

Run a first simulation, read its output, view it in ParaView and change a parameter.
:::

:::{grid-item-card} {fas}`book-open` User guide
:link: user_guide/index
:link-type: doc

The model, every parameter and compile-time option, input meshes, output files and tools.
:::

:::{grid-item-card} {fas}`flask` Examples
:link: examples/index
:link-type: doc

Complete simulations of vesicles, tubes, sheets and growing tissues, with their parameter files.
:::

:::{grid-item-card} {fas}`gauge-high` Benchmarks
:link: benchmarks
:link-type: ref

Which cases complete in 1.0, 2.0 and 2.1.0, wall time, the stable time step and the limits of
the model, measured in November 2026.
:::

:::{grid-item-card} {fas}`code` API reference
:link: api/index
:link-type: doc

The C++ classes of the solver and the Python module.
:::

:::{grid-item-card} {fas}`screwdriver-wrench` Developer guide
:link: developers/index
:link-type: doc

Contributing, development builds, the code architecture, tests and CI.
:::
::::

:::{note}
Results of 2.1 differ from 1.0 because 1.0 defects that changed the physics are fixed
({doc}`whats_new/v2.1`). Safeguards can remove cells, and a run can still exit with status 0
({doc}`user_guide/09_safeguards`). To compare with the published figures, use release 1.0
({doc}`whats_new/v1.0`).
:::

## About this software

SimuCell3D was created by Steve Runser in the Iber Lab (D-BSSE, ETH Zurich, and the Swiss
Institute of Bioinformatics) and is described in Runser, Vetter & Iber (2024), *Nature
Computational Science* ({doc}`citing`); the concept is by Roman Vetter and Dagmar Iber
({doc}`about`). This documentation covers the 2.x maintenance releases, maintained by Nilesh
Patil and not reviewed or endorsed by the authors of the article, the Iber Lab or ETH Zurich.

## Citing

If SimuCell3D contributes to a publication, cite the paper:

> Runser, S., Vetter, R. & Iber, D. SimuCell3D: three-dimensional simulation of tissue
> mechanics with cell polarization. *Nat. Comput. Sci.* **4**, 299-309 (2024).
> <https://doi.org/10.1038/s43588-024-00620-9>

BibTeX and the software citation are on the {doc}`citing` page.

```{toctree}
:hidden:
:maxdepth: 2

Install <install>
Get started <getting_started>
User guide <user_guide/index>
Examples <examples/index>
Benchmarks <benchmarks/index>
API <api/index>
Developers <developers/index>
Releases <whats_new/index>
Glossary <glossary>
FAQ <faq>
Citing <citing>
About <about>
```
