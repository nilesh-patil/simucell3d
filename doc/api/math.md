(api-math)=
# Mathematics

Small value types used throughout the solver: {cpp:class}`vec3` (three-component vector),
{cpp:class}`mat33` (3 x 3 matrix) and {cpp:class}`quaternion` (rotations). `eigen_solver.hpp`
contains a symmetric 3 x 3 eigensolver adapted from the Geometric Tools library (namespace
`gte`); {cpp:func}`mat33::eigen_decomposition` uses it, for example to find the principal
axes of a cell from the covariance matrix of its nodes. The `math_modules` library is
compiled without OpenMP: the `#pragma omp atomic` lines in `vec3` have no effect, and a vector
that several threads add to must use `vec3::atomic_translate()`
({ref}`dev-performance-internals`).

| Class | Header | Role |
|---|---|---|
| {cpp:class}`vec3` | `math_modules/vec3.hpp` | Positions (m), forces (N), momenta, normals |
| {cpp:class}`mat33` | `math_modules/mat33.hpp` | 3 x 3 matrix stored by rows |
| {cpp:class}`quaternion` | `math_modules/quaternion.hpp` | Rotations, for example of a cell onto its division plane |
| {cpp:class}`gte::SymmetricEigensolver3x3`, {cpp:class}`gte::NISymmetricEigensolver3x3` | `math_modules/eigen_solver.hpp` | Iterative and non-iterative symmetric eigensolvers |

## Vectors and matrices

```{doxygenclass} vec3
:project: simucell3d
:members:
```

```{doxygenclass} mat33
:project: simucell3d
:members:
```

```{doxygenclass} quaternion
:project: simucell3d
:members:
```

## Eigensolver

```{doxygenclass} gte::SymmetricEigensolver3x3
:project: simucell3d
:members:
```

```{doxygenclass} gte::NISymmetricEigensolver3x3
:project: simucell3d
:members:
```
