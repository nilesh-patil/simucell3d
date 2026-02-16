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

// Number of iterations for extreme parameter tests
static constexpr int NUM_ITERATIONS = 50;

//---------------------------------------------------------------------------------------------------------
static std::string create_unique_output_dir(const std::string& test_name) {
    pid_t pid = getpid();
    std::string unique_dir = "/tmp/simucell3d_extreme_" + test_name + "_" + std::to_string(pid);
    std::filesystem::remove_all(unique_dir);
    return unique_dir;
}

static void cleanup_output_dir(const std::string& output_dir) {
    try { std::filesystem::remove_all(output_dir); }
    catch (const std::exception&) {}
}

//---------------------------------------------------------------------------------------------------------
// Helper: load simulation from the stable single-sphere parameter file
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
// Helper: run N iterations using run_iteration()
//---------------------------------------------------------------------------------------------------------
static void run_n_iterations(solver& s, int n) {
    for (int i = 0; i < n; ++i) {
        s.run_iteration();
    }
}

//---------------------------------------------------------------------------------------------------------
/**
 * @brief Test: bulk_modulus = 0 should complete without crash
 *
 * When bulk_modulus = 0, there's no pressure regulation. The solver should
 * handle this gracefully (existing fix checks for bulk_modulus > 0 before
 * computing pressure energy with log(V/V_target)).
 *
 * @return 0 on success, 1 on failure
 */
int test_zero_bulk_modulus() {
    std::cout << "Running test_zero_bulk_modulus (" << NUM_ITERATIONS << " iterations)..." << std::endl;

    std::string out_dir = create_unique_output_dir("zero_bulk");

    try {
        auto [cell_lst, sim_params] = load_test_simulation(out_dir);

        // Set all cell types to bulk_modulus = 0
        for (auto& cell : cell_lst) {
            cell->get_cell_type()->bulk_modulus_ = 0.0;
        }

        solver s(sim_params, cell_lst, 1, true, false, "static");
        run_n_iterations(s, NUM_ITERATIONS);

        // With bulk_modulus=0, cells may collapse below min_vol and be removed.
        // The key assertion is: the solver completed without crash/NaN/Inf.
        std::cout << "  Cells remaining: " << s.get_cell_lst().size() << std::endl;
        bool t1 = true;  // Solver completed without exception

        // If cells remain, verify no NaN in positions
        bool t2 = true;
        for (const auto& cell : s.get_cell_lst()) {
            for (size_t i = 0; i < cell->get_node_lst().size(); ++i) {
                if (cell->get_node_lst()[i].is_used()) {
                    const auto& pos = cell->get_node_lst()[i].pos();
                    if (std::isnan(pos.dx()) || std::isnan(pos.dy()) || std::isnan(pos.dz())) {
                        t2 = false;
                        break;
                    }
                }
            }
            if (!t2) break;
        }
        std::cout << "  t1 (solver completed): " << t1 << std::endl;
        std::cout << "  t2 (no NaN positions in surviving cells): " << t2 << std::endl;
        if (!t2) { cleanup_output_dir(out_dir); return 1; }

        // If cells remain, verify all volumes are finite
        bool t3 = true;
        for (const auto& cell : s.get_cell_lst()) {
            if (!std::isfinite(cell->get_volume())) {
                t3 = false;
                break;
            }
        }
        std::cout << "  t3 (surviving cell volumes finite): " << t3 << std::endl;
        if (!t3) { cleanup_output_dir(out_dir); return 1; }

        std::cout << "test_zero_bulk_modulus PASSED" << std::endl;
        cleanup_output_dir(out_dir);
        return 0;

    } catch (const std::exception& e) {
        std::cout << "FAILED with exception: " << e.what() << std::endl;
        cleanup_output_dir(out_dir);
        return 1;
    }
}

//---------------------------------------------------------------------------------------------------------
/**
 * @brief Test: high pressure (10x physiological)
 *
 * Tests numerical stability when cells experience very high internal pressure.
 * The simulation should complete without energy explosion or NaN values.
 *
 * @return 0 on success, 1 on failure
 */
int test_high_pressure() {
    std::cout << "Running test_high_pressure (" << NUM_ITERATIONS << " iterations)..." << std::endl;

    std::string out_dir = create_unique_output_dir("high_pressure");

    try {
        auto [cell_lst, sim_params] = load_test_simulation(out_dir);

        // Increase bulk_modulus by 10x to generate high pressure
        for (auto& cell : cell_lst) {
            cell->get_cell_type()->bulk_modulus_ *= 10.0;
            cell->get_cell_type()->max_pressure_ *= 10.0;
        }

        solver s(sim_params, cell_lst, 1, true, false, "static");
        run_n_iterations(s, NUM_ITERATIONS);

        // Verify simulation completed
        bool t1 = (s.get_cell_lst().size() > 0);
        std::cout << "  t1 (cells survived): " << t1 << std::endl;
        if (!t1) { cleanup_output_dir(out_dir); return 1; }

        // Verify no NaN or Inf in node positions
        bool t2 = true;
        for (const auto& cell : s.get_cell_lst()) {
            for (size_t i = 0; i < cell->get_node_lst().size(); ++i) {
                if (cell->get_node_lst()[i].is_used()) {
                    if (!std::isfinite(cell->get_node_lst()[i].pos().norm())) {
                        t2 = false;
                        break;
                    }
                }
            }
            if (!t2) break;
        }
        std::cout << "  t2 (all positions finite): " << t2 << std::endl;
        if (!t2) { cleanup_output_dir(out_dir); return 1; }

        // Verify kinetic energies are bounded (not exploding)
        bool t3 = true;
        for (const auto& cell : s.get_cell_lst()) {
            float ke = cell->get_kinetic_energy();
            if (!std::isfinite(ke)) {
                std::cout << "  Non-finite kinetic energy: " << ke << std::endl;
                t3 = false;
                break;
            }
        }
        std::cout << "  t3 (kinetic energies finite): " << t3 << std::endl;
        if (!t3) { cleanup_output_dir(out_dir); return 1; }

        std::cout << "test_high_pressure PASSED" << std::endl;
        cleanup_output_dir(out_dir);
        return 0;

    } catch (const std::exception& e) {
        std::cout << "FAILED with exception: " << e.what() << std::endl;
        cleanup_output_dir(out_dir);
        return 1;
    }
}

//---------------------------------------------------------------------------------------------------------
/**
 * @brief Test: degenerate mesh handling
 *
 * Sets a very small minimum edge length to test mesh refinement behavior
 * at the degenerate limit. The solver should not crash even when encountering
 * nearly-degenerate triangles.
 *
 * @return 0 on success, 1 on failure
 */
int test_degenerate_mesh() {
    std::cout << "Running test_degenerate_mesh (" << NUM_ITERATIONS << " iterations)..." << std::endl;

    std::string out_dir = create_unique_output_dir("degenerate_mesh");

    try {
        auto [cell_lst, sim_params] = load_test_simulation(out_dir);

        // Set smaller minimum edge length to stress mesh refinement
        sim_params.min_edge_len_ *= 0.5;

        solver s(sim_params, cell_lst, 1, true, false, "static");
        run_n_iterations(s, NUM_ITERATIONS);

        // Verify simulation completed
        bool t1 = (s.get_cell_lst().size() > 0);
        std::cout << "  t1 (cells survived): " << t1
                  << " (" << s.get_cell_lst().size() << " cells)" << std::endl;
        if (!t1) { cleanup_output_dir(out_dir); return 1; }

        // Verify all cells have valid positive volume
        bool t2 = true;
        for (const auto& cell : s.get_cell_lst()) {
            if (cell->get_volume() <= 0.0 || !std::isfinite(cell->get_volume())) {
                std::cout << "  Cell has invalid volume: " << cell->get_volume() << std::endl;
                t2 = false;
                break;
            }
        }
        std::cout << "  t2 (all volumes valid): " << t2 << std::endl;
        if (!t2) { cleanup_output_dir(out_dir); return 1; }

        // Verify no NaN in positions
        bool t3 = true;
        for (const auto& cell : s.get_cell_lst()) {
            for (size_t i = 0; i < cell->get_node_lst().size(); ++i) {
                if (cell->get_node_lst()[i].is_used() && !std::isfinite(cell->get_node_lst()[i].pos().norm())) {
                    t3 = false;
                    break;
                }
            }
            if (!t3) break;
        }
        std::cout << "  t3 (no NaN positions): " << t3 << std::endl;
        if (!t3) { cleanup_output_dir(out_dir); return 1; }

        std::cout << "test_degenerate_mesh PASSED" << std::endl;
        cleanup_output_dir(out_dir);
        return 0;

    } catch (const std::exception& e) {
        std::cout << "FAILED with exception: " << e.what() << std::endl;
        cleanup_output_dir(out_dir);
        return 1;
    }
}

//---------------------------------------------------------------------------------------------------------
/**
 * @brief Test: contact cutoff = min_edge_len boundary case
 *
 * Sets contact_cutoff_adhesion and contact_cutoff_repulsion equal to
 * min_edge_length. This is a boundary condition where contacts happen
 * right at the mesh resolution limit.
 *
 * @return 0 on success, 1 on failure
 */
int test_contact_cutoff_boundary() {
    std::cout << "Running test_contact_cutoff_boundary (" << NUM_ITERATIONS << " iterations)..." << std::endl;

    std::string out_dir = create_unique_output_dir("cutoff_boundary");

    try {
        auto [cell_lst, sim_params] = load_test_simulation(out_dir);

        // Set contact cutoffs equal to min_edge_length
        sim_params.contact_cutoff_adhesion_ = sim_params.min_edge_len_;
        sim_params.contact_cutoff_repulsion_ = sim_params.min_edge_len_;

        solver s(sim_params, cell_lst, 1, true, false, "static");
        run_n_iterations(s, NUM_ITERATIONS);

        // Verify simulation completed
        bool t1 = (s.get_cell_lst().size() > 0);
        std::cout << "  t1 (cells survived): " << t1 << std::endl;
        if (!t1) { cleanup_output_dir(out_dir); return 1; }

        // Verify all positions are finite
        bool t2 = true;
        for (const auto& cell : s.get_cell_lst()) {
            for (size_t i = 0; i < cell->get_node_lst().size(); ++i) {
                if (cell->get_node_lst()[i].is_used() && !std::isfinite(cell->get_node_lst()[i].pos().norm())) {
                    t2 = false;
                    break;
                }
            }
            if (!t2) break;
        }
        std::cout << "  t2 (all positions finite): " << t2 << std::endl;
        if (!t2) { cleanup_output_dir(out_dir); return 1; }

        // Verify energies are finite
        bool t3 = true;
        for (const auto& cell : s.get_cell_lst()) {
            if (!std::isfinite(cell->get_kinetic_energy()) ||
                !std::isfinite(cell->get_pressure_energy())) {
                t3 = false;
                break;
            }
        }
        std::cout << "  t3 (all energies finite): " << t3 << std::endl;
        if (!t3) { cleanup_output_dir(out_dir); return 1; }

        std::cout << "test_contact_cutoff_boundary PASSED" << std::endl;
        cleanup_output_dir(out_dir);
        return 0;

    } catch (const std::exception& e) {
        std::cout << "FAILED with exception: " << e.what() << std::endl;
        cleanup_output_dir(out_dir);
        return 1;
    }
}

//---------------------------------------------------------------------------------------------------------
int main(int argc, char** argv) {
    assert(argc == 2);
    std::string test_name = argv[1];

    if (test_name == "test_zero_bulk_modulus")       return test_zero_bulk_modulus();
    if (test_name == "test_high_pressure")           return test_high_pressure();
    if (test_name == "test_degenerate_mesh")         return test_degenerate_mesh();
    if (test_name == "test_contact_cutoff_boundary") return test_contact_cutoff_boundary();

    std::cout << "TEST NAME: " << test_name << " DOES NOT EXIST" << std::endl;
    return 1;
}
//---------------------------------------------------------------------------------------------------------
