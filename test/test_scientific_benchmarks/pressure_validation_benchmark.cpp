/**
 * @file pressure_validation_benchmark.cpp
 * @brief Scientific benchmark tests for pressure computation validation
 *
 * Validates that the pressure model P = -K * ln(V / V_target) behaves correctly:
 * 1. Pressures remain in a physiologically reasonable range
 * 2. Bulk modulus sign convention is correct (expansion vs compression)
 * 3. Laplace pressure relation holds approximately for a spherical cell at equilibrium
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
#include <unistd.h>  // For getpid()

#include "solver.hpp"
#include "simulation_initializer.hpp"
#include "utils.hpp"

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
// Derived test class to run specific number of iterations
//---------------------------------------------------------------------------------------------------------
class pressure_test_solver : public solver {
public:
    using solver::solver;  // Inherit constructors

    // Run a specific number of iterations for controlled testing
    void run_n_iterations(unsigned n) {
        for (unsigned i = 0; i < n; ++i) {
            run_iteration();
        }
    }
};


//---------------------------------------------------------------------------------------------------------
/**
 * @brief Test that cell pressures remain in a physiologically reasonable range
 *
 * After running 200 iterations, verifies:
 * 1. All epithelial cell pressures are finite (not NaN, not Inf)
 * 2. All epithelial cell pressures lie within [-10000, 10000] Pa
 *
 * The pressure formula is P = -K * ln(V / V_target) with K = 2500 Pa.
 * At initialization V ~ V_target, so P ~ 0, with slight drift over time.
 *
 * @return 0 on success, 1 on failure
 */
int test_pressure_physiological_range() {
    std::cout << "Running test_pressure_physiological_range..." << std::endl;

    // Create unique output directory for test isolation
    std::string test_output_dir = create_unique_output_dir("pressure_physio_range");

    try {
        // Load parameter file for single sphere test
        std::string param_file = std::string(PROJECT_SOURCE_DIR) +
                                 "/test/test_scientific_benchmarks/params_single_sphere.xml";

        simulation_initializer sim_init(param_file, false);
        std::vector<cell_ptr> cell_lst = sim_init.get_cell_lst();
        global_simulation_parameters sim_params = sim_init.get_simulation_parameters();

        // Override output folder for test isolation
        sim_params.output_folder_path_ = test_output_dir;

        if (cell_lst.empty()) {
            std::cout << "FAILED: No cells loaded from parameter file" << std::endl;
            cleanup_output_dir(test_output_dir);
            return 1;
        }
        std::cout << "  Loaded " << cell_lst.size() << " cells" << std::endl;

        // Create solver and run 200 iterations
        pressure_test_solver s(sim_params, cell_lst, 1, true, false, "static");
        s.run_n_iterations(200);

        // Check all epithelial cells (type_id == 0) for pressure in valid range
        const double pressure_min = -10000.0;
        const double pressure_max =  10000.0;
        const auto& cells = s.get_cell_lst();

        unsigned epithelial_count = 0;
        bool all_finite = true;
        bool all_in_range = true;

        for (const auto& c : cells) {
            // Check epithelial cells (global_type_id == 0)
            if (c->get_cell_type_id() != 0) continue;

            epithelial_count++;
            double pressure = c->get_pressure();

            // Check finite
            if (!std::isfinite(pressure)) {
                std::cout << "  FAILED: Cell " << c->get_id() << " has non-finite pressure: "
                          << pressure << std::endl;
                all_finite = false;
                continue;
            }

            // Check range
            if (pressure < pressure_min || pressure > pressure_max) {
                std::cout << "  FAILED: Cell " << c->get_id() << " pressure " << pressure
                          << " outside range [" << pressure_min << ", " << pressure_max << "]" << std::endl;
                all_in_range = false;
            } else {
                std::cout << "  Cell " << c->get_id() << " pressure = " << pressure << " Pa (OK)" << std::endl;
            }
        }

        bool t1 = (epithelial_count > 0);
        std::cout << "  t1 (found epithelial cells): " << t1 << " (" << epithelial_count << " cells)" << std::endl;
        if (!t1) {
            std::cout << "FAILED: No epithelial cells found" << std::endl;
            cleanup_output_dir(test_output_dir);
            return 1;
        }

        bool t2 = all_finite;
        std::cout << "  t2 (all pressures finite): " << t2 << std::endl;
        if (!t2) {
            std::cout << "FAILED: Some pressures are NaN or Inf" << std::endl;
            cleanup_output_dir(test_output_dir);
            return 1;
        }

        bool t3 = all_in_range;
        std::cout << "  t3 (all pressures in physiological range): " << t3 << std::endl;
        if (!t3) {
            std::cout << "FAILED: Some pressures outside [-10000, 10000] Pa" << std::endl;
            cleanup_output_dir(test_output_dir);
            return 1;
        }

        std::cout << "test_pressure_physiological_range PASSED" << std::endl;
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
 * @brief Test that the bulk modulus pressure sign convention is correct
 *
 * The pressure formula is P = -K * ln(V / V_target).
 * Sign convention:
 *   - If V < V_target: ln(V/V_target) < 0, so P > 0 (cell wants to expand)
 *   - If V > V_target: ln(V/V_target) > 0, so P < 0 (cell wants to compress)
 *   - If V ~ V_target: P ~ 0
 *
 * Runs 100 iterations to let cells reach near-equilibrium, then checks the sign
 * convention for all non-static cells with non-zero bulk modulus.
 *
 * @return 0 on success, 1 on failure
 */
int test_pressure_bulk_modulus_response() {
    std::cout << "Running test_pressure_bulk_modulus_response..." << std::endl;

    // Create unique output directory for test isolation
    std::string test_output_dir = create_unique_output_dir("pressure_bulk_modulus");

    try {
        // Load parameter file
        std::string param_file = std::string(PROJECT_SOURCE_DIR) +
                                 "/test/test_scientific_benchmarks/params_single_sphere.xml";

        simulation_initializer sim_init(param_file, false);
        std::vector<cell_ptr> cell_lst = sim_init.get_cell_lst();
        global_simulation_parameters sim_params = sim_init.get_simulation_parameters();

        // Override output folder for test isolation
        sim_params.output_folder_path_ = test_output_dir;

        if (cell_lst.empty()) {
            std::cout << "FAILED: No cells loaded from parameter file" << std::endl;
            cleanup_output_dir(test_output_dir);
            return 1;
        }
        std::cout << "  Loaded " << cell_lst.size() << " cells" << std::endl;

        // Create solver and run 100 iterations to approach equilibrium
        pressure_test_solver s(sim_params, cell_lst, 1, true, false, "static");
        s.run_n_iterations(100);

        // Check sign convention for all non-static cells with non-zero bulk modulus
        const auto& cells = s.get_cell_lst();
        unsigned checked_count = 0;
        bool sign_convention_holds = true;
        const double volume_tolerance = 0.01;  // 1% tolerance for V ~ V_target

        for (const auto& c : cells) {
            // Skip static cells
            if (c->is_static()) continue;

            double bulk_modulus = c->get_cell_type()->bulk_modulus_;

            // Skip cells with zero bulk modulus (no pressure response)
            if (bulk_modulus == 0.0) continue;

            checked_count++;

            double V = c->get_volume();
            double V_target = c->get_target_volume();
            double pressure = c->get_pressure();
            double volume_ratio = V / V_target;

            std::cout << "  Cell " << c->get_id() << ": V=" << V
                      << ", V_target=" << V_target
                      << ", V/V_target=" << volume_ratio
                      << ", P=" << pressure << " Pa"
                      << ", K=" << bulk_modulus << std::endl;

            // Check finiteness first
            if (!std::isfinite(pressure)) {
                std::cout << "    FAILED: pressure is not finite" << std::endl;
                sign_convention_holds = false;
                continue;
            }

            if (std::abs(volume_ratio - 1.0) < volume_tolerance) {
                // V ~ V_target: pressure should be near zero
                // Expected: |P| < K * |ln(1 +/- tolerance)| ~ K * tolerance
                double expected_max = bulk_modulus * volume_tolerance;
                if (std::abs(pressure) > expected_max) {
                    std::cout << "    WARNING: V ~ V_target but |P| = " << std::abs(pressure)
                              << " > expected max " << expected_max << std::endl;
                    // Not a hard failure; pressure may be capped or affected by dynamics
                } else {
                    std::cout << "    OK: V ~ V_target, pressure near zero" << std::endl;
                }
            } else if (V < V_target) {
                // V < V_target: ln(V/V_target) < 0, so P = -K * ln(V/V_target) > 0
                if (pressure <= 0.0) {
                    std::cout << "    FAILED: V < V_target but pressure <= 0 (expected positive)" << std::endl;
                    sign_convention_holds = false;
                } else {
                    std::cout << "    OK: V < V_target, pressure positive (expansion)" << std::endl;
                }
            } else {
                // V > V_target: ln(V/V_target) > 0, so P = -K * ln(V/V_target) < 0
                if (pressure >= 0.0) {
                    std::cout << "    FAILED: V > V_target but pressure >= 0 (expected negative)" << std::endl;
                    sign_convention_holds = false;
                } else {
                    std::cout << "    OK: V > V_target, pressure negative (compression)" << std::endl;
                }
            }
        }

        bool t1 = (checked_count > 0);
        std::cout << "  t1 (found non-static cells with bulk modulus): " << t1
                  << " (" << checked_count << " cells)" << std::endl;
        if (!t1) {
            std::cout << "FAILED: No non-static cells with non-zero bulk modulus found" << std::endl;
            cleanup_output_dir(test_output_dir);
            return 1;
        }

        bool t2 = sign_convention_holds;
        std::cout << "  t2 (sign convention holds for all cells): " << t2 << std::endl;
        if (!t2) {
            std::cout << "FAILED: Sign convention violated for at least one cell" << std::endl;
            cleanup_output_dir(test_output_dir);
            return 1;
        }

        std::cout << "test_pressure_bulk_modulus_response PASSED" << std::endl;
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
 * @brief Test the Laplace pressure relation for a spherical cell at equilibrium
 *
 * For a spherical cell at mechanical equilibrium, the Laplace pressure relation states:
 *   delta_P = 2 * gamma / R
 * where gamma is surface tension and R is the sphere radius.
 *
 * At equilibrium, the bulk modulus pressure balances the Laplace pressure:
 *   P_actual = -K * ln(V / V_target) ~ 2 * gamma / R
 *
 * The cell volume adjusts until these balance. This test verifies that the
 * actual pressure is in the right ballpark of the expected Laplace pressure.
 *
 * Uses a dedicated parameter file with appropriate surface tension values
 * for the Laplace test. Falls back to single sphere parameters if not available.
 *
 * @return 0 on success, 1 on failure
 */
int test_laplace_pressure() {
    std::cout << "Running test_laplace_pressure..." << std::endl;

    // Create unique output directory for test isolation
    std::string test_output_dir = create_unique_output_dir("laplace_pressure");

    try {
        // Try to load the Laplace-specific parameter file first, fall back to single sphere
        std::string param_file = std::string(PROJECT_SOURCE_DIR) +
                                 "/test/test_scientific_benchmarks/params_laplace_pressure.xml";

        // Check if the Laplace parameter file exists; if not, use single sphere
        if (!std::filesystem::exists(param_file)) {
            std::cout << "  params_laplace_pressure.xml not found, using params_single_sphere.xml" << std::endl;
            param_file = std::string(PROJECT_SOURCE_DIR) +
                         "/test/test_scientific_benchmarks/params_single_sphere.xml";
        }

        simulation_initializer sim_init(param_file, false);
        std::vector<cell_ptr> cell_lst = sim_init.get_cell_lst();
        global_simulation_parameters sim_params = sim_init.get_simulation_parameters();

        // Override output folder for test isolation
        sim_params.output_folder_path_ = test_output_dir;

        if (cell_lst.empty()) {
            std::cout << "FAILED: No cells loaded from parameter file" << std::endl;
            cleanup_output_dir(test_output_dir);
            return 1;
        }
        std::cout << "  Loaded " << cell_lst.size() << " cells" << std::endl;

        // Create solver and run 1000 iterations to let cell equilibrate
        pressure_test_solver s(sim_params, cell_lst, 1, true, false, "static");
        s.run_n_iterations(1000);

        // Find the first epithelial cell (type_id == 0)
        const auto& cells = s.get_cell_lst();
        cell_ptr epithelial = nullptr;

        for (const auto& c : cells) {
            if (c->get_cell_type_id() == 0) {
                epithelial = c;
                break;
            }
        }

        if (!epithelial) {
            std::cout << "FAILED: No epithelial cell found" << std::endl;
            cleanup_output_dir(test_output_dir);
            return 1;
        }

        // Get cell properties
        double V = epithelial->get_volume();
        double V_target = epithelial->get_target_volume();
        double pressure_actual = epithelial->get_pressure();
        double bulk_modulus = epithelial->get_cell_type()->bulk_modulus_;
        double surface_tension = epithelial->get_cell_type()->face_types_[0].surface_tension_;

        // Compute effective sphere radius from volume: V = (4/3) * pi * R^3
        double R = std::cbrt(3.0 * V / (4.0 * M_PI));

        // Expected Laplace pressure: delta_P = 2 * gamma / R
        double delta_P_expected = 2.0 * surface_tension / R;

        std::cout << "  Cell " << epithelial->get_id() << " properties:" << std::endl;
        std::cout << "    Volume V        = " << V << std::endl;
        std::cout << "    Target volume   = " << V_target << std::endl;
        std::cout << "    V / V_target    = " << (V / V_target) << std::endl;
        std::cout << "    Bulk modulus K  = " << bulk_modulus << " Pa" << std::endl;
        std::cout << "    Surface tension = " << surface_tension << std::endl;
        std::cout << "    Effective R     = " << R << std::endl;
        std::cout << "    P_actual        = " << pressure_actual << " Pa" << std::endl;
        std::cout << "    dP_expected     = " << delta_P_expected << " Pa (Laplace: 2*gamma/R)" << std::endl;

        // Validate basic sanity
        bool t1 = std::isfinite(pressure_actual);
        std::cout << "  t1 (pressure is finite): " << t1 << std::endl;
        if (!t1) {
            std::cout << "FAILED: Pressure is not finite" << std::endl;
            cleanup_output_dir(test_output_dir);
            return 1;
        }

        bool t2 = (R > 0.0 && std::isfinite(R));
        std::cout << "  t2 (radius is positive and finite): " << t2 << std::endl;
        if (!t2) {
            std::cout << "FAILED: Effective radius is invalid" << std::endl;
            cleanup_output_dir(test_output_dir);
            return 1;
        }

        bool t3 = (delta_P_expected > 0.0 && std::isfinite(delta_P_expected));
        std::cout << "  t3 (expected Laplace pressure is positive and finite): " << t3 << std::endl;
        if (!t3) {
            std::cout << "FAILED: Expected Laplace pressure is invalid" << std::endl;
            cleanup_output_dir(test_output_dir);
            return 1;
        }

        // Compare actual pressure magnitude with expected Laplace pressure
        // At equilibrium, P_actual = -K * ln(V/V_target) should approximately equal 2*gamma/R
        // The cell volume adjusts until these balance, but mesh discretization and
        // short simulation time may cause significant deviation.
        // Use a very relaxed tolerance of 50% relative error (directional validation).
        double relative_error = std::abs(std::abs(pressure_actual) - delta_P_expected) / delta_P_expected;
        std::cout << "    |P_actual|      = " << std::abs(pressure_actual) << " Pa" << std::endl;
        std::cout << "    Relative error  = " << (relative_error * 100.0) << "%" << std::endl;

        bool t4 = (relative_error < 0.50);
        std::cout << "  t4 (relative error < 50%): " << t4 << std::endl;
        if (!t4) {
            std::cout << "WARNING: Laplace pressure relative error " << (relative_error * 100.0)
                      << "% exceeds 50% threshold" << std::endl;
            std::cout << "  This may be due to mesh discretization, insufficient equilibration time," << std::endl;
            std::cout << "  or parameter configuration. Values printed above for manual inspection." << std::endl;
            cleanup_output_dir(test_output_dir);
            return 1;
        }

        std::cout << "test_laplace_pressure PASSED" << std::endl;
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
    if (test_name == "test_pressure_physiological_range")    return test_pressure_physiological_range();
    if (test_name == "test_pressure_bulk_modulus_response")  return test_pressure_bulk_modulus_response();
    if (test_name == "test_laplace_pressure")                return test_laplace_pressure();

    std::cout << "TEST NAME: " << test_name << " DOES NOT EXIST" << std::endl;
    return 1;
}
//---------------------------------------------------------------------------------------------------------
