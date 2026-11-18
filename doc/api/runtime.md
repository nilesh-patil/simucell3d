(api-runtime)=
# Runtime support

Thread placement, performance measurement and small utilities. The `cpu_topology` namespace
detects the physical cores available to the process and pins one OpenMP thread per physical
core unless `SIMUCELL3D_PIN_THREADS=0` or `OMP_PLACES` is set ({doc}`/user_guide/18_parallel_performance`).
{cpp:class}`PerformanceMonitor` times the phases of each iteration and feeds the diagnostics
CSV. `utils.hpp` holds helper templates for number formatting and floating-point comparison.

## Thread placement

```{doxygennamespace} cpu_topology
:project: simucell3d
```

## Performance monitoring

```{doxygenclass} PerformanceMonitor
:project: simucell3d
:members:
```

## Utilities

```{doxygenfile} utils.hpp
:project: simucell3d
```
