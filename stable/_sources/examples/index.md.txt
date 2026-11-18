(examples)=
# Examples

Each example runs one of the parameter files shipped in `parameters/`, states the compile-time
configuration it needs, and describes what to look at in the output. All commands are run from
the `build/` directory of a Release build ({doc}`/install`), unless stated otherwise.

The length of a run depends on the number of iterations (simulated time divided by the time
step), the number of cells and the resolution of their meshes. *Short* runs take minutes, *long*
runs hours to days on a workstation.

::::{grid} 1 2 2 2
:gutter: 3

:::{grid-item-card} Short vesicle run
:link: vesicle_quick
:link-type: doc

A short run of the 434-cell vesicle; a check that the build works. Short.
:::

:::{grid-item-card} Epithelial vesicle with lumen and ECM
:link: vesicle
:link-type: doc

The initial tissue of Fig. 3 of the paper, relaxed for $10^5$ iterations. Long.
:::

:::{grid-item-card} Monolayer and multilayer epithelia
:link: monolayer_multilayer
:link-type: doc

Weak and strong apical surface tension on the same vesicle (Fig. 3 of the paper). Long.
:::

:::{grid-item-card} Epithelial sheet
:link: sheet
:link-type: doc

A flat sheet of 118 cells. Long.
:::

:::{grid-item-card} Epithelial tube
:link: tube
:link-type: doc

A tube of 192 cells. Long.
:::

:::{grid-item-card} Growing cell: full and overdamped dynamics
:link: single_cell_dynamic_overdamped
:link-type: doc

The two default parameter files of release 1.0; each needs its own build. Long.
:::

:::{grid-item-card} Tissue growth from a single cell
:link: growth_from_one_cell
:link-type: doc

The exponential-growth benchmark of Fig. 1e of the paper, to tens of thousands of cells. Very long.
:::

:::{grid-item-card} Batch runs from Python
:link: python_batch_runs
:link-type: doc

A parameter sweep with the Python module. Short to long.
:::
::::

```{toctree}
:hidden:

vesicle_quick
vesicle
monolayer_multilayer
sheet
tube
single_cell_dynamic_overdamped
growth_from_one_cell
python_batch_runs
```
