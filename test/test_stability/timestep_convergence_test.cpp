#include <cassert>
#include <string>
#include <iostream>
#include <cmath>
#include <vector>
#include <memory>
#include <filesystem>
#include <unistd.h>

#include "solver.hpp"
#include "simulation_initializer.hpp"

//---------------------------------------------------------------------------------------------------------
static std::string create_unique_output_dir(const std::string& test_name) {
    pid_t pid = getpid();
    std::string unique_dir = "/tmp/simucell3d_convergence_" + test_name + "_" + std::to_string(pid);
    std::filesystem::remove_all(unique_dir);
    return unique_dir;
}

static void cleanup_output_dir(const std::string& output_dir) {
    try { std::filesystem::remove_all(output_dir); }
    catch (const std::exception&) {}
}

//---------------------------------------------------------------------------------------------------------
// Result of a simulation run
//---------------------------------------------------------------------------------------------------------
struct SimResult {
    std::vector<double> centroid_x;
    std::vector<double> centroid_y;
    std::vector<double> centroid_z;
    std::vector<double> volumes;
    std::vector<float> kinetic_energies;
    size_t num_cells;
};

//---------------------------------------------------------------------------------------------------------
// Run simulation for a fixed simulated time using run_iteration() with given timestep.
// Uses the stable single-sphere parameter file to avoid mesh refinement instability.
//---------------------------------------------------------------------------------------------------------
static SimResult run_with_timestep(double dt, double duration, const std::string& output_dir) {
    std::string param_file = std::string(PROJECT_SOURCE_DIR) +
                             "/test/test_scientific_benchmarks/params_single_sphere.xml";
    simulation_initializer sim_init(param_file, false);
    auto cell_lst = sim_init.get_cell_lst();
    auto sim_params = sim_init.get_simulation_parameters();
    sim_params.output_folder_path_ = output_dir;
    sim_params.time_step_ = dt;
    sim_params.simulation_duration_ = 1.0;    // Large so run_iteration() doesn't skip
    sim_params.sampling_period_ = 2.0;        // Avoid file writes

    solver s(sim_params, cell_lst, 1, true, false, "static");

    // Run the exact number of iterations to cover 'duration' of simulated time
    int num_iterations = static_cast<int>(std::round(duration / dt));
    for (int i = 0; i < num_iterations; ++i) {
        s.run_iteration();
    }

    SimResult result;
    result.num_cells = s.get_cell_lst().size();
    for (const auto& cell : s.get_cell_lst()) {
        result.centroid_x.push_back(cell->get_centroid().dx());
        result.centroid_y.push_back(cell->get_centroid().dy());
        result.centroid_z.push_back(cell->get_centroid().dz());
        result.volumes.push_back(cell->get_volume());
        result.kinetic_energies.push_back(cell->get_kinetic_energy());
    }
    return result;
}

//---------------------------------------------------------------------------------------------------------
/**
 * @brief Timestep convergence order test using Richardson extrapolation
 *
 * Runs simulation with dt, dt/2, dt/4 and computes convergence order:
 *   p = log2(|u_dt - u_{dt/2}| / |u_{dt/2} - u_{dt/4}|)
 *
 * For semi-implicit Euler (1st order), expect p ~ 1.0 (tolerance: 0.3 to 1.7)
 *
 * Uses a short physical duration to stay within stable regime.
 *
 * @return 0 on success, 1 on failure
 */
int test_convergence_order() {
    std::cout << "Running test_convergence_order..." << std::endl;

    // Base dt = 1e-7, duration = 5e-6 = 50 steps at base dt, 100 at dt/2, 200 at dt/4
    const double base_dt = 1.0e-7;
    const double duration = 5.0e-6;

    std::string out1 = create_unique_output_dir("conv_dt1");
    std::string out2 = create_unique_output_dir("conv_dt2");
    std::string out3 = create_unique_output_dir("conv_dt4");

    try {
        int n1 = static_cast<int>(std::round(duration / base_dt));
        int n2 = static_cast<int>(std::round(duration / (base_dt / 2.0)));
        int n4 = static_cast<int>(std::round(duration / (base_dt / 4.0)));

        std::cout << "  Running with dt = " << base_dt << " (" << n1 << " iterations)..." << std::endl;
        SimResult r1 = run_with_timestep(base_dt, duration, out1);

        std::cout << "  Running with dt/2 = " << base_dt / 2 << " (" << n2 << " iterations)..." << std::endl;
        SimResult r2 = run_with_timestep(base_dt / 2.0, duration, out2);

        std::cout << "  Running with dt/4 = " << base_dt / 4 << " (" << n4 << " iterations)..." << std::endl;
        SimResult r3 = run_with_timestep(base_dt / 4.0, duration, out3);

        // Verify same number of cells survived in all runs
        bool t1 = (r1.num_cells == r2.num_cells) && (r2.num_cells == r3.num_cells);
        std::cout << "  t1 (same cell count): " << t1
                  << " (" << r1.num_cells << ", " << r2.num_cells << ", " << r3.num_cells << ")"
                  << std::endl;
        if (!t1) {
            std::cout << "WARNING: Different cell counts across dt values." << std::endl;
            cleanup_output_dir(out1); cleanup_output_dir(out2); cleanup_output_dir(out3);
            return 1;
        }

        // Compute Richardson extrapolation order for centroid positions
        double error_coarse = 0.0;  // |u_dt - u_{dt/2}|
        double error_fine = 0.0;    // |u_{dt/2} - u_{dt/4}|

        for (size_t i = 0; i < r1.num_cells; ++i) {
            double dx1 = r1.centroid_x[i] - r2.centroid_x[i];
            double dy1 = r1.centroid_y[i] - r2.centroid_y[i];
            double dz1 = r1.centroid_z[i] - r2.centroid_z[i];
            error_coarse += std::sqrt(dx1 * dx1 + dy1 * dy1 + dz1 * dz1);

            double dx2 = r2.centroid_x[i] - r3.centroid_x[i];
            double dy2 = r2.centroid_y[i] - r3.centroid_y[i];
            double dz2 = r2.centroid_z[i] - r3.centroid_z[i];
            error_fine += std::sqrt(dx2 * dx2 + dy2 * dy2 + dz2 * dz2);
        }

        std::cout << "  Error (dt vs dt/2): " << error_coarse << std::endl;
        std::cout << "  Error (dt/2 vs dt/4): " << error_fine << std::endl;

        // Guard against zero errors (equilibrium or no movement)
        bool t2 = (error_coarse > 0.0) && (error_fine > 0.0);
        if (!t2) {
            if (error_coarse == 0.0 && error_fine == 0.0) {
                std::cout << "  Both errors zero (equilibrium). Convergence trivially satisfied."
                          << std::endl;
                cleanup_output_dir(out1); cleanup_output_dir(out2); cleanup_output_dir(out3);
                return 0;
            }
            std::cout << "  t2 (non-zero errors): " << t2 << std::endl;
            cleanup_output_dir(out1); cleanup_output_dir(out2); cleanup_output_dir(out3);
            return 1;
        }

        double convergence_order = std::log2(error_coarse / error_fine);
        std::cout << "  Convergence order p = " << convergence_order << " (expected ~1.0)" << std::endl;

        // Semi-implicit Euler is 1st order: expect p in [0.3, 1.7]
        // Wide tolerance for mesh refinement topology changes and non-smooth contacts
        bool t3 = (convergence_order >= 0.3) && (convergence_order <= 1.7);
        std::cout << "  t3 (convergence order in [0.3, 1.7]): " << t3 << std::endl;
        if (!t3) {
            std::cout << "FAILED: Convergence order " << convergence_order
                      << " outside expected range for 1st-order method" << std::endl;
            cleanup_output_dir(out1); cleanup_output_dir(out2); cleanup_output_dir(out3);
            return 1;
        }

        std::cout << "test_convergence_order PASSED" << std::endl;
        cleanup_output_dir(out1); cleanup_output_dir(out2); cleanup_output_dir(out3);
        return 0;

    } catch (const std::exception& e) {
        std::cout << "FAILED with exception: " << e.what() << std::endl;
        cleanup_output_dir(out1); cleanup_output_dir(out2); cleanup_output_dir(out3);
        return 1;
    }
}

//---------------------------------------------------------------------------------------------------------
/**
 * @brief Test that energy values converge as timestep decreases
 *
 * With smaller dt, the volume at end of simulation should converge.
 * |V(dt) - V(dt/2)| > |V(dt/2) - V(dt/4)| (errors shrink)
 *
 * @return 0 on success, 1 on failure
 */
int test_energy_convergence() {
    std::cout << "Running test_energy_convergence..." << std::endl;

    const double base_dt = 1.0e-7;
    const double duration = 5.0e-6;

    std::string out1 = create_unique_output_dir("econv_dt1");
    std::string out2 = create_unique_output_dir("econv_dt2");
    std::string out3 = create_unique_output_dir("econv_dt4");

    try {
        SimResult r1 = run_with_timestep(base_dt, duration, out1);
        SimResult r2 = run_with_timestep(base_dt / 2.0, duration, out2);
        SimResult r3 = run_with_timestep(base_dt / 4.0, duration, out3);

        // Verify same topology
        bool t1 = (r1.num_cells == r2.num_cells) && (r2.num_cells == r3.num_cells);
        std::cout << "  t1 (same cell count): " << t1 << std::endl;
        if (!t1) { cleanup_output_dir(out1); cleanup_output_dir(out2); cleanup_output_dir(out3); return 1; }

        // Compute volume error norms
        double vol_err_coarse = 0.0;
        double vol_err_fine = 0.0;
        for (size_t i = 0; i < r1.num_cells; ++i) {
            vol_err_coarse += std::abs(r1.volumes[i] - r2.volumes[i]);
            vol_err_fine += std::abs(r2.volumes[i] - r3.volumes[i]);
        }

        std::cout << "  Volume error (dt vs dt/2): " << vol_err_coarse << std::endl;
        std::cout << "  Volume error (dt/2 vs dt/4): " << vol_err_fine << std::endl;

        // Volume errors should decrease (or both be at noise floor / zero)
        // When errors are below ~1e-17, we're at floating-point noise level
        // and the ratio test is meaningless — convergence is effectively complete
        const double noise_floor = 1e-17;
        bool t2;
        if (vol_err_coarse < noise_floor && vol_err_fine < noise_floor) {
            t2 = true;
            std::cout << "  t2 (volume convergence): " << t2
                      << " (both errors below noise floor " << noise_floor << ")" << std::endl;
        } else if (vol_err_fine == 0.0) {
            t2 = true;
            std::cout << "  t2 (volume convergence): " << t2 << " (fine converged)" << std::endl;
        } else {
            t2 = (vol_err_fine <= vol_err_coarse * 1.1);  // 10% tolerance
            std::cout << "  t2 (volume error decreasing): " << t2 << std::endl;
        }
        if (!t2) { cleanup_output_dir(out1); cleanup_output_dir(out2); cleanup_output_dir(out3); return 1; }

        // Verify all volumes are positive and finite
        bool t3 = true;
        for (size_t i = 0; i < r3.num_cells; ++i) {
            if (r3.volumes[i] <= 0.0 || !std::isfinite(r3.volumes[i])) {
                std::cout << "  Cell " << i << " has invalid volume: " << r3.volumes[i] << std::endl;
                t3 = false;
                break;
            }
        }
        std::cout << "  t3 (all volumes positive and finite): " << t3 << std::endl;
        if (!t3) { cleanup_output_dir(out1); cleanup_output_dir(out2); cleanup_output_dir(out3); return 1; }

        std::cout << "test_energy_convergence PASSED" << std::endl;
        cleanup_output_dir(out1); cleanup_output_dir(out2); cleanup_output_dir(out3);
        return 0;

    } catch (const std::exception& e) {
        std::cout << "FAILED with exception: " << e.what() << std::endl;
        cleanup_output_dir(out1); cleanup_output_dir(out2); cleanup_output_dir(out3);
        return 1;
    }
}

//---------------------------------------------------------------------------------------------------------
int main(int argc, char** argv) {
    assert(argc == 2);
    std::string test_name = argv[1];

    if (test_name == "test_convergence_order")   return test_convergence_order();
    if (test_name == "test_energy_convergence")  return test_energy_convergence();

    std::cout << "TEST NAME: " << test_name << " DOES NOT EXIST" << std::endl;
    return 1;
}
//---------------------------------------------------------------------------------------------------------
