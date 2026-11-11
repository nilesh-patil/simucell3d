(ex-monolayer-multilayer)=
# Monolayer and multilayer epithelia

Run the vesicle of {doc}`vesicle` with weak and with strong apical surface tension. Runser,
Vetter & Iber (2024) use this setup to show that the layering of the epithelium is governed
mainly by cortical tension: weak tension keeps a monolayer, strong tension leads to
stratification (Fig. 3b,c of the paper; {doc}`/citing`). The two parameter files are meant to
give one run on each side of that transition, according to their header comments.

**Files**: `parameters/benchmarks/fig3e_monolayer.xml`,
`parameters/benchmarks/fig3e_multilayer.xml`, `data/input_meshes/fig_3_vesicle.vtk` (432
epithelial cells, one ECM surface, one lumen).
**Compile-time configuration**: the defaults (`POLARIZATION_MODE_INDEX 1`,
`CONTACT_MODEL_INDEX 2`, `DYNAMIC_MODEL_INDEX 0`).
**Length**: long. $10^{-2}$ s of simulated time at a time step of $10^{-7}$ s ($10^5$
iterations) for each file.

## The parameter files

The two files are identical except for the output folder and one value, the surface tension of
the apical faces of the epithelial cells:

| File | Apical $\gamma$ | Lateral $\gamma$ | Basal $\gamma$ | Output folder |
|---|---|---|---|---|
| `fig3e_monolayer.xml` | $10^{-5}$ N/m | $4\times10^{-4}$ N/m | $10^{-3}$ N/m | `simulation_results/nature_fig3e/monolayer` |
| `fig3e_multilayer.xml` | $10^{-3}$ N/m | $4\times10^{-4}$ N/m | $10^{-3}$ N/m | `simulation_results/nature_fig3e/multilayer` |

`fig3e_multilayer.xml` has the same parameters as `parameters/core/parameters_vesicle.xml`,
except for a sampling period of $10^{-4}$ s (100 pairs of mesh files instead of $10^4$). The
epithelial cells grow at $10^{-12}$ m$^3$/s and do not divide.

:::{note}
The paper expresses surface tension in the non-dimensional form
$\tilde\gamma = \gamma / (K l)$, with $K$ the bulk modulus and $l = \langle V(t=0) \rangle^{1/3}$
the cube root of the mean initial cell volume, and reports the transition between
$\tilde\gamma = 0.02$ and $0.10$ (Fig. 3e). For the 432 epithelial cells of `fig_3_vesicle.vtk`
the mean initial volume is about $1.74\times10^{-16}$ m$^3$, so $l \approx 5.6\times10^{-6}$ m, and
with $K = 2.5\times10^{3}$ Pa:

| Face type | $\gamma$ (N/m) | $\tilde\gamma$ |
|---|---|---|
| apical, `fig3e_monolayer.xml` | $10^{-5}$ | $7\times10^{-4}$ |
| apical, `fig3e_multilayer.xml` | $10^{-3}$ | 0.072 |
| lateral | $4\times10^{-4}$ | 0.029 |
| basal | $10^{-3}$ | 0.072 |

The paper varies one surface tension for the whole cell, whereas these files vary only the apical
tension, and the apical value of the monolayer file lies below the range explored in the paper.
The two files therefore illustrate the transition but do not reproduce the Fig. 3e sweep point
for point, and they have not been run to completion with release 2.1.
The publisher correction changes a label of Fig. 3c to $\tilde\gamma = 0.16$ ({doc}`/citing`).
:::

:::{important}
The paper's figures were produced with release 1.0. Results of 2.1 differ
({doc}`/whats_new/v2.1`), so the layer count of a 2.1 run is not expected to match the published
data point for point.
:::

## Run

From the `build/` directory:

```console
$ ./simucell3d ../parameters/benchmarks/fig3e_monolayer.xml
$ ./simucell3d ../parameters/benchmarks/fig3e_multilayer.xml
```

Each run deletes and recreates its own output folder, so the two runs do not overwrite each
other.

## What to look at

- Open the last `cell_data/result_*.vtk` of each run in ParaView, colour by `cell_type_id`, and
  apply a *Clip* filter through the centre of the vesicle to see the cross-section of the
  epithelium between the lumen and the ECM.
- Compare the shapes of the epithelial cells: the paper measures cell shape by the sphericity
  $\Psi = \pi^{1/3} (6V)^{2/3} / A$ (Fig. 3f), which can be computed from the `cell_volume` and
  `cell_area` arrays, or from the `volume` and `area` columns of `simulation_statistics.csv`.

:::{seealso}
- {doc}`vesicle`: the same tissue with the default parameters.
- {doc}`/user_guide/02_cell_mechanics`: surface tension and the other mechanical parameters.
:::
