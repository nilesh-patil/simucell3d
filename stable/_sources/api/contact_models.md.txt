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
