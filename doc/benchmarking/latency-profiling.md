# Latency Profiling Infrastructure

Hardware counter profiling and micro-benchmark suite for identifying performance bottlenecks in SimuCell3D's contact detection pipeline.

## Quick Start

```bash
# Build with optimizations
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)

# Run all latency benchmarks via CTest
cd build && ctest -R benchmark_ --output-on-failure

# Run with perf hardware counters (requires perf)
./scripts/benchmarking/latency/perf_wrapper.sh --all --output-dir=results/

# Generate roofline analysis
python3 scripts/benchmarking/latency/roofline_analysis.py --json results/ --output roofline.png
```

## Micro-Benchmarks

### Contact Detection Latency (`test_contact_latency`)

Measures per-cell latency for the contact detection pipeline across both USPG and Sweep-and-Prune strategies at three scales:

| Test | Cells | Target |
|------|-------|--------|
| `test_uspg_latency_small` | 50 | <50 us/cell |
| `test_sap_latency_small` | 50 | <50 us/cell |
| `test_uspg_latency_medium` | 200 | <50 us/cell |
| `test_sap_latency_medium` | 200 | <50 us/cell |
| `test_uspg_latency_large` | 1000 | <50 us/cell |
| `test_sap_latency_large` | 1000 | <50 us/cell |
| `test_scaling_behavior` | 50-1000 | Sublinear scaling |

**Methodology**: Warmup iterations (3) + measurement trials (10) with median reporting. Uses `high_resolution_clock` for sub-microsecond precision.

Run individual test:
```bash
./build/test/test_benchmarks/test_contact_latency test_uspg_latency_large
```

### Atomic Contention (`test_atomic_contention`)

Profiles synchronization overhead in the contact force accumulation hot path:

| Test | What it measures |
|------|-----------------|
| `test_uncontended_mutex_cost` | Baseline lock/unlock cycle (<100 ns target) |
| `test_contended_mutex_scaling` | How contention grows with threads |
| `test_atomic_vs_mutex_accumulation` | CAS loop vs mutex vs reduction |
| `test_false_sharing_detection` | Cache line straddling overhead |
| `test_per_node_mutex_pattern` | Realistic per-node mutex simulation |

These benchmarks directly measure the `std::mutex` per-node overhead in SimuCell3D's contact model. The `test_atomic_vs_mutex_accumulation` test compares three approaches:
1. **Mutex** (current SimuCell3D) - `std::lock_guard<std::mutex>`
2. **Atomic CAS** - `compare_exchange_weak` loop
3. **OpenMP reduction** - `#pragma omp parallel reduction`

### Memory Bandwidth (`test_memory_bandwidth`)

Establishes the memory bandwidth baseline for roofline analysis:

| Test | Pattern |
|------|---------|
| `test_sequential_read_bandwidth` | STREAM-like copy (64MB arrays) |
| `test_vec3_aos_bandwidth` | vec3 Array-of-Structs iteration |
| `test_random_access_latency` | Pointer chasing (USPG model) |
| `test_soa_vs_aos_comparison` | Struct-of-Arrays vs Array-of-Structs |
| `test_parallel_bandwidth_scaling` | Bandwidth vs thread count |

The SoA vs AoS test validates the memory layout proposed in `gpu.md:230-310` for the JAX GPU reimplementation.

## perf Wrapper Script

`scripts/benchmarking/latency/perf_wrapper.sh` automates hardware counter collection:

```bash
# Single benchmark with JSON output
./scripts/benchmarking/latency/perf_wrapper.sh \
    build/test/test_benchmarks/test_contact_latency \
    test_uspg_latency_large \
    --output=results/uspg_large.json

# All benchmarks
./scripts/benchmarking/latency/perf_wrapper.sh --all --output-dir=results/

# Summary table from results
./scripts/benchmarking/latency/perf_wrapper.sh --summary results/*.json
```

### JSON Output Format

```json
{
    "benchmark": "test_uspg_latency_large",
    "timestamp": "2026-02-15T12:00:00Z",
    "counters": {
        "cache-misses": 12345,
        "cache-references": 67890,
        "instructions": 123456789,
        "cycles": 234567890,
        "L1-dcache-loads": 45678901,
        "LLC-loads": 23456,
        "LLC-load-misses": 1234
    },
    "derived": {
        "cache_miss_rate": 0.182,
        "ipc": 1.8,
        "branch_miss_rate": 0.02
    },
    "duration_seconds": 1.234,
    "exit_code": 0
}
```

### Hardware Counter Events

Default events collected:
- `cache-misses`, `cache-references` - L1/L2 cache behavior
- `instructions`, `cycles` - IPC measurement
- `branches`, `branch-misses` - Branch prediction
- `L1-dcache-loads`, `L1-dcache-load-misses` - L1 data cache
- `LLC-loads`, `LLC-load-misses` - Last-level cache
- `task-clock`, `context-switches`, `cpu-migrations` - Scheduling

Custom events:
```bash
./scripts/benchmarking/latency/perf_wrapper.sh \
    build/test/test_benchmarks/test_contact_latency \
    test_uspg_latency_large \
    --events=cycles,instructions,cache-misses,cache-references
```

### Prerequisites

```bash
# Install perf
sudo apt-get install linux-tools-$(uname -r)

# Enable hardware counters (required for non-root)
sudo sysctl -w kernel.perf_event_paranoid=-1
```

## Roofline Analysis

`scripts/benchmarking/latency/roofline_analysis.py` generates roofline model plots:

```bash
# Auto-detect hardware, generate plot
python3 scripts/benchmarking/latency/roofline_analysis.py \
    --detect-hardware --json results/ --output roofline.png

# Specify hardware manually
python3 scripts/benchmarking/latency/roofline_analysis.py \
    --peak-flops 50 --peak-bandwidth 25 \
    --json results/ --output roofline.png

# Summary table only (no matplotlib required)
python3 scripts/benchmarking/latency/roofline_analysis.py \
    --json results/ --summary-only
```

The roofline model identifies whether each kernel is:
- **Memory-bound**: Below the sloped line (optimize data layout, reduce cache misses)
- **Compute-bound**: Below the horizontal line (optimize arithmetic, use SIMD)

## CI Integration

The `latency-profiling` job in `.github/workflows/cmake.yml` runs benchmarks on every PR:

1. Builds Release configuration
2. Runs `ctest -R benchmark_` to execute all micro-benchmarks
3. Reports results as a markdown table in the job summary

Benchmarks are **soft gates**: failures are reported but don't block merges. This avoids flaky CI due to shared runner variability.

## Baseline Metrics

Target baselines (from initial profiling on 4-core cloud VM):

| Metric | Target | Notes |
|--------|--------|-------|
| Cache miss rate | 7.3% +/- 0.8% | L1 data cache |
| IPC | 1.8 +/- 0.1 | Instructions per cycle |
| Memory bandwidth | >60% of peak | Sequential access pattern |
| Contact latency | <50 us/cell | At 1000 cells |
| Mutex cost | <100 ns uncontended | Per lock/unlock cycle |

## File Layout

```
scripts/benchmarking/latency/
    perf_wrapper.sh          # perf stat automation with JSON output
    roofline_analysis.py     # FLOP counting + roofline plots

test/test_benchmarks/
    CMakeLists.txt           # Build configuration for benchmarks
    test_contact_latency.cpp # Contact detection micro-benchmark
    test_atomic_contention.cpp # Atomic/mutex contention profiling
    test_memory_bandwidth.cpp  # Memory bandwidth profiling

doc/benchmarking/
    latency-profiling.md     # This file
```
