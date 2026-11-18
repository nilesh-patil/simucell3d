(api-math)=
# Mathematics

Small value types used throughout the solver: {cpp:class}`vec3` (three-component vector),
{cpp:class}`mat33` (3 x 3 matrix) and {cpp:class}`quaternion` (rotations). `eigen_solver.hpp`
contains a symmetric 3 x 3 eigensolver adapted from the Geometric Tools library (namespace
`gte`); {cpp:func}`mat33::eigen_decomposition` uses it, for example to find the principal
axes of a cell from the covariance matrix of its nodes. The `math_modules` library is
compiled without OpenMP.

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
