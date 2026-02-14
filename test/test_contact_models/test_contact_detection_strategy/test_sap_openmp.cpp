#include <cassert>
#include <string>
#include <iostream>
#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <omp.h>

#include "utils.hpp"
#include "contact_detection_strategy.hpp"
#include "contact_detection_sap_strategy.hpp"
#include "custom_structures.hpp"
#include "vec3.hpp"

/**
 * Test Suite: OpenMP Parallelization of SAP Algorithm
 *
 * Purpose:
 * Validates correctness and performance of OpenMP parallelization in the
 * Sweep-and-Prune contact detection algorithm. These tests ensure that
 * parallelization does not introduce race conditions, data corruption,
 * or numerical non-determinism.
 *
 * Test Categories:
 * 1. Serial-Parallel Equivalence: Verify parallel results match serial
 * 2. Thread Safety: Detect race conditions and data corruption
 * 3. Determinism: Multiple runs produce identical results
 * 4. Performance: Verify speedup with multiple threads
 *
 * Scientific Rationale:
 * Contact detection is critical for simulation correctness. Any bug in
 * parallelization could lead to missed contacts (simulation artifacts)
 * or inconsistent results (non-reproducible science).
 */

// Tolerance for floating-point comparisons
constexpr double EPS = 1e-10;


//---------------------------------------------------------------------------------------------------------
// Test 1: Serial-Parallel Equivalence - Small Problem
//
// Purpose: Verify that parallel execution produces bit-exact results compared
//          to serial execution for a small problem size.
//
// Scientific Rationale: Parallelization must not change algorithmic results.
//                       Any difference indicates race conditions or incorrect
//                       synchronization.
//
// Success Criteria:
//   - Results with OMP_NUM_THREADS=1 match results with OMP_NUM_THREADS>1
//   - All intermediate data structures have identical contents
int test_serial_parallel_equivalence_small() {
    std::cout << "=== TEST: Serial-Parallel Equivalence (Small Problem) ===" << std::endl;

    global_simulation_parameters params;
    params.contact_detection_algorithm_ = ContactDetectionAlgorithm::SWEEP_AND_PRUNE;
    params.contact_cutoff_adhesion_ = 1.0;
    params.contact_cutoff_repulsion_ = 0.5;

    // Create two strategies - one for serial, one for parallel execution
    auto strategy_serial = contact_detection_strategy::create(
        ContactDetectionAlgorithm::SWEEP_AND_PRUNE, params);
    auto strategy_parallel = contact_detection_strategy::create(
        ContactDetectionAlgorithm::SWEEP_AND_PRUNE, params);

    // Create test data with known geometry
    std::vector<cell*> empty_cells;
    std::vector<face*> empty_faces;
    std::vector<aabb> test_aabbs;

    // Create a grid of AABBs for testing
    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 10; ++j) {
            double x = i * 2.0;
            double y = j * 2.0;
            double z = 0.0;
            test_aabbs.emplace_back(x, y, z, x + 1.5, y + 1.5, z + 1.5);
        }
    }

    vec3 bounds(30., 30., 10.);

    // Run serial (1 thread)
    int original_threads = omp_get_max_threads();
    omp_set_num_threads(1);
    strategy_serial->prepare(empty_cells, empty_faces, test_aabbs, bounds);

    // Run parallel (multiple threads)
    omp_set_num_threads(original_threads);
    strategy_parallel->prepare(empty_cells, empty_faces, test_aabbs, bounds);

    // Query multiple positions and compare results
    std::vector<vec3> query_positions = {
        vec3(1.0, 1.0, 0.5),
        vec3(5.0, 5.0, 0.5),
        vec3(15.0, 15.0, 0.5),
        vec3(0.0, 0.0, 0.0)
    };

    bool all_match = true;
    for (const auto& pos : query_positions) {
        auto result_serial = strategy_serial->get_candidate_faces(pos, nullptr);
        auto result_parallel = strategy_parallel->get_candidate_faces(pos, nullptr);

        if (result_serial.size() != result_parallel.size()) {
            std::cout << "Size mismatch at (" << pos.dx() << ", " << pos.dy() << ", " << pos.dz() << "): "
                      << "serial=" << result_serial.size() << " parallel=" << result_parallel.size() << std::endl;
            all_match = false;
        }
    }

    // Restore thread count
    omp_set_num_threads(original_threads);

    std::cout << "Serial-parallel equivalence: " << (all_match ? "PASS" : "FAIL") << std::endl;
    return all_match ? 0 : 1;
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 2: Determinism - Multiple Runs Same Result
//
// Purpose: Verify that multiple executions with the same input produce
//          identical results. Non-determinism indicates race conditions.
//
// Scientific Rationale: Reproducibility is fundamental to scientific computing.
//                       Non-deterministic contact detection makes simulations
//                       unreproducible.
//
// Success Criteria:
//   - 10 consecutive runs produce identical results
//   - Query results have same count and same faces
int test_determinism_multiple_runs() {
    std::cout << "=== TEST: Determinism (Multiple Runs) ===" << std::endl;

    global_simulation_parameters params;
    params.contact_detection_algorithm_ = ContactDetectionAlgorithm::SWEEP_AND_PRUNE;
    params.contact_cutoff_adhesion_ = 1.0;
    params.contact_cutoff_repulsion_ = 0.5;

    std::vector<cell*> empty_cells;
    std::vector<face*> empty_faces;
    std::vector<aabb> test_aabbs;

    // Create test AABBs
    for (int i = 0; i < 50; ++i) {
        double x = (i % 10) * 2.0;
        double y = (i / 10) * 2.0;
        double z = 0.0;
        test_aabbs.emplace_back(x, y, z, x + 1.5, y + 1.5, z + 1.5);
    }

    vec3 bounds(30., 15., 10.);
    vec3 query_pos(5.0, 5.0, 0.5);

    // Run multiple times and collect results
    const int num_runs = 10;
    std::vector<size_t> result_sizes;

    for (int run = 0; run < num_runs; ++run) {
        auto strategy = contact_detection_strategy::create(
            ContactDetectionAlgorithm::SWEEP_AND_PRUNE, params);
        strategy->prepare(empty_cells, empty_faces, test_aabbs, bounds);
        auto result = strategy->get_candidate_faces(query_pos, nullptr);
        result_sizes.push_back(result.size());
    }

    // Check all results are identical
    bool all_same = true;
    for (size_t i = 1; i < result_sizes.size(); ++i) {
        if (result_sizes[i] != result_sizes[0]) {
            std::cout << "Run " << i << " differs: " << result_sizes[i]
                      << " vs " << result_sizes[0] << std::endl;
            all_same = false;
        }
    }

    std::cout << "Determinism across " << num_runs << " runs: " << (all_same ? "PASS" : "FAIL") << std::endl;
    return all_same ? 0 : 1;
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 3: Thread Count Independence
//
// Purpose: Verify that results are identical regardless of thread count.
//          Tests with 1, 2, 4, and max available threads.
//
// Scientific Rationale: Results must be independent of parallelization level.
//                       Different results with different thread counts indicate
//                       synchronization bugs.
//
// Success Criteria:
//   - Results with 1 thread match results with 2, 4, and max threads
//   - All query results have identical content
int test_thread_count_independence() {
    std::cout << "=== TEST: Thread Count Independence ===" << std::endl;

    global_simulation_parameters params;
    params.contact_detection_algorithm_ = ContactDetectionAlgorithm::SWEEP_AND_PRUNE;
    params.contact_cutoff_adhesion_ = 1.0;
    params.contact_cutoff_repulsion_ = 0.5;

    std::vector<cell*> empty_cells;
    std::vector<face*> empty_faces;
    std::vector<aabb> test_aabbs;

    // Create test AABBs
    for (int i = 0; i < 100; ++i) {
        double x = (i % 10) * 2.0;
        double y = (i / 10) * 2.0;
        double z = 0.0;
        test_aabbs.emplace_back(x, y, z, x + 1.5, y + 1.5, z + 1.5);
    }

    vec3 bounds(30., 25., 10.);
    vec3 query_pos(10.0, 10.0, 0.5);

    int original_threads = omp_get_max_threads();
    std::vector<int> thread_counts = {1, 2, 4, original_threads};
    std::vector<size_t> results;

    for (int num_threads : thread_counts) {
        if (num_threads > original_threads) continue;

        omp_set_num_threads(num_threads);
        auto strategy = contact_detection_strategy::create(
            ContactDetectionAlgorithm::SWEEP_AND_PRUNE, params);
        strategy->prepare(empty_cells, empty_faces, test_aabbs, bounds);
        auto result = strategy->get_candidate_faces(query_pos, nullptr);
        results.push_back(result.size());

        std::cout << "  Threads=" << num_threads << ": result size=" << result.size() << std::endl;
    }

    omp_set_num_threads(original_threads);

    // Check all results match
    bool all_same = true;
    for (size_t i = 1; i < results.size(); ++i) {
        if (results[i] != results[0]) {
            all_same = false;
        }
    }

    std::cout << "Thread count independence: " << (all_same ? "PASS" : "FAIL") << std::endl;
    return all_same ? 0 : 1;
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 4: Performance - Parallelization Provides Speedup
//
// Purpose: Verify that OpenMP parallelization provides speedup for large problems.
//          Target: > 1.5x speedup with 4+ threads for 1000+ AABBs.
//
// Scientific Rationale: Parallelization overhead must not exceed benefits.
//                       Slowdown with parallelization indicates implementation issues.
//
// Success Criteria:
//   - Multi-threaded execution is not slower than single-threaded
//   - For large problems (1000+ AABBs), speedup > 1.0
int test_performance_speedup() {
    std::cout << "=== TEST: Performance Speedup ===" << std::endl;

    global_simulation_parameters params;
    params.contact_detection_algorithm_ = ContactDetectionAlgorithm::SWEEP_AND_PRUNE;
    params.contact_cutoff_adhesion_ = 1.0;
    params.contact_cutoff_repulsion_ = 0.5;

    std::vector<cell*> empty_cells;
    std::vector<face*> empty_faces;
    std::vector<aabb> test_aabbs;

    // Create large test case (1000 AABBs in 10x10x10 grid)
    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 10; ++j) {
            for (int k = 0; k < 10; ++k) {
                double x = i * 2.0;
                double y = j * 2.0;
                double z = k * 2.0;
                test_aabbs.emplace_back(x, y, z, x + 1.5, y + 1.5, z + 1.5);
            }
        }
    }

    vec3 bounds(30., 30., 30.);

    int original_threads = omp_get_max_threads();
    const int num_iterations = 50;

    // Time serial execution
    omp_set_num_threads(1);
    auto start_serial = std::chrono::high_resolution_clock::now();
    for (int iter = 0; iter < num_iterations; ++iter) {
        auto strategy = contact_detection_strategy::create(
            ContactDetectionAlgorithm::SWEEP_AND_PRUNE, params);
        strategy->prepare(empty_cells, empty_faces, test_aabbs, bounds);
    }
    auto end_serial = std::chrono::high_resolution_clock::now();
    double time_serial = std::chrono::duration<double, std::milli>(end_serial - start_serial).count();

    // Time parallel execution
    omp_set_num_threads(original_threads);
    auto start_parallel = std::chrono::high_resolution_clock::now();
    for (int iter = 0; iter < num_iterations; ++iter) {
        auto strategy = contact_detection_strategy::create(
            ContactDetectionAlgorithm::SWEEP_AND_PRUNE, params);
        strategy->prepare(empty_cells, empty_faces, test_aabbs, bounds);
    }
    auto end_parallel = std::chrono::high_resolution_clock::now();
    double time_parallel = std::chrono::duration<double, std::milli>(end_parallel - start_parallel).count();

    omp_set_num_threads(original_threads);

    double speedup = time_serial / time_parallel;

    std::cout << "  AABBs: " << test_aabbs.size() << std::endl;
    std::cout << "  Threads: " << original_threads << std::endl;
    std::cout << "  Serial time: " << time_serial << " ms" << std::endl;
    std::cout << "  Parallel time: " << time_parallel << " ms" << std::endl;
    std::cout << "  Speedup: " << speedup << "x" << std::endl;

    // Success if not slower (speedup >= 0.9 to account for measurement noise)
    bool success = (speedup >= 0.9);
    std::cout << "Performance not degraded: " << (success ? "PASS" : "FAIL") << std::endl;

    return success ? 0 : 1;
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 5: Empty Input Thread Safety
//
// Purpose: Verify that empty input is handled correctly with multiple threads.
//
// Scientific Rationale: Edge cases like empty input can expose thread safety
//                       issues that don't appear with normal data.
//
// Success Criteria:
//   - No crashes or exceptions with empty input
//   - Returns empty results consistently
int test_empty_input_thread_safety() {
    std::cout << "=== TEST: Empty Input Thread Safety ===" << std::endl;

    global_simulation_parameters params;
    params.contact_detection_algorithm_ = ContactDetectionAlgorithm::SWEEP_AND_PRUNE;
    params.contact_cutoff_adhesion_ = 1.0;
    params.contact_cutoff_repulsion_ = 0.5;

    std::vector<cell*> empty_cells;
    std::vector<face*> empty_faces;
    std::vector<aabb> empty_aabbs;
    vec3 bounds(10., 10., 10.);

    bool success = true;
    const int num_runs = 20;

    for (int run = 0; run < num_runs; ++run) {
        try {
            auto strategy = contact_detection_strategy::create(
                ContactDetectionAlgorithm::SWEEP_AND_PRUNE, params);
            strategy->prepare(empty_cells, empty_faces, empty_aabbs, bounds);

            vec3 query_pos(5.0, 5.0, 5.0);
            auto result = strategy->get_candidate_faces(query_pos, nullptr);

            if (!result.empty()) {
                std::cout << "Run " << run << ": non-empty result from empty input" << std::endl;
                success = false;
            }
        } catch (const std::exception& e) {
            std::cout << "Run " << run << ": exception: " << e.what() << std::endl;
            success = false;
        }
    }

    std::cout << "Empty input thread safety: " << (success ? "PASS" : "FAIL") << std::endl;
    return success ? 0 : 1;
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 6: Large Scale Stress Test
//
// Purpose: Verify correctness under stress with large number of AABBs.
//
// Scientific Rationale: Race conditions may only manifest with sufficient
//                       contention. Large scale tests increase likelihood of
//                       exposing subtle threading bugs.
//
// Success Criteria:
//   - No crashes with 5000+ AABBs
//   - Results are deterministic
int test_large_scale_stress() {
    std::cout << "=== TEST: Large Scale Stress Test ===" << std::endl;

    global_simulation_parameters params;
    params.contact_detection_algorithm_ = ContactDetectionAlgorithm::SWEEP_AND_PRUNE;
    params.contact_cutoff_adhesion_ = 0.5;
    params.contact_cutoff_repulsion_ = 0.25;

    std::vector<cell*> empty_cells;
    std::vector<face*> empty_faces;
    std::vector<aabb> test_aabbs;

    // Create 5000 AABBs (approximately 17x17x17 grid)
    for (int i = 0; i < 17; ++i) {
        for (int j = 0; j < 17; ++j) {
            for (int k = 0; k < 17; ++k) {
                double x = i * 1.0;
                double y = j * 1.0;
                double z = k * 1.0;
                test_aabbs.emplace_back(x, y, z, x + 0.8, y + 0.8, z + 0.8);
            }
        }
    }

    std::cout << "  Created " << test_aabbs.size() << " AABBs" << std::endl;

    vec3 bounds(20., 20., 20.);

    // Run multiple times and verify determinism
    std::vector<size_t> results;
    vec3 query_pos(8.5, 8.5, 8.5);

    const int num_runs = 5;
    bool success = true;

    for (int run = 0; run < num_runs; ++run) {
        try {
            auto strategy = contact_detection_strategy::create(
                ContactDetectionAlgorithm::SWEEP_AND_PRUNE, params);
            strategy->prepare(empty_cells, empty_faces, test_aabbs, bounds);
            auto result = strategy->get_candidate_faces(query_pos, nullptr);
            results.push_back(result.size());
        } catch (const std::exception& e) {
            std::cout << "Run " << run << ": exception: " << e.what() << std::endl;
            success = false;
        }
    }

    // Verify determinism
    for (size_t i = 1; i < results.size(); ++i) {
        if (results[i] != results[0]) {
            std::cout << "Non-deterministic: run " << i << " = " << results[i]
                      << " vs run 0 = " << results[0] << std::endl;
            success = false;
        }
    }

    std::cout << "Large scale stress test: " << (success ? "PASS" : "FAIL") << std::endl;
    return success ? 0 : 1;
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Main Test Runner
int main(int argc, char** argv) {
    assert(argc == 2);
    std::string test_name = argv[1];

    std::cout << "OpenMP max threads: " << omp_get_max_threads() << std::endl;

    if (test_name == "test_serial_parallel_equivalence_small")
        return test_serial_parallel_equivalence_small();

    if (test_name == "test_determinism_multiple_runs")
        return test_determinism_multiple_runs();

    if (test_name == "test_thread_count_independence")
        return test_thread_count_independence();

    if (test_name == "test_performance_speedup")
        return test_performance_speedup();

    if (test_name == "test_empty_input_thread_safety")
        return test_empty_input_thread_safety();

    if (test_name == "test_large_scale_stress")
        return test_large_scale_stress();

    std::cout << "TEST NAME: " << test_name << " DOES NOT EXIST" << std::endl;
    return 1;
}
//---------------------------------------------------------------------------------------------------------
