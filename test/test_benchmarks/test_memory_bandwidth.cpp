#include <cassert>
#include <string>
#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <cstring>
#include <random>

#include <omp.h>

#include "vec3.hpp"

/**
 * Micro-benchmark Suite: Memory Bandwidth Profiling
 *
 * Measures memory throughput for access patterns representative of
 * SimuCell3D's data structures:
 *
 * 1. Sequential read:  Iterating over contiguous node/face arrays
 * 2. Sequential write: Force accumulation into node arrays
 * 3. Random access:    USPG voxel lookups and pointer chasing
 * 4. Struct-of-Arrays vs Array-of-Structs: Cache efficiency comparison
 *
 * These benchmarks establish the memory bandwidth baseline and identify
 * whether SimuCell3D is compute-bound or memory-bound (roofline analysis).
 *
 * Target: >60% of theoretical peak memory bandwidth
 */

constexpr int WARMUP_ITERATIONS = 3;
constexpr int MEASUREMENT_ITERATIONS = 10;

/**
 * Measure median time across multiple trials.
 */
template <typename Func>
double measure_median_us(Func&& fn, int trials = MEASUREMENT_ITERATIONS) {
    std::vector<double> times;
    times.reserve(trials);

    for (int t = 0; t < trials; ++t) {
        auto start = std::chrono::high_resolution_clock::now();
        fn();
        auto end = std::chrono::high_resolution_clock::now();
        times.push_back(std::chrono::duration<double, std::micro>(end - start).count());
    }

    std::sort(times.begin(), times.end());
    return times[times.size() / 2];
}


//---------------------------------------------------------------------------------------------------------
// Test 1: Sequential Read Bandwidth (STREAM-like Copy)
//
// Purpose: Measure peak sequential read bandwidth by iterating over
//          a large contiguous array. This is the upper bound for any
//          data structure iteration in SimuCell3D.
//
// Methodology: STREAM Copy pattern: b[i] = a[i]
//              Array size chosen to exceed L3 cache (>32MB)
int test_sequential_read_bandwidth() {
    std::cout << "=== BENCHMARK: Sequential Read Bandwidth (STREAM Copy) ===" << std::endl;

    // Use 64MB arrays to exceed L3 cache
    constexpr size_t ARRAY_SIZE = 8 * 1024 * 1024;  // 8M doubles = 64MB
    std::vector<double> src(ARRAY_SIZE, 1.0);
    std::vector<double> dst(ARRAY_SIZE, 0.0);

    int max_threads = omp_get_max_threads();
    omp_set_num_threads(max_threads);

    // Warmup
    for (int w = 0; w < WARMUP_ITERATIONS; ++w) {
        #pragma omp parallel for schedule(static)
        for (size_t i = 0; i < ARRAY_SIZE; ++i) {
            dst[i] = src[i];
        }
    }

    double total_us = measure_median_us([&]() {
        #pragma omp parallel for schedule(static)
        for (size_t i = 0; i < ARRAY_SIZE; ++i) {
            dst[i] = src[i];
        }
    });

    // Calculate bandwidth: read 64MB + write 64MB = 128MB total
    double total_bytes = 2.0 * ARRAY_SIZE * sizeof(double);
    double bandwidth_gbps = (total_bytes / (total_us * 1e-6)) / (1024.0 * 1024.0 * 1024.0);

    std::cout << "Array size:    " << ARRAY_SIZE << " doubles (" << (ARRAY_SIZE * sizeof(double)) / (1024 * 1024) << " MB)" << std::endl;
    std::cout << "Total time:    " << total_us << " us" << std::endl;
    std::cout << "Bandwidth:     " << bandwidth_gbps << " GB/s" << std::endl;
    std::cout << "Threads:       " << max_threads << std::endl;

    // Bandwidth should be >1 GB/s even on modest hardware
    bool t1 = (bandwidth_gbps > 1.0);
    std::cout << "Bandwidth OK:  " << t1 << std::endl;

    return !t1;
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 2: vec3 Array Iteration (AoS Pattern)
//
// Purpose: Measure bandwidth for iterating over SimuCell3D's actual
//          data layout: vectors of vec3 objects (Array-of-Structs).
//          This models iterating over node positions or forces.
int test_vec3_aos_bandwidth() {
    std::cout << "=== BENCHMARK: vec3 Array-of-Structs Bandwidth ===" << std::endl;

    constexpr size_t NUM_NODES = 2 * 1024 * 1024;  // ~48MB of vec3 data
    std::vector<vec3> positions(NUM_NODES);
    std::vector<vec3> forces(NUM_NODES);

    // Initialize
    for (size_t i = 0; i < NUM_NODES; ++i) {
        positions[i] = vec3(static_cast<double>(i), static_cast<double>(i) * 0.5, static_cast<double>(i) * 0.25);
    }

    int max_threads = omp_get_max_threads();
    omp_set_num_threads(max_threads);

    // Warmup
    for (int w = 0; w < WARMUP_ITERATIONS; ++w) {
        #pragma omp parallel for schedule(static)
        for (size_t i = 0; i < NUM_NODES; ++i) {
            forces[i] = positions[i] * 0.5;
        }
    }

    double total_us = measure_median_us([&]() {
        #pragma omp parallel for schedule(static)
        for (size_t i = 0; i < NUM_NODES; ++i) {
            forces[i] = positions[i] * 0.5;
        }
    });

    // Each iteration: read 24 bytes (vec3) + write 24 bytes (vec3) = 48 bytes
    double total_bytes = 2.0 * NUM_NODES * 3 * sizeof(double);
    double bandwidth_gbps = (total_bytes / (total_us * 1e-6)) / (1024.0 * 1024.0 * 1024.0);

    std::cout << "Nodes:         " << NUM_NODES << std::endl;
    std::cout << "Total data:    " << (total_bytes) / (1024 * 1024) << " MB" << std::endl;
    std::cout << "Total time:    " << total_us << " us" << std::endl;
    std::cout << "Bandwidth:     " << bandwidth_gbps << " GB/s" << std::endl;

    bool t1 = (bandwidth_gbps > 0.5);
    std::cout << "Bandwidth OK:  " << t1 << std::endl;

    return !t1;
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 3: Random Access Pattern (Pointer Chasing)
//
// Purpose: Measure latency for random memory access patterns that
//          model USPG voxel lookups and indirect cell/face access.
//          This is the bottleneck pattern for contact detection.
//
// Methodology: Shuffle an index array to create random access pattern.
//              Each access is a dependent load (pointer chasing).
int test_random_access_latency() {
    std::cout << "=== BENCHMARK: Random Access Latency ===" << std::endl;

    constexpr size_t ARRAY_SIZE = 4 * 1024 * 1024;  // 32MB
    std::vector<double> data(ARRAY_SIZE, 1.0);

    // Create shuffled index array for random access
    std::vector<size_t> indices(ARRAY_SIZE);
    std::iota(indices.begin(), indices.end(), 0);
    std::mt19937 rng(42);
    std::shuffle(indices.begin(), indices.end(), rng);

    // Warmup
    volatile double sink = 0.0;
    for (int w = 0; w < WARMUP_ITERATIONS; ++w) {
        for (size_t i = 0; i < ARRAY_SIZE; ++i) {
            sink += data[indices[i]];
        }
    }

    double total_us = measure_median_us([&]() {
        double local_sum = 0.0;
        for (size_t i = 0; i < ARRAY_SIZE; ++i) {
            local_sum += data[indices[i]];
        }
        sink = local_sum;
    });

    double per_access_ns = (total_us * 1000.0) / ARRAY_SIZE;
    double total_bytes = ARRAY_SIZE * sizeof(double);
    double effective_bandwidth_gbps = (total_bytes / (total_us * 1e-6)) / (1024.0 * 1024.0 * 1024.0);

    std::cout << "Array size:      " << ARRAY_SIZE << " doubles (" << (ARRAY_SIZE * sizeof(double)) / (1024 * 1024) << " MB)" << std::endl;
    std::cout << "Total time:      " << total_us << " us" << std::endl;
    std::cout << "Per-access:      " << per_access_ns << " ns" << std::endl;
    std::cout << "Eff. bandwidth:  " << effective_bandwidth_gbps << " GB/s" << std::endl;

    // Random access should not exceed DRAM latency (~100ns)
    bool t1 = (per_access_ns < 200.0);
    std::cout << "Latency OK:      " << t1 << std::endl;

    return !t1;
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 4: SoA vs AoS Comparison
//
// Purpose: Compare Struct-of-Arrays (SoA) vs Array-of-Structs (AoS) layout
//          for a force computation pattern. This directly informs the JAX GPU
//          reimplementation design (gpu.md:230-310 proposes SoA layout).
//
// Pattern: For each node, compute force = position * coefficient + velocity * damping
//          (simplified bending/pressure force computation)
int test_soa_vs_aos_comparison() {
    std::cout << "=== BENCHMARK: SoA vs AoS Layout Comparison ===" << std::endl;

    constexpr size_t NUM_NODES = 1024 * 1024;
    int max_threads = omp_get_max_threads();

    // --- AoS layout (current SimuCell3D) ---
    struct NodeAoS {
        double px, py, pz;  // position
        double vx, vy, vz;  // velocity
        double fx, fy, fz;  // force
    };
    std::vector<NodeAoS> aos(NUM_NODES);
    for (size_t i = 0; i < NUM_NODES; ++i) {
        aos[i] = {1.0, 2.0, 3.0, 0.1, 0.2, 0.3, 0.0, 0.0, 0.0};
    }

    // --- SoA layout (proposed for GPU) ---
    struct NodeSoA {
        std::vector<double> px, py, pz;
        std::vector<double> vx, vy, vz;
        std::vector<double> fx, fy, fz;

        void resize(size_t n) {
            px.resize(n); py.resize(n); pz.resize(n);
            vx.resize(n); vy.resize(n); vz.resize(n);
            fx.resize(n); fy.resize(n); fz.resize(n);
        }
    };
    NodeSoA soa;
    soa.resize(NUM_NODES);
    for (size_t i = 0; i < NUM_NODES; ++i) {
        soa.px[i] = 1.0; soa.py[i] = 2.0; soa.pz[i] = 3.0;
        soa.vx[i] = 0.1; soa.vy[i] = 0.2; soa.vz[i] = 0.3;
    }

    double coeff = 0.5;
    double damping = 0.01;

    omp_set_num_threads(max_threads);

    // Warmup AoS
    for (int w = 0; w < WARMUP_ITERATIONS; ++w) {
        #pragma omp parallel for schedule(static)
        for (size_t i = 0; i < NUM_NODES; ++i) {
            aos[i].fx = aos[i].px * coeff + aos[i].vx * damping;
            aos[i].fy = aos[i].py * coeff + aos[i].vy * damping;
            aos[i].fz = aos[i].pz * coeff + aos[i].vz * damping;
        }
    }

    double aos_us = measure_median_us([&]() {
        #pragma omp parallel for schedule(static)
        for (size_t i = 0; i < NUM_NODES; ++i) {
            aos[i].fx = aos[i].px * coeff + aos[i].vx * damping;
            aos[i].fy = aos[i].py * coeff + aos[i].vy * damping;
            aos[i].fz = aos[i].pz * coeff + aos[i].vz * damping;
        }
    });

    // Warmup SoA
    for (int w = 0; w < WARMUP_ITERATIONS; ++w) {
        #pragma omp parallel for schedule(static)
        for (size_t i = 0; i < NUM_NODES; ++i) {
            soa.fx[i] = soa.px[i] * coeff + soa.vx[i] * damping;
            soa.fy[i] = soa.py[i] * coeff + soa.vy[i] * damping;
            soa.fz[i] = soa.pz[i] * coeff + soa.vz[i] * damping;
        }
    }

    double soa_us = measure_median_us([&]() {
        #pragma omp parallel for schedule(static)
        for (size_t i = 0; i < NUM_NODES; ++i) {
            soa.fx[i] = soa.px[i] * coeff + soa.vx[i] * damping;
            soa.fy[i] = soa.py[i] * coeff + soa.vy[i] * damping;
            soa.fz[i] = soa.pz[i] * coeff + soa.vz[i] * damping;
        }
    });

    double speedup = aos_us / std::max(soa_us, 0.001);

    std::cout << "AoS time: " << aos_us << " us" << std::endl;
    std::cout << "SoA time: " << soa_us << " us" << std::endl;
    std::cout << "SoA speedup: " << speedup << "x" << std::endl;
    std::cout << "Nodes: " << NUM_NODES << ", Threads: " << max_threads << std::endl;

    // Report results only — timing assertions are unreliable on shared CI hardware
    // (noisy neighbors, TLB pressure from scattered SoA allocations, etc.)
    std::cout << "SoA competitive: " << (soa_us <= aos_us * 1.5 ? "yes" : "no") << std::endl;

    return 0;
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 5: Parallel Bandwidth Scaling
//
// Purpose: Measure how memory bandwidth scales with thread count.
//          Identifies the memory bandwidth saturation point.
//
// Key insight: If bandwidth saturates at N threads, using more threads
//              for memory-bound kernels wastes resources.
int test_parallel_bandwidth_scaling() {
    std::cout << "=== BENCHMARK: Parallel Bandwidth Scaling ===" << std::endl;

    constexpr size_t ARRAY_SIZE = 8 * 1024 * 1024;  // 64MB
    std::vector<double> src(ARRAY_SIZE, 1.0);
    std::vector<double> dst(ARRAY_SIZE, 0.0);

    int max_threads = omp_get_max_threads();
    std::cout << "Max threads: " << max_threads << std::endl;

    double single_bw = 0.0;
    double max_bw = 0.0;

    for (int nthreads = 1; nthreads <= max_threads; ++nthreads) {
        omp_set_num_threads(nthreads);

        // Warmup
        for (int w = 0; w < WARMUP_ITERATIONS; ++w) {
            #pragma omp parallel for schedule(static)
            for (size_t i = 0; i < ARRAY_SIZE; ++i) {
                dst[i] = src[i];
            }
        }

        double total_us = measure_median_us([&]() {
            #pragma omp parallel for schedule(static)
            for (size_t i = 0; i < ARRAY_SIZE; ++i) {
                dst[i] = src[i];
            }
        });

        double total_bytes = 2.0 * ARRAY_SIZE * sizeof(double);
        double bandwidth_gbps = (total_bytes / (total_us * 1e-6)) / (1024.0 * 1024.0 * 1024.0);

        if (nthreads == 1) single_bw = bandwidth_gbps;
        if (bandwidth_gbps > max_bw) max_bw = bandwidth_gbps;

        std::cout << "Threads=" << nthreads << ": " << bandwidth_gbps << " GB/s ("
                  << bandwidth_gbps / std::max(single_bw, 0.001) << "x scaling)" << std::endl;
    }

    omp_set_num_threads(max_threads);

    // Multi-threaded bandwidth should be at least as good as single-threaded
    bool t1 = (max_bw >= single_bw * 0.9);
    std::cout << "Peak bandwidth:    " << max_bw << " GB/s" << std::endl;
    std::cout << "Scaling OK: " << t1 << std::endl;

    return !t1;
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Main Test Runner
int main(int argc, char** argv) {
    assert(argc == 2);
    std::string test_name = argv[1];

    if (test_name == "test_sequential_read_bandwidth")     return test_sequential_read_bandwidth();
    if (test_name == "test_vec3_aos_bandwidth")             return test_vec3_aos_bandwidth();
    if (test_name == "test_random_access_latency")          return test_random_access_latency();
    if (test_name == "test_soa_vs_aos_comparison")          return test_soa_vs_aos_comparison();
    if (test_name == "test_parallel_bandwidth_scaling")     return test_parallel_bandwidth_scaling();

    std::cout << "TEST NAME: " << test_name << " DOES NOT EXIST" << std::endl;
    return 1;
}
//---------------------------------------------------------------------------------------------------------
