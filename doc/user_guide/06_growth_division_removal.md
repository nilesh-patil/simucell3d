(ug-growth-division)=
# Growth, division and cell death

The number and size of cells change during a run. Cells grow by increasing their
{term}`target volume`, epithelial cells divide when they reach a division volume, and cells die when their volume
falls below a minimum (a {term}`minimum-volume death`, part of the model). This chapter describes each rule as
implemented in 2.1, with its parameters, and the divisions that 2.1 refuses.

## Growth

Growth acts on the target volume $V_0$, not on the volume directly. At every iteration, before
the pressure is computed,

$$
V_0 \leftarrow \max\left(V_0 + g\, \Delta t,\ V_{\min}\right),
$$ (eq-growth)

which is the linear growth law $\mathrm dV_0/\mathrm dt = g$ of the paper's Methods, with
$V_{\min}$ (`min_vol`) as a lower bound. The cell's volume then follows $V_0$ through the pressure
term ({doc}`02_cell_mechanics`): a cell whose volume lags behind its target volume has a positive
pressure and expands. A negative $g$ shrinks the cell. Growth applies to every cell class whose
internal forces are computed, that is all classes except the ECM; the shipped files give growth
only to epithelial cells.

The growth rate $g$ is a property of each cell. It is drawn when the cell is created (at start-up
or at its birth by division) from a normal distribution with mean `avg_growth_rate` and standard
deviation `std_growth_rate`, clipped to the mean $\pm 3$ standard deviations. With a standard
deviation of 0 every cell gets the mean. The growth rate is in m³ per second of simulation time;
the shipped files do not define a conversion to biological time.

## Division

Only epithelial cells divide. Every 5 iterations, each epithelial cell whose current volume
$V$ has reached its division volume $V_\mathrm{div}$ is divided. Like the growth rate,
$V_\mathrm{div}$ is drawn per cell from a normal distribution (`avg_division_volume`,
`std_division_volume`), clipped to $\pm 3$ standard deviations; if the standard deviation is 0 or
the mean is `inf`, the mean is used. `avg_division_volume` set to `inf` disables division.

The division of a mother cell proceeds as follows:

1. **Plane.** The cutting plane passes through the area-weighted centroid of the surface and is
   normal to the cell's longest axis: the eigenvector with the largest eigenvalue of the
   covariance matrix of the node positions. This is Hertwig's rule.
2. **Cut.** The points where the plane crosses the edges of the mesh are inserted, and the faces
   they split are re-triangulated.
3. **Interface.** The outline of the cut is a polygon in the plane. It is filled with points by
   Poisson disk sampling at spacing $l_{\min}$ and triangulated with a two-dimensional Delaunay
   triangulation, so that the new wall respects the edge-length bounds and the rest of the surface
   is kept from the mother.
4. **Daughters.** The surface is split into two closed meshes, one on each side of the cut, each
   closed by the triangulated interface. Both are refined by the {term}`local mesh refiner`.
5. **Properties.** Each daughter receives half the target volume of the mother, a new identifier,
   and newly drawn growth rate and division volume. The daughters have the cell type of the mother.

The mother is removed and the two daughters are added to the tissue. Runser, Vetter & Iber (2024)
report that a division takes a few microseconds of computation time.

:::{note}
The paper's Methods allow a randomly oriented division plane as an alternative. The 2.1 code has
only the longest-axis plane.
:::

### Cuts with several loops

If the plane also crosses a thin protrusion of the cell, its intersection with the surface
consists of more than one closed loop.

:::{versionchanged} 2.1
Every loop is traced, the cell is cut along the longest one, and the two daughters are separated
by a flood fill over the surface that does not cross the cut. In 1.0 only one loop was followed,
the division failed, and the cell tried again at every division check (every 5 iterations).
:::

### Refused divisions

The two-dimensional Delaunay triangulation is not constrained by the outline of the interface. If
the outline is not convex, some of its sides can be missing from the triangulation, which would
leave a hole between the interface and the rest of each daughter.

:::{versionchanged} 2.1
Such a division is refused: the mother is left unchanged and divides at a later division check,
with new random interior points. The log line is

```text
Cell division failed for cell <id>: Interface outline not covered by the Delaunay triangulation (<k> of <n> sides missing); division refused
```

An earlier 2.1 development version patched these interfaces instead; the refiner could not repair
the resulting daughters, which were then removed as unstable.
:::

Refusals become more frequent as the tissue grows. In an 8-core run of the reconstructed
paper-like configuration (`parameters/longrun_fast_growth.xml`, not the lab's configuration;
{doc}`/examples/growth_from_one_cell`) from 1 to 150,498 cells, 14% of division attempts were refused (5% below 30,000 cells, 21% between
125,000 and 150,000 cells); a refused mother was refused 1.75 times on average, at most 31 times.
A cell with a fixed shape can be refused every time until its shape changes. Refused divisions
therefore delay some divisions; they do not lose cells. The CHANGELOG lists them as a known issue.
The lab's growth benchmark (contact model 1) refuses more: in the November 2026 benchmark, 2.1.0
refused about 21% of division attempts below 30,000 cells and 27% between 125,000 and 150,000
cells, about 170,000 refusals in a 48-hour run ({ref}`benchmarks-validity`).

The two-dimensional Delaunay step itself has no iteration limit: its edge-flipping loop runs
until the triangulation is legal. Two intermediate development builds hung there in the
November 2026 benchmark; no 2.1.0 run did, but the code is unchanged. A run that stops
printing progress lines while a division check is due may be stuck in this loop.

Other errors during a division (for example a cut that does not split the surface in two) are
logged with the same `Cell division failed for cell <id>:` prefix, and the mother also retries at
the next division check.

(ug-min-volume-death)=
## Cell death at the minimum volume

At the end of every iteration, a cell is deleted if its volume is below its minimum volume:

$$
V < V_{\min}.
$$

This {term}`minimum-volume death` models cell death (apoptosis) in the paper. It applies to every
cell class and is not logged: no log line, no counter, and no column of the diagnostics CSV records it. Because the
target volume never falls below $V_{\min}$ ({eq}`eq-growth`), shrinkage alone brings a cell only
down to about $V_{\min}$; cells die when their neighbours compress them.

These deaths can be numerous. In the lab's growth benchmark, measured with 2.1.0 in
November 2026, 360,009 cells had died by the time 144,000 were alive, 36% of all cells ever born
({ref}`benchmarks-validity`). To count them, use the `cell_id` column of the statistics CSV:
identifiers are given in sequence from 0, so $N_\mathrm{created}$, the largest identifier plus
one, counts every cell ever created, and each division adds two cells, so there were
$(N_\mathrm{created} - N_\mathrm{initial})/2$ divisions. The deaths are then
$N_\mathrm{created} - N_\mathrm{alive} - N_\mathrm{divisions} - N_\mathrm{safeguard}$, where
$N_\mathrm{safeguard}$ counts the safeguard {term}`removals <removal>` in the log
({doc}`09_safeguards`).

## Removal of unstable cells

:::{versionchanged} 2.1
The same step also removes numerically unstable cells, and a cell whose mesh refinement fails is
removed earlier in the iteration. These {term}`removals <removal>` are {term}`safeguards <safeguard>`,
not part of the published model, and unlike deaths they are logged; see {doc}`09_safeguards`.
:::

## Parameters

| XML tag | Symbol | Section | Unit | Meaning |
|---|---|---|---|---|
| `avg_growth_rate` | $\bar g$ | `<cell_type>` | m³/s | Mean growth rate of the target volume |
| `std_growth_rate` | | `<cell_type>` | m³/s | Standard deviation of the growth rate |
| `avg_division_volume` | $\bar V_\mathrm{div}$ | `<cell_type>` | m³ | Mean division volume; `inf` disables division |
| `std_division_volume` | | `<cell_type>` | m³ | Standard deviation of the division volume |
| `min_vol` | $V_{\min}$ | `<cell_type>` | m³ | Volume below which the cell dies; lower bound of $V_0$ |

The paper's Table 2 gives $g = 10^{-11}$ m³/s, $V_\mathrm{div} = 1.4\times10^{-15}$ m³ (called
$V_{\max}$ there) and $V_{\min} = 3.7\times10^{-16}$ m³; `parameters/longrun_fast_growth.xml` uses
these values with standard deviations of 10% of the means.

## Practical guidance

- **Growth rate and time step.** The target volume grows by $g\Delta t$ per iteration. With the
  Table 2 values, a cell takes $(V_\mathrm{div} - V_\mathrm{div}/2)/g = 7\times10^{-5}$ s, or 700
  iterations at a time step of `1e-7` s, from birth to division. The paper describes this rate as
  "pushed to the limit" for its growth benchmark; the volume can lag behind the target volume at
  such rates.
- **Random draws.** The generators are seeded from the wall clock, so with non-zero standard
  deviations two runs never draw the same values. The point sampling that triangulates the division
  interface is also seeded from the clock, so runs in which cells divide differ even with standard
  deviations of 0 and one thread. There is no parameter for a fixed seed, so a growth run cannot be
  repeated bit for bit. In the November 2026 benchmark, repeats of the growth benchmark differed
  from the first iteration on, while there was still one cell, because its initial triangulation
  is sampled the same way ({ref}`benchmarks-reproducibility`).
- **Synchronous divisions.** With standard deviations of 0, cells born together grow at the same
  rate and tend to divide at the same division check, which produces bursts of divisions and of
  mesh operations.
- **Counting divisions.** The `divisions` column of the diagnostics CSV counts divisions per
  100 iterations. Refusals and other division failures appear only in the log
  ({doc}`09_safeguards`).

:::{seealso}
- {doc}`/examples/growth_from_one_cell` for the growth benchmark of the paper (Fig. 1e).
- {doc}`05_mesh_refinement` for the refinement of the daughters.
- {doc}`/api/triangulation` for the `cell_divider` class.
:::
