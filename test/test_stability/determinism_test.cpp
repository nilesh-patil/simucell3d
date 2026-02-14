#include <cassert>
#include <string>
#include <iostream>
#include <cmath>
#include <cstring>
#include <vector>
#include <memory>
#include <filesystem>
#include <unistd.h>

#include "solver.hpp"
#include "simulation_initializer.hpp"

// Number of iterations for determinism tests
static constexpr int NUM_ITERATIONS = 200;

//---------------------------------------------------------------------------------------------------------
// Test isolation helpers
//---------------------------------------------------------------------------------------------------------
static std::string create_unique_output_dir(const std::string& test_name) {
    pid_t pid = getpid();
    std::string unique_dir = "/tmp/simucell3d_stability_" + test_name + "_" + std::to_string(pid);
    std::filesystem::remove_all(unique_dir);
    return unique_dir;
}

static void cleanup_output_dir(const std::string& output_dir) {
    try { std::filesystem::remove_all(output_dir); }
    catch (const std::exception&) {}
}

//---------------------------------------------------------------------------------------------------------
// Helper: load cells and params from the stable single-sphere parameter file
//---------------------------------------------------------------------------------------------------------
static std::pair<std::vector<cell_ptr>, global_simulation_parameters>
load_test_simulation(const std::string& output_dir) {
    std::string param_file = std::string(PROJECT_SOURCE_DIR) +
                             "/test/test_scientific_benchmarks/params_single_sphere.xml";
    simulation_initializer sim_init(param_file, false);
    auto cell_lst = sim_init.get_cell_lst();
    auto sim_params = sim_init.get_simulation_parameters();
    sim_params.output_folder_path_ = output_dir;
    sim_params.simulation_duration_ = 1.0;   // Large so run_iteration() doesn't skip
    sim_params.sampling_period_ = 2.0;       // Avoid file writes
    return {cell_lst, sim_params};
}

//---------------------------------------------------------------------------------------------------------
// Helper: run N iterations of the solver using run_iteration()
//---------------------------------------------------------------------------------------------------------
static void run_n_iterations(solver& s, int n) {
    for (int i = 0; i < n; ++i) {
        s.run_iteration();
    }
}

//---------------------------------------------------------------------------------------------------------
// Helper: extract ordered list of all node positions from cell list
//---------------------------------------------------------------------------------------------------------
static std::vector<double> extract_node_positions(const std::vector<cell_ptr>& cells) {
    std::vector<double> positions;
    for (const auto& cell : cells) {
        const auto& node_lst = cell->get_node_lst();
        for (size_t i = 0; i < node_lst.size(); ++i) {
            const auto& n = node_lst[i];
            if (n.is_used()) {
                positions.push_back(n.pos().dx());
                positions.push_back(n.pos().dy());
                positions.push_back(n.pos().dz());
            }
        }
    }
    return positions;
}

//---------------------------------------------------------------------------------------------------------
// Helper: extract ordered list of cell volumes
//---------------------------------------------------------------------------------------------------------
static std::vector<double> extract_cell_volumes(const std::vector<cell_ptr>& cells) {
    std::vector<double> volumes;
    for (const auto& cell : cells) {
        volumes.push_back(cell->get_volume());
    }
    return volumes;
}

//---------------------------------------------------------------------------------------------------------
/**
 * @brief Test bitwise determinism with single thread and static scheduling
 *
 * Runs the same simulation twice with 1 thread and static scheduling for
 * a controlled number of iterations. Final node positions must be bitwise
 * identical (memcmp).
 *
 * @return 0 on success, 1 on failure
 */
int test_determinism_single_thread() {
    std::cout << "Running test_determinism_single_thread (" << NUM_ITERATIONS << " iterations)..." << std::endl;

    std::string out_dir_1 = create_unique_output_dir("det_run1");
    std::string out_dir_2 = create_unique_output_dir("det_run2");

    try {
        // Run 1
        std::vector<double> positions_1;
        std::vector<double> volumes_1;
        {
            auto [cell_lst, sim_params] = load_test_simulation(out_dir_1);
            solver s(sim_params, cell_lst, 1, true, false, "static");
            run_n_iterations(s, NUM_ITERATIONS);
            positions_1 = extract_node_positions(s.get_cell_lst());
            volumes_1 = extract_cell_volumes(s.get_cell_lst());
        }

        // Run 2
        std::vector<double> positions_2;
        std::vector<double> volumes_2;
        {
            auto [cell_lst, sim_params] = load_test_simulation(out_dir_2);
            solver s(sim_params, cell_lst, 1, true, false, "static");
            run_n_iterations(s, NUM_ITERATIONS);
            positions_2 = extract_node_positions(s.get_cell_lst());
            volumes_2 = extract_cell_volumes(s.get_cell_lst());
        }

        // Verify same number of nodes
        bool t1 = (positions_1.size() == positions_2.size());
        std::cout << "  t1 (same node count): " << t1
                  << " (" << positions_1.size() / 3 << " vs " << positions_2.size() / 3 << " nodes)"
                  << std::endl;
        if (!t1) { cleanup_output_dir(out_dir_1); cleanup_output_dir(out_dir_2); return 1; }

        // Verify bitwise identical positions (memcmp for exact equality)
        bool t2 = (positions_1.size() > 0) &&
                  (std::memcmp(positions_1.data(), positions_2.data(),
                               positions_1.size() * sizeof(double)) == 0);
        std::cout << "  t2 (bitwise identical positions): " << t2 << std::endl;
        if (!t2) {
            for (size_t i = 0; i < positions_1.size(); ++i) {
                if (positions_1[i] != positions_2[i]) {
                    std::cout << "    First difference at index " << i
                              << ": " << positions_1[i] << " vs " << positions_2[i]
                              << " (diff=" << std::abs(positions_1[i] - positions_2[i]) << ")"
                              << std::endl;
                    break;
                }
            }
            cleanup_output_dir(out_dir_1);
            cleanup_output_dir(out_dir_2);
            return 1;
        }

        // Verify same number of cells
        bool t3 = (volumes_1.size() == volumes_2.size());
        std::cout << "  t3 (same cell count): " << t3 << std::endl;
        if (!t3) { cleanup_output_dir(out_dir_1); cleanup_output_dir(out_dir_2); return 1; }

        // Verify bitwise identical volumes
        bool t4 = (volumes_1.size() > 0) &&
                  (std::memcmp(volumes_1.data(), volumes_2.data(),
                               volumes_1.size() * sizeof(double)) == 0);
        std::cout << "  t4 (bitwise identical volumes): " << t4 << std::endl;
        if (!t4) { cleanup_output_dir(out_dir_1); cleanup_output_dir(out_dir_2); return 1; }

        std::cout << "test_determinism_single_thread PASSED" << std::endl;
        cleanup_output_dir(out_dir_1);
        cleanup_output_dir(out_dir_2);
        return 0;

    } catch (const std::exception& e) {
        std::cout << "FAILED with exception: " << e.what() << std::endl;
        cleanup_output_dir(out_dir_1);
        cleanup_output_dir(out_dir_2);
        return 1;
    }
}

//---------------------------------------------------------------------------------------------------------
/**
 * @brief Test energy reproducibility across runs
 *
 * Verifies that kinetic energy values are identical between two runs
 * with the same initial conditions and deterministic scheduling.
 *
 * @return 0 on success, 1 on failure
 */
int test_determinism_energy_reproducibility() {
    std::cout << "Running test_determinism_energy_reproducibility (" << NUM_ITERATIONS << " iterations)..." << std::endl;

    std::string out_dir_1 = create_unique_output_dir("energy_run1");
    std::string out_dir_2 = create_unique_output_dir("energy_run2");

    try {
        auto run_and_collect_energies = [](const std::string& output_dir) {
            auto [cell_lst, sim_params] = load_test_simulation(output_dir);
            solver s(sim_params, cell_lst, 1, true, false, "static");
            run_n_iterations(s, NUM_ITERATIONS);
            std::vector<float> energies;
            for (const auto& cell : s.get_cell_lst()) {
                energies.push_back(cell->get_kinetic_energy());
                energies.push_back(cell->get_pressure_energy());
                energies.push_back(cell->get_surface_tension_energy());
            }
            return energies;
        };

        auto energies_1 = run_and_collect_energies(out_dir_1);
        auto energies_2 = run_and_collect_energies(out_dir_2);

        // Verify same number of energy values
        bool t1 = (energies_1.size() == energies_2.size());
        std::cout << "  t1 (same energy count): " << t1 << std::endl;
        if (!t1) { cleanup_output_dir(out_dir_1); cleanup_output_dir(out_dir_2); return 1; }

        // Verify bitwise identical energies
        bool t2 = (energies_1.size() > 0) &&
                  (std::memcmp(energies_1.data(), energies_2.data(),
                               energies_1.size() * sizeof(float)) == 0);
        std::cout << "  t2 (bitwise identical energies): " << t2 << std::endl;
        if (!t2) { cleanup_output_dir(out_dir_1); cleanup_output_dir(out_dir_2); return 1; }

        // Verify energies are finite
        bool t3 = true;
        for (size_t i = 0; i < energies_1.size(); ++i) {
            if (std::isnan(energies_1[i]) || std::isinf(energies_1[i])) {
                std::cout << "  Energy at index " << i << " is not finite: " << energies_1[i] << std::endl;
                t3 = false;
                break;
            }
        }
        std::cout << "  t3 (all energies finite): " << t3 << std::endl;
        if (!t3) { cleanup_output_dir(out_dir_1); cleanup_output_dir(out_dir_2); return 1; }

        std::cout << "test_determinism_energy_reproducibility PASSED" << std::endl;
        cleanup_output_dir(out_dir_1);
        cleanup_output_dir(out_dir_2);
        return 0;

    } catch (const std::exception& e) {
        std::cout << "FAILED with exception: " << e.what() << std::endl;
        cleanup_output_dir(out_dir_1);
        cleanup_output_dir(out_dir_2);
        return 1;
    }
}

//---------------------------------------------------------------------------------------------------------
// Main
int main(int argc, char** argv) {
    assert(argc == 2);
    std::string test_name = argv[1];

    if (test_name == "test_determinism_single_thread")          return test_determinism_single_thread();
    if (test_name == "test_determinism_energy_reproducibility")  return test_determinism_energy_reproducibility();

    std::cout << "TEST NAME: " << test_name << " DOES NOT EXIST" << std::endl;
    return 1;
}
//---------------------------------------------------------------------------------------------------------
