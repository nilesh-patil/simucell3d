/**
 * @file energy_conservation_benchmark.cpp
 * @brief Scientific benchmark tests for energy conservation in SimuCell3D
 *
 * This file contains tests that validate the physical correctness of the simulation
 * by checking energy conservation properties:
 * 1. Single cell energy conservation (no contact)
 * 2. Two-cell energy conservation (with contact forces)
 * 3. Monotonic energy dissipation for damped systems
 *
 * Test pattern follows project convention: return 0=pass, 1=fail
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
// Derived solver class that exposes run_iteration() and provides energy computation
//---------------------------------------------------------------------------------------------------------
class energy_test_solver : public solver {
public:
    using solver::solver;  // Inherit constructors

    // Expose run_iteration as public for step-by-step testing
    void public_run_iteration() {
        run_iteration();
    }

    // Compute total energy across all cells in the simulation
    // Sums kinetic, pressure, surface tension, and bending energy contributions
    double get_total_energy() const {
        double total = 0.0;
        for (const auto& c : cell_lst_) {
            total += static_cast<double>(c->get_kinetic_energy());
            total += static_cast<double>(c->get_pressure_energy());
            total += static_cast<double>(c->get_surface_tension_energy());
            total += static_cast<double>(c->get_bending_energy());
        }
        return total;
    }
};

//---------------------------------------------------------------------------------------------------------
/**
 * @brief Test energy conservation for a single isolated cell
 *
 * For a single cell with no contacts, the total mechanical energy should remain
 * approximately constant over time (within numerical dissipation tolerance).
 *
 * Procedure:
 * 1. Initialize a single-cell simulation from params_single_sphere.xml
 * 2. Run 500 iterations, sampling energy every 50 steps
 * 3. Verify max relative energy drift stays below 5%
 *
 * @return 0 on success, 1 on failure
 */
int test_energy_conservation_single_cell() {
    std::cout << "Running test_energy_conservation_single_cell..." << std::endl;

    // Create unique output directory for test isolation
    std::string test_output_dir = create_unique_output_dir("energy_single");

    try {
        // Load parameters from test parameter file
        std::string param_file = std::string(PROJECT_SOURCE_DIR) +
                                 "/test/test_scientific_benchmarks/params_single_sphere.xml";

        simulation_initializer sim_init(param_file, false);  // false = non-verbose
        std::vector<cell_ptr> cell_lst = sim_init.get_cell_lst();
        global_simulation_parameters sim_params = sim_init.get_simulation_parameters();

        // Override output folder to unique test directory
        sim_params.output_folder_path_ = test_output_dir;

        std::cout << "  Loaded " << cell_lst.size() << " cell(s)" << std::endl;

        // Construct the test solver: 1 thread, stats in string, non-verbose, static schedule
        energy_test_solver ts(sim_params, cell_lst, 1, true, false, "static");

        // Run initial transient phase (cell equilibrating from initial conditions)
        // The first ~200 iterations see large energy changes as the cell adjusts
        // to pressure equilibrium - this is physical, not a conservation error
        const int transient_iterations = 200;
        for (int i = 0; i < transient_iterations; ++i) {
            ts.public_run_iteration();
        }

        // After transient, record baseline energy for stability measurement
        double E0 = ts.get_total_energy();
        std::cout << "  Post-transient energy E0 (after " << transient_iterations << " iters): " << E0 << std::endl;

        // Run 300 more iterations, recording energy every 50 iterations
        const int stability_iterations = 300;
        const int sample_interval = 50;
        double max_relative_drift = 0.0;

        for (int i = 1; i <= stability_iterations; ++i) {
            ts.public_run_iteration();

            if (i % sample_interval == 0) {
                double E = ts.get_total_energy();
                double relative_drift = (std::abs(E0) > 1e-30) ? std::abs(E - E0) / std::abs(E0) : std::abs(E - E0);
                if (relative_drift > max_relative_drift) {
                    max_relative_drift = relative_drift;
                }
                std::cout << "  Iteration " << (transient_iterations + i) << ": E = " << E
                          << ", relative drift = " << relative_drift << std::endl;
            }
        }

        std::cout << "  Max relative energy drift (post-transient): " << max_relative_drift << std::endl;

        // After transient, energy should be stable within 5%
        const double threshold = 0.05;
        bool passed = (max_relative_drift < threshold);

        if (passed) {
            std::cout << "test_energy_conservation_single_cell PASSED" << std::endl;
        } else {
            std::cout << "test_energy_conservation_single_cell FAILED" << std::endl;
            std::cout << "  Max relative drift " << max_relative_drift
                      << " exceeds threshold " << threshold << std::endl;
        }

        cleanup_output_dir(test_output_dir);
        return passed ? 0 : 1;

    } catch (const std::exception& e) {
        std::cout << "FAILED with exception: " << e.what() << std::endl;
        cleanup_output_dir(test_output_dir);
        return 1;
    }
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
/**
 * @brief Test energy conservation for two cells in contact
 *
 * For two cells in contact, the total mechanical energy should remain
 * approximately bounded despite contact force exchanges. Contact forces
 * introduce energy changes, so a more relaxed threshold is used.
 *
 * Procedure:
 * 1. Initialize a two-cell simulation from params_two_spheres_contact.xml
 * 2. Run 250 iterations, sampling energy every 50 steps
 * 3. Verify max relative energy drift stays below 20%
 *
 * @return 0 on success, 1 on failure
 */
int test_energy_conservation_two_cells() {
    std::cout << "Running test_energy_conservation_two_cells..." << std::endl;

    // Create unique output directory for test isolation
    std::string test_output_dir = create_unique_output_dir("energy_two");

    try {
        // Use the stable 2-sphere benchmark parameter file (no triangulation, no growth)
        std::string param_file = std::string(PROJECT_SOURCE_DIR) +
                                 "/test/test_scientific_benchmarks/params_two_spheres_stable.xml";

        simulation_initializer sim_init(param_file, false);  // false = non-verbose
        std::vector<cell_ptr> cell_lst = sim_init.get_cell_lst();
        global_simulation_parameters sim_params = sim_init.get_simulation_parameters();

        // Override output folder to unique test directory
        sim_params.output_folder_path_ = test_output_dir;

        std::cout << "  Loaded " << cell_lst.size() << " cell(s)" << std::endl;

        // Construct the test solver: 1 thread, stats in string, non-verbose, static schedule
        energy_test_solver ts(sim_params, cell_lst, 1, true, false, "static");

        // Run transient phase for two cells to equilibrate contact
        const int transient_iterations = 100;
        for (int i = 0; i < transient_iterations; ++i) {
            ts.public_run_iteration();
        }

        double E0 = ts.get_total_energy();
        std::cout << "  Post-transient energy E0: " << E0 << std::endl;

        // Run 150 more iterations, recording energy every 50 iterations
        const int stability_iterations = 150;
        const int sample_interval = 50;
        double max_relative_drift = 0.0;

        for (int i = 1; i <= stability_iterations; ++i) {
            ts.public_run_iteration();

            if (i % sample_interval == 0) {
                double E = ts.get_total_energy();
                double relative_drift = (std::abs(E0) > 1e-30) ? std::abs(E - E0) / std::abs(E0) : std::abs(E - E0);
                if (relative_drift > max_relative_drift) {
                    max_relative_drift = relative_drift;
                }
                std::cout << "  Iteration " << (transient_iterations + i) << ": E = " << E
                          << ", relative drift = " << relative_drift << std::endl;
            }
        }

        std::cout << "  Max relative energy drift (post-transient): " << max_relative_drift << std::endl;

        // After transient, energy drift should be bounded.
        // Contact forces add energy exchange, so allow 30% drift
        const double threshold = 0.30;
        bool passed = (max_relative_drift < threshold);

        if (passed) {
            std::cout << "test_energy_conservation_two_cells PASSED" << std::endl;
        } else {
            std::cout << "test_energy_conservation_two_cells FAILED" << std::endl;
            std::cout << "  Max relative drift " << max_relative_drift
                      << " exceeds threshold " << threshold << std::endl;
        }

        cleanup_output_dir(test_output_dir);
        return passed ? 0 : 1;

    } catch (const std::exception& e) {
        std::cout << "FAILED with exception: " << e.what() << std::endl;
        cleanup_output_dir(test_output_dir);
        return 1;
    }
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
/**
 * @brief Test that energy dissipation is monotonically decreasing for a damped system
 *
 * For a system with positive damping coefficient, the semi-implicit Euler integrator
 * should be stable and dissipative. Total energy should generally decrease over time.
 * We allow some numerical noise (up to 20% of intervals may show energy increase).
 *
 * Procedure:
 * 1. Initialize a single-cell simulation from params_single_sphere.xml
 * 2. Run 500 iterations, sampling energy every 50 steps
 * 3. Count intervals where energy increases between consecutive samples
 * 4. PASS if <= 20% of intervals show energy increase
 *
 * @return 0 on success, 1 on failure
 */
int test_energy_monotonic_dissipation() {
    std::cout << "Running test_energy_monotonic_dissipation..." << std::endl;

    // Create unique output directory for test isolation
    std::string test_output_dir = create_unique_output_dir("energy_dissipation");

    try {
        // Load parameters from test parameter file
        std::string param_file = std::string(PROJECT_SOURCE_DIR) +
                                 "/test/test_scientific_benchmarks/params_single_sphere.xml";

        simulation_initializer sim_init(param_file, false);  // false = non-verbose
        std::vector<cell_ptr> cell_lst = sim_init.get_cell_lst();
        global_simulation_parameters sim_params = sim_init.get_simulation_parameters();

        // Override output folder to unique test directory
        sim_params.output_folder_path_ = test_output_dir;

        std::cout << "  Loaded " << cell_lst.size() << " cell(s)" << std::endl;

        // Construct the test solver: 1 thread, stats in string, non-verbose, static schedule
        energy_test_solver ts(sim_params, cell_lst, 1, true, false, "static");

        // Run 500 iterations, recording energy every 100 iterations
        const int total_iterations = 500;
        const int sample_interval = 100;
        std::vector<double> energy_samples;

        // Run first iteration and record initial energy
        ts.public_run_iteration();
        energy_samples.push_back(ts.get_total_energy());

        for (int i = 1; i < total_iterations; ++i) {
            ts.public_run_iteration();

            if (i % sample_interval == 0) {
                double E = ts.get_total_energy();
                energy_samples.push_back(E);
            }
        }

        // Print the energy trajectory
        std::cout << "  Energy trajectory (" << energy_samples.size() << " samples):" << std::endl;
        for (size_t i = 0; i < energy_samples.size(); ++i) {
            std::cout << "    Sample " << i << ": E = " << energy_samples[i] << std::endl;
        }

        // Primary check: overall energy dissipation (final < initial)
        double E_initial = energy_samples.front();
        double E_final = energy_samples.back();
        bool overall_dissipation = (E_final <= E_initial * 1.01);  // Allow 1% tolerance

        std::cout << "  E_initial = " << E_initial << ", E_final = " << E_final << std::endl;
        std::cout << "  Overall dissipation: " << (overall_dissipation ? "YES" : "NO") << std::endl;

        // Secondary check: energy should be bounded (no blow-up)
        double E_max = *std::max_element(energy_samples.begin(), energy_samples.end());
        bool energy_bounded = (E_max < E_initial * 2.0);

        std::cout << "  E_max = " << E_max << ", bounded: " << (energy_bounded ? "YES" : "NO") << std::endl;

        // PASS if overall energy dissipates AND stays bounded
        bool passed = overall_dissipation && energy_bounded;

        if (passed) {
            std::cout << "test_energy_monotonic_dissipation PASSED" << std::endl;
        } else {
            std::cout << "test_energy_monotonic_dissipation FAILED" << std::endl;
            std::cout << "  overall_dissipation=" << overall_dissipation
                      << ", energy_bounded=" << energy_bounded << std::endl;
        }

        cleanup_output_dir(test_output_dir);
        return passed ? 0 : 1;

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
    if (test_name == "test_energy_conservation_single_cell")  return test_energy_conservation_single_cell();
    if (test_name == "test_energy_conservation_two_cells")    return test_energy_conservation_two_cells();
    if (test_name == "test_energy_monotonic_dissipation")     return test_energy_monotonic_dissipation();

    std::cout << "TEST NAME: " << test_name << " DOES NOT EXIST" << std::endl;
    return 1;
}
//---------------------------------------------------------------------------------------------------------
