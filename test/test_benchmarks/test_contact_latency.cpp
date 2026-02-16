#include <cassert>
#include <string>
#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <random>

#include "contact_detection_strategy.hpp"
#include "contact_detection_sap_strategy.hpp"
#include "custom_structures.hpp"
#include "vec3.hpp"

/**
 * Micro-benchmark Suite: Contact Detection Latency
 *
 * Measures per-cell latency for the contact detection pipeline:
 *   prepare() + get_candidate_faces() per query
 *
 * Target: <50 us/cell for systems up to 1000 cells
 *
 * Methodology:
 * - Uses high_resolution_clock for sub-microsecond precision
 * - Warmup iterations to stabilize caches and branch predictors
 * - Multiple trials with median reporting to reject outliers
 * - Tests both USPG and SAP strategies at multiple scales
 *
 * Hardware counters (cache misses, IPC, bandwidth) are measured externally
 * via the perf_wrapper.sh script wrapping these executables.
 */

// Timing configuration
constexpr int WARMUP_ITERATIONS = 3;
constexpr int MEASUREMENT_ITERATIONS = 10;

// Scale thresholds
constexpr int SMALL_SCALE = 50;    // 50 cells
constexpr int MEDIUM_SCALE = 200;  // 200 cells
constexpr int LARGE_SCALE = 1000;  // 1000 cells

// Target latency: 50 us per cell
constexpr double TARGET_LATENCY_US_PER_CELL = 50.0;

/**
 * Generate a 3D grid of AABBs simulating cells.
 *
 * Creates a cubic arrangement of cells with slight overlap (controlled by
 * gap_fraction). This models the common biological simulation scenario of
 * tightly packed cells.
 *
 * @param num_cells     Target number of cells (rounded to nearest cube)
 * @param cell_size     Size of each cell's bounding box
 * @param gap_fraction  Fraction of cell_size as gap between cells (negative = overlap)
 * @return Vector of AABBs and actual cell count
 */
std::pair<std::vector<aabb>, int> generate_cell_grid(int num_cells, double cell_size, double gap_fraction) {
    int cells_per_axis = static_cast<int>(std::ceil(std::cbrt(static_cast<double>(num_cells))));
    double spacing = cell_size * (1.0 + gap_fraction);
    std::vector<aabb> aabbs;
    aabbs.reserve(cells_per_axis * cells_per_axis * cells_per_axis);

    for (int i = 0; i < cells_per_axis; ++i) {
        for (int j = 0; j < cells_per_axis; ++j) {
            for (int k = 0; k < cells_per_axis; ++k) {
                double x = i * spacing;
                double y = j * spacing;
                double z = k * spacing;
                aabbs.emplace_back(x, y, z, x + cell_size, y + cell_size, z + cell_size);
            }
        }
    }

    return {aabbs, cells_per_axis * cells_per_axis * cells_per_axis};
}

/**
 * Generate random query positions within the domain.
 */
std::vector<vec3> generate_query_positions(int count, double domain_size, unsigned seed = 42) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> dist(0.0, domain_size);
    std::vector<vec3> positions;
    positions.reserve(count);
    for (int i = 0; i < count; ++i) {
        positions.emplace_back(dist(rng), dist(rng), dist(rng));
    }
    return positions;
}

/**
 * Measure the median latency of prepare() + query cycle for a given strategy.
 *
 * Returns: pair of (total_time_us, per_cell_time_us)
 */
std::pair<double, double> measure_prepare_query_latency(
    ContactDetectionAlgorithm algo,
    int num_cells,
    int num_queries
) {
    global_simulation_parameters params;
    params.contact_detection_algorithm_ = algo;
    params.contact_cutoff_adhesion_ = 1.0;
    params.contact_cutoff_repulsion_ = 0.5;

    auto [aabbs, actual_cells] = generate_cell_grid(num_cells, 1.0, -0.1);
    double domain_size = std::cbrt(static_cast<double>(actual_cells)) * 0.9 + 1.0;
    vec3 bounds(domain_size, domain_size, domain_size);

    auto queries = generate_query_positions(num_queries, domain_size);

    std::vector<cell*> empty_cells;
    std::vector<face*> empty_faces;

    // Warmup
    for (int w = 0; w < WARMUP_ITERATIONS; ++w) {
        auto strategy = contact_detection_strategy::create(algo, params);
        strategy->prepare(empty_cells, empty_faces, aabbs, bounds);
        for (const auto& q : queries) {
            auto result = strategy->get_candidate_faces(q, nullptr);
            // Prevent dead-code elimination
            if (result.size() > 1000000) std::cout << "unreachable" << std::endl;
        }
    }

    // Measurement
    std::vector<double> trial_times;
    trial_times.reserve(MEASUREMENT_ITERATIONS);

    for (int t = 0; t < MEASUREMENT_ITERATIONS; ++t) {
        auto strategy = contact_detection_strategy::create(algo, params);

        auto start = std::chrono::high_resolution_clock::now();

        strategy->prepare(empty_cells, empty_faces, aabbs, bounds);
        for (const auto& q : queries) {
            auto result = strategy->get_candidate_faces(q, nullptr);
            if (result.size() > 1000000) std::cout << "unreachable" << std::endl;
        }

        auto end = std::chrono::high_resolution_clock::now();
        double elapsed_us = std::chrono::duration<double, std::micro>(end - start).count();
        trial_times.push_back(elapsed_us);
    }

    // Report median
    std::sort(trial_times.begin(), trial_times.end());
    double median_us = trial_times[trial_times.size() / 2];
    double per_cell_us = median_us / static_cast<double>(actual_cells);

    return {median_us, per_cell_us};
}

//---------------------------------------------------------------------------------------------------------
// Test 1: USPG Latency at Small Scale (50 cells)
//
// Purpose: Measure baseline USPG per-cell latency at small scale.
//          Establishes the low-overhead baseline for the spatial grid.
//
// Target: <50 us/cell
int test_uspg_latency_small() {
    std::cout << "=== BENCHMARK: USPG Latency (Small Scale, " << SMALL_SCALE << " cells) ===" << std::endl;

    auto [total_us, per_cell_us] = measure_prepare_query_latency(
        ContactDetectionAlgorithm::USPG, SMALL_SCALE, SMALL_SCALE * 10
    );

    std::cout << "Total time:    " << total_us << " us" << std::endl;
    std::cout << "Per-cell time: " << per_cell_us << " us/cell" << std::endl;
    std::cout << "Target:        " << TARGET_LATENCY_US_PER_CELL << " us/cell" << std::endl;

    bool t1 = (per_cell_us < TARGET_LATENCY_US_PER_CELL);
    std::cout << "Within target: " << t1 << std::endl;

    return !t1;
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 2: SAP Latency at Small Scale (50 cells)
//
// Purpose: Measure SAP per-cell latency at small scale.
//          Compares against USPG baseline to quantify SAP overhead.
//
// Target: <50 us/cell
int test_sap_latency_small() {
    std::cout << "=== BENCHMARK: SAP Latency (Small Scale, " << SMALL_SCALE << " cells) ===" << std::endl;

    auto [total_us, per_cell_us] = measure_prepare_query_latency(
        ContactDetectionAlgorithm::SWEEP_AND_PRUNE, SMALL_SCALE, SMALL_SCALE * 10
    );

    std::cout << "Total time:    " << total_us << " us" << std::endl;
    std::cout << "Per-cell time: " << per_cell_us << " us/cell" << std::endl;
    std::cout << "Target:        " << TARGET_LATENCY_US_PER_CELL << " us/cell" << std::endl;

    bool t1 = (per_cell_us < TARGET_LATENCY_US_PER_CELL);
    std::cout << "Within target: " << t1 << std::endl;

    return !t1;
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 3: USPG Latency at Medium Scale (200 cells)
//
// Purpose: Measure USPG scaling behavior at moderate cell count.
//          Verifies sublinear scaling of the spatial grid approach.
//
// Target: <50 us/cell
int test_uspg_latency_medium() {
    std::cout << "=== BENCHMARK: USPG Latency (Medium Scale, " << MEDIUM_SCALE << " cells) ===" << std::endl;

    auto [total_us, per_cell_us] = measure_prepare_query_latency(
        ContactDetectionAlgorithm::USPG, MEDIUM_SCALE, MEDIUM_SCALE * 5
    );

    std::cout << "Total time:    " << total_us << " us" << std::endl;
    std::cout << "Per-cell time: " << per_cell_us << " us/cell" << std::endl;
    std::cout << "Target:        " << TARGET_LATENCY_US_PER_CELL << " us/cell" << std::endl;

    bool t1 = (per_cell_us < TARGET_LATENCY_US_PER_CELL);
    std::cout << "Within target: " << t1 << std::endl;

    return !t1;
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 4: SAP Latency at Medium Scale (200 cells)
//
// Purpose: Measure SAP scaling behavior at moderate cell count.
//          SAP's O(C log C) prepare should scale better than USPG for non-uniform distributions.
//
// Target: <50 us/cell
int test_sap_latency_medium() {
    std::cout << "=== BENCHMARK: SAP Latency (Medium Scale, " << MEDIUM_SCALE << " cells) ===" << std::endl;

    auto [total_us, per_cell_us] = measure_prepare_query_latency(
        ContactDetectionAlgorithm::SWEEP_AND_PRUNE, MEDIUM_SCALE, MEDIUM_SCALE * 5
    );

    std::cout << "Total time:    " << total_us << " us" << std::endl;
    std::cout << "Per-cell time: " << per_cell_us << " us/cell" << std::endl;
    std::cout << "Target:        " << TARGET_LATENCY_US_PER_CELL << " us/cell" << std::endl;

    bool t1 = (per_cell_us < TARGET_LATENCY_US_PER_CELL);
    std::cout << "Within target: " << t1 << std::endl;

    return !t1;
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 5: USPG Latency at Large Scale (1000 cells)
//
// Purpose: Stress test USPG at the upper end of the latency target range.
//          This is the primary acceptance criterion for the profiling infrastructure.
//
// Target: <50 us/cell
int test_uspg_latency_large() {
    std::cout << "=== BENCHMARK: USPG Latency (Large Scale, " << LARGE_SCALE << " cells) ===" << std::endl;

    auto [total_us, per_cell_us] = measure_prepare_query_latency(
        ContactDetectionAlgorithm::USPG, LARGE_SCALE, LARGE_SCALE * 2
    );

    std::cout << "Total time:    " << total_us << " us" << std::endl;
    std::cout << "Per-cell time: " << per_cell_us << " us/cell" << std::endl;
    std::cout << "Target:        " << TARGET_LATENCY_US_PER_CELL << " us/cell" << std::endl;

    bool t1 = (per_cell_us < TARGET_LATENCY_US_PER_CELL);
    std::cout << "Within target: " << t1 << std::endl;

    return !t1;
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 6: SAP Latency at Large Scale (1000 cells)
//
// Purpose: Stress test SAP at the upper end of the latency target range.
//
// Target: <50 us/cell
int test_sap_latency_large() {
    std::cout << "=== BENCHMARK: SAP Latency (Large Scale, " << LARGE_SCALE << " cells) ===" << std::endl;

    auto [total_us, per_cell_us] = measure_prepare_query_latency(
        ContactDetectionAlgorithm::SWEEP_AND_PRUNE, LARGE_SCALE, LARGE_SCALE * 2
    );

    std::cout << "Total time:    " << total_us << " us" << std::endl;
    std::cout << "Per-cell time: " << per_cell_us << " us/cell" << std::endl;
    std::cout << "Target:        " << TARGET_LATENCY_US_PER_CELL << " us/cell" << std::endl;

    bool t1 = (per_cell_us < TARGET_LATENCY_US_PER_CELL);
    std::cout << "Within target: " << t1 << std::endl;

    return !t1;
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 7: Scaling Behavior - Per-Cell Latency Should Not Grow Linearly
//
// Purpose: Verify that the contact detection algorithms exhibit sublinear
//          scaling as expected from their O(C log C) or O(C) complexity.
//
// Success Criteria:
//   - Large-scale per-cell latency <= 3x small-scale per-cell latency
//   - This rules out O(C^2) degradation
int test_scaling_behavior() {
    std::cout << "=== BENCHMARK: Scaling Behavior Analysis ===" << std::endl;

    // Measure USPG at small and large scale
    auto [uspg_small_total, uspg_small_per_cell] = measure_prepare_query_latency(
        ContactDetectionAlgorithm::USPG, SMALL_SCALE, SMALL_SCALE * 10
    );
    auto [uspg_large_total, uspg_large_per_cell] = measure_prepare_query_latency(
        ContactDetectionAlgorithm::USPG, LARGE_SCALE, LARGE_SCALE * 2
    );

    // Measure SAP at small and large scale
    auto [sap_small_total, sap_small_per_cell] = measure_prepare_query_latency(
        ContactDetectionAlgorithm::SWEEP_AND_PRUNE, SMALL_SCALE, SMALL_SCALE * 10
    );
    auto [sap_large_total, sap_large_per_cell] = measure_prepare_query_latency(
        ContactDetectionAlgorithm::SWEEP_AND_PRUNE, LARGE_SCALE, LARGE_SCALE * 2
    );

    double uspg_ratio = uspg_large_per_cell / std::max(uspg_small_per_cell, 0.001);
    double sap_ratio = sap_large_per_cell / std::max(sap_small_per_cell, 0.001);

    std::cout << "USPG small per-cell: " << uspg_small_per_cell << " us" << std::endl;
    std::cout << "USPG large per-cell: " << uspg_large_per_cell << " us" << std::endl;
    std::cout << "USPG ratio (large/small): " << uspg_ratio << "x" << std::endl;
    std::cout << std::endl;
    std::cout << "SAP small per-cell:  " << sap_small_per_cell << " us" << std::endl;
    std::cout << "SAP large per-cell:  " << sap_large_per_cell << " us" << std::endl;
    std::cout << "SAP ratio (large/small):  " << sap_ratio << "x" << std::endl;

    // Sublinear scaling: per-cell cost should not grow more than 3x
    // when going from SMALL_SCALE to LARGE_SCALE (20x increase in cells)
    bool t1 = (uspg_ratio < 3.0);
    bool t2 = (sap_ratio < 3.0);

    std::cout << "USPG sublinear scaling: " << t1 << std::endl;
    std::cout << "SAP sublinear scaling:  " << t2 << std::endl;

    return !(t1 && t2);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Main Test Runner
int main(int argc, char** argv) {
    assert(argc == 2);
    std::string test_name = argv[1];

    if (test_name == "test_uspg_latency_small")   return test_uspg_latency_small();
    if (test_name == "test_sap_latency_small")    return test_sap_latency_small();
    if (test_name == "test_uspg_latency_medium")  return test_uspg_latency_medium();
    if (test_name == "test_sap_latency_medium")   return test_sap_latency_medium();
    if (test_name == "test_uspg_latency_large")   return test_uspg_latency_large();
    if (test_name == "test_sap_latency_large")    return test_sap_latency_large();
    if (test_name == "test_scaling_behavior")     return test_scaling_behavior();

    std::cout << "TEST NAME: " << test_name << " DOES NOT EXIST" << std::endl;
    return 1;
}
//---------------------------------------------------------------------------------------------------------
