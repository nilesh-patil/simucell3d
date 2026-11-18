(api-time-integration)=
# Time integration

{cpp:class}`time_integration_scheme` moves the nodes at the end of each iteration from the
forces accumulated on them. `DYNAMIC_MODEL_INDEX` selects the equations at compile time:
0 solves the full equations of motion with the semi-implicit Euler scheme, 1 solves the
overdamped equations with the forward Euler scheme. With contact models 1 and 2, coupled nodes
on neighbouring cells are moved together. The narrative description is in
{doc}`/user_guide/04_time_integration`.

## Integrator

```{doxygenclass} time_integration_scheme
:project: simucell3d
:members:
```
