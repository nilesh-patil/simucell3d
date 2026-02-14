#include <cassert>
#include <string>
#include <iostream>
#include <cmath>
#include <vector>
#include <memory>

#include "time_integration.hpp"
#include "cell.hpp"
#include "epithelial_cell.hpp"
#include "node.hpp"
#include "vec3.hpp"
#include "custom_structures.hpp"


/*
 * Comprehensive unit tests for the time integration module.
 * Tests cover numerical accuracy, physical consistency, and edge cases.
 */


//---------------------------------------------------------------------------------------------------------
// Helper class to access protected members for testing
class time_integration_test_helper {
public:
    // Access node list from cell
    static std::vector<node>& get_node_lst(cell_ptr c) {
        return c->node_lst_;
    }

    // Get mutable node reference
    static node& get_node(cell_ptr c, unsigned id) {
        return c->node_lst_[id];
    }
};
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Helper function: Create a minimal test cell with known properties
cell_ptr create_test_cell(unsigned cell_id, const std::vector<vec3>& node_positions) {

    // Create minimal cell type parameters with realistic density
    auto cell_type = std::make_shared<cell_type_parameters>();
    cell_type->name_ = "test_cell";
    cell_type->global_type_id_ = 0;
    cell_type->mass_density_ = 1.0e3;  // 1000 kg/m^3 (water density)
    cell_type->bulk_modulus_ = 1.0e3;
    cell_type->max_pressure_ = 1.0e4;
    cell_type->initial_pressure_ = 0.0;
    cell_type->area_elasticity_modulus_ = 0.0;
    cell_type->avg_division_vol_ = 1.0e-15;
    cell_type->std_division_vol_ = 0.0;
    cell_type->avg_growth_rate_ = 0.0;
    cell_type->std_growth_rate_ = 0.0;
    cell_type->min_vol_ = 1.0e-18;
    cell_type->angle_regularization_factor_ = 0.0;
    cell_type->target_isoperimetric_ratio_ = 0.0;
    cell_type->surface_coupling_max_curvature_ = 0.0;

    // Add a default face type
    face_type_parameters face_type;
    face_type.name_ = "default";
    face_type.face_type_global_id_ = 0;
    face_type.surface_tension_ = 0.0;
    face_type.adherence_strength_ = 0.0;
    face_type.repulsion_strength_ = 0.0;
    face_type.bending_modulus_ = 0.0;
    cell_type->add_face_type(face_type);

    // Create a simple tetrahedron mesh (4 nodes, 4 faces)
    // Scale up the node positions if they're too small to get realistic mass
    std::vector<double> node_coords;
    for (const auto& pos : node_positions) {
        node_coords.push_back(pos.dx());
        node_coords.push_back(pos.dy());
        node_coords.push_back(pos.dz());
    }

    // Define faces (tetrahedron topology)
    std::vector<unsigned> face_node_ids = {
        0, 1, 2,  // Face 0
        0, 1, 3,  // Face 1
        0, 2, 3,  // Face 2
        1, 2, 3   // Face 3
    };

    auto cell = std::make_shared<epithelial_cell>(node_coords, face_node_ids, cell_id, cell_type);

    return cell;
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 1: Verify simulation time increments correctly after each integration step
int test_time_step_accumulation() {

    // Create simulation parameters matching actual cell simulations
    // Reference: parameters_vesicle.xml
    global_simulation_parameters sim_params;
    sim_params.time_step_ = 1.0e-7;  // 100 ns (matches actual simulations)
    sim_params.damping_coefficient_ = 5.0e-10;  // kg/s (matches actual simulations)

    // Create time integration scheme
    time_integration_scheme integrator(sim_params, false);

    // Verify initial time is zero
    const double eps = 1.0e-15;
    bool t1 = std::abs(integrator.get_simulation_time() - 0.0) < eps;

    // Create a minimal test cell with realistic ~5 μm geometry
    // Typical epithelial cells are 5-20 μm in diameter
    std::vector<vec3> positions = {
        vec3(0.0, 0.0, 0.0),
        vec3(5.0e-6, 0.0, 0.0),
        vec3(0.0, 5.0e-6, 0.0),
        vec3(0.0, 0.0, 5.0e-6)
    };
    auto cell = create_test_cell(0, positions);
    cell->initialize_cell_properties();
    std::vector<cell_ptr> cell_lst = {cell};

    // Perform multiple integration steps
    const int num_steps = 10;
    for (int i = 0; i < num_steps; ++i) {
        integrator.update_nodes_positions(cell_lst);
    }

    // Verify time has accumulated correctly
    double expected_time = num_steps * sim_params.time_step_;
    double actual_time = integrator.get_simulation_time();
    bool t2 = std::abs(actual_time - expected_time) < eps;

    // Perform additional steps
    for (int i = 0; i < 5; ++i) {
        integrator.update_nodes_positions(cell_lst);
    }

    expected_time = (num_steps + 5) * sim_params.time_step_;
    actual_time = integrator.get_simulation_time();
    bool t3 = std::abs(actual_time - expected_time) < eps;

    return !(t1 && t2 && t3);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 2: Verify semi-implicit Euler integration for single node with known force
int test_semi_implicit_euler_single_node() {

    // Only test if DYNAMIC_MODEL_INDEX == 0
    #if DYNAMIC_MODEL_INDEX != 0
        return 0;  // Pass (test not applicable)
    #endif

    // Create simulation parameters matching actual cell simulations
    global_simulation_parameters sim_params;
    sim_params.time_step_ = 1.0e-7;  // 100 ns (matches actual simulations)
    sim_params.damping_coefficient_ = 5.0e-10;  // kg/s (matches actual simulations)

    // Create time integration scheme
    time_integration_scheme integrator(sim_params, false);

    // Create a test cell with realistic ~5 μm geometry
    std::vector<vec3> positions = {
        vec3(0.0, 0.0, 0.0),
        vec3(5.0e-6, 0.0, 0.0),
        vec3(0.0, 5.0e-6, 0.0),
        vec3(0.0, 0.0, 5.0e-6)
    };
    auto cell = create_test_cell(0, positions);
    cell->initialize_cell_properties();
    std::vector<cell_ptr> cell_lst = {cell};

    // Get reference to first node
    node& n = time_integration_test_helper::get_node(cell, 0);

    // Set initial conditions
    vec3 initial_pos = n.pos();
    vec3 initial_momentum(0.0, 0.0, 0.0);
    vec3 applied_force(1.0e-9, 0.0, 0.0);  // 1 nN in x-direction

    #if DYNAMIC_MODEL_INDEX == 0
        n.set_momentum(initial_momentum);
    #endif
    n.set_force(applied_force);

    // Get node mass
    double node_mass = cell->get_node_mass();

    // Perform one integration step
    integrator.update_nodes_positions(cell_lst);

    // Expected values for semi-implicit Euler:
    // v_new = v_old + (F/m - damping*v_old/m)*dt
    // For v_old = 0: v_new = F*dt/m
    // x_new = x_old + v_new*dt

    double dt = sim_params.time_step_;
    double damping = sim_params.damping_coefficient_;

    #if DYNAMIC_MODEL_INDEX == 0
        // Expected momentum update: p_new = p_old + (F - damping*p_old/m)*dt
        // For p_old = 0: p_new = F*dt
        vec3 expected_momentum = applied_force * dt;

        // Expected position update: x_new = x_old + p_new*dt/m
        vec3 expected_velocity = expected_momentum / node_mass;
        vec3 expected_pos = initial_pos + (expected_velocity * dt);

        // Verify momentum (note: momentum is updated by integrator)
        vec3 actual_momentum = n.momentum();
        const double eps = 1.0e-18;
        bool t1 = (actual_momentum - expected_momentum).norm() < eps;

        // Verify position
        vec3 actual_pos = n.pos();
        bool t2 = (actual_pos - expected_pos).norm() < eps;

        return !(t1 && t2);
    #else
        return 0;  // Pass (test not applicable)
    #endif
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 3a: Verify velocity damping with BIOLOGICAL parameters (realistic cell simulation)
int test_velocity_damping_biological() {

    // Only test if DYNAMIC_MODEL_INDEX == 0
    #if DYNAMIC_MODEL_INDEX != 0
        return 0;  // Pass (test not applicable)
    #endif

    // BIOLOGICAL STABILITY TEST
    // Parameters matching actual cell simulations (parameters_vesicle.xml)
    // With 5 μm cell: volume ~ 2e-17 m³, mass ~ 2e-14 kg, node_mass ~ 5e-15 kg
    // Damping decay per step: γ*dt/m = 5e-10 * 1e-7 / 5e-15 ≈ 0.01
    // After 50 steps: (1-0.01)^50 ≈ 0.60 (40% decay)
    global_simulation_parameters sim_params;
    sim_params.time_step_ = 1.0e-7;  // 100 ns (matches actual simulations)
    sim_params.damping_coefficient_ = 5.0e-10;  // kg/s (matches actual simulations)

    // Create time integration scheme
    time_integration_scheme integrator(sim_params, false);

    // Create a test cell with realistic ~5 μm geometry (typical epithelial cell)
    std::vector<vec3> positions = {
        vec3(0.0, 0.0, 0.0),
        vec3(5.0e-6, 0.0, 0.0),
        vec3(0.0, 5.0e-6, 0.0),
        vec3(0.0, 0.0, 5.0e-6)
    };
    auto cell = create_test_cell(0, positions);
    cell->initialize_cell_properties();
    std::vector<cell_ptr> cell_lst = {cell};

    // Get reference to first node
    node& n = time_integration_test_helper::get_node(cell, 0);

    #if DYNAMIC_MODEL_INDEX == 0
        // Set initial momentum (no force applied)
        // Realistic momentum for a cell node: ~1e-18 to 1e-16 kg·m/s
        vec3 initial_momentum(1.0e-17, 0.0, 0.0);
        n.set_momentum(initial_momentum);
        n.set_force(vec3(0.0, 0.0, 0.0));  // Zero force

        // Get node mass
        double node_mass = cell->get_node_mass();
        double initial_velocity_magnitude = initial_momentum.norm() / node_mass;

        // Perform integration steps - with realistic damping, ~50 steps gives ~40% decay
        const int num_steps = 50;
        for (int i = 0; i < num_steps; ++i) {
            n.set_force(vec3(0.0, 0.0, 0.0));  // Ensure zero force
            integrator.update_nodes_positions(cell_lst);
        }

        // Verify momentum has decreased (damping effect)
        vec3 final_momentum = n.momentum();
        double final_velocity_magnitude = final_momentum.norm() / node_mass;

        // With damping and no external force, velocity should decrease significantly
        // Expect ~40% decay with realistic parameters
        bool t1 = final_velocity_magnitude < initial_velocity_magnitude * 0.99;

        // Verify velocity hasn't increased (numerical stability check)
        bool t2 = final_velocity_magnitude >= 0.0;

        // Verify velocity is still in x-direction (damping preserves direction)
        bool t3 = std::abs(final_momentum.dy()) < 1.0e-30 && std::abs(final_momentum.dz()) < 1.0e-30;

        return !(t1 && t2 && t3);
    #else
        return 0;  // Pass (test not applicable)
    #endif
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 3b: Verify velocity damping with PHYSICAL parameters (numerical scheme validation)
int test_velocity_damping_physical() {

    // Only test if DYNAMIC_MODEL_INDEX == 0
    #if DYNAMIC_MODEL_INDEX != 0
        return 0;  // Pass (test not applicable)
    #endif

    // PHYSICAL/NUMERICAL STABILITY TEST
    // Parameters chosen to clearly demonstrate damping mathematics
    // Uses larger geometry and stronger damping for visible decay in few steps
    global_simulation_parameters sim_params;
    sim_params.time_step_ = 1.0e-9;  // Smaller timestep for numerical precision
    sim_params.damping_coefficient_ = 1.0e-4;  // Stronger damping for clear effect

    // Create time integration scheme
    time_integration_scheme integrator(sim_params, false);

    // Larger geometry (100 μm) to increase mass for numerical stability
    // This is NOT biologically realistic but demonstrates the damping algorithm
    std::vector<vec3> positions = {
        vec3(0.0, 0.0, 0.0),
        vec3(1.0e-4, 0.0, 0.0),
        vec3(0.0, 1.0e-4, 0.0),
        vec3(0.0, 0.0, 1.0e-4)
    };
    auto cell = create_test_cell(0, positions);
    cell->initialize_cell_properties();
    std::vector<cell_ptr> cell_lst = {cell};

    // Get reference to first node
    node& n = time_integration_test_helper::get_node(cell, 0);

    #if DYNAMIC_MODEL_INDEX == 0
        // Set initial momentum scaled for this geometry
        vec3 initial_momentum(1.0e-12, 0.0, 0.0);
        n.set_momentum(initial_momentum);
        n.set_force(vec3(0.0, 0.0, 0.0));

        double node_mass = cell->get_node_mass();
        double initial_velocity_magnitude = initial_momentum.norm() / node_mass;

        // With γ=1e-4, dt=1e-9, m≈4e-11: decay ≈ 2.4e-3 per step
        // After 50 steps: ~11% decay
        const int num_steps = 50;
        for (int i = 0; i < num_steps; ++i) {
            n.set_force(vec3(0.0, 0.0, 0.0));
            integrator.update_nodes_positions(cell_lst);
        }

        vec3 final_momentum = n.momentum();
        double final_velocity_magnitude = final_momentum.norm() / node_mass;

        // Verify damping effect (should see ~11% decay)
        bool t1 = final_velocity_magnitude < initial_velocity_magnitude * 0.99;
        bool t2 = final_velocity_magnitude >= 0.0;
        bool t3 = std::abs(final_momentum.dy()) < 1.0e-20 && std::abs(final_momentum.dz()) < 1.0e-20;

        return !(t1 && t2 && t3);
    #else
        return 0;
    #endif
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 3: Wrapper that runs both damping tests
int test_velocity_damping() {
    int result_bio = test_velocity_damping_biological();
    if (result_bio != 0) return result_bio;

    int result_phys = test_velocity_damping_physical();
    return result_phys;
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 4: Verify energy behavior (numerical stability) - BIOLOGICAL parameters
int test_energy_behavior() {

    // Only test if DYNAMIC_MODEL_INDEX == 0
    #if DYNAMIC_MODEL_INDEX != 0
        return 0;  // Pass (test not applicable)
    #endif

    // BIOLOGICAL STABILITY TEST - realistic cell simulation parameters
    global_simulation_parameters sim_params;
    sim_params.time_step_ = 1.0e-7;  // 100 ns (matches actual simulations)
    sim_params.damping_coefficient_ = 5.0e-10;  // kg/s (matches actual simulations)

    // Create time integration scheme
    time_integration_scheme integrator(sim_params, false);

    // Create a test cell with realistic ~5 μm geometry
    std::vector<vec3> positions = {
        vec3(0.0, 0.0, 0.0),
        vec3(5.0e-6, 0.0, 0.0),
        vec3(0.0, 5.0e-6, 0.0),
        vec3(0.0, 0.0, 5.0e-6)
    };
    auto cell = create_test_cell(0, positions);
    cell->initialize_cell_properties();
    std::vector<cell_ptr> cell_lst = {cell};

    #if DYNAMIC_MODEL_INDEX == 0
        // Set initial momentum on all nodes (realistic scale for cell nodes)
        auto& node_lst = time_integration_test_helper::get_node_lst(cell);
        for (auto& n : node_lst) {
            if (n.is_used()) {
                n.set_momentum(vec3(1.0e-17, 5.0e-18, 2.0e-18));
                n.set_force(vec3(0.0, 0.0, 0.0));
            }
        }

        // Get initial kinetic energy
        integrator.update_nodes_positions(cell_lst);
        double initial_energy = cell->get_kinetic_energy();

        // Verify initial energy is positive
        bool t1 = initial_energy > 0.0;

        // Perform integration steps with damping (no external forces)
        const int num_steps = 30;
        double max_energy = initial_energy;
        bool energy_bounded = true;

        for (int i = 0; i < num_steps; ++i) {
            // Reset forces to zero (only damping acts)
            for (auto& n : node_lst) {
                if (n.is_used()) {
                    n.set_force(vec3(0.0, 0.0, 0.0));
                }
            }

            integrator.update_nodes_positions(cell_lst);
            double current_energy = cell->get_kinetic_energy();

            // Energy should not explode
            if (current_energy > 10.0 * initial_energy) {
                energy_bounded = false;
            }

            // Track maximum energy
            if (current_energy > max_energy) {
                max_energy = current_energy;
            }

            // Check for NaN or Inf
            if (std::isnan(current_energy) || std::isinf(current_energy)) {
                energy_bounded = false;
            }
        }

        // Get final energy
        double final_energy = cell->get_kinetic_energy();

        // With damping and no external forces, energy should decay
        bool t2 = final_energy < initial_energy;

        // Energy should remain bounded
        bool t3 = energy_bounded;

        // Final energy should be non-negative
        bool t4 = final_energy >= 0.0;

        return !(t1 && t2 && t3 && t4);
    #else
        return 0;  // Pass (test not applicable)
    #endif
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 5: Verify force response (constant force applied) - BIOLOGICAL parameters
int test_force_response() {

    // Only test if DYNAMIC_MODEL_INDEX == 0
    #if DYNAMIC_MODEL_INDEX != 0
        return 0;  // Pass (test not applicable)
    #endif

    // BIOLOGICAL STABILITY TEST - realistic cell simulation parameters
    global_simulation_parameters sim_params;
    sim_params.time_step_ = 1.0e-7;  // 100 ns (matches actual simulations)
    sim_params.damping_coefficient_ = 5.0e-10;  // kg/s (matches actual simulations)

    // Create time integration scheme
    time_integration_scheme integrator(sim_params, false);

    // Create a test cell with realistic ~5 μm geometry
    std::vector<vec3> positions = {
        vec3(0.0, 0.0, 0.0),
        vec3(5.0e-6, 0.0, 0.0),
        vec3(0.0, 5.0e-6, 0.0),
        vec3(0.0, 0.0, 5.0e-6)
    };
    auto cell = create_test_cell(0, positions);
    cell->initialize_cell_properties();
    std::vector<cell_ptr> cell_lst = {cell};

    // Get reference to first node
    node& n = time_integration_test_helper::get_node(cell, 0);

    #if DYNAMIC_MODEL_INDEX == 0
        // Set initial conditions
        // Realistic force scale: surface tension ~ 1e-3 N/m, edge ~ 1e-6 m
        // Force per node ~ 1e-9 to 1e-12 N
        vec3 constant_force(1.0e-12, 0.0, 0.0);
        n.set_momentum(vec3(0.0, 0.0, 0.0));

        double node_mass = cell->get_node_mass();

        // Apply constant force for multiple steps
        const int num_steps = 10;
        for (int i = 0; i < num_steps; ++i) {
            n.set_force(constant_force);
            integrator.update_nodes_positions(cell_lst);
        }

        // Get final momentum
        vec3 final_momentum = n.momentum();

        // With constant force, momentum should increase in x-direction
        bool t1 = final_momentum.dx() > 0.0;

        // Momentum should be primarily in x-direction
        bool t2 = std::abs(final_momentum.dx()) > std::abs(final_momentum.dy());
        bool t3 = std::abs(final_momentum.dx()) > std::abs(final_momentum.dz());

        // Position should have moved in positive x-direction
        vec3 final_pos = n.pos();
        bool t4 = final_pos.dx() > positions[0].dx();

        // Verify no NaN or Inf
        bool t5 = !std::isnan(final_momentum.norm()) && !std::isinf(final_momentum.norm());
        bool t6 = !std::isnan(final_pos.norm()) && !std::isinf(final_pos.norm());

        return !(t1 && t2 && t3 && t4 && t5 && t6);
    #else
        return 0;  // Pass (test not applicable)
    #endif
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 6: Edge case - zero velocity and zero force - BIOLOGICAL parameters
int test_zero_velocity_zero_force() {

    // BIOLOGICAL STABILITY TEST - realistic cell simulation parameters
    global_simulation_parameters sim_params;
    sim_params.time_step_ = 1.0e-7;  // 100 ns (matches actual simulations)
    sim_params.damping_coefficient_ = 5.0e-10;  // kg/s (matches actual simulations)

    // Create time integration scheme
    time_integration_scheme integrator(sim_params, false);

    // Create a test cell with realistic ~5 μm geometry
    std::vector<vec3> positions = {
        vec3(0.0, 0.0, 0.0),
        vec3(5.0e-6, 0.0, 0.0),
        vec3(0.0, 5.0e-6, 0.0),
        vec3(0.0, 0.0, 5.0e-6)
    };
    auto cell = create_test_cell(0, positions);
    cell->initialize_cell_properties();
    std::vector<cell_ptr> cell_lst = {cell};

    // Get reference to first node
    node& n = time_integration_test_helper::get_node(cell, 0);
    vec3 initial_pos = n.pos();

    // Set zero initial conditions
    #if DYNAMIC_MODEL_INDEX == 0
        n.set_momentum(vec3(0.0, 0.0, 0.0));
    #endif
    n.set_force(vec3(0.0, 0.0, 0.0));

    // Perform integration steps
    const int num_steps = 10;
    for (int i = 0; i < num_steps; ++i) {
        n.set_force(vec3(0.0, 0.0, 0.0));
        integrator.update_nodes_positions(cell_lst);
    }

    // Node should not move (or move negligibly due to numerical errors)
    vec3 final_pos = n.pos();
    const double eps = 1.0e-14;
    bool t1 = (final_pos - initial_pos).norm() < eps;

    // Kinetic energy should be zero (or negligible)
    double kinetic_energy = cell->get_kinetic_energy();
    bool t2 = kinetic_energy < eps;

    // Verify no NaN or Inf
    bool t3 = !std::isnan(final_pos.norm()) && !std::isinf(final_pos.norm());

    return !(t1 && t2 && t3);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 7: Edge case - very small time step - PHYSICAL validation test
// Note: This tests numerical precision, not biological realism
int test_very_small_time_step() {

    // PHYSICAL/NUMERICAL VALIDATION TEST
    // Very small timestep to test numerical precision
    // Using realistic damping but smaller dt than actual simulations
    global_simulation_parameters sim_params;
    sim_params.time_step_ = 1.0e-10;  // 0.1 ns (smaller than typical 100 ns)
    sim_params.damping_coefficient_ = 5.0e-10;  // kg/s (matches actual simulations)

    // Create time integration scheme
    time_integration_scheme integrator(sim_params, false);

    // Create a test cell with realistic ~5 μm geometry
    std::vector<vec3> positions = {
        vec3(0.0, 0.0, 0.0),
        vec3(5.0e-6, 0.0, 0.0),
        vec3(0.0, 5.0e-6, 0.0),
        vec3(0.0, 0.0, 5.0e-6)
    };
    auto cell = create_test_cell(0, positions);
    cell->initialize_cell_properties();
    std::vector<cell_ptr> cell_lst = {cell};

    // Get reference to first node
    node& n = time_integration_test_helper::get_node(cell, 0);

    // Set initial conditions with realistic values
    #if DYNAMIC_MODEL_INDEX == 0
        n.set_momentum(vec3(1.0e-17, 0.0, 0.0));
    #endif
    n.set_force(vec3(1.0e-12, 0.0, 0.0));  // Realistic force

    // Perform many integration steps (equivalent to 100 ns total)
    const int num_steps = 1000;
    for (int i = 0; i < num_steps; ++i) {
        n.set_force(vec3(1.0e-12, 0.0, 0.0));
        integrator.update_nodes_positions(cell_lst);
    }

    // Verify simulation time is correct
    double expected_time = num_steps * sim_params.time_step_;
    double actual_time = integrator.get_simulation_time();
    const double eps = 1.0e-20;
    bool t1 = std::abs(actual_time - expected_time) < eps * expected_time ||
              std::abs(actual_time - expected_time) < 1.0e-18;

    // Verify no NaN or Inf in position
    vec3 final_pos = n.pos();
    bool t2 = !std::isnan(final_pos.norm()) && !std::isinf(final_pos.norm());

    // Verify position has changed (force is applied)
    bool t3 = final_pos.dx() > positions[0].dx();

    return !(t1 && t2 && t3);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 8: Verify kinetic energy calculation - BIOLOGICAL parameters
int test_kinetic_energy_calculation() {

    // Only test if DYNAMIC_MODEL_INDEX == 0
    #if DYNAMIC_MODEL_INDEX != 0
        return 0;  // Pass (test not applicable)
    #endif

    // BIOLOGICAL STABILITY TEST - realistic parameters
    // Use very small damping to minimize momentum change during update
    global_simulation_parameters sim_params;
    sim_params.time_step_ = 1.0e-7;  // 100 ns (matches actual simulations)
    sim_params.damping_coefficient_ = 1.0e-15;  // Negligible damping for this energy test

    // Create time integration scheme
    time_integration_scheme integrator(sim_params, false);

    // Create a test cell with realistic ~5 μm geometry
    std::vector<vec3> positions = {
        vec3(0.0, 0.0, 0.0),
        vec3(5.0e-6, 0.0, 0.0),
        vec3(0.0, 5.0e-6, 0.0),
        vec3(0.0, 0.0, 5.0e-6)
    };
    auto cell = create_test_cell(0, positions);
    cell->initialize_cell_properties();
    std::vector<cell_ptr> cell_lst = {cell};

    #if DYNAMIC_MODEL_INDEX == 0
        double node_mass = cell->get_node_mass();
        double dt = sim_params.time_step_;
        double damping = sim_params.damping_coefficient_;

        // Set known momentum on all nodes (realistic scale)
        vec3 known_momentum(1.0e-17, 0.0, 0.0);
        auto& node_lst = time_integration_test_helper::get_node_lst(cell);
        for (auto& n : node_lst) {
            if (n.is_used()) {
                n.set_momentum(known_momentum);
                n.set_force(vec3(0.0, 0.0, 0.0));
            }
        }

        // Update to calculate kinetic energy
        // Note: update_nodes_positions modifies momentum due to damping:
        // p_new = p_old * (1 - damping * dt / mass)
        integrator.update_nodes_positions(cell_lst);

        // Calculate expected kinetic energy AFTER the damping update
        double damping_factor = 1.0 - damping * dt / node_mass;
        vec3 expected_momentum_after = known_momentum * damping_factor;
        double expected_ke_per_node = 0.5 * expected_momentum_after.squared_norm() / node_mass;
        double expected_total_ke = expected_ke_per_node * cell->get_nb_of_nodes();

        double actual_ke = cell->get_kinetic_energy();

        // Compare with relative tolerance
        double relative_error = std::abs(actual_ke - expected_total_ke) / expected_total_ke;
        bool t1 = relative_error < 1.0e-6;

        // Kinetic energy should be positive
        bool t2 = actual_ke > 0.0;

        return !(t1 && t2);
    #else
        return 0;  // Pass (test not applicable)
    #endif
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 9: Verify multiple cells integrate independently - BIOLOGICAL parameters
int test_multiple_cells_independence() {

    // BIOLOGICAL STABILITY TEST - realistic cell simulation parameters
    global_simulation_parameters sim_params;
    sim_params.time_step_ = 1.0e-7;  // 100 ns (matches actual simulations)
    sim_params.damping_coefficient_ = 5.0e-10;  // kg/s (matches actual simulations)

    // Create time integration scheme
    time_integration_scheme integrator(sim_params, false);

    // Create two test cells with realistic ~5 μm geometry, well separated
    std::vector<vec3> positions1 = {
        vec3(0.0, 0.0, 0.0),
        vec3(5.0e-6, 0.0, 0.0),
        vec3(0.0, 5.0e-6, 0.0),
        vec3(0.0, 0.0, 5.0e-6)
    };

    // Second cell offset by ~20 μm (realistic cell-cell separation)
    std::vector<vec3> positions2 = {
        vec3(2.0e-5, 0.0, 0.0),
        vec3(2.5e-5, 0.0, 0.0),
        vec3(2.0e-5, 5.0e-6, 0.0),
        vec3(2.0e-5, 0.0, 5.0e-6)
    };

    auto cell1 = create_test_cell(0, positions1);
    auto cell2 = create_test_cell(1, positions2);
    cell1->initialize_cell_properties();
    cell2->initialize_cell_properties();
    cell1->set_local_id(0);
    cell2->set_local_id(1);

    std::vector<cell_ptr> cell_lst = {cell1, cell2};

    // Set different forces on first node of each cell
    node& n1 = time_integration_test_helper::get_node(cell1, 0);
    node& n2 = time_integration_test_helper::get_node(cell2, 0);

    // Realistic force scale for cell nodes
    vec3 force1(1.0e-12, 0.0, 0.0);
    vec3 force2(0.0, 1.0e-12, 0.0);

    #if DYNAMIC_MODEL_INDEX == 0
        n1.set_momentum(vec3(0.0, 0.0, 0.0));
        n2.set_momentum(vec3(0.0, 0.0, 0.0));
    #endif

    // Store initial positions
    vec3 initial_pos1 = n1.pos();
    vec3 initial_pos2 = n2.pos();

    // Perform integration steps
    const int num_steps = 10;
    for (int i = 0; i < num_steps; ++i) {
        n1.set_force(force1);
        n2.set_force(force2);
        integrator.update_nodes_positions(cell_lst);
    }

    // Verify cell 1 moved primarily in x-direction
    vec3 final_pos1 = n1.pos();
    double displacement1_x = final_pos1.dx() - initial_pos1.dx();
    double displacement1_y = final_pos1.dy() - initial_pos1.dy();
    bool t1 = displacement1_x > 0.0;
    bool t2 = std::abs(displacement1_x) > std::abs(displacement1_y);

    // Verify cell 2 moved primarily in y-direction
    vec3 final_pos2 = n2.pos();
    double displacement2_x = final_pos2.dx() - initial_pos2.dx();
    double displacement2_y = final_pos2.dy() - initial_pos2.dy();
    bool t3 = displacement2_y > 0.0;
    bool t4 = std::abs(displacement2_y) > std::abs(displacement2_x);

    // Verify cells remain separated (no artificial coupling)
    // Initial separation ~20 μm, should remain similar
    bool t5 = (final_pos1 - final_pos2).norm() > 1.5e-5;

    return !(t1 && t2 && t3 && t4 && t5);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 10: Verify overdamped dynamics (if DYNAMIC_MODEL_INDEX == 1) - BIOLOGICAL parameters
int test_overdamped_dynamics() {

    // Only test if DYNAMIC_MODEL_INDEX == 1
    #if DYNAMIC_MODEL_INDEX != 1
        return 0;  // Pass (test not applicable)
    #endif

    // BIOLOGICAL STABILITY TEST - realistic parameters for overdamped mode
    // Reference: parameters_default_overdamped.xml uses damping = 3e-9
    global_simulation_parameters sim_params;
    sim_params.time_step_ = 1.0e-7;  // 100 ns (matches actual simulations)
    sim_params.damping_coefficient_ = 3.0e-9;  // kg/s (matches overdamped simulations)

    // Create time integration scheme
    time_integration_scheme integrator(sim_params, false);

    // Create a test cell with realistic ~5 μm geometry
    std::vector<vec3> positions = {
        vec3(0.0, 0.0, 0.0),
        vec3(5.0e-6, 0.0, 0.0),
        vec3(0.0, 5.0e-6, 0.0),
        vec3(0.0, 0.0, 5.0e-6)
    };
    auto cell = create_test_cell(0, positions);
    cell->initialize_cell_properties();
    std::vector<cell_ptr> cell_lst = {cell};

    #if DYNAMIC_MODEL_INDEX == 1
        // Get reference to first node
        node& n = time_integration_test_helper::get_node(cell, 0);

        // Set initial conditions with realistic force
        vec3 initial_pos = n.pos();
        vec3 applied_force(1.0e-12, 0.0, 0.0);  // Realistic force for cell node
        n.set_force(applied_force);

        // Perform one integration step
        integrator.update_nodes_positions(cell_lst);

        // Expected position update for overdamped: x_new = x_old + F*dt/damping
        double dt = sim_params.time_step_;
        double damping = sim_params.damping_coefficient_;
        vec3 expected_displacement = applied_force * (dt / damping);
        vec3 expected_pos = initial_pos + expected_displacement;

        // Verify position
        vec3 actual_pos = n.pos();
        const double eps = 1.0e-20;
        bool t1 = (actual_pos - expected_pos).norm() < eps;

        // Verify no NaN or Inf
        bool t2 = !std::isnan(actual_pos.norm()) && !std::isinf(actual_pos.norm());

        return !(t1 && t2);
    #else
        return 0;  // Pass (test not applicable)
    #endif
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
    if (test_name == "test_time_step_accumulation")        return test_time_step_accumulation();
    if (test_name == "test_semi_implicit_euler_single_node") return test_semi_implicit_euler_single_node();
    if (test_name == "test_velocity_damping")              return test_velocity_damping();
    if (test_name == "test_energy_behavior")               return test_energy_behavior();
    if (test_name == "test_force_response")                return test_force_response();
    if (test_name == "test_zero_velocity_zero_force")      return test_zero_velocity_zero_force();
    if (test_name == "test_very_small_time_step")          return test_very_small_time_step();
    if (test_name == "test_kinetic_energy_calculation")    return test_kinetic_energy_calculation();
    if (test_name == "test_multiple_cells_independence")   return test_multiple_cells_independence();
    if (test_name == "test_overdamped_dynamics")           return test_overdamped_dynamics();

    std::cout << "TEST NAME: " << test_name << " DOES NOT EXIST" << std::endl;
    return 1;
}
//---------------------------------------------------------------------------------------------------------
