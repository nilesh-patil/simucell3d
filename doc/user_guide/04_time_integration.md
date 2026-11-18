(ug-time-integration)=
# Time integration

The nodes of the cell meshes move under the sum of the internal and contact forces. SimuCell3D
solves either the full equations of motion with viscous damping or their overdamped limit,
selected at compile time with `DYNAMIC_MODEL_INDEX`. This chapter states both models and their
integration schemes, explains how coupled nodes are advanced, and gives guidance on the time
step and the output intervals.

## Equations of motion

Following Runser, Vetter & Iber (2024), Methods, each node $i$ of a cell obeys

$$
m\,\ddot{\mathbf r}_i + \zeta\,\dot{\mathbf r}_i = \mathbf f_i ,
$$ (eq-motion)

where $\mathbf f_i$ is the total force on the node ({doc}`02_cell_mechanics`,
{doc}`03_contact`), $\zeta$ the damping coefficient (`damping_coefficient`) and $m$ the node
mass. The mass of a cell, $\rho V$, is shared equally by its nodes:

$$
m = \frac{\rho\, V}{N_n},
$$ (eq-node-mass)

with $\rho$ the mass density of the cell type (`cell_mass_density`), $V$ the current volume
and $N_n$ the current number of nodes. The damping is a drag against a fixed background; there
is no friction between cells.

## Dynamic model (index 0, default)

The full equations are integrated for the positions $\mathbf r_i$ and momenta
$\mathbf p_i = m \dot{\mathbf r}_i$ with the semi-implicit Euler scheme of the paper:

$$
\mathbf p_i \leftarrow \mathbf p_i + \Delta t\left(\mathbf f_i - \frac{\zeta}{m}\,\mathbf p_i\right),
\qquad
\mathbf r_i \leftarrow \mathbf r_i + \Delta t\, \frac{\mathbf p_i}{m} .
$$ (eq-semi-implicit-euler)

The momentum is updated first and the new momentum moves the node. The scheme is first order
and resolves elastic oscillations, so it suits phenomena on short time scales. The dynamic
model is the one used for the figures of the paper, but with the default contact model 2 the
1.0 code did not implement this scheme: it moved every node with its old momentum, a forward
Euler step (see the note at the end of [Coupled nodes](#coupled-nodes)).

When the mesh refiner splits an edge, the two end nodes each give one third of their momentum
to the new node; when it collapses an edge, the new node receives the sum of the two momenta
({doc}`05_mesh_refinement`).

## Overdamped model (index 1)

Neglecting inertia ($m\ddot{\mathbf r}_i = 0$) gives a first-order equation, integrated with
the forward Euler scheme:

$$
\mathbf r_i \leftarrow \mathbf r_i + \frac{\Delta t}{\zeta}\, \mathbf f_i .
$$ (eq-forward-euler)

This model suits systems that relax viscously towards a quasi-static equilibrium. The mass
density has no effect on the motion. The damping coefficient must be strictly positive; the
parameter reader accepts 0, which then divides by zero.

:::{note}
The source comments of the time integrator mention a third scheme (index 2, improved Euler).
It is not implemented: with contact model 2 the build fails, and with contact models 0 and 1
the nodes do not move. Use index 0 or 1.
:::

## Coupled nodes

With contact models 1 and 2, nodes of adjacent epithelial cells can be coupled
({doc}`03_contact`). A coupled group (a node and its partners) is first moved to its mean
position during contact detection and is then advanced as one node: the forces, momenta and
masses of the group are averaged, and every node of the group receives the same update. In the
dynamic model this reads

$$
\bar{\mathbf p} \leftarrow \bar{\mathbf p} + \Delta t\left(\bar{\mathbf f} -
  \frac{\zeta}{\bar m}\,\bar{\mathbf p}\right),
\qquad
\mathbf r \leftarrow \mathbf r + \Delta t\, \frac{\bar{\mathbf p}}{\bar m}
\quad \text{for every node of the group,}
$$

where the bars denote averages over the group; in the overdamped model the group moves by
$\Delta t\, \bar{\mathbf f} / \zeta$. With contact model 2 this update runs serially; with
models 0 and 1 the integration runs in parallel over cells.

:::{versionchanged} 2.0
With the dynamic model and contact model 2 (the default build), 1.0 moved *every* node with its
old momentum, coupled or not: the integrator branch of contact model 2 also handled nodes
without a partner, and moved all nodes before using the updated momentum. That is an explicit
(forward) Euler step, not the semi-implicit scheme of the paper's Methods. 2.0 and 2.1 update
the momentum first and move every node, and every coupled group, with the new momentum, as in
{eq}`eq-semi-implicit-euler`. Contact models 0 and 1 were already semi-implicit in 1.0.

For a mode of stiffness $k$, one forward Euler step damps it as if the damping coefficient were
$\zeta - k\,\Delta t$ instead of $\zeta$. Modes stiffer than $\zeta/\Delta t$ were therefore
anti-damped in 1.0: with $\Delta t = 10^{-7}$ s and $\zeta = 2.5	imes10^{-10}$ kg/s, that is
$k > 2.5	imes10^{-3}$ N/m. The relaxation of every cell surface, not only of cell-cell
contacts, changes compared with 1.0, so 2.1 does not reproduce the dynamics of results
computed with 1.0 in this configuration, which is the default build.
:::

Cells of a {term}`cell class` that does not move (ECM and static cells) are skipped by the
integrator; their nodes keep their positions for the whole run.

## Choosing the time step

The time step $\Delta t$ (`time_step`) is fixed for the whole run; there is no adaptive step
control. Both schemes are explicit in the forces and are stable only for small enough steps.
For a single node with an effective stiffness $k$ (for example $\xi A_f$ for a contact spring),
standard linear stability estimates are

$$
\Delta t < 2\sqrt{\frac{m}{k}} \ \text{ and } \ \Delta t < \frac{2m}{\zeta}
\quad \text{(dynamic)}, \qquad
\Delta t < \frac{2\zeta}{k} \quad \text{(overdamped)} .
$$

The node mass falls with the mesh resolution (smaller $l_{\min}$ means more nodes per cell), so
a finer mesh needs a smaller time step. In practice, check a configuration by halving the time
step and comparing the results.

All shipped parameter files use $\Delta t = 10^{-7}$ s, the value of the paper's Table 2, except
`parameters/core/parameters_vesicle_quick.xml` ($5\times10^{-7}$ s, a short smoke test). The
damping coefficients are $2.5\times10^{-10}$ to $2\times10^{-9}$ kg/s with the dynamic model and
$3\times10^{-9}$ kg/s in `parameters_default_overdamped.xml`; Table 2 gives
$2.5\times10^{-10}$ kg/s (dynamic) and $3\times10^{-9}$ kg/s (overdamped).

A diverging simulation shows up as cells that stretch to many times their size or nodes with
non-finite coordinates. 2.1 removes such cells and logs them ({doc}`09_safeguards`); frequent
removals are a sign that the time step is too large or a parameter is out of range. The
CHANGELOG also lists rare single-node blow-ups of unknown cause as a known issue.

## Time step, sampling period and other intervals

| Interval | Set by | What happens |
|---|---|---|
| every iteration | `time_step` | refinement, contact, forces, integration, removal of dead cells |
| every 5 iterations | fixed | division of the cells that are ready to divide |
| every 50 iterations | fixed | one row per cell appended to the statistics CSV |
| every 100 iterations | fixed | one row appended to the diagnostics CSV |
| every `sampling_period` | parameter file | cell and face meshes written (`result_<n>.vtk`) |

The number of iterations is `simulation_duration` / `time_step`. The sampling period must be at
least the time step; the mesh file number is $n = \lfloor t / T_s \rfloor + 1$, where $T_s$ is the
sampling period, so the first file is `result_1.vtk`. For example,
`parameters/core/parameters_vesicle.xml` runs $10^{-2}/10^{-7} = 10^5$ iterations and writes
meshes every $10^{-6}$ s, about $10^4$ files of each kind. See {doc}`14_output_files` for the
files.

## Reproducibility

Runs with more than one thread are not bit-reproducible: two runs with the same input can give
slightly different results, because the order in which threads add forces and select coupling
partners varies. In addition, several random generators are seeded from the system clock, so
runs that use them differ even on one thread:

- the growth rate and division volume of each cell, when their standard deviation is not zero
  ({doc}`06_growth_division_removal`);
- the point sampling that triangulates the division interface, whenever a cell divides,
  whatever the standard deviations;
- the surface sampling of the initial triangulation (`perform_initial_triangulation`).

A run on one thread is repeatable only when none of these is used, which excludes any run in
which cells divide. Compare runs statistically, not iteration by iteration.

:::{note}
The `kinetic_energy` value written to the output is $\sum p_i^2/(2m)$ with the dynamic model.
With the overdamped model it is computed from $\mathbf f_i$ and $\zeta$ and is not a kinetic
energy in the physical sense.
:::

## Parameters

| XML tag | Symbol | Section | Unit | Meaning |
|---|---|---|---|---|
| `time_step` | $\Delta t$ | `<numerical_parameters>` | s | Time step; $> 0$ |
| `simulation_duration` | | `<numerical_parameters>` | s | Simulated time; $> 0$ |
| `sampling_period` | $T_s$ | `<numerical_parameters>` | s | Interval between mesh files; $\ge \Delta t$ |
| `damping_coefficient` | $\zeta$ | `<numerical_parameters>` | kg/s | Viscous damping |
| `cell_mass_density` | $\rho$ | `<cell_type>` | kg/m³ | Sets the node mass {eq}`eq-node-mass` (dynamic model only) |

:::{seealso}
- {doc}`10_compile_time_configuration` to switch between the dynamic and overdamped models.
- {doc}`/examples/single_cell_dynamic_overdamped` for one cell under both models.
- {doc}`/api/time_integration` for the `time_integration_scheme` class.
:::
