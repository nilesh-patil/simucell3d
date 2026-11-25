(api-runtime)=
# Runtime support

Thread placement, performance measurement and small utilities. The `cpu_topology` namespace
detects the physical cores available to the process and pins one OpenMP thread per physical
core unless `SIMUCELL3D_PIN_THREADS=0` or `OMP_PLACES` is set ({doc}`/user_guide/18_parallel_performance`).
{cpp:class}`PerformanceMonitor` times the phases of each iteration and feeds the diagnostics
CSV. `utils.hpp` holds helper templates for number formatting and floating-point comparison.

Thread placement runs once, in the solver constructor. The performance monitor times four
phases of every iteration (`mesh_refinement`, `contact_detection`,
`polarization_and_internal_forces`, `time_integration`); the solver writes their means to the
{term}`diagnostics CSV` every 100 iterations.

| Entity | Header | Role |
|---|---|---|
| namespace `cpu_topology` | `cpu_topology.hpp` | Allowed CPUs, physical cores, placement order, pinning |
| {cpp:class}`PerformanceMonitor` | `performance_monitor.hpp` | Accumulates the wall-clock time of named phases |
| {cpp:class}`PerformanceMonitor::ScopedTimer` | `performance_monitor.hpp` | Times one scope |
| `utils.hpp` | `utils.hpp` | Number formatting, floating-point comparison |

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
