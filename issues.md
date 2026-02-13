# SimuCell3D — Verified Issues Audit

> **Date:** 2026-02-09
> **Branch:** `version-cpp-next`
> **Methodology:** Every issue claim from two independent audits (CSCC, CSOC) was verified against the actual source code, not just git messages.
> **GitHub:** https://github.com/nilesh-patil/simucell3d
> **PR #72:** Merge commit `3ed2fac` (merged 2026-02-07) — closed all 28 original issues (#44–#71)

---

## Master Issue Table

| # | Title | Severity | Verified Status | GitHub Link |
|---|-------|----------|----------------|-------------|
| [#44](#44) | Duplicate variable check in output folder validation | CRITICAL | FIXED | [#44](https://github.com/nilesh-patil/simucell3d/issues/44) |
| [#45](#45) | Assignment operator in assertion (`=` vs `==`) | CRITICAL | FIXED | [#45](https://github.com/nilesh-patil/simucell3d/issues/45) |
| [#46](#46) | Uninitialized return in `face::get_opposite_node()` | CRITICAL | FIXED | [#46](https://github.com/nilesh-patil/simucell3d/issues/46) |
| [#47](#47) | Data race on `kinetic_energy_` in parallel time integration | CRITICAL | FIXED (see note) | [#47](https://github.com/nilesh-patil/simucell3d/issues/47) |
| [#48](#48) | Data race on `node::force_` in parallel contact forces | CRITICAL | FIXED (per-component atomics) | [#48](https://github.com/nilesh-patil/simucell3d/issues/48) |
| [#49](#49) | Null pointer dereference in contact force computation | HIGH | FIXED | [#49](https://github.com/nilesh-patil/simucell3d/issues/49) |
| [#50](#50) | Exceptions created but never thrown | HIGH | **INCOMPLETE** — 6 instances remain | [#50](https://github.com/nilesh-patil/simucell3d/issues/50) |
| [#51](#51) | Missing bounds check in AABB intersection test | HIGH | FIXED (assert-based) | [#51](https://github.com/nilesh-patil/simucell3d/issues/51) |
| [#52](#52) | Typo in exception class name (`intialization_exception`) | MEDIUM | OPEN — retained for API compat | [#52](https://github.com/nilesh-patil/simucell3d/issues/52) |
| [#53](#53) | Non-unit normals from `get_face_normal()` | MEDIUM | FIXED (documentation) | [#53](https://github.com/nilesh-patil/simucell3d/issues/53) |
| [#54](#54) | Missing atomic on `kinetic_energy_` reset | MEDIUM | FIXED | [#54](https://github.com/nilesh-patil/simucell3d/issues/54) |
| [#55](#55) | Developer warning flag in BPA | MEDIUM | FIXED | [#55](https://github.com/nilesh-patil/simucell3d/issues/55) |
| [#56](#56) | Delaunator TODO — algorithm equivalence unverified | LOW | FIXED | [#56](https://github.com/nilesh-patil/simucell3d/issues/56) |
| [#57](#57) | Division by zero in barycentric coordinate calculation | CRITICAL | FIXED | [#57](https://github.com/nilesh-patil/simucell3d/issues/57) |
| [#58](#58) | NULL pointer dereference in XML parsing (`GetText()`) | CRITICAL | FIXED | [#58](https://github.com/nilesh-patil/simucell3d/issues/58) |
| [#59](#59) | Unhandled exceptions in `std::stod` conversions | CRITICAL | **INCOMPLETE** — helpers defined but never called | [#59](https://github.com/nilesh-patil/simucell3d/issues/59) |
| [#60](#60) | Uninitialized variable in face-face contact detection | HIGH | FIXED | [#60](https://github.com/nilesh-patil/simucell3d/issues/60) |
| [#61](#61) | Missing file stream validation in mesh reader | HIGH | FIXED | [#61](https://github.com/nilesh-patil/simucell3d/issues/61) |
| [#62](#62) | Missing `std::out_of_range` handler in mesh parsing | HIGH | **INCOMPLETE** — only catches `invalid_argument` | [#62](https://github.com/nilesh-patil/simucell3d/issues/62) |
| [#63](#63) | Parameter validation accepts zero values | HIGH | **INCOMPLETE** — `damping_coefficient_` allows zero | [#63](https://github.com/nilesh-patil/simucell3d/issues/63) |
| [#64](#64) | Vector division by zero (no scalar validation) | MEDIUM | FIXED | [#64](https://github.com/nilesh-patil/simucell3d/issues/64) |
| [#65](#65) | Floating-point exact comparison in contact detection | MEDIUM | NOT VERIFIED — no epsilon tolerance found | [#65](https://github.com/nilesh-patil/simucell3d/issues/65) |
| [#66](#66) | Race condition in `map::operator[]` (non-thread-safe) | MEDIUM | FIXED | [#66](https://github.com/nilesh-patil/simucell3d/issues/66) |
| [#67](#67) | Validation error message references wrong variable | MEDIUM | **RESIDUAL BUG** at line 377 | [#67](https://github.com/nilesh-patil/simucell3d/issues/67) |
| [#68](#68) | Wrong variable in validation check (copy-paste) | MEDIUM | **RESIDUAL BUG** at line 407 | [#68](https://github.com/nilesh-patil/simucell3d/issues/68) |
| [#69](#69) | Stale momentum in semi-implicit Euler integration | CRITICAL | FIXED | [#69](https://github.com/nilesh-patil/simucell3d/issues/69) |
| [#70](#70) | Mixed sync primitives (`omp_lock_t` + `std::mutex`) | MEDIUM | FIXED | [#70](https://github.com/nilesh-patil/simucell3d/issues/70) |
| [#71](#71) | Node copy/move constructors fail with `std::mutex` | LOW | FIXED | [#71](https://github.com/nilesh-patil/simucell3d/issues/71) |
| [NEW-01](#new-01) | `sprintf` buffer overflow in `format_number()` | HIGH | OPEN | [#73](https://github.com/nilesh-patil/simucell3d/issues/73) |
| [NEW-02](#new-02) | `cot()` div-by-zero at multiples of pi | MEDIUM | OPEN | [#74](https://github.com/nilesh-patil/simucell3d/issues/74) |
| [NEW-03](#new-03) | `map::operator[]` silent default insertion in `face::update_node_ids()` | MEDIUM | OPEN | [#75](https://github.com/nilesh-patil/simucell3d/issues/75) |
| [NEW-04](#new-04) | `assert()`-only bounds checking disabled in Release | MEDIUM | OPEN | [#76](https://github.com/nilesh-patil/simucell3d/issues/76) |
| [NEW-05](#new-05) | Adhesion force numerical explosion near zero distance | MEDIUM | OPEN | [#77](https://github.com/nilesh-patil/simucell3d/issues/77) |
| [NEW-06](#new-06) | `vec3::translate()` 3 separate atomics not group-atomic | LOW | OPEN — safe in practice | [#78](https://github.com/nilesh-patil/simucell3d/issues/78) |
| [NEW-07](#new-07) | Magic numbers for scheduling thresholds | LOW | OPEN | [#79](https://github.com/nilesh-patil/simucell3d/issues/79) |
| [NEW-08](#new-08) | `kinetic_energy_` data race for cross-cell updates | MEDIUM | OPEN | [#80](https://github.com/nilesh-patil/simucell3d/issues/80) |
| [NEW-09](#new-09) | TODO: Oscillation detection in mesh refinement | MEDIUM | OPEN | [#81](https://github.com/nilesh-patil/simucell3d/issues/81) |

---

## Summary Statistics

| Category | Count |
|----------|:-----:|
| Fully fixed | 21 |
| Intentionally open (#52 typo — API compat) | 1 |
| Incomplete fix (residual bugs) | 6 (#50, #59, #62, #63, #67, #68) |
| Not verified (#65) | 1 |
| New issues (open) | 9 (NEW-01 through NEW-09) |
| **Total** | **38** |

---

## Part 1 — Issues Confirmed Fixed

These issues were verified as fully resolved by inspecting the source code.

### #44 — Duplicate variable check in output folder validation {#44}

- **Severity:** CRITICAL | **Component:** Solver / I/O | **Fix commit:** `3b7b8e8`
- **Verification:** `src/solver.cpp:783-801` now validates all folder creation results independently (`t1`, `t2`, `t3`, `t4` each checked separately).

### #45 — Assignment operator in assertion (`=` vs `==`) {#45}

- **Severity:** CRITICAL | **Component:** Mesh | **Fix commit:** `218ba4f`
- **Verification:** `src/mesh/node.cpp` assertion corrected to use `==`.

### #46 — Uninitialized return in `face::get_opposite_node()` {#46}

- **Severity:** CRITICAL | **Component:** Mesh | **Fix commit:** `218ba4f`
- **Verification:** `src/mesh/face.cpp:118` now initializes to `std::numeric_limits<unsigned>::max()` as sentinel with post-condition assert.

### #47 — Data race on `kinetic_energy_` in parallel time integration {#47}

- **Severity:** CRITICAL | **Component:** Thread Safety | **Fix commit:** `bda3f9e`
- **Verification:** `kinetic_energy_` reset moved to serial `std::for_each` at `time_integration.cpp:18`. Accumulation uses `+=` inside per-cell parallel loops. For `CONTACT_MODEL_INDEX==0`, each cell only updates its own `kinetic_energy_`, which is safe. See [NEW-08](#new-08) for a residual race in `CONTACT_MODEL_INDEX==1`.

### #48 — Data race on `node::force_` in parallel contact forces {#48}

- **Severity:** CRITICAL | **Component:** Thread Safety | **Fix commit:** `bda3f9e`
- **Verification:** `node::add_force()` at `node.hpp:217` delegates to `vec3::translate()` at `vec3.cpp:33-43`, which uses three `#pragma omp atomic update` directives — one per component. This is NOT a mutex-based fix (CSOC's claim of "NOT ATOMIC" is factually wrong). The per-component atomics prevent data corruption. However, the three updates are not group-atomic (see [NEW-06](#new-06)).

### #49 — Null pointer dereference in contact force computation {#49}

- **Severity:** HIGH | **Component:** Contact Models | **Fix commit:** `218ba4f`
- **Verification:** `contact_node_face_via_spring.cpp:111-113` includes runtime null check: `if (f == nullptr) { continue; }`.

### #51 — Missing bounds check in AABB intersection test {#51}

- **Severity:** HIGH | **Component:** Contact Models | **Fix commit:** `218ba4f`
- **Verification:** `contact_node_face_via_spring.cpp:106` has `assert(voxel_id < grid_.voxel_lst_.size())`. Note: this is an assert, disabled in Release builds. See [NEW-04](#new-04).

### #53 — Non-unit normals from `get_face_normal()` {#53}

- **Severity:** MEDIUM | **Component:** Mesh | **Fix commit:** `218ba4f`
- **Verification:** Documentation and handling updated in `src/mesh/cell.cpp`.

### #54 — Missing atomic on `kinetic_energy_` reset {#54}

- **Severity:** MEDIUM | **Component:** Thread Safety | **Fix commit:** `bda3f9e`
- **Verification:** Reset now occurs in a serial `std::for_each` before parallel loops at `time_integration.cpp:18`.

### #55 — Developer warning flag in BPA {#55}

- **Severity:** MEDIUM | **Component:** Triangulation | **Fix commit:** `e739374`
- **Verification:** Warning flags replaced with proper diagnostic checks in `cell_divider.cpp`.

### #56 — Delaunator TODO — algorithm equivalence unverified {#56}

- **Severity:** LOW | **Component:** Triangulation | **Fix commit:** `e739374`
- **Verification:** Safety assertions added to validate Delaunator output.

### #57 — Division by zero in barycentric coordinate calculation {#57}

- **Severity:** CRITICAL | **Component:** Math | **Fix commit:** `a0b3ec9`
- **Verification:** `mat33.cpp:279-283` checks determinant against `epsilon = 1e-12` and throws `std::domain_error`. `vec3.cpp:124-127` checks for zero-length vectors.

### #58 — NULL pointer dereference in XML parsing (`GetText()`) {#58}

- **Severity:** CRITICAL | **Component:** I/O | **Fix commit:** `3b7b8e8`
- **Verification:** `parameter_reader.cpp` includes null checks on all `GetText()` calls.

### #60 — Uninitialized variable in face-face contact detection {#60}

- **Severity:** HIGH | **Component:** Contact Models | **Fix commit:** `218ba4f`
- **Verification:** Variable initialization added. Safe by control flow in practice — the variable is always assigned before first read.

### #61 — Missing file stream validation in mesh reader {#61}

- **Severity:** HIGH | **Component:** I/O | **Fix commit:** `3b7b8e8`
- **Verification:** `mesh_reader.cpp` validates stream state with `is_open() && good()`.

### #64 — Vector division by zero (no scalar validation) {#64}

- **Severity:** MEDIUM | **Component:** Math | **Fix commit:** `a0b3ec9`
- **Verification:** `vec3.cpp:124-127` checks for zero-length vectors and returns 0.0. `normalize()` at line 94 handles zero-norm. `get_angle_with()` uses `std::clamp` to avoid domain errors.

### #66 — Race condition in `map::operator[]` (non-thread-safe) {#66}

- **Severity:** MEDIUM | **Component:** Thread Safety | **Fix commit:** `bda3f9e`
- **Verification:** Thread-safe access patterns established with proper synchronization.

### #69 — Stale momentum in semi-implicit Euler integration {#69}

- **Severity:** CRITICAL | **Component:** Time Integration | **Fix commit:** `11a1e76`
- **Verification:** `time_integration.cpp:266-292` now: (1) updates all coupled node momenta, (2) recomputes average momentum post-update, (3) uses the new average for position updates.

### #70 — Mixed sync primitives (`omp_lock_t` + `std::mutex`) {#70}

- **Severity:** MEDIUM | **Component:** Thread Safety | **Fix commit:** `bda3f9e`
- **Verification:** `node.hpp` now uses `std::mutex` exclusively.

### #71 — Node copy/move constructors fail with `std::mutex` {#71}

- **Severity:** LOW | **Component:** Mesh | **Fix commit:** `bda3f9e`
- **Verification:** `node.cpp` has explicit copy/move constructors that create fresh mutexes.

---

## Part 2 — Issues with Incomplete or Residual Fixes

These issues are marked CLOSED on GitHub but source code inspection reveals they are not fully resolved.

### #50 — Exceptions created but never thrown {#50}

- **Severity:** HIGH | **Component:** Error Handling
- **GitHub:** [#50](https://github.com/nilesh-patil/simucell3d/issues/50) (CLOSED)
- **Status:** **INCOMPLETE** — 6 instances remain without `throw`
- **Evidence:**
  - `time_integration.cpp:60` — `std::runtime_error(...)` without `throw`
  - `time_integration.cpp:148` — same
  - `time_integration.cpp:187` — same
  - `time_integration.cpp:303` — same
  - `time_integration.cpp:339` — same
  - `automatic_polarizer.cpp:272` — same
  - `solver.cpp:1443` — has `throw` (correct)
- **Note:** These are all inside `#else` preprocessor branches, so they only trigger for unsupported compile-time configuration combinations. Impact is low in practice but the pattern is incorrect.
- **Residual issue:** [#82](https://github.com/nilesh-patil/simucell3d/issues/82)

### #59 — Unhandled exceptions in `std::stod` conversions {#59}

- **Severity:** CRITICAL | **Component:** I/O
- **GitHub:** [#59](https://github.com/nilesh-patil/simucell3d/issues/59) (CLOSED)
- **Status:** **INCOMPLETE** — `safe_stod()` / `safe_stoi()` defined but never called
- **Evidence:**
  - `parameter_reader.cpp:8-26` — `safe_stod()` and `safe_stoi()` defined in anonymous namespace with proper try-catch
  - `parameter_reader.cpp:109` — `std::stod(damping_coefficient_opt.value())` (raw, not safe)
  - Lines 120, 126, 132, 140, 146, 152, etc. — all ~28 conversion calls use raw `std::stod`/`std::stoi`
  - Zero calls to `safe_stod` or `safe_stoi` anywhere in the codebase
- **Impact:** Malformed numeric values in XML config will crash the parser with an unhandled exception.
- **Residual issue:** [#83](https://github.com/nilesh-patil/simucell3d/issues/83)

### #62 — Missing `std::out_of_range` handler in mesh parsing {#62}

- **Severity:** HIGH | **Component:** I/O
- **GitHub:** [#62](https://github.com/nilesh-patil/simucell3d/issues/62) (CLOSED)
- **Status:** **INCOMPLETE** — only catches `std::invalid_argument`, not `std::out_of_range`
- **Evidence:** `mesh_reader.cpp` try-catch blocks (lines 132-137, 216-221, 312-315, 501-504) all use `catch (const std::invalid_argument&)`. There are also 5+ bare `std::stoi`/`std::stod` calls with no try-catch at all.
- **Impact:** Values exceeding numeric limits (e.g., `"999999999999999999"`) crash the mesh parser.
- **Residual issue:** [#84](https://github.com/nilesh-patil/simucell3d/issues/84)

### #63 — Parameter validation accepts zero values {#63}

- **Severity:** HIGH | **Component:** I/O
- **GitHub:** [#63](https://github.com/nilesh-patil/simucell3d/issues/63) (CLOSED)
- **Status:** **INCOMPLETE** — `damping_coefficient_` still allows zero
- **Evidence:**
  - `parameter_reader.cpp:110` — `if(sim_parameters.damping_coefficient_ < 0.0)` (uses `<`, allows zero)
  - Lines 121, 127, 133, 141, 147, 153 — other parameters correctly use `<= 0.0`
  - Error message says "must be strictly positive" but the check permits zero
- **Impact:** A zero damping coefficient causes division by zero in `time_integration.cpp` (e.g., `dt_ / damping_coeff_`).
- **Residual issue:** [#85](https://github.com/nilesh-patil/simucell3d/issues/85)

### #67 — Validation error message references wrong variable {#67}

- **Severity:** MEDIUM | **Component:** I/O
- **GitHub:** [#67](https://github.com/nilesh-patil/simucell3d/issues/67) (CLOSED)
- **Status:** **RESIDUAL BUG** — one instance remains
- **Evidence:** `parameter_reader.cpp:377` reads XML markup `"std_growth_rate"` but the error message on line 377-378 says `"avg_growth_rate"` (copy-paste error from line 371).
- **Impact:** Misleading error message if `std_growth_rate` is missing from config.
- **Residual issue:** [#86](https://github.com/nilesh-patil/simucell3d/issues/86)

### #68 — Wrong variable in validation check (copy-paste) {#68}

- **Severity:** MEDIUM | **Component:** I/O
- **GitHub:** [#68](https://github.com/nilesh-patil/simucell3d/issues/68) (CLOSED)
- **Status:** **RESIDUAL BUG** — wrong variable still validated
- **Evidence:** `parameter_reader.cpp:407` validates `cell_parameters->target_isoperimetric_ratio_` instead of `cell_parameters->surface_coupling_max_curvature_` (the variable just parsed on line 406).
- **Impact:** Negative `surface_coupling_max_curvature_` values pass validation unchecked. The check on `target_isoperimetric_ratio_` is redundant (already validated at line 386).
- **Residual issue:** [#87](https://github.com/nilesh-patil/simucell3d/issues/87)

---

## Part 3 — Issue Not Verified

### #65 — Floating-point exact comparison in contact detection {#65}

- **Severity:** MEDIUM | **Component:** Contact Models
- **GitHub:** [#65](https://github.com/nilesh-patil/simucell3d/issues/65) (CLOSED)
- **Status:** **NOT VERIFIED** — Commit `218ba4f` changes in `local_mesh_refiner.cpp` add oscillation detection and stability fixes, but no epsilon-based floating-point comparison was found in the contact detection code. The `min_squared_distance != 0.0` check at `contact_node_face_via_spring.cpp:163` is still an exact comparison.

---

## Part 4 — New Issues (Open)

These issues were identified through static code analysis and verified against the source. They are not covered by any existing GitHub issue.

### NEW-01 — `sprintf` buffer overflow in `format_number()` {#new-01}

- **Severity:** HIGH | **File:** `include/utils.hpp:121-125`
- **GitHub:** [#73](https://github.com/nilesh-patil/simucell3d/issues/73)

```cpp
template<typename T>
inline std::string format_number(T number, std::string fmt){
    char buffer_char[30];
    sprintf(buffer_char, fmt.c_str(), number);  // No bounds check
    return std::string(buffer_char);
}
```

A caller-controlled format string with a 30-byte fixed buffer. A format like `"%.50f"` easily overflows. Replace with `snprintf(buffer_char, sizeof(buffer_char), ...)`.

### NEW-02 — `cot()` division by zero at multiples of pi {#new-02}

- **Severity:** MEDIUM | **File:** `include/utils.hpp:180`
- **GitHub:** [#74](https://github.com/nilesh-patil/simucell3d/issues/74)

```cpp
inline double cot(const double angle){return 1. / std::tan(angle);}
```

No guard against `tan(angle) == 0`. Used in mesh refinement (edge swap scoring). Degenerate triangles produce Inf values.

### NEW-03 — `map::operator[]` silent default insertion in `face::update_node_ids()` {#new-03}

- **Severity:** MEDIUM | **File:** `src/mesh/face.cpp:99-101`
- **GitHub:** [#75](https://github.com/nilesh-patil/simucell3d/issues/75)

```cpp
n1_id_ = node_id_correspondence[n1_id_];  // Inserts 0 if key missing
```

If a node ID is missing from the correspondence map, `operator[]` silently inserts 0. Use `map::at()` or `find()` + assert.

### NEW-04 — `assert()`-only bounds checking disabled in Release {#new-04}

- **Severity:** MEDIUM | **File:** `src/time_integration/time_integration.cpp:93,99,240,244,272,274`
- **GitHub:** [#76](https://github.com/nilesh-patil/simucell3d/issues/76)

```cpp
assert(c2_id < cell_lst.size());
cell_ptr c2 = cell_lst[c2_id];  // UB if assert stripped in Release
```

Critical bounds checks use `assert()` which is removed with `-DNDEBUG`. Use runtime checks for safety-critical paths.

### NEW-05 — Adhesion force numerical explosion near zero distance {#new-05}

- **Severity:** MEDIUM | **File:** `src/contact_models/contact_node_face_via_spring.cpp:204`
- **GitHub:** [#77](https://github.com/nilesh-patil/simucell3d/issues/77)

```cpp
force_amplitude = face_type.adherence_strength_ * (interaction_cutoff_adhesion_ / min_distance - 1)
                  * integration_region;
```

The outer guard checks `min_squared_distance != 0.0`, so `min_distance` is never exactly zero. But for very small values (e.g., `1e-15`), `1/min_distance` produces forces on the order of `1e15`. The `hardening_distance_` partially mitigates this but doesn't cap the force.

### NEW-06 — `vec3::translate()` 3 separate atomics not group-atomic {#new-06}

- **Severity:** LOW | **File:** `src/math_modules/vec3.cpp:33-43`
- **GitHub:** [#78](https://github.com/nilesh-patil/simucell3d/issues/78)

Each `dx_`, `dy_`, `dz_` update is individually `#pragma omp atomic`, but not atomic as a group. Safe in practice because the accumulated sum is only read after a parallel barrier. Fragile if future code reads forces during accumulation.

### NEW-07 — Magic numbers for scheduling thresholds {#new-07}

- **Severity:** LOW | **File:** `src/solver.cpp:268,273,291`
- **GitHub:** [#79](https://github.com/nilesh-patil/simucell3d/issues/79)

Scheduling thresholds (0.6, 0.15, 0.30) are hardcoded magic numbers. Extract to `constexpr double` constants.

### NEW-08 — `kinetic_energy_` data race for cross-cell updates {#new-08}

- **Severity:** MEDIUM | **File:** `src/time_integration/time_integration.cpp:131-132,144-145`
- **GitHub:** [#80](https://github.com/nilesh-patil/simucell3d/issues/80)

For `CONTACT_MODEL_INDEX==1`, the coupled-node code block updates both `c1->kinetic_energy_` and `c2->kinetic_energy_` inside a parallel loop. Since `c2` may be processed by another thread simultaneously, `c2->kinetic_energy_ +=` is a data race. `kinetic_energy_` is a plain `double` (not atomic) in `cell.hpp`.

### NEW-09 — Stale TODO comment for oscillation detection {#new-09}

- **Severity:** LOW | **File:** `src/triangulation_modules/local_mesh_refiner.cpp:60`
- **GitHub:** [#81](https://github.com/nilesh-patil/simucell3d/issues/81)

Line 60 has a TODO comment `// TODO(human): Implement the oscillation detection logic here`, but the logic **IS actually implemented** below it (lines 69-146): hash function, position tracking map, `MAX_POSITION_OPERATIONS = 50` threshold, and graceful loop termination. The TODO is stale/misleading and should be removed.

---

## Part 5 — Corrections to Prior Audit Claims

During verification, several claims from the two audit documents (CSCC and CSOC) were found to be inaccurate:

| Claim | Source | Correction |
|-------|--------|------------|
| `add_force()` uses `force_ += force; // NOT ATOMIC` | CSOC BUG-002 | **Wrong.** Actual code is `force_.translate(force)` which uses 3 `#pragma omp atomic update` directives. |
| #59 is FIXED | CSOC | **Wrong.** `safe_stod`/`safe_stoi` are defined but never called — dead code. |
| #62 is FIXED | CSOC | **Wrong.** Only catches `std::invalid_argument`, not `std::out_of_range`. |
| #63 is FIXED | CSOC | **Wrong.** `damping_coefficient_` at line 110 still uses `< 0.0` (allows zero). |
| #65 is FIXED (epsilon comparison) | CSOC | **Not verified.** No epsilon tolerance found in contact detection code. |
| #67 is FIXED | CSCC, CSOC | **Partially wrong.** Line 377 error message still says "avg_growth_rate" instead of "std_growth_rate". |
| #68 is FIXED | CSCC, CSOC | **Partially wrong.** Line 407 still validates `target_isoperimetric_ratio_` instead of `surface_coupling_max_curvature_`. |
| NEW-006 (CSOC): Division by zero in `mat33::inverse()` and `vec3::get_angle_with()` is OPEN | CSOC | **Wrong.** Guards already exist: `mat33.cpp:279-283` (epsilon 1e-12) and `vec3.cpp:124-135` (zero-length check + clamp). |
| #47/#48 migrated to `std::mutex` | CSCC | **Partially wrong.** `add_force()` does NOT use mutex. It uses `vec3::translate()` with per-component OMP atomics. The mutex exists in `node.hpp` but is only used by `set_coupled_node_and_min_distance()`. |
| BUG-004 (vector modification during parallel iteration) | CSOC | **Needs review.** Cell division and mesh refinement run in serial regions (not inside parallel loops), so iterator invalidation may not actually occur. Requires further investigation. |

---

## Part 6 — Code Quality Observations

These are design-level observations, not bugs. They are documented here for completeness but do not have associated GitHub issues.

| ID | Description | Location | Priority |
|----|-------------|----------|----------|
| CQ-001 | Excessive `const_cast` usage (10+ instances) | `src/mesh/cell.cpp` | MEDIUM |
| CQ-002 | Heavy `friend` declarations reducing encapsulation | `node.hpp`, `face.hpp`, `cell.hpp` | LOW |
| CQ-003 | Ad-hoc test framework instead of GoogleTest | `test/` directory | MEDIUM |
| CQ-004 | Array-of-Structs memory layout limits SIMD | Core data structures | LOW (perf) |
| CQ-005 | No SIMD vectorization in vec3 operations | `src/math_modules/vec3.cpp` | LOW (perf) |
| CQ-006 | Inconsistent error handling patterns | Multiple locations | LOW |

---

## Appendix: Fix Commit Reference

| Commit | Message | Issues Fixed |
|--------|---------|:------------:|
| `3b7b8e8` | fix(io): Improve I/O robustness | #44, #50*, #52, #58, #59*, #61, #62*, #63*, #67*, #68* |
| `218ba4f` | fix(mesh): Initialize vars, fix overflow | #45, #46, #49, #51, #53, #60, #65? |
| `bda3f9e` | fix(mesh): Migrate to std::mutex | #47, #48, #54, #66, #70, #71 |
| `e739374` | fix(cell-divider): Improve safety checks | #55, #56 |
| `a0b3ec9` | fix(math): Prevent division by zero | #57, #64 |
| `11a1e76` | fix(time-integration): Fix stale momentum | #69 |

\* = incomplete fix (see Part 2)
? = not verified (see Part 3)
