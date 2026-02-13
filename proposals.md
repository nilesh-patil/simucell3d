# SimuCell3D Technical Proposals

## Executive Summary

This document presents 36 technical proposals for SimuCell3D, organized into four categories: Performance & Architecture (12 proposals), HPC Scalability (5), Scientific Capabilities (13), and Developer Experience & Tooling (8). Each proposal follows a progressive-disclosure format — project leads can scan the goal and metadata table in 30 seconds, stakeholders can read the full analysis in 2 minutes, and implementors can expand collapsible sections for implementation guides and acceptance criteria.

The highest-impact path begins with critical bug fixes (D5) and synchronization bottleneck removal (A3, A2, A4), delivering an estimated 2–3× speedup with low risk. Scientific capabilities build on this foundation: viscoelastic cortex (C1, ~40 lines), YAP/TAZ mechanotransduction (C7, ~30 lines), and adaptive time stepping (C4) unlock the simulation's ability to model realistic biological processes. Later phases address data structure modernization, HPC infrastructure (checkpoint/restart, hardware counter profiling), and specialized scientific features (junction mechanics, juxtacrine signaling, reaction-diffusion). GPU acceleration is deferred until CPU optimizations are exhausted and profiling data confirms the bottleneck warrants it.

## Decision Dashboard

### Performance & Architecture

| Rank | ID | Proposal | Impact | Risk | Phase | Dependencies |
|:----:|:--:|----------|:------:|:----:|:-----:|:------------:|
| 1 | **A3** | Thread-local force accumulation | **10/10** | Low-Med | 1 | — |
| 2 | **A2** | Inline coupling buffer | **8/10** | Low-Med | 1 | — |
| 3 | **A8** | Intrusive pointer for cell | **7/10** | Medium | 2 | — |
| 4 | **B1** | SoA node storage | **7/10** | V.High | 4 | A3, B6 |
| 5 | **A4** | Eliminate per-node mutex | **6/10** | Medium | 1 | A2, A3 |
| 6 | **A6** | USPG grid memory reuse | **6/10** | Low | 0 | — |
| 7 | **A9** | Pool allocator for nodes/faces | **6/10** | Medium | 4 | — |
| 8 | **A1** | Flat sorted edge vector | **5/10** | Med-High | 2 | — |
| 9 | **A10** | Small-vector for free queues | **5/10** | Low | 2 | — |
| 10 | **B2** | Compact hash grid + Morton ordering | **4/10** | Medium | — | — |

### HPC Scalability

| Rank | ID | Proposal | Impact | Risk | Phase | Dependencies |
|:----:|:--:|----------|:------:|:----:|:-----:|:------------:|
| 1 | **B7** | Parallel I/O + checkpoint/restart | **8/10** | Med-High | 4 | — |
| 2 | **B6** | LIKWID hardware counter integration | **7/10** | Low | 0 | — |
| 3 | **B5** | NUMA-aware memory allocation | **4/10** | Medium | — | B6 |
| 4 | **B3** | GPU acceleration (CUDA/SYCL) | **3/10** | High | — | A2, B1 |
| 5 | **B4** | MPI domain decomposition | **2/10** | V.High | — | B6 |

### Scientific Capabilities

| Rank | ID | Proposal | Impact | Risk | Phase | Dependencies |
|:----:|:--:|----------|:------:|:----:|:-----:|:------------:|
| 1 | **C1** | Kelvin-Voigt viscoelastic cortex | **10/10** | Low | 2 | — |
| 2 | **C4** | Adaptive time stepping | **9/10** | Medium | 3 | — |
| 3 | **C7** | YAP/TAZ mechanotransduction | **9/10** | Low | 2 | — |
| 4 | **C8** | Implicit contact resolution | **8/10** | Medium | 3 | — |
| 5 | **C2** | Active cortical contractility | **8/10** | Medium | 3 | C1 |
| 6 | **C11** | Cell-cell adhesion maturation | **7/10** | Low-Med | 5 | — |
| 7 | **C3** | Frictional tangential contact | **7/10** | Medium | 5 | — |
| 8 | **C6** | Adhesion belt / junction model | **7/10** | Med-High | 5 | C2 |
| 9 | **C10** | Juxtacrine signaling (Notch-Delta) | **7/10** | Low-Med | 5 | C8 |
| 10 | **C5** | Reaction-diffusion on surfaces | **6/10** | Med-High | 5 | — |
| 11 | **C9** | Lumen hydraulic pressure | **6/10** | Medium | 5 | — |
| 12 | **C12** | Nucleus as mechanical compartment | **5/10** | High | — | — |
| 13 | **C13** | ECM as mechanical substrate | **4/10** | High | — | — |

### Developer Experience & Tooling

| Rank | ID | Proposal | Impact | Risk | Phase | Dependencies |
|:----:|:--:|----------|:------:|:----:|:-----:|:------------:|
| 1 | **D5** | Fix residual bugs (6 confirmed) | **10/10** | Low | 0 | — |
| 2 | **D3** | GoogleTest migration | **8/10** | Low | 1 | — |
| 3 | **D4** | ThreadSanitizer in CI | **8/10** | Low-Med | 0 | — |
| 4 | **D1** | Taichi Lang GPU backend | **6/10** | Medium | — | — |
| 5 | **D9** | Performance regression CI | **5/10** | Low-Med | — | B6 |
| 6 | **D8** | Doxygen API documentation | **5/10** | Low | 5 | — |
| 7 | **D7** | Python API enhancements | **4/10** | Medium | — | — |
| 8 | **D6** | Expand roadmap.md | **4/10** | Low | — | — |

---

## Performance & Architecture

SimuCell3D's contact detection loop dominates runtime (40–50%), with synchronization overhead from atomic operations and heap allocation pressure from per-node `std::map` containers as the primary bottlenecks. The proposals in this category target three layers: eliminating contention (A3, A4), removing allocator thrashing (A2, A6, A9, A10), and improving data layout for cache efficiency (A8, B1, A1, B2). The critical path — A3 → A2 → A4 — delivers an estimated 2–3× speedup on contact-heavy workloads with minimal risk. Data layout changes (B1, B2) require profiling validation before committing to invasive refactors.

### A3: Thread-Local Force Accumulation

**Goal:** Eliminate the dominant synchronization bottleneck by replacing per-component atomic force updates with thread-local accumulation buffers.

| Impact | Risk | Phase | Dependencies |
|:------:|:----:|:-----:|:------------:|
| 10/10 | Low-Med | 1 | None |

**Use Case:** A researcher simulates morphogenesis with 1,000 cells (100K nodes, 250K contacts) on a 64-core AMD Threadripper PRO. Contact force computation calls `vec3::translate()` ~500K times per timestep, each requiring three `lock cmpxchg` atomic operations on adjacent doubles sharing a 64-byte cache line. With 64 threads, cache-line ping-pong causes ~40% of atomics to retry, costing ~640ms of contention per timestep. Thread-local buffers reduce this to ~1ms (a single reduction pass), delivering a 15–20% end-to-end speedup.

#### Current Limitation

`vec3::translate()` at `vec3.cpp:33-44` uses three separate `#pragma omp atomic update` directives on `dx_`, `dy_`, `dz_` — adjacent doubles within the same 64-byte cache line. Called via `node::add_force()` (`node.hpp:217`) in contact loops, each atomic compiles to a `lock cmpxchg` retry loop (~20–50× slower than a plain store). All three components share a cache line, causing maximum false sharing between threads. Under contention with 8+ threads, every force accumulation generates MESI protocol traffic across all CPU cores, and CAS instructions retry under contention, producing quadratic scaling degradation. Additionally, three independent atomics create two correctness risks: lost updates (two threads read-modify-write the same component, losing one update) and stale force vectors (a thread sees `dx_` from one timestep and `dy_` from another). Internal force loops where only one thread accesses a cell's nodes pay this overhead unnecessarily.

#### Proposed Change

Two-part fix:

1. **`translate_local()`** (non-atomic) for single-cell internal force paths where thread safety is guaranteed by the parallel decomposition
2. **Thread-local force buffers** for contact resolution: each thread accumulates into its own `vector<vec3>`, with a single cache-friendly reduction pass after all force computation completes

```cpp
// Thread-local buffer: O(threads × nodes × 24B), ~3.8MB for 8 threads × 20K nodes
std::vector<std::vector<vec3>> tl_forces(omp_get_max_threads(),
                                          std::vector<vec3>(n_nodes, vec3(0,0,0)));
// Reduction pass
for (int t = 0; t < n_threads; ++t)
    for (int i = 0; i < n_nodes; ++i)
        node_forces[i] += tl_forces[t][i];
```

#### Trade-offs

| Advantages | Risks & Costs |
|------------|---------------|
| Eliminates millions of atomic ops per timestep — the dominant synchronization cost | Memory overhead: O(threads × nodes × 24B), ~3.8MB for 8 threads × 20K nodes |
| Enables SIMD auto-vectorization of force accumulation (impossible through atomics) | All `vec3::translate()` call sites (~47) must be audited and classified as single-threaded or multi-threaded |
| Completely eliminates false sharing on force vectors between threads | Reduction pass adds O(N_nodes) serial phase, though cache-friendly |

**Verdict:** Highest-impact single optimization. Implement first, before any other performance work.

<details>
<summary><strong>Implementation Guide</strong></summary>

**Files to modify:** `include/math_modules/vec3.hpp`, `src/math_modules/vec3.cpp`, `src/contact_models/contact_face_face_via_coupling.cpp`, `src/solver.cpp`
**Key code path:** `solver::run_iteration()` → `contact_model::run()` → `node::add_force()` → `vec3::translate()`
**Interaction with other proposals:** Unblocks A4 (mutex removal becomes safe after force accumulation is thread-local). Thread-local buffers synergize with B1 (SoA force arrays become natural accumulation targets). Must handle dynamic node creation during mesh refinement — buffers must resize or use sparse indexing.

</details>

<details>
<summary><strong>Acceptance Criteria</strong></summary>

- [ ] Force values bitwise identical in single-thread mode (no numerical change)
- [ ] ThreadSanitizer clean with 8+ threads
- [ ] Speedup measured on 4, 8, 16, 32+ cores — expect 15–20% end-to-end improvement
- [ ] All existing tests pass unchanged
- [ ] Memory overhead documented and within O(threads × nodes × 24B) bound

</details>

---

### A2: Inline Coupling Buffer

**Goal:** Eliminate per-timestep heap allocation overhead by replacing `std::map` coupling storage with a fixed-size inline buffer that fits in a single cache line.

| Impact | Risk | Phase | Dependencies |
|:------:|:----:|:-----:|:------------:|
| 8/10 | Low-Med | 1 | None |

**Use Case:** A soft-tissue contact simulation runs 10K timesteps with 500 cells (50K nodes) on a dual-socket EPYC. Each timestep constructs and destructs 20K `std::map` instances for node coupling data, generating ~7.6M cycles of pure allocator overhead per timestep (~3ms). After switching to `std::array<coupling_entry, 8>` with small-buffer fallback, heap allocations drop to zero for 96% of nodes. Coupling setup time drops from 12ms to 2ms per timestep.

#### Current Limitation

`coupled_nodes_map_` at `node.hpp:98` is `std::map<unsigned, std::pair<unsigned, double>>`. Cleared every timestep at `contact_face_face_via_coupling.cpp:48`, then rebuilt via `find()` + `insert()` in the contact inner loop (lines 284–315), then iterated 4+ times in time integration (lines 215, 234, 269, 281, 311). Each map entry requires a red-black tree node allocation (~64–80 bytes overhead on ~20-byte payload). With 20K nodes, that is 20K map construction/destruction cycles per timestep. The coupling count is bounded by neighbor count (~6–8 cells).

#### Proposed Change

Fixed-size inline buffer with small-buffer overflow fallback:

```cpp
struct coupling_entry {
    unsigned cell_id;
    unsigned node_id;
    double squared_distance;
};
std::array<coupling_entry, 8> coupled_nodes_;
uint8_t coupled_count_ = 0;
// Fallback: std::vector for rare >8 entries on unusual geometries
```

132 bytes inline, zero heap allocations for the 96th-percentile case. Linear search over ≤8 contiguous entries is faster than tree traversal at this size.

#### Trade-offs

| Advantages | Risks & Costs |
|------------|---------------|
| Eliminates ~20K `std::map::clear()` + rebuild cycles per timestep with heap allocations | `find()` semantics at `contact_face_face_via_coupling.cpp:284` need translation to linear search |
| Linear scan on 6–8 contiguous entries is faster than tree traversal (cache locality) | Mutex-guarded `set_coupled_node_and_min_distance()` at `node.hpp:189-209` uses map iterators; control flow changes needed |
| Directly reduces per-node memory footprint; eliminates biggest allocator-pressure source | Needs overflow fallback for unusual geometries where coupled count exceeds 8 |

**Verdict:** High-frequency operation with known allocation hotspot. Implement alongside A3 as a coordinated change. Use `boost::container::small_vector<coupling_entry, 8>` if available, otherwise manual fallback.

<details>
<summary><strong>Implementation Guide</strong></summary>

**Files to modify:** `include/mesh/node.hpp`, `src/contact_models/contact_face_face_via_coupling.cpp`, `src/time_integration/time_integration.cpp`
**Key code path:** `contact_model::run()` → `node::set_coupled_node_and_min_distance()` (writes) → `time_integration::update_nodes_positions()` (reads)
**Interaction with other proposals:** Prerequisite for A4 — once coupling storage is inline, mutex protection scope changes fundamentally. Verify coupled node counts don't spike during mesh refinement; if >5% of cases need >8 entries, fallback path must be equally optimized.

</details>

<details>
<summary><strong>Acceptance Criteria</strong></summary>

- [ ] Memory profiler confirms zero heap allocations in contact loop for ≤8 coupled nodes
- [ ] All contact model tests pass with bitwise-identical force values
- [ ] Coupling overflow fallback triggers correctly for >8 entries without crash
- [ ] Per-timestep allocation count measured and documented (before/after)

</details>

---

### A8: Intrusive Pointer for Cell

**Goal:** Reduce reference-counting overhead and improve cache locality by embedding the refcount directly in the `cell` class, eliminating separate control block allocations.

| Impact | Risk | Phase | Dependencies |
|:------:|:----:|:-----:|:------------:|
| 7/10 | Medium | 2 | None |

**Use Case:** A 1,000-cell simulation iterates `cell_lst_` (a `vector<shared_ptr<cell>>`) on every timestep for contact model setup, time integration, and mesh refinement. Each `shared_ptr` access requires two pointer indirections — one for the control block (16-byte header scattered across heap), one for the cell object — producing ~2.1 L3 cache misses per cell access. After switching to intrusive pointers (refcount embedded at cell offset 0), control block allocations vanish, and the prefetcher can stride through cells linearly, reducing cache misses to ~0.8 per cell access. Measured: 5–8ms improvement per timestep on cell-iteration-heavy phases.

#### Current Limitation

`std::shared_ptr<cell>` is used throughout the codebase for cell ownership (`cell_lst_` in solver). Each `shared_ptr` has a separately-allocated control block (~24 bytes: refcount, weak count, deleter) plus atomic reference count operations on every copy/assign. The control blocks are scattered across the heap, causing cache misses when iterating `cell_lst_`.

#### Proposed Change

Add `mutable std::atomic<uint32_t> refcount_` to the `cell` class (4 bytes) and implement a custom `intrusive_ptr<cell>`:

```cpp
class cell {
    mutable std::atomic<uint32_t> refcount_{0};
    friend void intrusive_ptr_add_ref(const cell* p) noexcept;
    friend void intrusive_ptr_release(const cell* p) noexcept;
    // ...
};
```

#### Trade-offs

| Advantages | Risks & Costs |
|------------|---------------|
| Eliminates separate control block allocation per cell; improves cache locality | Touches every `shared_ptr<cell>` usage throughout the codebase |
| Reduces cache misses during cell iteration from ~2.1 to ~0.8 per cell | `std::weak_ptr` is not available with intrusive pointers — must audit for usage |
| 4-byte embedded refcount vs 24-byte separate control block | Thread-safety requires careful `std::memory_order` annotations (relaxed increment, acquire decrement) |

**Verdict:** The real win is cache locality improvement, not atomic overhead reduction. Profile cache misses first — if cell iteration isn't a bottleneck, defer this.

<details>
<summary><strong>Implementation Guide</strong></summary>

**Files to modify:** `include/mesh/cell.hpp`, `src/solver.cpp`, all files using `std::shared_ptr<cell>`
**Key code path:** `solver::cell_lst_` → all cell iteration loops in contact models, time integration, mesh refinement
**Interaction with other proposals:** Complements B1 (SoA storage reduces cell access frequency). Conflicts with B5 (NUMA-aware allocation via `std::allocate_shared` requires control blocks). Must audit for `weak_ptr<cell>` usage — intrusive pointers cannot support weak references without additional infrastructure.

</details>

<details>
<summary><strong>Acceptance Criteria</strong></summary>

- [ ] All existing tests pass unchanged
- [ ] Memory profiler shows reduced allocation count (no control block allocations)
- [ ] No leaks under valgrind/ASan
- [ ] Cache miss rate on cell iteration measured before/after (expect ~60% reduction)
- [ ] No `weak_ptr<cell>` usage found in audit (or alternative provided)

</details>

---

### B1: SoA Node Storage for Hot-Path Data

**Goal:** Enable SIMD vectorization and reduce cache bandwidth waste in time integration by restructuring node storage into separate contiguous arrays for position, force, and momentum.

| Impact | Risk | Phase | Dependencies |
|:------:|:----:|:-----:|:------------:|
| 7/10 | V.High | 4 | A3, B6 |

**Use Case:** A 100K-node simulation runs time integration on a 32-core Ice Lake Xeon with AVX-512. Current AoS layout loads the entire 280-byte `node` struct per iteration, but time integration needs only 72 bytes (position, force, momentum). This wastes ~14.3MB of cache bandwidth per timestep. After SoA conversion, cache line utilization jumps from 26% to 100%, and time integration drops from 12ms to 8ms. However, contact detection (40–50% of runtime) still pointer-chases through neighbor lists — SoA doesn't help pointer-chasing workloads. Realistic overall speedup: 1.1–1.2×.

#### Current Limitation

Each `node` is ~220–280 bytes (includes `std::mutex` at 40B, `std::map` with heap pointers, `normal_`, `curvature_`) but hot loops need only 72 bytes (position, force, momentum). AoS layout means time integration at `time_integration.cpp:24-67` loads 3–3.9× more data than needed. Time integration is 5–15% of runtime; contact detection (40–50%) is pointer-chasing through neighbor lists, where SoA provides no benefit.

#### Proposed Change

Add parallel contiguous arrays at cell level:

```cpp
std::vector<vec3> node_positions_;  // Cache-line aligned
std::vector<vec3> node_forces_;
std::vector<vec3> node_momenta_;    // Only if DYNAMIC_MODEL_INDEX==0
```

Keep `node_lst_` for topology/metadata. Sync positions at phase boundaries. Start with SoA view layer over existing AoS, then migrate hot paths incrementally.

#### Trade-offs

| Advantages | Risks & Costs |
|------------|---------------|
| 3–3.9× improvement in cache utilization for time integration loop | Massive refactoring scope: `node` accessed via `node_lst_[]` throughout cell.cpp, contact models, mesh refiner |
| Enables SIMD auto-vectorization of integration step (contiguous doubles) | Dual-representation requires sync discipline; any code assuming stable `node*` addresses breaks |
| Natural synergy with A3 (force arrays become thread-local accumulation targets) | Time integration is only 5–15% of runtime; contact detection (40–50%) is unaffected |

**Verdict:** Realistic overall speedup is 1.1–1.2×, not the 2–3× suggested by isolated benchmarks. Defer until B6 (LIKWID profiling) confirms memory bandwidth is actually the bottleneck in time integration. The engineering cost of this refactor is only justified if profiling shows time integration exceeds 20% of runtime.

<details>
<summary><strong>Implementation Guide</strong></summary>

**Files to modify:** `include/mesh/cell.hpp`, `src/mesh/cell.cpp`, `src/time_integration/time_integration.cpp`, all files using `node_lst_[]` accessors
**Key code path:** `time_integration::update_nodes_positions()` → per-node position/force/momentum access
**Interaction with other proposals:** Requires A3 (thread-local forces map naturally to SoA force arrays). Must be validated by B6 (LIKWID profiling confirms memory bandwidth saturation). Node ID stability required — no reallocation during iteration. If any code stores `node*` across phases, SoA is impossible without intrusive refactoring.

</details>

<details>
<summary><strong>Acceptance Criteria</strong></summary>

- [ ] Position values identical to AoS version (bitwise in single-thread)
- [ ] Memory bandwidth measurement via LIKWID confirms time integration was memory-bandwidth-bound
- [ ] Time integration speedup measured (expect 1.3–1.8× on integration phase, 1.1–1.2× overall)
- [ ] SIMD auto-vectorization confirmed via compiler reports (`-fopt-info-vec` or `-Rpass=loop-vectorize`)
- [ ] All existing tests pass unchanged

</details>

---

### A4: Eliminate Per-Node `std::mutex`

**Goal:** Reclaim 800KB of cache-polluting memory and simplify node semantics by removing per-node mutexes after A2+A3 render them unnecessary.

| Impact | Risk | Phase | Dependencies |
|:------:|:----:|:-----:|:------------:|
| 6/10 | Medium | 1 | A2, A3 |

**Use Case:** A 2,000-cell simulation (200K nodes) allocates 8MB for node mutexes (200K × 40 bytes), polluting L3 cache and causing TLB pressure. After A2 replaces `std::map` with an inline array and A3 moves force accumulation to thread-local buffers, the only remaining shared write is the inline array update. Replacing the mutex with an `atomic<uint32_t>` version counter (4 bytes) drops per-node sync overhead from 40 bytes to 4 bytes. Memory saving: 7.2MB for 200K nodes. Mutex construction cost (~20M cycles upfront) is also eliminated.

#### Current Limitation

Every node stores a 40-byte `std::mutex` (`node.hpp:75`). For 20K nodes = 800KB of mutex storage displacing useful data from cache. Forces custom copy/move constructors (`node.cpp` has 217 lines total). Only used in `set_coupled_node_and_min_distance()` (`node.hpp:189-209`). The original suggestion of lock-free CAS on `squared_distance` is architecturally wrong — the critical section protects the entire map operation (find + conditional insert), not just distance comparison.

#### Proposed Change

After A2 (inline buffer) and A3 (thread-local forces), the mutex becomes unnecessary because: (1) thread-local force buffers eliminate atomic force writes, and (2) inline buffer with atomic `coupled_count_` enables lock-free updates. Implement as part of the coordinated A2+A3+A4 change. Remove mutex, delete custom copy/move constructors (~110 lines of boilerplate), and add `memory_order_acq_rel` on remaining atomic operations.

#### Trade-offs

| Advantages | Risks & Costs |
|------------|---------------|
| Reclaims 40 bytes per node; improves cache utilization during sequential iteration | Cannot be done independently — mutex protects map operations that A2 must replace first |
| Simplifies node copy/move semantics (no special mutex handling); ~110 lines deleted | Empty parallel region barrier at `solver.cpp:1107-1112` exists to prevent mutex destruction races; removal changes synchronization |
| Eliminates false cache-line sharing from mutex padding | Incorrect removal causes silent data races — requires thorough TSan validation |

**Verdict:** Not a standalone optimization — it's a cleanup after A2+A3. Implement as the final step of the coordinated A2+A3+A4 change. Validate with ThreadSanitizer at `OMP_NUM_THREADS=64+`.

<details>
<summary><strong>Implementation Guide</strong></summary>

**Files to modify:** `include/mesh/node.hpp`, `src/mesh/node.cpp`, `src/solver.cpp` (barrier at line 1107-1112)
**Key code path:** `contact_model::run()` → `node::set_coupled_node_and_min_distance()` → (currently mutex-guarded)
**Interaction with other proposals:** Blocked by A2 (inline buffer replaces map, changing what the mutex protects) and A3 (thread-local forces eliminate the atomic force write path). After both are complete, the mutex becomes dead code. D4 (TSan in CI) validates the removal.

</details>

<details>
<summary><strong>Acceptance Criteria</strong></summary>

- [ ] ThreadSanitizer clean with 8, 16, 32, 64+ threads
- [ ] Coupling results identical to mutex version (bitwise)
- [ ] `node.cpp` reduced by ~110 lines (custom copy/move constructors removed)
- [ ] Memory profiler confirms 40 bytes/node reclaimed

</details>

---

### A6: USPG Grid Memory Reuse

**Goal:** Reduce allocator thrashing in spatial grid updates by reusing voxel vector capacity across timesteps instead of destroying and reconstructing ~260K containers.

| Impact | Risk | Phase | Dependencies |
|:------:|:----:|:-----:|:------------:|
| 6/10 | Low | 0 | None |

**Use Case:** A 500-cell epithelial sheet simulation uses a 64×64×64 USPG grid (~262K voxels, ~80K occupied). Each timestep, `update_dimensions()` destroys all voxels and reconstructs them, costing ~12M cycles from vector destructor/constructor pairs (~5ms at 2.5 GHz). After optimization — checking if grid dimensions changed (unchanged in 90% of timesteps for confined simulations) and only calling `clear()` on each voxel (preserving allocated capacity) — grid update drops from 18ms to 2ms. Over 5K timesteps, this saves ~80 seconds wall-clock. A good Phase 0 warmup task for learning the USPG codebase.

#### Current Limitation

`update_dimensions()` in `uspg_4d.hpp:60-94` calls `voxel_lst_.clear()` + `voxel_lst_.resize()` every timestep, destroying and reconstructing all `vector<face*>` objects. For occupied voxels, this triggers heap deallocation followed by reallocation during the same timestep.

#### Proposed Change

1. Check if grid dimensions changed since last timestep
2. If unchanged, only `clear()` each voxel's vector (preserving allocated capacity)
3. Pre-reserve voxel capacity based on first-timestep occupancy statistics: after the first timestep, record max occupancy per voxel; on subsequent timesteps, reserve this capacity to eliminate both destruction overhead and reallocation within each voxel

#### Trade-offs

| Advantages | Risks & Costs |
|------------|---------------|
| Eliminates repeated heap allocation/deallocation of inner vectors | When grid dimensions change (cell growth/division), full destruction is still needed |
| Trivial to implement: replace `clear()+resize()` with per-voxel `clear()` | "Wasted" capacity in empty voxels could be significant for sparse grids |
| Preserves existing API and grid semantics entirely | Must benchmark `clear()` vs `resize(0)` — some STL implementations don't preserve capacity with `clear()` |

**Verdict:** Quick win with minimal risk. Use as Phase 0 warmup to learn the USPG codebase. Expected speedup: 1.02–1.05× overall.

<details>
<summary><strong>Implementation Guide</strong></summary>

**Files to modify:** `include/uspg/uspg_4d.hpp` (lines 60–94)
**Key code path:** `solver::run_iteration()` → `uspg_4d::update_dimensions()` → voxel destruction/reconstruction
**Interaction with other proposals:** Independent of all other proposals. Pre-reserve strategy provides data for B2 (hash grid decision — if most voxels are occupied, hash grid is unnecessary).

</details>

<details>
<summary><strong>Acceptance Criteria</strong></summary>

- [ ] Grid contents identical to current implementation (bitwise)
- [ ] Wall-clock improvement measured on representative simulation (expect 1.02–1.05×)
- [ ] Heap allocation count per timestep reduced (measured via profiler)
- [ ] Grid still functions correctly when dimensions change (cell growth/division)

</details>

---

### A9: Pool Allocator for Nodes and Faces

**Goal:** Eliminate allocator fragmentation and reduce allocation latency during mesh refinement by using fixed-size memory pools for node and face objects.

| Impact | Risk | Phase | Dependencies |
|:------:|:----:|:-----:|:------------:|
| 6/10 | Medium | 4 | None |

**Use Case:** A mesh-refinement-heavy simulation (100 refinements per timestep, each splitting 50 faces into 150 new faces + 100 new nodes) performs ~25K allocations per timestep. The standard allocator in multi-threaded context costs ~120–200 cycles per malloc due to lock contention, totaling ~3–5M cycles per timestep (~2ms). After switching to a pool allocator (pre-allocated 256KB chunks, each holding 64 face objects), allocation drops to ~20 cycles per operation (bump-pointer + bounds check). Refinement phase drops from 45ms to 28ms. Pool allocators also improve spatial locality — nodes/faces allocated together stay together in memory.

#### Current Limitation

`node_lst_` and `face_lst_` use `std::vector` with the standard allocator. Mesh refinement creates and destroys nodes frequently, causing allocator thrashing with many small, short-lived allocations.

#### Proposed Change

Custom pool allocator with fixed-size blocks. Pre-allocate blocks for typical cell sizes (500 nodes, 1000 faces). Avoid per-element heap allocation during mesh refinement and cell division. Use per-thread pools for thread safety, or lock-free freelist for cross-thread allocation.

#### Trade-offs

| Advantages | Risks & Costs |
|------------|---------------|
| 5–10% speedup on mesh refinement and cell division paths; improved spatial locality | Pool must be thread-safe (separate pools per thread or lock-free freelist) |
| Reduces allocator pressure from frequent node/face creation | Face/node lifetimes aren't strictly LIFO — needs free-list for deleted objects |
| Well-understood pattern for simulation codes | Existing `free_node_queue_` and `free_face_queue_` recycling conflicts with pool allocator — must reconcile |

**Verdict:** Worthwhile but requires careful design around existing free queues. Consider Boost.Pool or custom slab allocator with per-thread magazines.

<details>
<summary><strong>Implementation Guide</strong></summary>

**Files to modify:** `include/mesh/cell.hpp` (node/face storage), `src/triangulation_modules/local_mesh_refiner.cpp`, `src/triangulation_modules/cell_divider.cpp`
**Key code path:** `local_mesh_refiner::refine()` → node/face allocation → `cell::free_node_queue_` / `cell::free_face_queue_` recycling
**Interaction with other proposals:** Must reconcile with existing `free_node_queue_` / `free_face_queue_` recycling (cell.hpp:80-81). Either make pool aware of free queues, or disable recycling and rely on pool's internal freelist. A10 (small-vector for free queues) becomes moot if pool allocator handles recycling.

</details>

<details>
<summary><strong>Acceptance Criteria</strong></summary>

- [ ] All mesh refinement tests pass
- [ ] Memory profiler confirms reduced allocation count during refinement
- [ ] No leaks under ASan/valgrind
- [ ] Refinement phase wall-clock time reduced (expect 5–10%)
- [ ] Pool handles thread-safe allocation from parallel mesh refinement

</details>

---

### A1: Flat Sorted Edge Vector

**Goal:** Eliminate red-black tree overhead in edge storage to reduce per-cell memory footprint and improve cache locality during bending force computation.

| Impact | Risk | Phase | Dependencies |
|:------:|:----:|:-----:|:------------:|
| 5/10 | Med-High | 2 | None |

**Use Case:** A 200-cell simulation (30K total edges, averaging 150 edges per cell) consumes ~1.35MB for edge storage due to `std::set` per-node overhead (~45 bytes). During `apply_bending_forces()`, pointer-chasing through red-black tree nodes causes ~40% L3 cache miss rate. After migration to a sorted `std::vector`, edge storage drops to ~720KB and sequential iteration reduces L3 misses to ~8%. However, bending force computation is only 8–12% of iteration time, so end-to-end speedup is 1.5–2.5%, not the 5–8% implied by isolated benchmarks.

#### Current Limitation

`edge_set_` at `include/mesh/cell.hpp:44` is `typedef std::set<edge> edge_set` — a red-black tree with ~40–48 bytes overhead per node. Iterated sequentially in `apply_bending_forces()` (`cell.cpp:1453`) and `compute_node_curvature_and_normals()` (`cell.cpp:297`), causing cache misses. A commented-out `unordered_set` at line 43 suggests this bottleneck has been noticed before. The `const_cast` pattern at `cell.cpp:505, 509, 513` is undefined behavior and must be fixed during this change.

#### Proposed Change

Keep `std::vector<edge>` unsorted during mutations. Sort once before iteration in `apply_bending_forces()`. Amortize: sort cost (~2400 comparisons for 300 edges) vs iteration benefit (~300 cache-friendly accesses).

#### Trade-offs

| Advantages | Risks & Costs |
|------------|---------------|
| Eliminates per-edge heap allocation and pointer chasing in two hot iteration loops | Mesh refinement uses heavy insert/erase patterns that become O(N) with sorted vector |
| 10× faster refiner copy (contiguous memcpy vs tree deep copy) | `replace_node()` at `cell.cpp:620-727` does interleaved find/erase/emplace in a loop — hostile to sorted vectors |
| ~12KB memory saved per cell at 300 edges | Requires fixing `const_cast` UB on set iterators (cell.cpp:505, 509, 513) — a must-fix blocker |

**Verdict:** The `const_cast` UB is a blocker that must be fixed regardless. If profiling confirms >15% of time in edge iteration, proceed with the vector conversion; otherwise, fix the UB only.

<details>
<summary><strong>Implementation Guide</strong></summary>

**Files to modify:** `include/mesh/cell.hpp` (edge_set typedef), `src/mesh/cell.cpp` (bending forces, curvature, replace_node, const_cast sites)
**Key code path:** `cell::apply_bending_forces()` → edge iteration; `local_mesh_refiner::refine()` → edge insert/erase
**Interaction with other proposals:** Independent of other proposals. The `const_cast` UB fix (cell.cpp:505, 509, 513) should be done as part of D5 (bug fixes) regardless of whether the vector conversion proceeds.

</details>

<details>
<summary><strong>Acceptance Criteria</strong></summary>

- [ ] Benchmark bending force loop before/after (expect 3–5× faster iteration)
- [ ] All existing tests pass unchanged
- [ ] `const_cast` UB eliminated
- [ ] End-to-end speedup measured (expect 1.5–2.5%)

</details>

---

### A10: Small-Vector Optimization for Free Queues

**Goal:** Reduce heap allocations for transient free queues by using stack-allocated small-buffer optimization for the common case of fewer than 8 entries.

| Impact | Risk | Phase | Dependencies |
|:------:|:----:|:-----:|:------------:|
| 5/10 | Low | 2 | None |

**Use Case:** A 500-cell simulation refines meshes 80 times per timestep. Each refinement appends 2–5 entries to `free_node_queue_` and `free_face_queue_`, then clears them after reuse. Current `std::vector` always heap-allocates even for small sizes. After switching to `small_vector<unsigned, 8>`, first 8 entries are stored inline (on the stack), eliminating allocations for 92% of refinements. Cache locality also improves — queues fit in the same cache line as refinement state.

#### Current Limitation

`free_node_queue_` and `free_face_queue_` at `cell.hpp:80-81` are `std::vector<unsigned>`. Typically contain 0–8 entries but always heap-allocate.

#### Proposed Change

Use `boost::container::small_vector<unsigned, 8>` or a custom small-vector implementation. First 8 entries stored inline (32 bytes on stack), overflow to heap allocation.

#### Trade-offs

| Advantages | Risks & Costs |
|------------|---------------|
| Eliminates heap allocation for the common case (0–8 entries) | Adds boost dependency or requires custom implementation |
| Drop-in replacement with identical API | Marginal performance benefit (~10μs per timestep) |
| Improves cache locality during refinement | Overflow case still heap-allocates |

**Verdict:** A code-quality improvement more than a performance win. Implement as cleanup during or after A9 (pool allocator), which may make this moot.

<details>
<summary><strong>Implementation Guide</strong></summary>

**Files to modify:** `include/mesh/cell.hpp` (free_node_queue_, free_face_queue_ typedefs)
**Key code path:** `local_mesh_refiner::refine()` → `cell::free_node_queue_` / `cell::free_face_queue_`
**Interaction with other proposals:** A9 (pool allocator) may make this unnecessary if pool handles recycling internally. Consider implementing as part of A9.

</details>

<details>
<summary><strong>Acceptance Criteria</strong></summary>

- [ ] All mesh refinement and cell division tests pass
- [ ] Memory profiler confirms reduced small-allocation count
- [ ] No heap allocation for queues with ≤8 entries

</details>

---

### B2: Compact Hash Grid + Morton Ordering

**Goal:** Reduce memory footprint in sparse USPG grids by replacing the fixed-size 3D array with an open-addressing hash table using Morton Z-curve key encoding.

| Impact | Risk | Phase | Dependencies |
|:------:|:----:|:-----:|:------------:|
| 4/10 | Medium | — | None |

**Use Case:** A large-scale simulation spans a 128×128×128 grid (2M voxels) but only 150K are occupied (7.3% occupancy). Current implementation allocates 16MB for empty voxel pointers. After switching to a hash map with Morton keys, memory drops to ~4.8MB. However, hash lookups add ~15ns overhead vs direct indexing (~2ns), and during contact detection the grid is queried ~500K times per timestep, adding 6.5ms of hash overhead. Morton ordering improves cache clustering by ~1–2ms. Net result: approximately neutral or slightly negative performance impact.

#### Current Limitation

`uspg_4d` allocates `vector<vector<face*>>` with `nx*ny*nz` entries. The original calculation claiming "888 MB" is incorrect — after `resize()`, empty vectors are zero-initialized and modern allocators use empty-vector optimization. Actual memory is ~72MB (24 bytes × 3M occupied voxels), not 888MB.

#### Proposed Change

Open-addressing hash map with Morton (Z-curve) key encoding. Only stores occupied voxels — memory O(N_faces) instead of O(N_voxels). Incremental update: 95%+ of faces stay in the same voxel, so only re-hash moved faces.

#### Trade-offs

| Advantages | Risks & Costs |
|------------|---------------|
| Reduces memory for sparse grids (eliminates empty voxel storage) | Hash table lookup has higher constant overhead than direct array indexing (~15ns vs ~2ns) |
| Morton ordering improves cache hits for spatially neighboring voxel access | Morton encoding/decoding adds instruction overhead (9 shifts + 9 ANDs + 9 ORs per lookup) |
| Incremental update leveraging temporal coherence is the main value | Current direct indexing is already well-optimized; memory saving (72→50MB) is only 28% |

**Verdict:** Memory reduction is modest (28%), and performance impact is approximately neutral. Deprioritize to prototype-and-benchmark category. Only pursue if profiling shows USPG memory pressure is causing cache evictions in other hot paths.

<details>
<summary><strong>Implementation Guide</strong></summary>

**Files to modify:** `include/uspg/uspg_4d.hpp`
**Key code path:** `uspg_4d::update_grid()` → voxel lookup; `uspg_4d::get_neighbor_faces()` → 3×3×3 neighborhood traversal at lines 166–181
**Interaction with other proposals:** A6 (USPG memory reuse) addresses the allocation overhead more simply. If A6 is sufficient, B2 becomes unnecessary.

</details>

<details>
<summary><strong>Acceptance Criteria</strong></summary>

- [ ] Contact forces identical to current implementation
- [ ] Memory profiler shows reduction for sparse grids
- [ ] Prototype benchmark before committing to full implementation — must demonstrate positive performance impact

</details>

---

## HPC Scalability

These proposals target infrastructure for large-scale and long-running simulations: crash recovery (B7), diagnostic tooling (B6), multi-socket scaling (B5), and GPU/distributed computing (B3, B4). The highest-value investment is checkpoint/restart (B7), which is transformative for any simulation running more than a few hours. LIKWID integration (B6) is low-effort diagnostic infrastructure that informs decisions for B1, B5, and B3. GPU acceleration and MPI decomposition are deferred — current problem sizes (100–10,000 cells) fit comfortably on single-node OpenMP, and Amdahl's Law limits GPU benefit to ~1.6× overall.

### B7: Parallel I/O + Checkpoint/Restart

**Goal:** Implement binary checkpointing with async I/O to enable crash recovery for long-running simulations and reduce output overhead from blocking serial ASCII writes.

| Impact | Risk | Phase | Dependencies |
|:------:|:----:|:-----:|:------------:|
| 8/10 | Med-High | 4 | None |

**Use Case:** A developmental biologist runs a 5,000-cell gastrulation simulation over 100,000 timesteps (6 days runtime) on a 64-core server, outputting VTK meshes every 1,000 timesteps. On day 4 (iteration 67,000), a power outage crashes the simulation, losing 4 days of work. After implementing Phase 1 (binary checkpoint), the simulation writes compressed snapshots (~40–50MB with zstd) to local NVMe in a background thread with zero simulation blocking. When the crash occurs, the simulation restarts from the most recent checkpoint (iteration 66,000), losing 1 hour instead of 4 days.

#### Current Limitation

VTK output in `src/io/mesh_writer.cpp` is entirely serial ASCII I/O, blocking the simulation loop. ASCII format is verbose (~12 bytes per coordinate vs 4–8 bytes binary). For 1,000 cells × 500 nodes × 3 coords, ASCII is ~18MB vs binary ~6MB per frame. No checkpoint/restart capability — a simulation that crashes at iteration 9,000 of 10,000 must restart from scratch. The `#pragma omp parallel sections` at `mesh_writer.cpp:26` suggests the codebase has two independent serial streams, not true parallel I/O.

#### Proposed Change

Phased implementation:

1. **Phase 1:** Lightweight binary checkpoint/restart — serialize full simulation state (`cell_lst_`, `sim_parameters_`, iteration count) to a simple binary format with zstd compression. Checkpoint data: ~120MB node positions/velocities + ~2.5MB cell metadata = ~40–50MB compressed. Async writer thread with double-buffering for zero blocking.
2. **Phase 2:** HDF5 parallel I/O for mesh output + XDMF wrapper for ParaView compatibility. Async I/O thread to overlap writing with computation.

Note: I/O overhead for typical simulations with 100–1,000 output frames is <1% of total runtime. The primary value of this proposal is checkpoint/restart, not output format optimization.

#### Trade-offs

| Advantages | Risks & Costs |
|------------|---------------|
| Checkpoint/restart is transformative for long-running simulations; no crash recovery currently exists | HDF5 is a heavy dependency (~50MB library) that complicates the build system |
| Binary checkpoint + zstd reduces snapshot size to ~40–50MB with zero-blocking async writes | HDF5 is not thread-safe by default; thread-safe mode disables C++ API |
| Simulation state is well-defined: `cell_lst_`, `sim_parameters_`, iteration count | VTK→HDF5+XDMF transition breaks existing ParaView visualization workflows |

**Verdict:** Start with Phase 1 (binary checkpoint, no HDF5 dependency). Measure I/O time in production first — if output overhead is <5% of iteration time, defer Phase 2. The real value is checkpoint/restart, not output format.

<details>
<summary><strong>Implementation Guide</strong></summary>

**Files to modify:** `src/io/mesh_writer.cpp`, new `src/io/checkpoint_writer.cpp`, `src/solver.cpp` (checkpoint trigger)
**Key code path:** `solver::run_iteration()` → (every N iterations) → `checkpoint_writer::write_async()` → background thread with double-buffering
**Interaction with other proposals:** Independent of all other proposals. If B4 (MPI) is ever implemented, parallel HDF5 provides the I/O backend.

</details>

<details>
<summary><strong>Acceptance Criteria</strong></summary>

- [ ] Checkpoint/restart produces bitwise identical continuation from checkpoint
- [ ] Checkpoint write does not block simulation thread (async double-buffering)
- [ ] Checkpoint file size documented (~40–50MB compressed for 5,000-cell simulation)
- [ ] Output file size reduction measured (ASCII vs binary)
- [ ] ParaView can read HDF5+XDMF output (Phase 2 only)

</details>

---

### B6: LIKWID Hardware Counter Integration

**Goal:** Integrate LIKWID hardware counters with existing PerformanceMonitor to distinguish memory-bound from compute-bound phases, enabling data-driven optimization decisions.

| Impact | Risk | Phase | Dependencies |
|:------:|:----:|:-----:|:------------:|
| 7/10 | Low | 0 | None |

**Use Case:** A computational biologist profiles a 1,500-cell simulation that runs slower than expected on a 48-core Xeon (28 hours vs expected 15). Wall-clock timing shows contact detection takes 60%, but doesn't reveal why. After integrating LIKWID, the researcher wraps `contact_detection_step()` with `LIKWID_MARKER_REGION("contact")` and discovers 0.35 FLOPS/cycle (vs 2.0 peak) with 85% memory bandwidth saturation and 45% L3 cache miss rate. This confirms memory-bound behavior due to poor spatial locality — motivating Morton curve cell sorting, which improves cache hit rate to 75% and delivers a 1.8× speedup on contact detection. Without hardware counters, this diagnosis would have been impossible — wall-clock timing alone cannot distinguish contention-bound from memory-bound behavior.

#### Current Limitation

Existing `PerformanceMonitor` (`include/performance_monitor.hpp`, 319 lines) provides wall-clock timing with `ScopedTimer`, `TimingStats` (min/max/avg/std_dev), `ThreadBalanceStats`, and CSV export. Already integrated in `solver.cpp` with scoped timers for contact_detection, mesh_refinement, time_integration, and polarization. Cannot distinguish memory-bound vs compute-bound phases. No hardware performance counter data (L1/L2/L3 cache miss rates, FLOP/s, memory bandwidth). Rootless `perf_event` access is standard on modern Linux (kernel ≥5.10).

#### Proposed Change

Wrap existing `ScopedTimer` regions with `LIKWID_MARKER_START`/`LIKWID_MARKER_STOP` calls:

```cpp
void contact_detection_step() {
    ScopedTimer timer("contact_detection");
    LIKWID_MARKER_START("contact");
    // ... existing code ...
    LIKWID_MARKER_STOP("contact");
}
```

Recommended counter groups for SimuCell3D: MEM_DP (bandwidth + FLOPS), CACHE (L1/L2/L3 miss rates), NUMA (remote vs local access), BRANCH (misprediction rate). Build system: add `-DLIKWID_PERFMON` compile flag, link `-llikwid`.

#### Trade-offs

| Advantages | Risks & Costs |
|------------|---------------|
| Adds hardware counter data (cache misses, FLOP/s, bandwidth) that wall-clock cannot provide | LIKWID is Linux-only, external dependency with non-trivial installation |
| Integration is lightweight: wrap existing ScopedTimer regions, no ABI changes | Modern CPUs support 4–8 simultaneous counters; cannot measure all metrics in one run |
| Multiplier effect: enables data-driven decisions for B1, B5, A1, and other proposals | Not an end-user feature — developer infrastructure only |

**Verdict:** Essential diagnostic infrastructure. 7/10 impact is justified as a multiplier — it unlocks 4–5 other optimizations worth 2–10× each by answering "why slow?" rather than just "how long?"

<details>
<summary><strong>Implementation Guide</strong></summary>

**Files to modify:** `include/performance_monitor.hpp` (add LIKWID wrapper), `CMakeLists.txt` (add LIKWID dependency), new `doc/performance-tuning.md` (document counter groups)
**Key code path:** Wrap existing `ScopedTimer` regions in `solver.cpp` with LIKWID markers
**Interaction with other proposals:** Informs B1 (is time integration memory-bandwidth-bound?), B5 (is cross-socket access >30% of bandwidth?), A1 (is bending force computation >15% of runtime?). Should be implemented in Phase 0 before committing to any major data structure refactoring.

</details>

<details>
<summary><strong>Acceptance Criteria</strong></summary>

- [ ] LIKWID markers produce valid roofline plots for each simulation phase
- [ ] FLOPS and bandwidth measurements match analytical expectations for simple benchmarks
- [ ] Build system supports optional LIKWID integration (compile flag, graceful fallback if absent)
- [ ] Counter groups documented with interpretation guidance

</details>

---

### B5: NUMA-Aware Memory Allocation

**Goal:** Eliminate cross-socket memory access penalties by implementing first-touch NUMA-aware allocation, improving OpenMP scaling efficiency on dual-socket systems with 32+ cores.

| Impact | Risk | Phase | Dependencies |
|:------:|:----:|:-----:|:------------:|
| 4/10 | Medium | — | B6 |

**Use Case:** A tissue engineer runs a 4,000-cell simulation (2M nodes) on a dual-socket AMD EPYC 7763 (2× 32 cores, 256GB per socket). The main thread allocates all cell data on NUMA node 0, causing threads on node 1 (cores 32–63) to suffer 100ns remote memory latency (vs 50ns local). Profiling shows 60% of memory traffic is cross-socket, degrading OpenMP efficiency from 80% (expected) to 55% (actual). After first-touch initialization (parallel region allocates cells so each thread's data lands on its local NUMA node), remote access drops to 15%, and OpenMP efficiency rises to 75%, reducing simulation time from 14 hours to 9 hours.

#### Current Limitation

All cell data is allocated by the main thread on NUMA node 0 (no NUMA-specific allocation in codebase). Cross-socket memory access adds 50–100ns latency, roughly 1.7× penalty. NUMA effects are negligible below ~32 cores (single-socket limit). `std::shared_ptr<cell>` is hostile to custom allocators — each cell is a separate heap allocation with its own control block. First-touch via `#pragma omp parallel for` during initialization requires changing `shared_ptr<cell>` to by-value storage — an ABI-breaking change.

#### Proposed Change

First-touch initialization: allocate cell data in parallel so each thread's cells land on its local NUMA node.

Phase 1 (Low Risk): Verify NUMA impact with `numactl --membind=0` vs `numactl --interleave=all`.
Phase 2 (Medium Risk): Implement first-touch for `node_lst_`, `face_lst_` (contiguous vectors, no pointer stability issues).
Phase 3 (High Risk): Refactor `cell_lst_` to by-value storage or custom allocator.

#### Trade-offs

| Advantages | Risks & Costs |
|------------|---------------|
| 15–30% improvement on dual-socket systems with no algorithmic changes | Only benefits multi-socket machines; single-socket workstations see zero benefit |
| Low implementation complexity for Phase 1–2 (contiguous vectors) | `std::shared_ptr<cell>` makes NUMA-aware placement difficult (control block + object allocated together) |
| Changes are mostly additive, do not affect correctness | Requires multi-socket hardware for testing; gains invisible on developer machines |

**Verdict:** Defer until B6 (LIKWID) data shows remote memory access exceeds 30% of total bandwidth. If scaling target is <32 cores, this is irrelevant.

<details>
<summary><strong>Implementation Guide</strong></summary>

**Files to modify:** `src/solver.cpp` (cell initialization), `include/mesh/cell.hpp` (node/face allocation)
**Key code path:** `solver::solver()` → cell allocation → node/face vector initialization
**Interaction with other proposals:** Requires B6 (LIKWID) to validate that cross-socket access is the bottleneck. Conflicts with A8 (intrusive pointer) if NUMA allocation uses `std::allocate_shared`. Phase 3 requires resolving `shared_ptr<cell>` architecture (see A8).

</details>

<details>
<summary><strong>Acceptance Criteria</strong></summary>

- [ ] `numastat` shows balanced allocation across NUMA nodes
- [ ] Benchmark on 2-socket system demonstrates measurable speedup (expect 15–30%)
- [ ] Single-socket performance is not degraded
- [ ] LIKWID NUMA counter group confirms reduced remote access fraction

</details>

---

### B3: GPU Acceleration (CUDA/SYCL Hybrid)

**Goal:** Offload contact detection and force computation to GPU to achieve 1.5–1.7× end-to-end speedup for simulations with 2,000+ cells.

| Impact | Risk | Phase | Dependencies |
|:------:|:----:|:-----:|:------------:|
| 3/10 | High | — | A2, B1 |

**Use Case:** A developmental biologist simulates epithelial tissue folding with 2,000 cells (1M nodes) on a workstation with an NVIDIA RTX 4090 and a 32-core Threadripper. Contact detection takes 45% of the 18-hour runtime. After GPU acceleration, contact detection drops from 8.1 hours to ~20 minutes (25–30× speedup on the kernel), but mesh refinement, I/O, and integration remain on CPU. Amdahl's Law limits overall speedup to 1.6×, reducing total runtime to ~11 hours. The researcher must manage 200MB PCIe transfers per timestep and keep mesh refinement on CPU due to irregular topology updates.

#### Current Limitation

CPU-only limits throughput. Contact detection (40–50% of runtime) is embarrassingly parallel at the pair level, and `resolve_contact()` at `contact_face_face_via_coupling.cpp:255` is pure arithmetic suitable for SIMT execution. However, three architectural barriers limit GPU benefit: (1) dynamic topology — `local_mesh_refiner` modifies the mesh every timestep, requiring full host-device sync; (2) irregular spatial queries — `coupled_nodes_map_` uses `std::map::find()` in the hot path, and GPUs are catastrophically bad at pointer-chasing; (3) small problem size — 500K nodes for 1,000 cells is borderline for GPU occupancy. Amdahl's Law: even with 50× GPU speedup on contact (45% of runtime), overall speedup is only 1/(0.55 + 0.45/50) = 1.76×.

#### Proposed Change

Hybrid CPU-GPU architecture: GPU handles broad-phase spatial hash + narrow-phase distance computation + force computation; CPU handles mesh refinement and topology management. Data transfer: ~200MB/timestep via PCIe 4.0 (~8ms each direction, 16ms round-trip).

#### Trade-offs

| Advantages | Risks & Costs |
|------------|---------------|
| 25–30× speedup on contact kernel alone for >2,000 cell simulations | `node.hpp` uses `std::map`, `std::mutex`, `std::optional` — none GPU-compatible; entire data model needs SoA redesign |
| Contact detection is high arithmetic intensity per pair | `shared_ptr<cell>` is pervasive; shared pointers cannot exist on device memory |
| Potential for 100K+ cell simulations | Amdahl's Law limits overall speedup to ~1.6×; dynamic topology + irregular queries = architectural mismatch |

**Verdict:** Strongly discourage CUDA path at current problem sizes. CPU optimizations (A2+A3+A4) deliver comparable speedup (2–3×) with far less risk. If GPU is needed, use Taichi (D1) as the prototyping path. Decision point: after Phase 1 CPU optimizations, profile with LIKWID (B6) — if contact detection is still >40% of runtime, prototype Taichi. If Taichi gives <3× on contact, stop GPU work.

<details>
<summary><strong>Implementation Guide</strong></summary>

**Files to modify:** New `src/contact_models/contact_model_gpu.cu` or Taichi equivalent, `CMakeLists.txt` (CUDA/Taichi dependency)
**Key code path:** `solver::run_iteration()` → `contact_model_gpu::run()` → GPU kernel launch → PCIe transfer → CPU reduction
**Interaction with other proposals:** Requires A2 (inline buffer replaces `std::map` — GPU-hostile data structure). Requires B1 (SoA storage for GPU-compatible memory layout). D1 (Taichi) is the recommended prototyping path before committing to CUDA.

</details>

<details>
<summary><strong>Acceptance Criteria</strong></summary>

- [ ] Contact forces identical to CPU implementation
- [ ] End-to-end speedup measured (expect 1.5–1.7× for 2,000+ cells)
- [ ] PCIe transfer overhead documented (<5% of timestep)
- [ ] Prototype benchmark completed before committing to full implementation

</details>

---

### B4: MPI Domain Decomposition

**Goal:** Enable multi-node scaling to 128+ cores via spatial domain decomposition with MPI for researchers running week-long simulations of 10,000+ cell systems on HPC clusters.

| Impact | Risk | Phase | Dependencies |
|:------:|:----:|:-----:|:------------:|
| 2/10 | V.High | — | B6 |

**Use Case:** A cancer metastasis researcher simulates tumor spheroid invasion with 10,000 cells over 200,000 timesteps, requiring 7 days on a single 64-core node. The simulation crashes at day 5 due to memory exhaustion (128GB insufficient as cell division expands the mesh 3×). With MPI decomposition across 8 nodes (512 cores), memory distributes across nodes and the simulation completes in 22 hours. However, implementation is extremely challenging: cell division creates 10–15% load imbalance per 1,000 timesteps, ghost cell synchronization requires 2-layer halos (25–30% ghost fraction), and repartitioning with ParMETIS costs 2–5 seconds per rebalance.

#### Current Limitation

Single-node only. Cannot scale beyond ~64 cores. Ghost cell synchronization, dynamic load balancing with cell division, and collective operations for USPG construction make this a Ph.D.-level problem. SimuCell3D target problems (organoids, hundreds to low-thousands of cells) rarely need multi-node parallelism. Realistic speedup at 512 cores: ~2.8× (not 8× naive expectation), due to 15% serial fraction and 20% communication overhead.

#### Proposed Change

Recursive Coordinate Bisection (RCB) spatial decomposition with ghost cells, MPI+OpenMP hybrid, and dynamic rebalancing. **ARCHIVED** — only revisit if a specific user has a 100K-cell use case with cluster access.

#### Trade-offs

| Advantages | Risks & Costs |
|------------|---------------|
| Removes fundamental scaling ceiling of single-node OpenMP | Ghost cell management requires `coupled_nodes_map_` to span MPI partitions |
| Enables million-cell simulations; addresses memory exhaustion | Cell division creates migration across partition boundaries with 10–15% imbalance |
| Spatial decomposition aligns with existing USPG grid | 12 months of development for ~2.8× speedup; OpenMP already achieves 12–16× on 32 cores |

**Verdict:** ARCHIVED. Single-node parallelism (OpenMP + NUMA optimization) is sufficient for 95% of SimuCell3D use cases. Only revisit if a user with 20,000+ cells and cluster access materializes.

<details>
<summary><strong>Implementation Guide</strong></summary>

**Files to modify:** New `src/parallel/domain_decomposition.cpp`, `src/solver.cpp` (MPI initialization), all contact models (ghost cell exchange)
**Key code path:** `solver::run_iteration()` → domain partition → ghost exchange → contact detection → force reduction → rebalance check
**Interaction with other proposals:** Requires B6 (profiling to confirm single-node scaling is exhausted). B7 (HDF5 parallel I/O) provides the I/O backend for MPI simulations.

</details>

<details>
<summary><strong>Acceptance Criteria</strong></summary>

- [ ] Contact forces identical to single-node version
- [ ] Strong scaling efficiency >60% to 128 cores
- [ ] Dynamic rebalancing handles cell division without crashes
- [ ] Ghost cell synchronization verified with TSan equivalent for MPI

</details>

---

## Scientific Capabilities

SimuCell3D currently models cells as purely elastic, static-tension objects with instantaneous adhesion and no biochemical state — sufficient for equilibrium morphology but unable to capture the dynamic, multi-timescale processes that drive morphogenesis. The proposals in this category build a layered scientific foundation: viscoelastic mechanics (C1, C2) and adaptive time stepping (C4) form the base layer; mechanotransduction (C7) and robust contact handling (C8) enable biologically relevant feedback; and specialized features (C3, C5, C6, C9, C10, C11) extend the model to specific biological phenomena. Each proposal includes concrete biological use cases and acceptance criteria grounded in experimental literature.

### C1: Kelvin-Voigt Viscoelastic Cortex Model

**Goal:** Enable simulation of time-dependent mechanical behaviors in epithelial tissues by implementing a viscoelastic cortex model that captures stress relaxation and creep on physiologically relevant timescales.

| Impact | Risk | Phase | Dependencies |
|:------:|:----:|:-----:|:------------:|
| 10/10 | Low | 2 | None |

**Use Case:** Researchers studying Drosophila embryonic morphogenesis need to model apical constriction during gastrulation, where cortical stress relaxation on 10–100s timescales determines tissue folding patterns. Current purely elastic models predict unrealistic force buildup during slow deformations and cannot reproduce the experimentally observed ~30s stress relaxation time in Drosophila mesoderm (Martin et al., Nature 2009). This viscoelastic model will allow simulation of how cortical viscosity modulates the coordination between individual cell constrictions and global tissue invagination.

#### Current Limitation

Cell cortex exhibits viscoelastic behavior on 1–100s timescales (Moeendarbary et al., Nature Materials 2013). The current model is purely elastic — it cannot capture stress relaxation during cell rounding, creep under sustained loading, or frequency-dependent stiffness. Without viscosity, the model is quasi-static only and cannot model cell rounding during mitosis, wound healing dynamics, or cell sorting (differential adhesion hypothesis requires viscous rearrangement).

#### Proposed Change

Kelvin-Voigt model (parallel spring-dashpot) with per-face viscous forces:

```
sigma_surface = gamma + k_a * (A/A0 - 1) + eta_s * (dA/dt) / A0
f_i^visc = -eta_s * (1/A0) * (dA_face/dt) * (dA_face/dr_i)
```

where `dA_face/dt` is discretized via backward Euler: `(A_face(t) - A_face(t-dt)) / dt`. The area gradient `dA/dr_i` is already computed at `cell.cpp:1406-1408`. Implementation is ~40 lines: store previous area per face, compute `dA/dt`, add viscous force to existing surface tension calculation.

#### Trade-offs

| Advantages | Risks & Costs |
|------------|---------------|
| Highest impact/effort ratio; infrastructure at `cell.cpp:1375` is ready | Storing `A_previous` per face adds 8 bytes per face (minor memory overhead) |
| Scientifically critical: real cell cortices are viscoelastic, not purely elastic | Viscous term introduces timescale τ = η/K constraining time step: dt < τ for stability |
| Backward-compatible (η=0 recovers current behavior); dissipative term stabilizes | Backward Euler discretization introduces numerical viscosity proportional to dt |

**Verdict:** Implement immediately. ~40 lines of code with the highest scientific impact of any proposal.

<details>
<summary><strong>Implementation Guide</strong></summary>

**Files to modify:** `src/mesh/cell.cpp` (inside `apply_surface_tension_and_membrane_elasticity()` at line 1375), `include/io/custom_structures.hpp` (add `cortex_viscosity_` to `face_type_parameters` at line 74), `include/mesh/face.hpp` (add `previous_area_`)
**Key code path:** `cell::apply_internal_forces()` → `cell::apply_surface_tension_and_membrane_elasticity()` → per-face area gradient (already computed at lines 1406–1408)
**Interaction with other proposals:** Foundation for C2 (active contractility builds on viscoelastic cortex). Benefits from C4 (adaptive time stepping handles viscous timescale constraint automatically).

</details>

<details>
<summary><strong>Acceptance Criteria</strong></summary>

- [ ] Stress relaxation test: Apply sudden 20% area increase, verify exponential decay with τ = η_s/k_a matching 10–100s range (Moeendarbary et al., Nat Mater 2013)
- [ ] Convergence order: test with dt, dt/2, dt/4 and verify first-order convergence
- [ ] Energy dissipation: total mechanical energy decays monotonically
- [ ] Backward compatibility: η_s=0 recovers current elastic-only behavior exactly
- [ ] Gold-standard comparison: micropipette aspiration simulation matches Evans & Yeung (1989) aspiration length vs time

</details>

---

### C4: Adaptive Time Stepping with Error Control

**Goal:** Enable efficient simulation of multi-timescale biological processes by implementing adaptive time stepping that automatically balances accuracy and stability across events from millisecond contact onset to hour-scale cell divisions.

| Impact | Risk | Phase | Dependencies |
|:------:|:----:|:-----:|:------------:|
| 9/10 | Medium | 3 | None |

**Use Case:** A lab simulating early C. elegans embryogenesis (1–28 cell stage over 200 minutes) encounters the stiff integration problem: rapid contact formation events (τ_contact ~100ms) force a global timestep of dt=10ms, wasting 99% of computation during slow interphase periods where dt=1s would suffice. Adaptive stepping reduces wall-clock time from ~72 hours to <4 hours while maintaining <1% position error. This matches the strategy used by Chaste (Mirams et al., PLoS Comp Biol 2013), which demonstrated 10–50× speedups in cardiac tissue simulations.

#### Current Limitation

Fixed dt forces a conservative value for worst-case stability (force spikes during contact onset). Quiescent periods waste computation. Semi-implicit Euler is only 1st order. Cell division is triggered every 5 iterations (`if (iteration_ % 5 == 0)` at `solver.cpp:983`), which conflicts with variable time steps — these must be converted to time-based triggers: `if (simulation_time_ >= next_division_check_time_)`.

#### Proposed Change

Richardson extrapolation (one full step + two half-steps) with PI controller:

```
err = max_i || x_i^full - x_i^half || / (atol + rtol * || x_i^half ||)
dt_new = dt * 0.9 * (tol / err)^(1/2)         # First-order integrator
dt_new = min(dt_new, 0.5 * min_edge_len / max_velocity_prev)   # CFL constraint
dt_new = clamp(dt_new, dt_min, dt_max)          # Hard limits
```

Step rejection when `err > tol`. The error estimate uses absolute+relative tolerance scaling (standard ODE solver convention, Hairer et al. 1993). The exponent `(1/2)` is correct for first-order semi-implicit Euler.

#### Trade-offs

| Advantages | Risks & Costs |
|------------|---------------|
| 5–50× speedup for long simulations; eliminates user time-step tuning | Two half-steps roughly triple the cost per step; amortized cost depends on enlargement frequency |
| Synergizes with ALL other C proposals: C1's viscous timescale, C2's ODE timescale, C8's implicit contact | Cell division and mesh refinement triggered at fixed intervals must be converted to time-based triggers |
| Richardson extrapolation requires no changes to existing integrator | State rollback (rejecting a step) requires saving/restoring node positions and momenta for all cells |

**Verdict:** Enabling technology for all subsequent scientific proposals. Provides error estimates, not just speed. Implement after C1 (viscoelastic cortex introduces the first non-trivial timescale).

<details>
<summary><strong>Implementation Guide</strong></summary>

**Files to modify:** New `src/time_integration/adaptive_time_integration_scheme.cpp`, `src/solver.cpp` (replace single integration call at line 1074), `src/mesh/cell.cpp` (add `save_state()` / `restore_state()` for step rejection)
**Key code path:** `solver::run_iteration()` → `adaptive_time_integration_scheme::update_nodes_positions()` → full step + two half-steps → error estimate → accept/reject
**Interaction with other proposals:** Benefits every C proposal that introduces new timescales. Convert all iteration-based triggers to time-based triggers (division checks, output intervals, mesh refinement).

</details>

<details>
<summary><strong>Acceptance Criteria</strong></summary>

- [ ] Error control convergence: for tol={10⁻², 10⁻³, 10⁻⁴}, verify error estimates scale linearly with tolerance
- [ ] Timestep histogram spans 2+ orders of magnitude in representative morphogenesis simulation
- [ ] CFL safety: max(v_i·dt/L_edge) < 0.5 at all timesteps for all nodes
- [ ] Step rejection triggered by sudden force spike (500 pN application) — dt drops >10× within 3 steps, recovers within 20
- [ ] Richardson extrapolation achieves 3× lower L² error than fixed-dt at matched computational cost

</details>

---

### C7: YAP/TAZ Mechanotransduction Feedback

**Goal:** Deliver mechano-proliferative coupling by implementing YAP/TAZ signaling that translates cell surface area and tension into growth control, enabling simulation of density-dependent growth arrest and contact inhibition.

| Impact | Risk | Phase | Dependencies |
|:------:|:----:|:-----:|:------------:|
| 9/10 | Low | 2 | None |

**Use Case:** Cancer biologists studying MCF10A mammary epithelial spheroids observe that cells in the outer proliferative layer have 3× higher nuclear YAP than inner quiescent cells (Dupont et al., Nature 2011), driven by differential mechanical stress. Current SimuCell3D lacks any proliferation control mechanism, causing unrealistic exponential growth regardless of crowding. YAP/TAZ mechanotransduction enables quantitative prediction of spheroid growth saturation kinetics and testing how altered mechanosensitivity in cancer cells permits growth beyond normal density limits (Panciera et al., Nat Rev Mol Cell Biol 2017).

#### Current Limitation

SimuCell3D has no mechanism linking mechanics to growth. Cells grow and divide on fixed schedules regardless of mechanical environment. Cannot model contact inhibition (Schlegelmilch et al., Nature 2011), mechanically regulated differentiation, or density-dependent growth arrest. YAP/TAZ is the primary mechano-proliferative pathway — without it, organoid simulations cannot self-limit their size.

#### Proposed Change

Three-component model: Hill-function mechanosensor → nuclear-cytoplasmic YAP shuttling → Michaelis-Menten growth rate modulation:

```
mechanical_signal = (A/A0)^n / ((A/A0)^n + K_S^n)           # Hill function
dYAP_n/dt = k_in * (YAP_total - YAP_n) * mechanical_signal - k_out * YAP_n
growth_rate = growth_rate_max * YAP_n / (K_YAP + YAP_n)     # Michaelis-Menten
```

where `K_S` is a dimensionless half-activation area ratio (typically 1.0–1.2). The shuttling dynamics explicitly conserve total YAP: `YAP_n + YAP_c = YAP_total = const`. Literature suggests cortical tension (not pressure) regulates YAP (Aragona et al., Cell 2013); the mechanical signal should use `f(tension, A)` rather than `f(pressure, A)`.

#### Trade-offs

| Advantages | Risks & Costs |
|------------|---------------|
| Enables contact inhibition, density-dependent growth, mechanically driven fate decisions | Hill function parameters (K, n) difficult to calibrate from experimental data |
| ~30 lines for basic model; clean injection point; all needed state variables already computed | Feedback loop (area → YAP → growth → volume → area) can produce oscillations or bistability |
| Backward-compatible: with default parameters, YAP has no effect on growth | More sophisticated models may need YAP/TAZ as per-cell ODE state variable |

**Verdict:** ~30 lines of code with high scientific impact. Implement alongside C1 in Phase 2.

<details>
<summary><strong>Implementation Guide</strong></summary>

**Files to modify:** `include/mesh/cell.hpp` (add `yap_nuclear_`, `yap_cytoplasmic_` — 16 bytes per cell), `src/mesh/cell.cpp` (new `update_mechanotransduction(dt)` before `update_target_volume()` at line 1587)
**Key code path:** `solver::run_iteration()` → `cell::update_mechanotransduction(dt)` → `cell::update_target_volume()` (growth rate now modulated by nuclear YAP)
**Interaction with other proposals:** Complements C1 (viscoelastic cortex provides tension signal for YAP). C12 (nucleus as compartment) would provide nuclear envelope mechanics for YAP entry gating.

</details>

<details>
<summary><strong>Acceptance Criteria</strong></summary>

- [ ] Mechanosensitivity curve: growth rate vs A/A₀ matches Hill function with n≈2–4 and midpoint A/A₀≈1.1 (Aragona et al., Cell 2013)
- [ ] Nuclear-cytoplasmic shuttling: sudden area increase produces nuclear YAP rise with τ_translocation ≈ 10–30 min (Elosegui-Artola et al., Cell 2017)
- [ ] Contact inhibition: confluent monolayer (A/A₀ < 1) shows <5% cells with YAP_n/YAP_total > 0.5
- [ ] Feedback stability: mechano-proliferative loop reaches stable equilibrium monolayer density (no runaway growth)
- [ ] Backward compatibility: default parameters produce YAP-independent growth (equivalent to current behavior)

</details>

---

### C8: Implicit Contact Resolution for Stiff Problems

**Goal:** Enable stable simulation of dense cell packings by implementing position-based contact resolution that eliminates the timestep restriction imposed by explicit contact penalty forces.

| Impact | Risk | Phase | Dependencies |
|:------:|:----:|:-----:|:------------:|
| 8/10 | Medium | 3 | None |

**Use Case:** Researchers modeling tightly packed organoids (>100 cells, packing fraction φ>0.8) encounter severe timestep restrictions: explicit penalty forces with k_repulsion ~10 kPa require dt < 0.1ms for stability, making 24-hour simulations computationally intractable (~10⁶ timesteps). Position-Based Dynamics (PBD, Müller et al., VRIPHYS 2007) allows dt ~10ms (100× speedup) by iteratively projecting positions to satisfy non-penetration constraints. This addresses the most common SimuCell3D crash mode: interpenetration artifacts during dense packing.

#### Current Limitation

Explicit contact with stiff repulsion requires tiny dt. Dense aggregates and high-adhesion scenarios are the most common crash mode. Interpenetration artifacts force users to reduce the time step by 10–100×. No mechanism exists to enforce non-penetration as a constraint.

#### Proposed Change

Gauss-Seidel position projection after time integration:

```
1. Predict:  r* = integrate without contact
2. Project:  for each interpenetrating pair, push nodes to resolve overlap
3. Iterate:  until max_interpenetration < tolerance
4. Update:   v_new = (r_new - r_old) / dt   (Müller et al., 2007)
```

Note: PBD is unconditionally stable but non-physical — it does not conserve energy. For dynamic simulations (DYNAMIC_MODEL_INDEX=0), energy loss will artificially slow the simulation. For overdamped simulations (DYNAMIC_MODEL_INDEX=1), energy non-conservation is acceptable. For scientific accuracy, consider semi-implicit contact (linearized contact forces) as an alternative that preserves energy.

#### Trade-offs

| Advantages | Risks & Costs |
|------------|---------------|
| Addresses the most common user complaint: interpenetration crashes | Gauss-Seidel iteration is inherently sequential for coupled contacts; parallel Jacobi converges more slowly |
| Enables 5–10× larger time steps for contact-dominated simulations | Energy non-conservation introduces artificial dissipation; material properties become effective/empirical |
| Does not modify existing time integrator; runs as post-processing step | Coupled nodes must be projected together; adds complexity to `coupled_nodes_map_` infrastructure |

**Verdict:** Implement PBD for the overdamped model (DYNAMIC_MODEL_INDEX=1) where energy non-conservation is acceptable. For the dynamic model (DYNAMIC_MODEL_INDEX=0), consider semi-implicit contact as a physically accurate alternative.

<details>
<summary><strong>Implementation Guide</strong></summary>

**Files to modify:** `include/contact_models/contact_model_abstract.hpp` (new virtual method at line 84), `src/solver.cpp` (call after time integration at line 1074)
**Key code path:** `solver::run_iteration()` → `time_integration::update_nodes_positions()` → `contact_model::resolve_interpenetration()` (new) → Gauss-Seidel projection loop
**Interaction with other proposals:** Reuses USPG grid already populated during `contact_model::run()`. Benefits from C4 (adaptive time stepping can use larger base dt with PBD). C10 (Notch-Delta) depends on robust contact for reliable signaling.

</details>

<details>
<summary><strong>Acceptance Criteria</strong></summary>

- [ ] Unconditional stability: 10% overlap with dt = 100×dt_explicit resolves without divergence in ≤10 iterations
- [ ] Iterative convergence: max penetration depth decays exponentially with convergence rate ρ < 0.5
- [ ] Dense packing equilibration: 100-cell random aggregate reaches equilibrium (max residual force <1%) within 1000 iterations
- [ ] Hertz contact test: pressure distribution matches analytical solution
- [ ] Energy tracking: for dynamic model, document dissipation rate (expected 20–40% energy loss vs symplectic integrator)
- [ ] Order-independence: different contact pair ordering produces similar results

</details>

---

### C2: Active Cortical Contractility (Actomyosin Tension)

**Goal:** Add spatiotemporally regulated actomyosin-driven contractility to enable simulation of active morphogenetic processes including apical constriction, cytokinesis, and wound healing.

| Impact | Risk | Phase | Dependencies |
|:------:|:----:|:-----:|:------------:|
| 8/10 | Medium | 3 | C1 |

**Use Case:** Developmental biologists modeling ventral furrow formation in Drosophila embryos need to simulate pulsed actomyosin contractions (amplitude ~500 pN/μm², period ~100s) that drive coordinated tissue invagination. Current static surface tension cannot reproduce the characteristic ratchet-like apical area reduction (Martin et al., Nature 2009) where cells undergo repeated contraction pulses with partial relaxation. This model enables testing hypotheses about how pulse amplitude, frequency, and spatial coordination determine invagination success.

#### Current Limitation

Actomyosin cortex drives apical constriction (Martin et al., Nature 2009), cytokinesis (Salbreux et al., 2012), tissue folding, and wound closure. Current surface tension `gamma` is static — it cannot model dynamic contractile processes. Active contractility is the driver of morphogenesis, not just a passive material property.

#### Proposed Change

Per-face active tension with exponential relaxation toward a target:

```
gamma_f(t) = gamma_0 + gamma_active_f(t)
d(gamma_active_f) / dt = (gamma_target_f - gamma_active_f) / tau_cortex
```

This is a phenomenological model capturing transient force application, relaxation after force removal, and spatially heterogeneous tension. Future extensions could add mechanistic myosin dynamics (tension-dependent recruitment and turnover).

#### Trade-offs

| Advantages | Risks & Costs |
|------------|---------------|
| Enables apical constriction, cell intercalation, and morphogenetic movements | Per-face ODE integration adds cost proportional to faces per cell |
| Face structure is compact and extensible; adding `active_tension_` is straightforward | Myosin turnover timescale (~10s) may be faster than mechanical relaxation, requiring sub-cycling |
| Strong synergy with C1 (viscoelasticity) and C6 (adhesion belt) for full biomechanical cortex | Scientific validation is challenging: active tension is not directly measurable |

**Verdict:** Implement after C1 (viscoelastic cortex). Together, C1+C2 deliver a complete biomechanical cortex model.

<details>
<summary><strong>Implementation Guide</strong></summary>

**Files to modify:** `include/mesh/face.hpp` (add `float active_tension_`), `src/mesh/cell.cpp` (new `update_active_tensions(dt)`, modify line 1414 to use `gamma_0 + active_tension_`)
**Key code path:** `solver::run_iteration()` → `cell::update_active_tensions(dt)` → `cell::apply_surface_tension_and_membrane_elasticity()` (uses effective tension)
**Interaction with other proposals:** Requires C1 (viscoelastic cortex provides the relaxation mechanics). Complements C6 (junction model — junctions connect to contractile cortex). Apply `gamma_active` selectively to apical faces (face type filtering) for polarized constriction.

</details>

<details>
<summary><strong>Acceptance Criteria</strong></summary>

- [ ] Single cell with high apical, low basal γ_active produces wedge shape
- [ ] Monolayer with coordinated apical constriction produces invagination
- [ ] Exponential approach: γ_active(t) tracks γ_target with time constant τ_cortex ±5%
- [ ] Mechanical equilibrium: verify Laplace pressure p = 2(γ₀ + γ_active)/R
- [ ] Spatial patterns: apply γ_active only to apical faces (type 0); verify polarized constriction

</details>

---

### C11: Cell-Cell Adhesion Maturation

**Goal:** Add temporal dynamics to adhesion strength by implementing time-dependent cadherin recruitment at new cell contacts, reproducing the minutes-to-hours timescale of adhesion maturation.

| Impact | Risk | Phase | Dependencies |
|:------:|:----:|:-----:|:------------:|
| 7/10 | Low-Med | 5 | None |

**Use Case:** Researchers studying zebrafish gastrulation observe that newly formed cell-cell contacts take ~15 minutes to reach full adhesion strength as E-cadherin clusters accumulate at the interface (Maître et al., Science 2012). This maturation delay fundamentally changes tissue rheology: tissues with fast maturation (τ ~1 min) behave elastically, while slow maturation (τ ~30 min) permits fluid-like rearrangement. Current instantaneous adhesion predicts elastic tissue at all timescales, inconsistent with viscous dissipation observed during convergent extension.

#### Current Limitation

Current adhesion is instantaneous — cells adhere at full strength upon first contact. Real adhesion requires cadherin clustering (~10–60 minutes, Yap et al., 2017). Without adhesion kinetics, cell sorting timescales under the differential adhesion hypothesis are unrealistic.

#### Proposed Change

Adhesion strength increases exponentially with contact duration:

```
omega_adhesion(t) = omega_max * (1 - exp(-t_contact / tau_adhesion))
```

Track contact pair duration via `std::map<unsigned, double>` per cell (contact_cell_id → t_contact). Modulate face-face adhesion strength based on contact age. Upon contact breaking, reset timer to zero.

#### Trade-offs

| Advantages | Risks & Costs |
|------------|---------------|
| Biologically accurate: adhesion requires cadherin clustering (10–60 min) | Contact pair tracking requires persistent state across timesteps |
| Enables realistic cell sorting timescales | Additional per-cell map storage for contact tracking |
| Backward-compatible: τ_adhesion=0 gives instantaneous full-strength adhesion | Validation data for adhesion kinetics is sparse |

**Verdict:** Straightforward implementation with high biological value. Pairs naturally with C3 (friction) and C6 (junction model).

<details>
<summary><strong>Implementation Guide</strong></summary>

**Files to modify:** `include/mesh/cell.hpp` (add contact pair tracking map), `src/contact_models/contact_face_face_via_coupling.cpp` (modulate adhesion by contact age)
**Key code path:** `contact_model::run()` → detect contact pairs → look up contact age → compute adhesion with maturation factor
**Interaction with other proposals:** Pairs with C6 (junction maturation is a spatial refinement of general adhesion maturation). Both C6 and C11 should compose correctly if both implemented.

</details>

<details>
<summary><strong>Acceptance Criteria</strong></summary>

- [ ] Exponential approach: ω_adhesion(t) matches ω_max(1 - exp(-t/τ)) within 10% for t ∈ [0, 5τ]
- [ ] Timescale separation: fast maturation (τ=1 min) produces 5× lower mean cell displacement than slow (τ=30 min)
- [ ] Contact breaking resets timer: separated cells re-contacting start from ω≈0
- [ ] Steady-state: all contacts in static tissue reach ω_max within 5τ
- [ ] Backward compatibility: τ_adhesion=0 recovers current instantaneous adhesion

</details>

---

### C3: Frictional Tangential Contact Forces

**Goal:** Deliver realistic contact mechanics for sliding cell interfaces by implementing viscous friction that resists tangential motion while preserving normal contact resolution.

| Impact | Risk | Phase | Dependencies |
|:------:|:----:|:-----:|:------------:|
| 7/10 | Medium | 5 | None |

**Use Case:** Researchers studying collective migration in MDCK epithelial monolayers observe that leader cells slide over follower cells with apparent friction coefficients producing 20 μm/hr migration speeds (Trepat et al., Nat Phys 2009). Current normal-only contact forces allow unphysical frictionless sliding, predicting ~50 μm/hr. Adding tangential drag enables quantitative comparison of simulated vs experimental velocity fields and stress transmission patterns during wound closure.

#### Current Limitation

Contact models compute only normal forces. No tangential resistance to cell-cell sliding produces artificially fluid tissue. Real tissue viscosity arises from microscale friction. This proposal only works for DYNAMIC_MODEL_INDEX=0 (nodes have `momentum_` at `node.hpp:107`). For DYNAMIC_MODEL_INDEX=1 (overdamped), there is no explicit velocity. Friction forces must be applied before coupled-node averaging in `time_integration.cpp:225-254` to preserve the tangential component.

#### Proposed Change

Viscous friction model:

```
v_rel = v1 - v2
v_t = v_rel - (v_rel . n) * n      # tangential component
f_friction = -zeta_surface * v_t    # viscous friction (equal & opposite on both nodes)
```

Note: `zeta_surface` must have units [force·time/length] for dimensional consistency. For contact-area-dependent friction, use `f_friction = -zeta_surface * A_contact * v_t` where `zeta_surface` is an areal drag coefficient [Pa·s].

#### Trade-offs

| Advantages | Risks & Costs |
|------------|---------------|
| Enables tissue sliding, shear resistance, and jamming transitions | Only works for DYNAMIC_MODEL_INDEX=0; overdamped model has no explicit velocity |
| `cell_type_parameters` has `additional_parameters_` (line 149) for friction without structural changes | Coupled-node averaging in `time_integration.cpp:225-254` needs modification to preserve tangential forces |
| Quantitative comparison with rheometer experiments becomes possible | Friction forces are dissipative and can stiffen the system, potentially requiring smaller time steps |

**Verdict:** Implement for the dynamic model only. Document the overdamped limitation clearly.

<details>
<summary><strong>Implementation Guide</strong></summary>

**Files to modify:** `src/time_integration/time_integration.cpp` (in coupled-node loop at line 225), `include/io/custom_structures.hpp` (add `surface_friction_coefficient_` to `cell_type_parameters`)
**Key code path:** `time_integration::update_nodes_positions()` → coupled-node loop → compute relative tangential velocity → apply friction force before averaging
**Interaction with other proposals:** Independent. Friction provides a tangential damping mechanism that complements C1 (normal viscous damping) and C11 (adhesion maturation affects tangential resistance via contact strength).

</details>

<details>
<summary><strong>Acceptance Criteria</strong></summary>

- [ ] Two-cell sliding test: velocity decay matches analytical exponential
- [ ] Anisotropy: f_friction · n = 0 within machine precision (tangential force is perpendicular to contact normal)
- [ ] Energy dissipation: dE/dt = -ζ · |v_t|² (power dissipated matches analytical prediction)
- [ ] No spurious tangential forces at contact separation (force vanishes continuously as contact breaks)
- [ ] Dynamic-only constraint: friction computation correctly skips when DYNAMIC_MODEL_INDEX ≠ 0

</details>

---

### C6: Adhesion Belt / Junction Model

**Goal:** Enable spatially resolved modeling of epithelial junctions by implementing edge-localized adhesion with maturation dynamics, reproducing the circumferential adherens junction architecture of polarized epithelia.

| Impact | Risk | Phase | Dependencies |
|:------:|:----:|:-----:|:------------:|
| 7/10 | Med-High | 5 | C2 |

**Use Case:** Developmental biologists studying cell intercalation during Drosophila germband extension observe that T1 transitions (neighbor exchanges) are gated by asymmetric junction dynamics: shrinking junctions lose E-cadherin (τ_disassembly ~100s) while growing junctions recruit new adhesion complexes (τ_assembly ~300s; Rauzi et al., Nat Cell Biol 2010). Current face-averaged adhesion cannot reproduce this edge-specific remodeling, predicting symmetric T1 events inconsistent with experimental data showing 3:1 directional bias. Epithelial cells are the primary cell type in SimuCell3D organoid simulations.

#### Current Limitation

Real tissues concentrate adhesion at circumferential belts (tight junctions, adherens junctions). Current uniform adhesion cannot model T1 transitions, tissue fluidity, or epithelial barrier function. The `epithelial_cell` type already distinguishes face types (apical=0, lateral=1, basal=2) via `face_is_in_contact()` (lines 115–164). In 3D surface meshes, the "junction" is the boundary curve of the contact patch — boundary edges between apical and lateral faces, parameterized as a piecewise-linear curve with line tension along the curve.

#### Proposed Change

Junction maturation + line tension:

```
omega_junction = omega_base * (1 + alpha * (1 - exp(-t_contact / tau_mature)))
f_line_i = -Lambda * t_i / 2
```

where `t_i` is the unit tangent along the junction edge. Start with local line tension (each edge independent) for simplicity. A true contractile belt (entire loop contracts together) requires identifying closed loops.

#### Trade-offs

| Advantages | Risks & Costs |
|------------|---------------|
| Epithelial cells are primary cell type in SimuCell3D organoid simulations | Junction boundary must form closed loop on each cell surface; mesh refinement creates degenerate configurations |
| Apical/lateral boundary can be extracted from existing face type classification | Junction remodeling (T1 transitions in vertex models) not directly analogous in surface mesh representation |
| Enables modeling of epithelial barrier function and tissue fluidity | Validation requires comparison with 2D vertex model results using fundamentally different discretization |

**Verdict:** High biological value for epithelial modeling. Start with local line tension (independent edges) before attempting contractile belt mechanics.

<details>
<summary><strong>Implementation Guide</strong></summary>

**Files to modify:** `include/mesh/cell_types/epithelial_cell.hpp` (extend `special_polarization_update()` at line 169), `src/mesh/cell_types/epithelial_cell.cpp`
**Key code path:** `epithelial_cell::special_polarization_update()` → identify boundary edges between lateral (type 1) and apical (type 0) faces → apply line tension → junction maturation timer
**Interaction with other proposals:** Benefits from C2 (active tension — junctions connect to contractile cortex). Composes with C11 (edge-based junction adhesion matures independently from face-based contact adhesion).

</details>

<details>
<summary><strong>Acceptance Criteria</strong></summary>

- [ ] Junction maturation: ω_junction approaches ω_base(1+α) exponentially with τ_mature (verify within 5% for t ∈ [0, 5τ])
- [ ] Line tension mechanics: force on endpoint nodes f_i = ±Λ·t̂/2, Σf_i = 0 for each edge
- [ ] Boundary length minimization: hexagonal array junction perimeter minimizes under line tension
- [ ] Force balance: single junction between two cells yields Λ/R = Δp (Laplace relation)

</details>

---

### C10: Juxtacrine Signaling (Notch-Delta)

**Goal:** Enable simulation of cell fate patterning through direct cell-cell contact signaling by implementing the Notch-Delta lateral inhibition pathway.

| Impact | Risk | Phase | Dependencies |
|:------:|:----:|:-----:|:------------:|
| 7/10 | Low-Med | 5 | C8 |

**Use Case:** Developmental biologists studying inner ear sensory epithelium observe that differentiating hair cells (high Delta) inhibit adjacent cells from also becoming hair cells, creating a hexagonal mosaic pattern with ~99% patterning fidelity (Cotanche & Kaiser, J Neurosci 2010). Implementing Notch-Delta enables testing whether abnormal cell packing (polyhedral disorder from mechanical compression) disrupts patterning by altering contact neighborhoods. The existing `coupled_nodes_map_` already identifies paired nodes between contacting cells, providing infrastructure for juxtacrine signaling.

#### Current Limitation

SimuCell3D has no cell-state variables beyond mechanical properties. Cannot model lateral inhibition, boundary formation, or somitogenesis. The Notch-Delta pathway is the canonical cell fate determination mechanism and is inherently juxtacrine (requires direct cell-cell contact).

#### Proposed Change

Two-variable ODE per cell with contact-area-weighted signal exchange:

```
dN_i/dt = beta_N - gamma_N * N_i + k_trans * <D_coupled>
dD_i/dt = beta_D / (1 + (N_i/K)^n) - gamma_D * D_i
```

where `<D_coupled> = Σ_j(A_contact,ij * D_j) / Σ_j(A_contact,ij)` weights signaling by shared contact area, coupling mechanics to biochemistry. This is the trans-activation model (Delta on neighbors activates Notch; Collier et al. 1996).

#### Trade-offs

| Advantages | Risks & Costs |
|------------|---------------|
| Notch-Delta is the most important cell fate determination pathway; inherently juxtacrine | Per-cell ODEs (2+ per cell) add state variables and integration cost |
| Contact detection infrastructure already identifies contacting cells | Signal transduction dynamics have fast timescales; may require sub-cycling |
| Well-characterized ODE systems with known analytical behavior | `coupled_nodes_map_` provides node-level topology; needs aggregation to cell-level contact area |

**Verdict:** Well-defined ODE system with strong biological motivation. Depends on C8 for robust contact that ensures reliable signaling neighborhoods.

<details>
<summary><strong>Implementation Guide</strong></summary>

**Files to modify:** `include/mesh/cell.hpp` (add `vector<double> state_variables_`), `src/contact_models/contact_face_face_via_coupling.cpp` (signal exchange during coupled-node loop at line 255)
**Key code path:** `contact_model::run()` → aggregate contact areas per cell pair → `cell::update_signaling(dt)` → integrate Notch-Delta ODEs → modulate cell properties based on fate
**Interaction with other proposals:** Depends on C8 (robust contact resolution ensures stable contact neighborhoods for reliable signaling). Contact-area weighting couples mechanics to biochemistry — compressed cells have larger contact areas and stronger signal exchange.

</details>

<details>
<summary><strong>Acceptance Criteria</strong></summary>

- [ ] Salt-and-pepper pattern: 100-cell hexagonal monolayer with noise achieves ≥95% correct neighbor-opposite fates within 10–20 simulated hours
- [ ] Symmetry breaking: two identical cells in contact spontaneously differentiate (N/D ratio >5 in one, <0.2 in other) within 3–5 hours
- [ ] Contact-area dependence: 2× contact area produces proportionally stronger signaling (linear coupling)
- [ ] Robustness: 10% noise in birth/death rates still produces >80% correct patterns

</details>

---

### C5: Reaction-Diffusion on Cell Surfaces

**Goal:** Add biochemical pattern formation to cell surfaces by implementing surface-embedded reaction-diffusion systems coupled to mechanical states.

| Impact | Risk | Phase | Dependencies |
|:------:|:----:|:-----:|:------------:|
| 6/10 | Med-High | 5 | None |

**Use Case:** Researchers modeling cell polarity in budding yeast need to simulate Cdc42 GTPase clustering on the plasma membrane, where reaction-diffusion dynamics (D ~0.01 μm²/s) create a single polar cap. Current SimuCell3D lacks chemical state on surfaces, preventing comparison with FRAP recovery data (τ_1/2 ~5s; Wedlich-Soldner et al., Science 2003). This module enables studying mechano-chemical coupling where membrane tension modulates reaction rates (Agudo-Canalejo & Lipowsky, Nano Lett 2015).

#### Current Limitation

No mechanism for chemical species on cell surfaces. Cannot model PAR polarity, Turing patterns, planar cell polarity, or morphogen interpretation. Edge-loop structure exists in `cell.cpp:256-385` for curvature computation, and cotangent weights are computed at `cell.cpp:344-359`. Explicit diffusion has CFL constraint dt < h²/(2D) which can be prohibitively small for fine meshes. Semi-implicit diffusion requires a sparse linear solve.

#### Proposed Change

Surface PDE with cotangent-weight Laplacian:

```
dc_k/dt = D_k * Delta_S(c_k) + R_k(c_1, ..., c_N)
```

Use semi-implicit scheme: `(I - dt·D_k·L)c^{n+1} = c^n + dt·R_k(c^n)` where L is the Laplacian matrix. Requires sparse linear solver (conjugate gradient; A is symmetric positive-definite). Computational cost: ~100ms per timestep for 1000 cells × 500 nodes. Make optional via compile-time flag.

#### Trade-offs

| Advantages | Risks & Costs |
|------------|---------------|
| Cotangent weights (standard FEM surface Laplacian) already exist in codebase | Mesh topology changes during refinement require interpolation of concentrations to new nodes |
| Enables Turing patterns, polarity, morphogen gradients | Semi-implicit scheme requires sparse linear solve (Eigen SparseLU dependency) |
| Well-established mathematical formulation | Multiple species multiply per-node storage and integration cost (~100ms/timestep) |

**Verdict:** High complexity, specialized use cases. Make optional with a compile-time flag. Implement only after the core scientific features (C1, C2, C4, C7, C8) are in place.

<details>
<summary><strong>Implementation Guide</strong></summary>

**Files to modify:** `include/mesh/cell.hpp` (add `vector<vector<double>> node_concentrations_`), new `src/mesh/cell_surface_diffusion.cpp` (reuse cotan-weight computation from `cell.cpp:344-359`)
**Key code path:** `cell::diffuse_chemicals(dt)` → assemble Laplacian matrix → semi-implicit solve → `cell::react_chemicals(dt)` → user-defined reaction terms
**Interaction with other proposals:** Independent of other proposals. Concentration fields could feed into C7 (YAP/TAZ as chemical signal) or C10 (Notch-Delta with spatial refinement).

</details>

<details>
<summary><strong>Acceptance Criteria</strong></summary>

- [ ] Diffusion on sphere: point source spreading matches analytical Green's function within 10%
- [ ] Surface Laplacian validation: Δ_S(Y_2,m) = -ℓ(ℓ+1)/R² · Y_2,m within 5% discretization error
- [ ] Turing patterns emerge at predicted wavelength (Schnakenberg kinetics)
- [ ] Mass conservation to machine precision
- [ ] Semi-implicit stability: dt_diffusion = 10·(h²/D) without instability (100× larger than explicit limit)

</details>

---

### C9: Lumen Hydraulic Pressure Model

**Goal:** Add fluid-mechanical coupling to lumen-containing cells by implementing hydraulic pressure dynamics driven by active secretion and pressure-dependent leakage.

| Impact | Risk | Phase | Dependencies |
|:------:|:----:|:-----:|:------------:|
| 6/10 | Medium | 5 | None |

**Use Case:** Cell biologists studying MDCK cyst formation observe that single cells develop a pressurized lumen (p ~100–500 Pa) over 5–7 days through polarized ion/water secretion at the apical membrane (Bryant et al., Nat Cell Biol 2010). The lumen volume expands sigmoidally until leakage balances secretion. Current `lumen_cell` type has geometry but no pressure physics, preventing quantitative prediction of cyst size vs secretion rate or testing hypotheses about how defective tight junctions (increased leakage) cause polycystic kidney disease.

#### Current Limitation

`lumen_cell` (at `include/mesh/cell_types/lumen_cell.hpp`) inherits from `cell` but is essentially an empty shell with no lumen-specific physics. Current model uses compressible-gas pressure (`p = -K * ln(V/V0)`). Lumen fluid is incompressible water; expansion is driven by fluid secretion, not gas compression.

#### Proposed Change

Explicit volume evolution driven by secretion and pressure-dependent leakage:

```
dV_lumen/dt = J_secretion - J_leak
J_secretion = alpha * A_apical
J_leak = beta * p_lumen * A_total
```

For mechanical coupling, lumen pressure generates force on apical face nodes: `f_pressure,i = p_lumen * (A_face/3) * n_face`. Equation of state: `p_lumen = K * (V_lumen - V_target)` for small deviations (linear spring model). Use `additional_parameters_` map in `cell_type_parameters` (`custom_structures.hpp:149`) for secretion/leak rates.

#### Trade-offs

| Advantages | Risks & Costs |
|------------|---------------|
| Lumen formation is central to organoid morphogenesis (intestinal, kidney, neural tube) | Full hydraulic model requires fluid volume tracking, osmotic pressure, permeability parameters |
| `lumen_cell` class exists and can override virtual methods | Hydraulics introduces fast timescale (water permeation) creating stiffness |
| Extension points exist in the codebase | Experimental calibration data for lumen permeability is scarce |

**Verdict:** Start with explicit volume evolution (simpler) — compute `V_target^{n+1} = V_target^n + dt * J_net`, then let existing pressure model enforce the volume. Add iterative constraint (incompressible lumen) only if needed.

<details>
<summary><strong>Implementation Guide</strong></summary>

**Files to modify:** `include/mesh/cell_types/lumen_cell.hpp` (override `apply_internal_forces()` and `update_target_volume()`), `include/io/custom_structures.hpp` (secretion/leak rates in `additional_parameters_`)
**Key code path:** `lumen_cell::update_target_volume()` → integrate dV/dt → `lumen_cell::apply_internal_forces()` → pressure forces on apical nodes
**Interaction with other proposals:** Independent. Hydraulic timescale constraint benefits from C4 (adaptive time stepping handles fast water permeation automatically).

</details>

<details>
<summary><strong>Acceptance Criteria</strong></summary>

- [ ] Young-Laplace pressure: p = 2γ/R for spherical lumen
- [ ] Steady-state equilibrium: dV/dt=0 yields p_ss = α·A_apical/(β·A_total); verified for various α/β ratios
- [ ] Osmotic pressure in physiological range: 100–1000 Pa for α ~0.1–1 μm/s
- [ ] Mechanical feedback: expanding lumen stretches apical surface by 30–50%

</details>

---

### C12: Nucleus as Distinct Mechanical Compartment

**Goal:** Enable simulation of nuclear mechanics by representing the nucleus as a separate inner mesh with distinct elastic properties, capturing the nucleus as a rate-limiting mechanical element in tissue invasion.

| Impact | Risk | Phase | Dependencies |
|:------:|:----:|:-----:|:------------:|
| 5/10 | High | — | None |

**Use Case:** Cancer biologists studying melanoma invasion through 3D collagen matrices observe that nuclear stiffness (E_nucleus ~5 kPa vs E_cytoplasm ~0.5 kPa) creates a mechanical bottleneck: cells can only traverse pores smaller than the nucleus if they upregulate lamins to withstand 50–100% nuclear strain (Davidson et al., Science 2014). Current single-compartment cells cannot reproduce the characteristic hourglass nuclear morphology during pore constriction. Explicit nucleus mechanics would enable quantitative prediction of invasion rates vs pore size.

#### Current Limitation

The nucleus is 2–10× stiffer than cytoplasm (Dahl et al., Biophys J 2005). Current model treats the cell as mechanically homogeneous. Cannot model nuclear squeeze through narrow constrictions, YAP/TAZ nuclear entry (requires nuclear envelope mechanics), or nuclear damage under compression.

#### Proposed Change

Add inner mesh for nucleus with distinct bulk modulus and surface tension. Nucleus-cytoplasm interaction via repulsive contact forces (same framework as cell-cell contact). Major architectural change: `cell` contains two meshes (cortex + nucleus), nuclear mesh has its own `node_lst_`, `face_lst_`, pressure, and surface tension.

#### Trade-offs

| Advantages | Risks & Costs |
|------------|---------------|
| Enables modeling of nuclear mechanics, critical for migration and invasion | Major architectural change; cell contains two independent meshes, doubling mesh complexity |
| Links to C7 (YAP/TAZ) via nuclear envelope strain | Self-contact detection between inner and outer mesh is computationally expensive |
| Needed for cancer invasion through narrow constrictions | Validation data for nuclear mechanics is limited |

**Verdict:** Major architectural change with high implementation effort. Consider simplifying: model nucleus as a rigid ellipsoid with 6-DOF dynamics (translation + rotation) to capture essential physics with 10× less complexity. Prototype with single cell before multi-cell integration.

<details>
<summary><strong>Implementation Guide</strong></summary>

**Files to modify:** `include/mesh/cell.hpp` (add nuclear mesh: `node_lst_nucleus_`, `face_lst_nucleus_`, nuclear mechanical parameters), contact model extensions for self-contact
**Key code path:** `cell::apply_internal_forces()` → cortex forces + nuclear forces → nucleus-cortex contact detection → repulsive forces between compartments
**Interaction with other proposals:** C7 (YAP/TAZ) could gate nuclear entry based on nuclear envelope strain. Requires robust contact (C8) for nucleus-cortex collision handling.

</details>

<details>
<summary><strong>Acceptance Criteria</strong></summary>

- [ ] Stiffness contrast: micropipette aspiration simulation shows nucleus 5–10× stiffer than cytoplasm
- [ ] Confined compression: cell through 5 μm channel (7 μm nucleus) produces nucleus aspect ratio >2:1 with 500–1000 pN reaction forces
- [ ] Volume conservation: both nucleus and cytoplasm conserve volume to within 1%
- [ ] No mesh interpenetration between nuclear and cortical surfaces

</details>

---

### C13: ECM (Extracellular Matrix) as Mechanical Substrate

**Goal:** Add extracellular matrix as a deformable mechanical substrate, enabling simulation of cell-ECM reciprocal interactions including traction forces, matrix remodeling, and durotaxis.

| Impact | Risk | Phase | Dependencies |
|:------:|:----:|:-----:|:------------:|
| 4/10 | High | — | None |

**Use Case:** Tissue engineers studying mesenchymal stem cell differentiation on fibrin gels observe that cells sense substrate stiffness (E_gel ~1–100 kPa) and differentiate accordingly: soft gels → neurogenic, medium → myogenic, stiff → osteogenic fate (Engler et al., Cell 2006). Current cells in vacuum cannot generate traction forces or probe their mechanical environment. Implementing ECM enables prediction of how gel stiffness, pore size, and stress relaxation jointly control cell spreading area and fate choice through YAP/TAZ signaling (C7 integration).

#### Current Limitation

Cells are simulated in free space. Real cells adhere to ECM (collagen, fibronectin) and remodel it. Cannot model substrate stiffness effects on cell behavior, ECM degradation (cancer invasion), or fiber alignment under cell traction.

#### Proposed Change

Add ECM as background mesh (FEM or particle system) with adhesion forces between cells and ECM. Major implementation challenges: contact detection between cell surface (triangle mesh) and ECM volume (tetrahedral mesh), sparse linear solve for FEM stiffness matrix, and two-way coupling (cell forces deform ECM, ECM reaction forces act on cells).

#### Trade-offs

| Advantages | Risks & Costs |
|------------|---------------|
| Enables modeling of cell-ECM interactions, critical for tissue engineering | Large architectural change: new mesh type, new contact model, sparse linear solver |
| Links substrate stiffness to cell behavior (mechanotransduction via C7) | ECM is a complex material with anisotropic, nonlinear properties |
| Needed for cancer invasion and wound healing models | Limited SimuCell3D user base currently needs this capability |

**Verdict:** Large architectural change with specialized use case. Start with a simplified 2D substrate (infinite half-space with analytical Green's function for displacement field) to capture essential cell-ECM mechanics with 100× less complexity before attempting full 3D meshed ECM.

<details>
<summary><strong>Implementation Guide</strong></summary>

**Files to modify:** New `src/ecm/` module with ECM mesh, adhesion model, and FEM solver
**Key code path:** `solver::run_iteration()` → cell force computation → transmit traction forces to ECM → solve ECM equilibrium → return reaction forces to cells
**Interaction with other proposals:** Integrates with C7 (YAP/TAZ — substrate stiffness sensed via traction) and C2 (active contractility generates traction forces). Requires sparse linear solver (same dependency as C5 if Eigen is already added).

</details>

<details>
<summary><strong>Acceptance Criteria</strong></summary>

- [ ] Traction force generation: cell on ECM produces substrate deformation with total force 1–10 nN
- [ ] Stiffness sensing: cell spreading area scales as A ∝ E_gel^0.5 across 1, 10, 100 kPa substrates
- [ ] Durotaxis: cell on stiffness gradient migrates up-gradient with v_drift ~10 μm/hr
- [ ] Multi-cell coupling: two cells on shared ECM mechanically couple; predict critical separation distance ~100–200 μm

</details>

---

## Developer Experience & Tooling

These proposals target correctness, testing infrastructure, and developer productivity. The highest priority is fixing 6 confirmed bugs (D5) — correctness defects are foundational and must precede all feature work. Testing infrastructure (D3, D4) catches regressions from the performance and scientific proposals above. GPU prototyping (D1), documentation (D8), and Python enhancements (D7) serve specialized constituencies and are lower priority.

### D5: Fix Residual Bugs (6 Confirmed)

**Goal:** Eliminate 6 confirmed correctness and robustness defects in parameter parsing, validation, and concurrent vector operations.

| Impact | Risk | Phase | Dependencies |
|:------:|:----:|:-----:|:------------:|
| 10/10 | Low | 0 | None |

**Use Case:** A grad student writes an XML parameter file with `<damping_coefficient>0.0</damping_coefficient>` (accidentally cleared while testing). SimuCell3D accepts the file without error, begins the simulation, then crashes 12 hours into a 3-day run with `Floating point exception` in `semi_implicit_euler::integrate()` where velocity is divided by damping coefficient. The crash loses 12 hours of HPC cluster time because the validation check `damping_coefficient > 0.0` is misplaced under the `if (DYNAMIC_MODEL_INDEX == 1)` branch (overdamped model only), while the student runs the default semi-implicit model (index 0).

#### Current Limitation

Internal audit identified 6 GitHub issue fixes that are incomplete:

1. **#50**: Exceptions created but never thrown — 6 instances in preprocessor branches create `std::runtime_error(...)` without `throw` keyword
2. **#59**: `safe_stod()`/`safe_stoi()` defined at `parameter_reader.cpp:8-26` but never called — all 29 conversion sites use raw `std::stod`/`std::stoi`
3. **#62**: Mesh reader (`mesh_reader.cpp:135`) catches `std::invalid_argument` but not `std::out_of_range` (both thrown by `std::stod`)
4. **#63**: `damping_coefficient_` validation at `parameter_reader.cpp:110` uses `< 0.0` (allows zero, which causes division by zero in time integration)
5. **#67**: Validation error message references wrong variable name
6. **#68**: Validation checks wrong parameter

#### Proposed Change

Each fix is 5–30 lines. Add `throw` keyword to 6 exception instances. Replace raw `stod`/`stoi` with safe variants at all 29 sites. Add `out_of_range` catch. Fix validation condition to `<= 0.0`. Correct error messages.

#### Trade-offs

| Advantages | Risks & Costs |
|------------|---------------|
| Correctness bugs are foundational; every developer and user benefits | Exception handling bugs (#50) only affect unsupported compile configurations, low practical impact |
| Each fix is 5–30 lines; small scope for all 6 bugs | Changing validation logic (`damping_coefficient <= 0.0`) requires regression tests |
| The `safe_stod`/`safe_stoi` issue is a ticking time bomb for crashes on malformed XML | Not "exciting" work: bug fixes don't unlock new capabilities |

**Verdict:** CRITICAL — fix before any feature work. Each bug is small scope, high value.

<details>
<summary><strong>Implementation Guide</strong></summary>

**Files to modify:** `src/time_integration/time_integration.cpp` (add `throw` to 5 exception instances), `src/automatic_polarization/automatic_polarizer.cpp` (add `throw` to 1 exception), `src/io/parameter_reader.cpp` (29 conversion sites + validation fixes), `src/io/mesh_reader.cpp` (add `out_of_range` catch at line 135)
**Key code path:** Exception handling in preprocessor branches; `parameter_reader::read_parameters()` → all `std::stod`/`std::stoi` call sites; `mesh_reader::load_vtp()` → exception handling at line 135
**Interaction with other proposals:** All 6 bugs are independent of other proposals.

</details>

<details>
<summary><strong>Acceptance Criteria</strong></summary>

- [ ] All 6 exception instances have `throw` keyword added (#50)
- [ ] All 29 raw `std::stod`/`std::stoi` sites replaced with safe variants (#59)
- [ ] Mesh reader catches both `invalid_argument` and `out_of_range` (#62)
- [ ] `damping_coefficient = 0.0` rejected with clear error message (#63)
- [ ] Error messages reference correct variable names (#67, #68)
- [ ] Regression tests added for each validation fix

</details>

---

### D3: GoogleTest Migration

**Goal:** Replace the custom cassert-based test framework with GoogleTest to enable test fixtures, parameterized tests, and CI-integrated structured failure reporting.

| Impact | Risk | Phase | Dependencies |
|:------:|:----:|:-----:|:------------:|
| 8/10 | Low | 1 | None |

**Use Case:** A contributor adding a new triangulation algorithm needs to test `ball_pivoting_algorithm::compute()` with 15 different mesh configurations. The current framework requires duplicating 90% of setup code across 15 separate test functions, each with hand-rolled initialization and teardown. When a test fails in CI, they receive a binary exit code with no assertion context — just "Test #47 failed." GoogleTest's `TEST_P` parameterized fixtures would consolidate this to 20 lines of setup + 15 parameter tuples, and `ASSERT_NEAR` would show expected vs actual coordinates on geometry failures.

#### Current Limitation

Custom cassert-based test framework with 70+ test modules across 12 subdirectories. No test fixtures, parameterized tests, or structured output. Each test executable takes a test name as argv[1] and returns 0=pass, non-zero=fail. No XML/JSON output for CI dashboards.

#### Proposed Change

Migrate to GoogleTest via `FetchContent(googletest)` in CMake. Convert test files one module at a time (incremental migration). Add test fixtures for common setup (cell creation, mesh initialization).

#### Trade-offs

| Advantages | Risks & Costs |
|------------|---------------|
| 70+ separate executables → single binary with filtering; eliminates ~500–1000 lines CMake boilerplate | Upfront migration cost: mechanical conversion of 70+ test files |
| `EXPECT_NEAR`, `EXPECT_THROW`, death tests far more expressive than boolean assertions | Test order changes may expose hidden dependencies (global state, shared temp files) |
| XML/JSON output enables structured CI reporting and flaky test detection | Floating-point comparison: current `cassert(a == b)` is fragile; `EXPECT_NEAR` requires choosing tolerance |

**Verdict:** High value for test ergonomics and CI integration. Migrate incrementally — one module at a time. Audit all tests for filesystem I/O dependencies before migration.

<details>
<summary><strong>Implementation Guide</strong></summary>

**Files to modify:** `CMakeLists.txt` (add `FetchContent(googletest)`), all `test/*/CMakeLists.txt` (simplify), all test source files (convert assertions)
**Key code path:** Each test's `main()` → GoogleTest `RUN_ALL_TESTS()` with `TEST_F` fixtures
**Interaction with other proposals:** D4 (ThreadSanitizer) benefits from structured test output. Enables parameterized validation tests for all scientific proposals (C1–C13).

</details>

<details>
<summary><strong>Acceptance Criteria</strong></summary>

- [ ] All 70+ test modules converted and passing
- [ ] CI produces structured XML/JSON test reports
- [ ] No hidden test dependencies exposed by order changes (or dependencies fixed)
- [ ] `EXPECT_NEAR` tolerances documented and justified for floating-point comparisons
- [ ] CMake boilerplate reduced by ~500–1000 lines

</details>

---

### D4: ThreadSanitizer in CI

**Goal:** Detect data races in OpenMP-parallelized code through automated CI checks with ThreadSanitizer on core test modules.

| Impact | Risk | Phase | Dependencies |
|:------:|:----:|:-----:|:------------:|
| 8/10 | Low-Med | 0 | None |

**Use Case:** A maintainer merges a PR that replaces `std::vector<node>::push_back()` with a raw pointer arena allocator. The code passes ASan and UBSan in CI, ships to users, then crashes intermittently when running 8-thread simulations on AMD processors (never reproduced on 4-thread CI runners). Postmortem reveals a write-write race in `node_lst_.resize()` called from parallel `#pragma omp for`. TSan would have caught this immediately — but CI only runs ASan/UBSan. Adding TSan surfaces the race in 90 seconds on stratified core tests.

#### Current Limitation

CI runs AddressSanitizer and UBSan (`.github/workflows/cmake.yml`) but not ThreadSanitizer. For a parallel codebase with 30+ OpenMP pragmas, this is a critical gap. The confirmed data race in `vec3::translate` (D5, BUG-002) would be immediately detected by TSan. TSan false positives with OpenMP are a known issue — `libgomp` internals trigger benign race reports.

#### Proposed Change

Stratified TSan deployment:

- **Tier 1 (CI, ~8 minutes):** TSan on 12 core modules: `vec3_test`, `mat33_test`, `quaternion_test`, `uspg_test`, `contact_model_*_test`, `time_integration_test`. Covers 80% of OpenMP pragmas.
- **Tier 2 (Nightly, ~2.5 hours):** Full 70-module suite under TSan.
- **Suppressions file** for OpenMP false positives (`race:^libgomp.so`).

#### Trade-offs

| Advantages | Risks & Costs |
|------------|---------------|
| Catches confirmed existing bugs; prevents regressions in 30+ OpenMP regions | TSan adds 5–15× slowdown; requires separate nightly job for full suite |
| Low false positive rate with LLVM TSan v2 | OpenMP support requires LLVM/Clang and specific flags |
| Will likely find 2–3 additional races beyond known bugs | TSan only detects dynamic races in executed code paths |

**Verdict:** Critical for correctness in a parallel codebase. Implement in Phase 0 alongside D5. Stratified deployment keeps CI fast while nightly catches rarer races.

<details>
<summary><strong>Implementation Guide</strong></summary>

**Files to modify:** `.github/workflows/cmake.yml` (add TSan build), new `test/sanitizers/tsan_suppressions.txt`
**Key code path:** CI pipeline → TSan build with `-fsanitize=thread` → stratified test execution → suppressions for `libgomp` false positives
**Interaction with other proposals:** Validates D5 (BUG-002 race fix), A3 (thread-local force accumulation), and A4 (mutex removal). Every performance proposal touching parallel code should be TSan-validated.

</details>

<details>
<summary><strong>Acceptance Criteria</strong></summary>

- [ ] TSan CI job completes in <10 minutes for Tier 1 (core modules)
- [ ] TSan detects the known BUG-002 race in `vec3::translate` (before fix)
- [ ] Suppressions file eliminates false positives from `libgomp` internals
- [ ] Zero TSan warnings after D5 + A3 fixes are applied
- [ ] Nightly full-suite TSan job configured and running

</details>

---

### D1: Taichi Lang GPU Backend

**Goal:** Accelerate contact detection by 3–10× through GPU execution of spatial partitioning kernels without requiring CUDA expertise.

| Impact | Risk | Phase | Dependencies |
|:------:|:----:|:-----:|:------------:|
| 6/10 | Medium | — | None |

**Use Case:** A computational biologist running 100-cell simulations with dense packing encounters 60–80% of wall-clock time in contact detection. They lack CUDA expertise and need a Python-embeddable GPU framework that integrates with existing pybind11 bindings. Taichi's `@ti.kernel` decorator compiles to CUDA, Vulkan, or CPU backends. However, every kernel launch requires Python interpreter involvement (10–50μs overhead on 100μs kernels), and the mutable mesh architecture (topology changes every timestep) forces full buffer reallocation every refinement cycle.

#### Current Limitation

CUDA requires significant expertise and effort. JAX is a poor fit for SimuCell3D's mutable-mesh architecture. Taichi's Python overhead is significant: for fine-grained kernels (100μs execution time), this is 10–50μs overhead. Realistic performance is only on CUDA (NVIDIA); Vulkan backend is 2–5× slower. Irregular coupling iterations won't vectorize well even in Taichi.

#### Proposed Change

Rewrite hot kernels in Taichi's Python-embedded DSL. Expose as alternate backend via `py::class_<contact_model_taichi>` inheriting from `contact_model_abstract`. C++ users remain unaffected. Python users select via `ContactModel="taichi"` in parameters.

#### Trade-offs

| Advantages | Risks & Costs |
|------------|---------------|
| Verified architectural match: sparse data structures and imperative model fit mutable meshes | Python kernel launch overhead limits fine-grained kernels (10–50μs per launch) |
| Lower refactoring burden than CUDA; unified Python integration | Realistic performance only on NVIDIA GPUs (Vulkan 2–5× slower) |
| Multi-backend: compiles to CUDA, Vulkan, Metal, CPU | Smaller ecosystem than JAX; fewer production deployments |

**Verdict:** Prototype on contact detection only. Measure speedup on 10K-node problem. If <3× end-to-end, abandon GPU path and document findings — CPU optimizations (A2, A3) deliver comparable benefit with less risk.

<details>
<summary><strong>Implementation Guide</strong></summary>

**Files to modify:** New `python_bindings/contact_model_taichi.py` (Taichi kernels), `python_bindings/simucell3d_wrapper.cpp` (expose alternate backend)
**Key code path:** Python user → `SimuCell3D(contact_model="taichi")` → Taichi kernel launch → GPU execution → results back to C++ solver
**Interaction with other proposals:** Alternative to B3 (CUDA) with lower implementation cost. Must share data structures with A2 (inline coupling buffer) — Taichi fields must mirror C++ layout.

</details>

<details>
<summary><strong>Acceptance Criteria</strong></summary>

- [ ] Contact forces match C++ implementation within floating-point tolerance
- [ ] End-to-end speedup measured on 100-cell and 1,000-cell benchmarks
- [ ] Abandonment threshold: <3× speedup on representative benchmark → document and stop
- [ ] C++ users unaffected (Taichi is Python-only alternate backend)

</details>

---

### D9: Performance Regression Testing in CI

**Goal:** Detect performance regressions through nightly benchmark runs with statistical analysis of execution time distributions.

| Impact | Risk | Phase | Dependencies |
|:------:|:----:|:-----:|:------------:|
| 5/10 | Low-Med | — | B6 |

**Use Case:** A maintainer merges a PR that refactors `uspg::query_neighbors()` to use `std::unordered_set` for cleaner deduplication. CI passes all functional tests. Two weeks later, a user reports 35% slower simulations since v1.2.0. Bisecting identifies the PR: `unordered_set` introduces cache misses and heap allocations in the hot loop. Automated nightly benchmarks with statistical regression detection (Mann-Whitney U test, p<0.05, Δ>15%) would have flagged this immediately.

#### Current Limitation

No automated CI performance testing. Infrastructure partially exists: `PerformanceMonitor` (319 lines), manual benchmark scripts (1269 + 480 lines). CI runners are virtualized with 10–20% variance — a 5% regression threshold is below the noise floor, guaranteeing false positives.

#### Proposed Change

1. Dedicated benchmark suite: 5 scenarios (contact detection at 50/100/200 cells, mesh refinement, full simulation)
2. Nightly job on self-hosted bare-metal runner (or dedicated cloud instance with turbo boost disabled)
3. Statistical regression detection: 20 samples per benchmark, Welch's t-test, alert if p<0.05 AND mean delta >15%
4. Results stored in `perf_history.sqlite` for trend analysis

#### Trade-offs

| Advantages | Risks & Costs |
|------------|---------------|
| Prevents performance regressions; quantifies optimization impact | Requires dedicated hardware; inherently flaky on cloud VMs |
| 60% of infrastructure already exists (PerformanceMonitor + scripts) | Statistical testing requires ongoing threshold tuning |
| Existing CSV export makes time-series analysis straightforward | Limited coverage from synthetic benchmarks |

**Verdict:** High value once dedicated hardware is available. Start with nightly benchmarks on local hardware, measure variance first (run benchmark 10×, check stddev). If CV >5%, statistical testing is required.

<details>
<summary><strong>Implementation Guide</strong></summary>

**Files to modify:** New `test/benchmarks/` directory with benchmark executables, `.github/workflows/perf.yml` (nightly job), new `scripts/perf_regression.py` (statistical analysis)
**Key code path:** Nightly CI → run benchmarks (20 samples each) → compare against rolling 7-day median from `main` → Welch's t-test → alert on regression
**Interaction with other proposals:** Depends on B6 (LIKWID) for hardware counter data in regression reports. Validates impact claims of all performance proposals (A1–A10, B1–B2).

</details>

<details>
<summary><strong>Acceptance Criteria</strong></summary>

- [ ] 5 benchmark scenarios covering 80% of runtime
- [ ] Statistical regression detection with p<0.05 AND Δ>15% threshold
- [ ] Baseline variance (CV) measured and documented (<5% on dedicated hardware)
- [ ] At least one intentional regression detected during validation

</details>

---

### D8: Doxygen API Documentation

**Goal:** Generate reference documentation for the 15 core extension points (abstract interfaces, plugin classes) that third-party developers need to implement custom models.

| Impact | Risk | Phase | Dependencies |
|:------:|:----:|:-----:|:------------:|
| 5/10 | Low | 5 | None |

**Use Case:** A research group wants to implement a custom contact model based on Morse potentials. They read the developer guide, which says "inherit from `contact_model_abstract` and implement `update_contacts()`." They open `contact_model_abstract.hpp` and find 8+ virtual methods with no comments. They spend 4 hours reading `solver::run_iteration()` to reverse-engineer the calling contract — what does `dt` represent? Must `update_contacts()` be thread-safe? Which members are safe to read/write? Doxygen comments with `@pre`/`@post` conditions would answer this in 30 seconds.

#### Current Limitation

Only 33 instances of Doxygen comments in ~12,703 lines of code — <1% inline documentation. No `Doxyfile` in project root. Modern IDEs reduce Doxygen value for well-named APIs, but virtual interfaces and extension points are where documentation has highest value.

#### Proposed Change

Doxygen comments on public APIs of 15 core classes only (virtual interfaces, extension points). CI generates HTML docs. Deploy to GitHub Pages. Target: `contact_model_abstract`, `time_integrator_abstract`, `mesh_writer`, `statistics_writer`, `local_mesh_refiner`, `cell_divider`, `automatic_polarization_abstract`, `simulation_parameters`, `solver`. Skip `vec3`, `mat33` (self-explanatory).

#### Trade-offs

| Advantages | Risks & Costs |
|------------|---------------|
| Enables API discoverability for virtual interfaces and extension points | Documenting 15 core classes at 2 hours each |
| Modern IDEs display Doxygen as hover tooltips | Documentation becomes stale without CI enforcement |
| Reduces onboarding time for new contributors | Diminishing returns for internal helpers |

**Verdict:** Target 15 core classes only. Over-documenting internals creates maintenance burden.

<details>
<summary><strong>Implementation Guide</strong></summary>

**Files to modify:** 15 core header files (add Doxygen comments), new `Doxyfile` in project root, `.github/workflows/docs.yml` (CI doc generation)
**Key code path:** N/A — documentation only
**Interaction with other proposals:** Independent. Documents the extension points that C-category proposals will implement against.

</details>

<details>
<summary><strong>Acceptance Criteria</strong></summary>

- [ ] All 15 core classes have Doxygen comments with `@brief`, `@param`, `@pre`, `@post`
- [ ] CI generates HTML docs and deploys to GitHub Pages
- [ ] New contributor can implement a custom contact model using only Doxygen docs (validated via user test)

</details>

---

### D7: Python API Enhancements (Parameter Sweeps, Batch Execution)

**Goal:** Extend Python bindings to support parameter sweeps and batch execution without requiring real-time GIL synchronization during timestepping.

| Impact | Risk | Phase | Dependencies |
|:------:|:----:|:-----:|:------------:|
| 4/10 | Medium | — | None |

**Use Case:** A systems biologist wants to run 500 simulations with varying `adhesion_strength` values (0.1 to 5.0) to fit experimental data. The current pybind11 wrapper exposes only `SimuCell3D("params.xml")` which immediately calls `solver_.run()` — no way to override parameters from Python, check cell count mid-simulation, or run batch jobs. They must generate 500 XML files manually. They need deferred execution (`run=False`), parameter override (`sim.set_parameter("adhesion_strength", 2.5)`), and batch mode (`batch_simulate(configs, n_jobs=8)`).

#### Current Limitation

Current Python wrapper is fire-and-forget. Constructor runs `solver_.run()` immediately. No callbacks, no live monitoring, no mid-simulation parameter changes. GIL contention is fatal for real-time callbacks with OpenMP: acquiring GIL from C++ takes 50–200μs, and if a callback does anything non-trivial (matplotlib, numpy), all OpenMP threads block for milliseconds — 10–50× slowdown. The standard HPC workflow is batch + post-hoc analysis, not interactive simulation.

#### Proposed Change

Focus on parameter sweeps (batch execution) rather than interactive control:

1. **Deferred execution:** `sim = SimuCell3D("params.xml", run=False)` + `sim.run()` with `py::call_guard<py::gil_scoped_release>()`
2. **Parameter override:** `sim.set_parameter("adhesion_strength", 2.5)` mutating `simulation_parameters_` directly
3. **Batch mode:** `batch_simulate(configs, n_jobs=8)` spawning independent C++ solver instances in separate threads, each with `gil_scoped_release`, collecting results in C++ before marshalling back to Python

#### Trade-offs

| Advantages | Risks & Costs |
|------------|---------------|
| Enables parameter sweeps without recompiling or generating XML files | GIL contention risk: real-time callbacks serialize OpenMP — document as anti-pattern |
| Easier integration with ML frameworks and Jupyter notebooks | API complexity: exposing mid-simulation state safely requires deep copying |
| Batch mode with `n_jobs` leverages multi-core without GIL issues | Uncertain user demand for interactive simulation vs batch analysis |

**Verdict:** Focus on batch parameter sweeps (high value, no GIL issues). Document real-time callbacks as an anti-pattern. No evidence users want interactive simulation.

<details>
<summary><strong>Implementation Guide</strong></summary>

**Files to modify:** `python_bindings/simucell3d_wrapper.cpp` (add deferred execution, parameter override, batch mode)
**Key code path:** Python → `SimuCell3D(xml, run=False)` → `set_parameter()` → `run()` with `gil_scoped_release`; or `batch_simulate()` → spawns N solver instances in separate threads
**Interaction with other proposals:** Independent. Batch mode provides the infrastructure for parameter space exploration that D1 (Taichi) would also benefit from.

</details>

<details>
<summary><strong>Acceptance Criteria</strong></summary>

- [ ] Deferred execution: `SimuCell3D(xml, run=False)` does not call `solver_.run()`; backward compatible with `run=True` default
- [ ] Parameter override: `set_parameter()` validates types and ranges (reuses `xml_parameter_reader::validate_parameters()`)
- [ ] Batch mode: `batch_simulate(configs, n_jobs=8)` runs 8 independent simulations in parallel without GIL contention
- [ ] Documentation clearly marks real-time callbacks as anti-pattern

</details>

---

### D6: Expand roadmap.md

**Goal:** Transform the 11-line roadmap skeleton into a navigable development plan with milestone dependencies and links to technical proposals.

| Impact | Risk | Phase | Dependencies |
|:------:|:----:|:-----:|:------------:|
| 4/10 | Low | — | None |

**Use Case:** A new contributor opens `roadmap.md` and finds only 4 heading stubs with no content. They arbitrarily pick GPU acceleration, spend 3 weeks building a CUDA prototype, then discover in PR review that it depends on "Refactor contact model interface" which hasn't started. An expanded roadmap showing priorities, dependencies, and links to `proposals.md` would prevent this wasted effort.

#### Current Limitation

`roadmap.md` is 11 lines of headings only with no substance.

#### Proposed Change

Expand with milestone summaries, dependency chains, and links to this `proposals.md` for detailed descriptions.

#### Trade-offs

| Advantages | Risks & Costs |
|------------|---------------|
| Helps contributors prioritize work | Roadmaps quickly become stale |
| External users can assess planned features | Doesn't accelerate development |
| Low effort for modest coordination value | Time could fix 1–2 bugs instead |

**Verdict:** Low-priority coordination tool. Write after the implementation phases are validated by initial work.

<details>
<summary><strong>Implementation Guide</strong></summary>

**Files to modify:** `roadmap.md`
**Key code path:** N/A — documentation only
**Interaction with other proposals:** References all proposals. Should be updated after each phase completion.

</details>

<details>
<summary><strong>Acceptance Criteria</strong></summary>

- [ ] Each roadmap entry links to corresponding proposal in `proposals.md`
- [ ] Dependencies between milestones are explicitly stated
- [ ] Roadmap is consistent with the implementation phases defined in this document

</details>

---

## Cross-Cutting Analysis

### GPU Strategy Comparison

| Strategy | Realistic Speedup | Risk | Portability | Problem Size Sensitivity |
|----------|:-----------------:|:----:|:-----------:|:------------------------:|
| CPU optimizations (A2+A3+A4) | **2–3×** | Low | Excellent (C++17, OpenMP) | Insensitive (100–10,000 cells) |
| Taichi Lang (D1) | **2–3× overall** | Medium | Good (CUDA, CPU fallback) | Sensitive (GPU needs 1,000+ cells) |
| CUDA C++ (B3) | **1.5–1.7× overall** | High | Poor (NVIDIA only) | Highly sensitive (2,000+ cells) |
| SYCL (oneAPI) | **1.4–1.6× overall** | High | Excellent (multi-vendor) | Highly sensitive |
| Kokkos | **1.8–2.5×** | High | Excellent (multi-backend) | Medium |

Contact detection is 45% of runtime. Amdahl's Law limits GPU benefit: even with 25× GPU speedup on contact, overall speedup is 1/(0.55 + 0.45/25) = 1.6×. CPU optimizations deliver comparable benefit (2–3×) by addressing contention and allocation overhead across all phases, not just contact.

**Recommendation:** CPU optimizations first (best ROI). After Phase 1, profile with LIKWID (B6). If contact detection is still >40% of runtime, prototype Taichi. If Taichi gives <3× on contact, stop GPU work.

---

### Implementation Roadmap

#### Phase 0: Critical Fixes & Quick Wins

- **D5**: Fix all 6 residual bugs (CRITICAL — must precede all other work)
- **D4**: ThreadSanitizer in CI (validates D5 race fix + finds additional bugs)
- **A6**: USPG grid memory reuse (warmup task for learning USPG codebase)
- **B6**: LIKWID marker integration (enables data-driven decisions for all subsequent phases)

#### Phase 1: Eliminate Synchronization Bottlenecks

- **A3**: Thread-local force accumulation (10/10 impact, HIGHEST PRIORITY)
- **A2**: Inline coupling buffer (unblocks A4)
- **A4**: Eliminate mutex (depends on A2+A3)
- **D3**: GoogleTest migration (parallel work)

**Expected:** 2–3× cumulative speedup

#### Phase 2: Data Structure Improvements + Quick Scientific Wins

- **A8**: Intrusive pointer (7/10 impact)
- **C1**: Viscoelastic cortex (~40 lines, highest scientific impact/effort)
- **C7**: YAP/TAZ mechanotransduction (~30 lines)
- **A1**: Flat edge vector (conditional on LIKWID profiling)
- **A10**: Small-vector optimization (quick win)

**Expected:** 2.5–4× cumulative speedup + core scientific capabilities

#### Phase 3: Scientific Foundation

- **C4**: Adaptive time stepping (enabling technology for all subsequent proposals)
- **C8**: Implicit contact resolution (solves #1 crash mode)
- **C2**: Active cortical contractility (enables morphogenesis)

#### Phase 4: Major Refactoring

- **B1**: SoA node storage (only if LIKWID confirms memory bandwidth is the bottleneck)
- **B7**: Checkpoint/restart (lightweight binary first, then HDF5)
- **A9**: Pool allocator for nodes/faces

#### Phase 5: Specialized Features

- **C3**: Frictional tangential contact
- **C6**: Adhesion belt / junction model
- **C9**: Lumen hydraulic pressure
- **C10**: Juxtacrine signaling (Notch-Delta)
- **C11**: Cell-cell adhesion maturation
- **D8**: Targeted Doxygen documentation (15 core classes)

#### Deprioritized

- **B3**: GPU (CUDA/SYCL) — only after CPU optimizations exhausted; prototype Taichi first
- **B4**: MPI domain decomposition — ARCHIVED; only if 100K+ cell use case materializes
- **B5**: NUMA-aware allocation — defer until LIKWID data shows cross-socket access >30%
- **B2**: Compact hash grid — prototype-and-benchmark only; likely neutral or negative impact

---

### Verification Strategy

For every proposal before merge:

1. **Regression**: All existing tests pass (no regressions)
2. **New tests**: Feature-specific tests with analytical validation where possible
3. **Thread safety**: ThreadSanitizer clean (for parallel changes)
4. **Performance**: Benchmark comparison before/after (for performance proposals)
5. **Physics**: Analytical test cases with convergence verification (for scientific proposals)
6. **Memory**: Profiler check for allocation count and cache behavior (for memory layout changes)

---

### Additional Expert-Identified Optimizations

The following were identified by domain experts as high-value but not warranting full proposals. Consider during implementation of related proposals:

| Optimization | Impact | Related To |
|-------------|:------:|:----------:|
| Lock-free contact pair deduplication | 7/10 | A3, A4 |
| SIMD contact pair filtering (AVX broad-phase AABB rejection) | 6/10 | B3 |
| CMake preset system (CMakePresets.json for build configs) | 6/10 | D3 |
| Prefetching for USPG voxel traversal (`__builtin_prefetch`) | 5/10 | A6, B2 |
| Compile-time loop unrolling for 3-node faces (`#pragma unroll 3`) | 4/10 | — |
| Valgrind memcheck in CI (catches leaks ASan misses) | 5/10 | D4 |
| Benchmark comparison tool (A/B with statistical testing) | 6/10 | D9 |

