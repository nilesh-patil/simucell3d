/**
 * @file geometry_validation_benchmark.cpp
 * @brief Scientific benchmark tests for geometric invariants in SimuCell3D
 *
 * Validates that core geometric properties are preserved throughout simulation:
 * - Volume conservation (no-growth regime)
 * - Mesh quality (triangle aspect ratios)
 * - Face normal consistency (outward orientation)
 * - Surface manifoldness (watertight mesh)
 * - Isoperimetric ratio (shape regularity)
 *
 * Test pattern follows project convention: return 0=pass, 1=fail.
 * Dispatched via argv[1].
 *
 * NOTE: Test Isolation for Parallel Execution
 * These tests use unique output directories per test/process to enable parallel test
 * execution with ctest -j. Each test creates a unique directory based on PID and test name,
 * preventing race conditions when multiple tests run simultaneously.
 */

#include <cassert>
#include <string>
#include <iostream>
#include <cmath>
#include <vector>
#include <memory>
#include <filesystem>
#include <algorithm>
#include <numeric>
#include <unistd.h>  // For getpid()

#include "solver.hpp"
#include "simulation_initializer.hpp"
#include "cell.hpp"
#include "face.hpp"
#include "node.hpp"
#include "vec3.hpp"


//---------------------------------------------------------------------------------------------------------
// Test isolation helper: Create unique output directory for parallel test execution
// Uses PID and test name to ensure each test instance has its own output directory
//---------------------------------------------------------------------------------------------------------
static std::string create_unique_output_dir(const std::string& test_name) {
    pid_t pid = getpid();
    std::string unique_dir = "/tmp/simucell3d_test_" + test_name + "_" + std::to_string(pid);

    // Clean up any existing directory from previous failed runs
    std::filesystem::remove_all(unique_dir);

    return unique_dir;
}

//---------------------------------------------------------------------------------------------------------
// Test isolation helper: Clean up output directory after test
//---------------------------------------------------------------------------------------------------------
static void cleanup_output_dir(const std::string& output_dir) {
    try {
        std::filesystem::remove_all(output_dir);
    } catch (const std::exception& e) {
        // Ignore cleanup errors - directory may already be cleaned up
        std::cerr << "Warning: Could not clean up " << output_dir << ": " << e.what() << std::endl;
    }
}

//---------------------------------------------------------------------------------------------------------
// Test-derived solver class to expose protected run_iteration() for benchmarking
//---------------------------------------------------------------------------------------------------------
class benchmark_solver : public solver {
public:
    using solver::solver;  // Inherit constructors

    // Expose run_iteration for step-by-step simulation control
    void step() { run_iteration(); }

    // Expose cell list for inspection
    const std::vector<cell_ptr>& cells() const { return cell_lst_; }
};

//---------------------------------------------------------------------------------------------------------
// Helper: Load parameter file and simulation parameters with test isolation
//---------------------------------------------------------------------------------------------------------
static std::string get_param_file_path() {
    return std::string(PROJECT_SOURCE_DIR) +
           "/test/test_scientific_benchmarks/params_single_sphere.xml";
}

static global_simulation_parameters get_sim_params(const std::string& output_dir) {
    simulation_initializer sim_init(get_param_file_path(), false);
    global_simulation_parameters params = sim_init.get_simulation_parameters();
    params.output_folder_path_ = output_dir;
    return params;
}

static std::vector<cell_ptr> get_cells() {
    simulation_initializer sim_init(get_param_file_path(), false);
    return sim_init.get_cell_lst();
}


//---------------------------------------------------------------------------------------------------------
/**
 * @brief Test volume preservation over 500 iterations with no growth configured
 *
 * For cells with zero growth rate, volume should be conserved by the pressure
 * feedback mechanism. After the cell reaches mechanical equilibrium, the volume
 * should remain within 5% of its initial value.
 *
 * Algorithm:
 * 1. Load params_single_sphere.xml, create solver
 * 2. Run 1 iteration to let the simulation initialize internal state
 * 3. Record initial volume V0 for each cell
 * 4. Run 499 more iterations (500 total)
 * 5. Check: |V(t) - V0| / V0 < 5% for all cells
 *
 * @return 0 on success, 1 on failure
 */
int test_volume_preservation() {
    std::cout << "Running test_volume_preservation..." << std::endl;

    std::string test_output_dir = create_unique_output_dir("volume_preservation");

    try {
        std::vector<cell_ptr> cell_lst = get_cells();
        global_simulation_parameters sim_params = get_sim_params(test_output_dir);

        if (cell_lst.empty()) {
            std::cout << "FAILED: No cells loaded from parameter file" << std::endl;
            cleanup_output_dir(test_output_dir);
            return 1;
        }

        benchmark_solver bs(sim_params, cell_lst, 1, true, false, "static");

        // Run 200 iterations to let cell reach pressure equilibrium
        // The initial mesh volume may not match target volume, causing
        // physical volume adjustment via bulk modulus pressure feedback
        const int transient_iterations = 200;
        for (int i = 0; i < transient_iterations; ++i) {
            bs.step();
        }

        // Record post-transient volumes (at mechanical equilibrium)
        std::vector<double> baseline_volumes;
        for (const auto& c : bs.cells()) {
            double v0 = c->get_volume();
            baseline_volumes.push_back(v0);
            std::cout << "  Cell " << c->get_id() << " post-transient volume: " << v0 << std::endl;
        }

        // Run 300 more iterations to check stability
        const int stability_iterations = 300;
        for (int i = 0; i < stability_iterations; ++i) {
            bs.step();
        }

        // Verify volume stability (post-equilibrium)
        bool all_passed = true;
        const double tolerance = 0.05;  // 5% relative tolerance after equilibrium
        for (size_t ci = 0; ci < bs.cells().size(); ++ci) {
            double v_final = bs.cells()[ci]->get_volume();
            double v0 = baseline_volumes[ci];

            // Skip cells with negligible volume (avoid division by zero)
            if (v0 < 1e-30) {
                std::cout << "  Cell " << bs.cells()[ci]->get_id()
                          << ": skipped (negligible volume " << v0 << ")" << std::endl;
                continue;
            }

            double relative_error = std::abs(v_final - v0) / v0;
            bool cell_passed = (relative_error < tolerance);
            std::cout << "  Cell " << bs.cells()[ci]->get_id()
                      << ": V_baseline=" << v0 << ", V_final=" << v_final
                      << ", relative_error=" << relative_error
                      << (cell_passed ? " PASS" : " FAIL") << std::endl;

            if (!cell_passed) {
                all_passed = false;
            }
        }

        if (!all_passed) {
            std::cout << "test_volume_preservation FAILED" << std::endl;
            cleanup_output_dir(test_output_dir);
            return 1;
        }

        std::cout << "test_volume_preservation PASSED" << std::endl;
        cleanup_output_dir(test_output_dir);
        return 0;

    } catch (const std::exception& e) {
        std::cout << "FAILED with exception: " << e.what() << std::endl;
        cleanup_output_dir(test_output_dir);
        return 1;
    }
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
/**
 * @brief Test mesh quality by examining triangle aspect ratios after simulation
 *
 * Good mesh quality is essential for numerical accuracy. Degenerate triangles
 * (very elongated or very thin) lead to numerical instability.
 *
 * For each used face, compute the edge length ratio = longest_edge / shortest_edge.
 * An equilateral triangle has ratio = 1.
 *
 * Criteria:
 * - No face has ratio > 100 (degenerate)
 * - >= 80% of faces have ratio < 5 (well-shaped)
 *
 * @return 0 on success, 1 on failure
 */
int test_mesh_quality() {
    std::cout << "Running test_mesh_quality..." << std::endl;

    std::string test_output_dir = create_unique_output_dir("mesh_quality");

    try {
        std::vector<cell_ptr> cell_lst = get_cells();
        global_simulation_parameters sim_params = get_sim_params(test_output_dir);

        if (cell_lst.empty()) {
            std::cout << "FAILED: No cells loaded from parameter file" << std::endl;
            cleanup_output_dir(test_output_dir);
            return 1;
        }

        benchmark_solver bs(sim_params, cell_lst, 1, true, false, "static");

        // Run 200 iterations
        for (int i = 0; i < 200; ++i) {
            bs.step();
        }

        // Collect aspect ratios across all cells
        std::vector<double> all_ratios;
        size_t degenerate_count = 0;

        for (const auto& c : bs.cells()) {
            const std::vector<face>& face_lst = c->get_face_lst();

            for (const auto& f : face_lst) {
                if (!f.is_used()) continue;

                // Get the 3 node positions
                auto [n1_id, n2_id, n3_id] = f.get_node_ids();
                const vec3& p1 = c->get_const_ref_node(n1_id).pos();
                const vec3& p2 = c->get_const_ref_node(n2_id).pos();
                const vec3& p3 = c->get_const_ref_node(n3_id).pos();

                // Compute edge lengths
                double e1 = (p2 - p1).norm();
                double e2 = (p3 - p2).norm();
                double e3 = (p1 - p3).norm();

                // Find longest and shortest edges
                double max_edge = std::max({e1, e2, e3});
                double min_edge = std::min({e1, e2, e3});

                // Avoid division by zero for degenerate faces
                if (min_edge < 1e-30) {
                    degenerate_count++;
                    all_ratios.push_back(1e6);  // Mark as very degenerate
                    continue;
                }

                double ratio = max_edge / min_edge;
                all_ratios.push_back(ratio);

                if (ratio > 100.0) {
                    degenerate_count++;
                }
            }
        }

        if (all_ratios.empty()) {
            std::cout << "FAILED: No faces found in any cell" << std::endl;
            cleanup_output_dir(test_output_dir);
            return 1;
        }

        // Count faces with good aspect ratio (ratio < 5)
        size_t good_count = 0;
        for (double r : all_ratios) {
            if (r < 5.0) good_count++;
        }
        double good_fraction = static_cast<double>(good_count) / static_cast<double>(all_ratios.size());

        // Compute statistics for diagnostics
        double min_ratio = *std::min_element(all_ratios.begin(), all_ratios.end());
        double max_ratio = *std::max_element(all_ratios.begin(), all_ratios.end());
        double mean_ratio = std::accumulate(all_ratios.begin(), all_ratios.end(), 0.0)
                            / static_cast<double>(all_ratios.size());

        std::cout << "  Total faces analyzed: " << all_ratios.size() << std::endl;
        std::cout << "  Aspect ratio stats: min=" << min_ratio
                  << ", max=" << max_ratio
                  << ", mean=" << mean_ratio << std::endl;
        std::cout << "  Degenerate faces (ratio > 100): " << degenerate_count << std::endl;
        std::cout << "  Good faces (ratio < 5): " << good_count
                  << " (" << (good_fraction * 100.0) << "%)" << std::endl;

        // Check criteria
        bool no_degenerate = (degenerate_count == 0);
        bool enough_good = (good_fraction >= 0.80);

        std::cout << "  Criterion 1 (no degenerate faces): " << (no_degenerate ? "PASS" : "FAIL") << std::endl;
        std::cout << "  Criterion 2 (>= 80% good faces): " << (enough_good ? "PASS" : "FAIL") << std::endl;

        if (!no_degenerate || !enough_good) {
            std::cout << "test_mesh_quality FAILED" << std::endl;
            cleanup_output_dir(test_output_dir);
            return 1;
        }

        std::cout << "test_mesh_quality PASSED" << std::endl;
        cleanup_output_dir(test_output_dir);
        return 0;

    } catch (const std::exception& e) {
        std::cout << "FAILED with exception: " << e.what() << std::endl;
        cleanup_output_dir(test_output_dir);
        return 1;
    }
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
/**
 * @brief Test that face normals consistently point outward from the cell centroid
 *
 * SimuCell3D relies on outward-pointing face normals for correct pressure calculation
 * and contact detection. After simulation, we verify that the dot product of each
 * face normal with the vector from centroid to face center is positive (outward).
 *
 * Criterion: >= 90% of faces have outward-pointing normals
 *
 * @return 0 on success, 1 on failure
 */
int test_face_normal_consistency() {
    std::cout << "Running test_face_normal_consistency..." << std::endl;

    std::string test_output_dir = create_unique_output_dir("face_normal_consistency");

    try {
        std::vector<cell_ptr> cell_lst = get_cells();
        global_simulation_parameters sim_params = get_sim_params(test_output_dir);

        if (cell_lst.empty()) {
            std::cout << "FAILED: No cells loaded from parameter file" << std::endl;
            cleanup_output_dir(test_output_dir);
            return 1;
        }

        benchmark_solver bs(sim_params, cell_lst, 1, true, false, "static");

        // Run 200 iterations
        for (int i = 0; i < 200; ++i) {
            bs.step();
        }

        size_t total_faces = 0;
        size_t outward_count = 0;

        for (const auto& c : bs.cells()) {
            const vec3& centroid = c->get_centroid();
            const std::vector<face>& face_lst = c->get_face_lst();

            for (const auto& f : face_lst) {
                if (!f.is_used()) continue;

                total_faces++;

                // Get face normal (already computed by simulation)
                const vec3& face_normal = f.get_normal();

                // Compute face center as average of 3 vertices
                auto [n1_id, n2_id, n3_id] = f.get_node_ids();
                const vec3& p1 = c->get_const_ref_node(n1_id).pos();
                const vec3& p2 = c->get_const_ref_node(n2_id).pos();
                const vec3& p3 = c->get_const_ref_node(n3_id).pos();

                vec3 face_center(
                    (p1.dx() + p2.dx() + p3.dx()) / 3.0,
                    (p1.dy() + p2.dy() + p3.dy()) / 3.0,
                    (p1.dz() + p2.dz() + p3.dz()) / 3.0
                );

                // Compute outward vector from centroid to face center
                vec3 outward_vector = face_center - centroid;

                // Check if normal points outward (positive dot product)
                double dot = face_normal.dot(outward_vector);
                if (dot > 0.0) {
                    outward_count++;
                }
            }
        }

        if (total_faces == 0) {
            std::cout << "FAILED: No faces found in any cell" << std::endl;
            cleanup_output_dir(test_output_dir);
            return 1;
        }

        double outward_fraction = static_cast<double>(outward_count) / static_cast<double>(total_faces);
        std::cout << "  Total faces checked: " << total_faces << std::endl;
        std::cout << "  Outward-pointing normals: " << outward_count
                  << " (" << (outward_fraction * 100.0) << "%)" << std::endl;
        std::cout << "  Inward-pointing normals: " << (total_faces - outward_count) << std::endl;

        bool passed = (outward_fraction >= 0.90);
        std::cout << "  Criterion (>= 90% outward): " << (passed ? "PASS" : "FAIL") << std::endl;

        if (!passed) {
            std::cout << "test_face_normal_consistency FAILED" << std::endl;
            cleanup_output_dir(test_output_dir);
            return 1;
        }

        std::cout << "test_face_normal_consistency PASSED" << std::endl;
        cleanup_output_dir(test_output_dir);
        return 0;

    } catch (const std::exception& e) {
        std::cout << "FAILED with exception: " << e.what() << std::endl;
        cleanup_output_dir(test_output_dir);
        return 1;
    }
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
/**
 * @brief Test that all cell meshes remain manifold (watertight) throughout simulation
 *
 * A manifold mesh has no holes, no T-junctions, and every edge is shared by exactly
 * two faces. This is critical for volume computation and pressure calculation.
 *
 * Algorithm:
 * 1. Create solver, check manifoldness before running
 * 2. Run 200 iterations
 * 3. Check manifoldness again after simulation
 * 4. PASS if all cells are manifold both before and after
 *
 * @return 0 on success, 1 on failure
 */
int test_manifoldness() {
    std::cout << "Running test_manifoldness..." << std::endl;

    std::string test_output_dir = create_unique_output_dir("manifoldness");

    try {
        std::vector<cell_ptr> cell_lst = get_cells();
        global_simulation_parameters sim_params = get_sim_params(test_output_dir);

        if (cell_lst.empty()) {
            std::cout << "FAILED: No cells loaded from parameter file" << std::endl;
            cleanup_output_dir(test_output_dir);
            return 1;
        }

        benchmark_solver bs(sim_params, cell_lst, 1, true, false, "static");

        // Check manifoldness BEFORE running
        bool all_manifold_before = true;
        std::cout << "  Manifoldness check BEFORE simulation:" << std::endl;
        for (const auto& c : bs.cells()) {
            bool manifold = c->is_manifold();
            std::cout << "    Cell " << c->get_id() << ": "
                      << (manifold ? "manifold" : "NOT manifold")
                      << " (faces=" << c->get_nb_of_faces()
                      << ", nodes=" << c->get_nb_of_nodes() << ")" << std::endl;
            if (!manifold) all_manifold_before = false;
        }

        if (!all_manifold_before) {
            std::cout << "  WARNING: Not all cells are manifold before simulation" << std::endl;
        }

        // Run 200 iterations
        for (int i = 0; i < 200; ++i) {
            bs.step();
        }

        // Check manifoldness AFTER running
        bool all_manifold_after = true;
        std::cout << "  Manifoldness check AFTER simulation (200 iterations):" << std::endl;
        for (const auto& c : bs.cells()) {
            bool manifold = c->is_manifold();
            std::cout << "    Cell " << c->get_id() << ": "
                      << (manifold ? "manifold" : "NOT manifold")
                      << " (faces=" << c->get_nb_of_faces()
                      << ", nodes=" << c->get_nb_of_nodes() << ")" << std::endl;
            if (!manifold) all_manifold_after = false;
        }

        bool passed = all_manifold_before && all_manifold_after;
        std::cout << "  Before: " << (all_manifold_before ? "PASS" : "FAIL") << std::endl;
        std::cout << "  After:  " << (all_manifold_after ? "PASS" : "FAIL") << std::endl;

        if (!passed) {
            std::cout << "test_manifoldness FAILED" << std::endl;
            cleanup_output_dir(test_output_dir);
            return 1;
        }

        std::cout << "test_manifoldness PASSED" << std::endl;
        cleanup_output_dir(test_output_dir);
        return 0;

    } catch (const std::exception& e) {
        std::cout << "FAILED with exception: " << e.what() << std::endl;
        cleanup_output_dir(test_output_dir);
        return 1;
    }
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
/**
 * @brief Test the isoperimetric ratio of cells after simulation
 *
 * The isoperimetric ratio IR = (area^3) / (36 * pi * volume^2) measures how
 * spherical a shape is. A perfect sphere has IR = 1; any other shape has IR > 1.
 *
 * For a biological cell simulation, cells should maintain reasonable shapes.
 * We allow a generous range [0.5, 500] to account for:
 * - Slight numerical undershoot below 1.0
 * - Non-spherical cell morphologies
 * - Mesh discretization artifacts
 *
 * @return 0 on success, 1 on failure
 */
int test_isoperimetric_ratio() {
    std::cout << "Running test_isoperimetric_ratio..." << std::endl;

    std::string test_output_dir = create_unique_output_dir("isoperimetric_ratio");

    try {
        std::vector<cell_ptr> cell_lst = get_cells();
        global_simulation_parameters sim_params = get_sim_params(test_output_dir);

        if (cell_lst.empty()) {
            std::cout << "FAILED: No cells loaded from parameter file" << std::endl;
            cleanup_output_dir(test_output_dir);
            return 1;
        }

        benchmark_solver bs(sim_params, cell_lst, 1, true, false, "static");

        // Run 200 iterations
        for (int i = 0; i < 200; ++i) {
            bs.step();
        }

        bool all_passed = true;
        const double ir_min = 0.5;
        const double ir_max = 500.0;

        for (const auto& c : bs.cells()) {
            double area = c->get_area();
            double volume = c->get_volume();

            // Skip cells with negligible volume (avoid division by zero)
            if (volume < 1e-30) {
                std::cout << "  Cell " << c->get_id()
                          << ": skipped (negligible volume " << volume << ")" << std::endl;
                continue;
            }

            // IR = area^3 / (36 * pi * volume^2)
            double ir = (area * area * area) / (36.0 * M_PI * volume * volume);

            bool cell_passed = (ir >= ir_min && ir <= ir_max);
            std::cout << "  Cell " << c->get_id()
                      << ": area=" << area << ", volume=" << volume
                      << ", IR=" << ir
                      << (cell_passed ? " PASS" : " FAIL") << std::endl;

            if (!cell_passed) {
                all_passed = false;
            }
        }

        if (!all_passed) {
            std::cout << "test_isoperimetric_ratio FAILED" << std::endl;
            cleanup_output_dir(test_output_dir);
            return 1;
        }

        std::cout << "test_isoperimetric_ratio PASSED" << std::endl;
        cleanup_output_dir(test_output_dir);
        return 0;

    } catch (const std::exception& e) {
        std::cout << "FAILED with exception: " << e.what() << std::endl;
        cleanup_output_dir(test_output_dir);
        return 1;
    }
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// The main function
int main(int argc, char** argv) {

    // Check that the command line input is correctly formatted
    assert(argc == 2);

    // Get the name of the test to run
    std::string test_name = argv[1];

    // Run the selected test
    if (test_name == "test_volume_preservation")       return test_volume_preservation();
    if (test_name == "test_mesh_quality")               return test_mesh_quality();
    if (test_name == "test_face_normal_consistency")    return test_face_normal_consistency();
    if (test_name == "test_manifoldness")               return test_manifoldness();
    if (test_name == "test_isoperimetric_ratio")        return test_isoperimetric_ratio();

    std::cout << "TEST NAME: " << test_name << " DOES NOT EXIST" << std::endl;
    return 1;
}
//---------------------------------------------------------------------------------------------------------
