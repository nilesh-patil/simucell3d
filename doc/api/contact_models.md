(api-contact-models)=
# Contact models

At each iteration, after mesh refinement, the contact model finds pairs of cells whose
surfaces are within the contact cutoffs and applies adhesion and repulsion. The class is
chosen at compile time by `CONTACT_MODEL_INDEX` in `global_configuration.hpp`:
0 {cpp:class}`contact_node_face_via_spring`, 1 {cpp:class}`contact_node_node_via_coupling`,
2 {cpp:class}`contact_face_face_via_coupling` (default). All three derive from
{cpp:class}`contact_model_abstract`.

With contact model 2, the `contact_detection_algorithm` parameter selects how candidate
node-face pairs are found: `uspg` (default) bins the bounding boxes of all faces in a uniform
grid built by the model itself; `sweep_and_prune` uses
{cpp:class}`contact_detection_sap_strategy`; `adaptive` uses the grid and switches to sweep
and prune when the number of cells reaches `ADAPTIVE_SAP_CELL_THRESHOLD` (500). Contact
models 0 and 1 ignore the parameter and always use the uniform spatial partitioning grid of
{cpp:class}`contact_model_abstract`. {cpp:class}`contact_detection_uspg_strategy` implements
the same strategy interface on a USPG but is not queried by the contact models during a
simulation. The narrative description is in {doc}`/user_guide/03_contact`.

The contact model runs once per iteration, after mesh refinement and before the internal
forces: `solver::run_iteration()` calls `contact_model_abstract::run()` with the whole cell
list, outside any parallel region; the model parallelises over cells itself. It may throw
{cpp:class}`unstable_simulation_exception` when the domain has diverged.

| Class | Header | Role |
|---|---|---|
| {cpp:class}`contact_model_abstract` | `contact_model_abstract.hpp` | Base class: face bounding boxes, the model's own USPG, node-triangle distance |
| {cpp:class}`contact_face_face_via_coupling` | `contact_face_face_via_coupling.hpp` | Contact model 2 (default) |
| {cpp:class}`contact_node_node_via_coupling` | `contact_node_node_via_coupling.hpp` | Contact model 1 |
| {cpp:class}`contact_node_face_via_spring` | `contact_node_face_via_spring.hpp` | Contact model 0 |
| {cpp:class}`contact_detection_strategy` | `contact_detection_strategy.hpp` | Interface of the broad-phase strategies; {cpp:struct}`aabb` |
| {cpp:class}`contact_detection_sap_strategy` | `contact_detection_sap_strategy.hpp` | Sweep and prune (contact model 2 only) |
| {cpp:class}`contact_detection_uspg_strategy` | `contact_detection_uspg_strategy.hpp` | USPG strategy; not queried during a simulation |

The threshold of the adaptive algorithm is the constant {cpp:var}`ADAPTIVE_SAP_CELL_THRESHOLD`
({doc}`solver`). Force accumulation in contact models 0 and 1 is not thread-safe in 2.1.0; see
the known issues in `CHANGELOG.md` and {ref}`dev-performance-internals`.

## Contact models

```{doxygenclass} contact_model_abstract
:project: simucell3d
:members:
```

```{doxygenclass} contact_face_face_via_coupling
:project: simucell3d
:members:
```

```{doxygenclass} contact_node_node_via_coupling
:project: simucell3d
:members:
```

```{doxygenclass} contact_node_face_via_spring
:project: simucell3d
:members:
```

## Contact detection

```{doxygenclass} contact_detection_strategy
:project: simucell3d
:members:
```

```{doxygenclass} contact_detection_uspg_strategy
:project: simucell3d
:members:
```

```{doxygenclass} contact_detection_sap_strategy
:project: simucell3d
:members:
```

```{doxygenstruct} aabb
:project: simucell3d
:members:
```
