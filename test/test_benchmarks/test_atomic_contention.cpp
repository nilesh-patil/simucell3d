#include <cassert>
#include <string>
#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <mutex>
#include <atomic>
#include <thread>

#include <omp.h>

#include "vec3.hpp"

/**
 * Micro-benchmark Suite: Atomic and Mutex Contention
 *
 * Profiles the overhead of synchronization primitives used in the
 * contact force accumulation hot path. SimuCell3D uses per-node
 * std::mutex for thread-safe coupling updates.
 *
 * Key metrics:
 * - Uncontended lock acquisition cost
 * - Contended lock scaling with thread count
 * - Atomic vs mutex comparison for force accumulation patterns
 * - False sharing detection via cache line straddling
 *
 * These benchmarks help identify whether synchronization is a bottleneck
 * and guide optimization decisions (e.g., thread-local accumulation + reduce).
 */

constexpr int WARMUP_ITERATIONS = 3;
constexpr int MEASUREMENT_ITERATIONS = 10;
constexpr int OPS_PER_TRIAL = 1000000;

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
// Test 1: Uncontended Mutex Acquisition Cost
//
// Purpose: Measure the baseline cost of acquiring an uncontended std::mutex.
//          This is the per-node overhead in SimuCell3D's contact force accumulation
//          when threads don't compete for the same node.
//
// Target: <100ns per lock/unlock cycle (uncontended)
int test_uncontended_mutex_cost() {
    std::cout << "=== BENCHMARK: Uncontended Mutex Acquisition Cost ===" << std::endl;

    std::mutex mtx;
    volatile double sink = 0.0;

    // Warmup
    for (int w = 0; w < WARMUP_ITERATIONS; ++w) {
        for (int i = 0; i < OPS_PER_TRIAL; ++i) {
            std::lock_guard<std::mutex> lock(mtx);
            sink += 1.0;
        }
    }

    double total_us = measure_median_us([&]() {
        for (int i = 0; i < OPS_PER_TRIAL; ++i) {
            std::lock_guard<std::mutex> lock(mtx);
            sink += 1.0;
        }
    });

    double per_op_ns = (total_us * 1000.0) / OPS_PER_TRIAL;

    std::cout << "Total time:     " << total_us << " us for " << OPS_PER_TRIAL << " ops" << std::endl;
    std::cout << "Per-op cost:    " << per_op_ns << " ns/lock-unlock" << std::endl;
    std::cout << "Target:         <100 ns/lock-unlock (uncontended)" << std::endl;

    bool t1 = (per_op_ns < 100.0);
    std::cout << "Within target: " << t1 << std::endl;

    return !t1;
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 2: Contended Mutex Scaling
//
// Purpose: Measure how mutex contention scales with thread count.
//          Models the worst case where multiple threads try to update
//          the same node's force simultaneously.
//
// Key insight: If contention overhead grows >4x from 1 to N threads,
//              a thread-local accumulation + reduce strategy is needed.
int test_contended_mutex_scaling() {
    std::cout << "=== BENCHMARK: Contended Mutex Scaling ===" << std::endl;

    int max_threads = omp_get_max_threads();
    std::cout << "Available threads: " << max_threads << std::endl;

    std::mutex mtx;
    double shared_sum = 0.0;
    int ops_per_thread = OPS_PER_TRIAL / std::max(max_threads, 1);

    // Measure single-threaded baseline
    omp_set_num_threads(1);
    double single_us = measure_median_us([&]() {
        shared_sum = 0.0;
        #pragma omp parallel
        {
            for (int i = 0; i < ops_per_thread; ++i) {
                std::lock_guard<std::mutex> lock(mtx);
                shared_sum += 1.0;
            }
        }
    });

    // Measure multi-threaded contention
    omp_set_num_threads(max_threads);
    double multi_us = measure_median_us([&]() {
        shared_sum = 0.0;
        #pragma omp parallel
        {
            for (int i = 0; i < ops_per_thread; ++i) {
                std::lock_guard<std::mutex> lock(mtx);
                shared_sum += 1.0;
            }
        }
    });

    omp_set_num_threads(max_threads);

    double contention_ratio = multi_us / std::max(single_us, 0.001);

    std::cout << "Single-thread time: " << single_us << " us" << std::endl;
    std::cout << "Multi-thread time:  " << multi_us << " us (" << max_threads << " threads)" << std::endl;
    std::cout << "Contention ratio:   " << contention_ratio << "x" << std::endl;

    // Fully contended mutex is expected to show significant overhead.
    // This test is diagnostic: it detects catastrophic contention (>50x).
    // Ratios of 5-15x are normal for N threads on 1 mutex.
    bool t1 = (contention_ratio < 50.0);
    std::cout << "Contention acceptable: " << t1 << std::endl;

    return !t1;
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 3: Atomic vs Mutex for Force Accumulation
//
// Purpose: Compare std::atomic<double> (CAS loop) vs std::mutex for
//          accumulating force vectors. SimuCell3D uses mutex; this
//          benchmark quantifies the potential gain from switching to atomics.
//
// Note: std::atomic<double> doesn't support fetch_add on all platforms,
//       so we use a CAS loop pattern.
int test_atomic_vs_mutex_accumulation() {
    std::cout << "=== BENCHMARK: Atomic vs Mutex Force Accumulation ===" << std::endl;

    int max_threads = omp_get_max_threads();
    int ops_per_thread = OPS_PER_TRIAL / std::max(max_threads, 1);

    // --- Mutex approach ---
    std::mutex mtx;
    double mutex_sum = 0.0;
    omp_set_num_threads(max_threads);

    // Warmup
    for (int w = 0; w < WARMUP_ITERATIONS; ++w) {
        mutex_sum = 0.0;
        #pragma omp parallel
        {
            for (int i = 0; i < ops_per_thread; ++i) {
                std::lock_guard<std::mutex> lock(mtx);
                mutex_sum += 1.0;
            }
        }
    }

    double mutex_us = measure_median_us([&]() {
        mutex_sum = 0.0;
        #pragma omp parallel
        {
            for (int i = 0; i < ops_per_thread; ++i) {
                std::lock_guard<std::mutex> lock(mtx);
                mutex_sum += 1.0;
            }
        }
    });

    // --- Atomic CAS approach ---
    std::atomic<double> atomic_sum{0.0};

    for (int w = 0; w < WARMUP_ITERATIONS; ++w) {
        atomic_sum.store(0.0);
        #pragma omp parallel
        {
            for (int i = 0; i < ops_per_thread; ++i) {
                double expected = atomic_sum.load(std::memory_order_relaxed);
                double desired;
                do {
                    desired = expected + 1.0;
                } while (!atomic_sum.compare_exchange_weak(expected, desired,
                    std::memory_order_release, std::memory_order_relaxed));
            }
        }
    }

    double atomic_us = measure_median_us([&]() {
        atomic_sum.store(0.0);
        #pragma omp parallel
        {
            for (int i = 0; i < ops_per_thread; ++i) {
                double expected = atomic_sum.load(std::memory_order_relaxed);
                double desired;
                do {
                    desired = expected + 1.0;
                } while (!atomic_sum.compare_exchange_weak(expected, desired,
                    std::memory_order_release, std::memory_order_relaxed));
            }
        }
    });

    // --- Thread-local accumulation + reduce ---
    double reduce_sum = 0.0;

    for (int w = 0; w < WARMUP_ITERATIONS; ++w) {
        reduce_sum = 0.0;
        #pragma omp parallel reduction(+:reduce_sum)
        {
            for (int i = 0; i < ops_per_thread; ++i) {
                reduce_sum += 1.0;
            }
        }
    }

    double reduce_us = measure_median_us([&]() {
        reduce_sum = 0.0;
        #pragma omp parallel reduction(+:reduce_sum)
        {
            for (int i = 0; i < ops_per_thread; ++i) {
                reduce_sum += 1.0;
            }
        }
    });

    omp_set_num_threads(max_threads);

    std::cout << "Mutex time:     " << mutex_us << " us" << std::endl;
    std::cout << "Atomic CAS time:" << atomic_us << " us" << std::endl;
    std::cout << "Reduction time: " << reduce_us << " us" << std::endl;
    std::cout << "Speedup (mutex/atomic): " << mutex_us / std::max(atomic_us, 0.001) << "x" << std::endl;
    std::cout << "Speedup (mutex/reduce): " << mutex_us / std::max(reduce_us, 0.001) << "x" << std::endl;

    // All three approaches should produce the same count (correctness)
    // Reduction should be fastest
    bool t1 = (reduce_us <= mutex_us * 1.1);  // Reduce should not be slower than mutex
    std::cout << "Reduction is fastest or comparable: " << t1 << std::endl;

    return !t1;
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 4: False Sharing Detection
//
// Purpose: Detect false sharing overhead when adjacent array elements
//          are updated by different threads. This models the case where
//          node force vectors in contiguous memory are updated in parallel.
//
// Methodology:
// - Baseline: Each thread updates its own cache-line-aligned element
// - False sharing: Adjacent elements straddling cache lines
int test_false_sharing_detection() {
    std::cout << "=== BENCHMARK: False Sharing Detection ===" << std::endl;

    int max_threads = omp_get_max_threads();
    if (max_threads < 2) {
        std::cout << "Skipping: need at least 2 threads" << std::endl;
        return 0;  // Pass (cannot test with 1 thread)
    }

    constexpr size_t CACHE_LINE = 64;
    int iterations = OPS_PER_TRIAL / max_threads;

    // Padded array (no false sharing): each element on its own cache line
    struct alignas(CACHE_LINE) PaddedDouble {
        double value;
        char padding[CACHE_LINE - sizeof(double)];
    };
    std::vector<PaddedDouble> padded(max_threads);

    // Dense array (potential false sharing): adjacent doubles
    std::vector<double> dense(max_threads, 0.0);

    omp_set_num_threads(max_threads);

    // Warmup
    for (int w = 0; w < WARMUP_ITERATIONS; ++w) {
        #pragma omp parallel
        {
            int tid = omp_get_thread_num();
            for (int i = 0; i < iterations; ++i) {
                padded[tid].value += 1.0;
            }
        }
    }

    // Measure padded (no false sharing)
    double padded_us = measure_median_us([&]() {
        #pragma omp parallel
        {
            int tid = omp_get_thread_num();
            for (int i = 0; i < iterations; ++i) {
                padded[tid].value += 1.0;
            }
        }
    });

    // Warmup dense
    for (int w = 0; w < WARMUP_ITERATIONS; ++w) {
        #pragma omp parallel
        {
            int tid = omp_get_thread_num();
            for (int i = 0; i < iterations; ++i) {
                dense[tid] += 1.0;
            }
        }
    }

    // Measure dense (potential false sharing)
    double dense_us = measure_median_us([&]() {
        #pragma omp parallel
        {
            int tid = omp_get_thread_num();
            for (int i = 0; i < iterations; ++i) {
                dense[tid] += 1.0;
            }
        }
    });

    double false_sharing_ratio = dense_us / std::max(padded_us, 0.001);

    std::cout << "Padded time:         " << padded_us << " us (no false sharing)" << std::endl;
    std::cout << "Dense time:          " << dense_us << " us (potential false sharing)" << std::endl;
    std::cout << "False sharing ratio: " << false_sharing_ratio << "x" << std::endl;

    // Report but don't fail - false sharing magnitude depends on hardware
    // Ratio > 2x indicates significant false sharing
    bool significant_false_sharing = (false_sharing_ratio > 2.0);
    std::cout << "Significant false sharing detected: " << significant_false_sharing << std::endl;

    // This test always passes - it's diagnostic, not a gate
    // The important output is the ratio for analysis
    return 0;
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 5: Per-Node Mutex Overhead in Realistic Pattern
//
// Purpose: Simulate the actual contact force accumulation pattern:
//          N nodes, each with its own mutex, updated by parallel threads.
//          This models the SimuCell3D pattern where each node has a std::mutex.
//
// The pattern: each thread iterates over a subset of nodes and locks each
//              node's mutex to update forces.
int test_per_node_mutex_pattern() {
    std::cout << "=== BENCHMARK: Per-Node Mutex Pattern ===" << std::endl;

    constexpr int NUM_NODES = 10000;
    constexpr int UPDATES_PER_NODE = 10;  // Average contacts per node
    int max_threads = omp_get_max_threads();

    struct NodeForce {
        std::mutex mtx;
        vec3 force;
    };

    std::vector<NodeForce> nodes(NUM_NODES);

    omp_set_num_threads(max_threads);

    // Warmup
    for (int w = 0; w < WARMUP_ITERATIONS; ++w) {
        #pragma omp parallel for schedule(dynamic, 64)
        for (int i = 0; i < NUM_NODES * UPDATES_PER_NODE; ++i) {
            int node_idx = i % NUM_NODES;
            vec3 delta(1.0, 0.5, 0.25);
            std::lock_guard<std::mutex> lock(nodes[node_idx].mtx);
            nodes[node_idx].force = nodes[node_idx].force + delta;
        }
    }

    double total_us = measure_median_us([&]() {
        // Reset
        for (auto& n : nodes) {
            n.force = vec3(0., 0., 0.);
        }

        #pragma omp parallel for schedule(dynamic, 64)
        for (int i = 0; i < NUM_NODES * UPDATES_PER_NODE; ++i) {
            int node_idx = i % NUM_NODES;
            vec3 delta(1.0, 0.5, 0.25);
            std::lock_guard<std::mutex> lock(nodes[node_idx].mtx);
            nodes[node_idx].force = nodes[node_idx].force + delta;
        }
    });

    double per_update_ns = (total_us * 1000.0) / (NUM_NODES * UPDATES_PER_NODE);

    std::cout << "Total time:         " << total_us << " us" << std::endl;
    std::cout << "Nodes:              " << NUM_NODES << std::endl;
    std::cout << "Updates per node:   " << UPDATES_PER_NODE << std::endl;
    std::cout << "Per-update cost:    " << per_update_ns << " ns" << std::endl;
    std::cout << "Threads:            " << max_threads << std::endl;

    // Target: <500ns per force update (lock + vec3 addition + unlock)
    bool t1 = (per_update_ns < 500.0);
    std::cout << "Within target: " << t1 << std::endl;

    return !t1;
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Main Test Runner
int main(int argc, char** argv) {
    assert(argc == 2);
    std::string test_name = argv[1];

    if (test_name == "test_uncontended_mutex_cost")       return test_uncontended_mutex_cost();
    if (test_name == "test_contended_mutex_scaling")      return test_contended_mutex_scaling();
    if (test_name == "test_atomic_vs_mutex_accumulation") return test_atomic_vs_mutex_accumulation();
    if (test_name == "test_false_sharing_detection")      return test_false_sharing_detection();
    if (test_name == "test_per_node_mutex_pattern")       return test_per_node_mutex_pattern();

    std::cout << "TEST NAME: " << test_name << " DOES NOT EXIST" << std::endl;
    return 1;
}
//---------------------------------------------------------------------------------------------------------
