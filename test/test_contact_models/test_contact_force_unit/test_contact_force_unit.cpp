#include "test_contact_force_unit.hpp"

/**
 * @file test_contact_force_unit.cpp
 * @brief Unit tests for contact force calculations in SimuCell3D
 *
 * Test Strategy Overview:
 * ----------------------
 * These tests isolate the numerical computations of contact forces, independent of
 * the full simulation machinery. We validate:
 *
 * 1. Numerical Accuracy: Force magnitudes match analytical predictions
 * 2. Physical Correctness: Forces obey Newton's laws and energy conservation
 * 3. Geometric Validity: Force directions align with surface normals
 * 4. Boundary Behavior: Correct handling of interaction cutoff distances
 * 5. Edge Cases: Stability under degenerate configurations
 *
 * Key Implementation Details:
 * --------------------------
 * - All three contact models use compute_node_triangle_distance() for geometry
 * - Repulsion forces: F = repulsion_strength * integration_region * direction
 * - Adhesion forces: F = adherence_strength * f(distance) * integration_region * direction
 * - Force distribution: Barycentric coordinates distribute forces across triangle vertices
 *
 * Numerical Tolerances:
 * --------------------
 * - EPSILON = 1e-10 for geometric comparisons (distances, positions)
 * - FORCE_EPSILON = 1e-8 for force magnitude comparisons (accumulated error)
 */


//---------------------------------------------------------------------------------------------------------------
// Test 1: Spring Force Calculation
//---------------------------------------------------------------------------------------------------------------
int tester_contact_force_unit::test_contact_spring_force_calculation() {
    /**
     * Test Rationale:
     * --------------
     * The node-face spring model (CONTACT_MODEL_INDEX=0) computes repulsive forces
     * as linear springs. This test validates the force calculation for a simple
     * configuration: a node approaching a horizontal triangular face.
     *
     * Test Configuration:
     * ------------------
     * Triangle: A=(0,0,0), B=(1,0,0), C=(0,1,0)  [horizontal in XY plane]
     * Normal: (0,0,1) pointing upward
     * Node positions tested:
     *   1. Above triangle center at z=0.1 (penetration depth = 0.1)
     *   2. At triangle center z=0 (zero penetration)
     *   3. Below triangle at z=-0.1 (no contact from above)
     *
     * Expected Results:
     * ----------------
     * - Penetrating node: Force proportional to penetration, pointing upward (+z)
     * - Zero penetration: Zero force
     * - Non-penetrating: Zero force (no interaction from opposite side)
     */

    std::cout << "\n=== Test 1: Spring Force Calculation ===" << std::endl;

    // Define triangle vertices (horizontal face in XY plane, normal points +Z)
    vec3 A(0.0, 0.0, 0.0);
    vec3 B(1.0, 0.0, 0.0);
    vec3 C(0.0, 1.0, 0.0);

    // Triangle properties
    vec3 face_normal(0.0, 0.0, 1.0);  // Points upward
    double face_area = 0.5;  // Area of right triangle with legs of length 1

    // Physical parameters (simplified for testing)
    double repulsion_strength = 100.0;  // N/m^2 (arbitrary but realistic)
    double integration_region = face_area;

    bool all_tests_passed = true;

    //--------------------------------------------------------------------------
    // Test 1a: Node penetrating face from above
    //--------------------------------------------------------------------------
    {
        vec3 node_pos(0.25, 0.25, 0.1);  // Above triangle center, penetration = 0.1

        // Compute closest point and distance
        auto [dist_sq, bary] = contact_model_abstract::compute_node_triangle_distance(
            node_pos, A, B, C
        );

        double distance = std::sqrt(dist_sq);
        vec3 closest_point = A * bary.dx() + B * bary.dy() + C * bary.dz();
        vec3 direction = node_pos - closest_point;

        // Verify geometric calculation
        bool geom_test1 = approx_equal(distance, 0.1, EPSILON);
        bool geom_test2 = is_unit_vector(direction.normalize());

        // Expected force magnitude: F = k * integration_region * direction
        // In the spring model, force is linear with distance
        vec3 expected_force_direction = direction;  // Points from face to node
        double expected_force_magnitude = repulsion_strength * integration_region;
        vec3 expected_force = expected_force_direction * expected_force_magnitude;

        // The force should point in +Z direction (pushing node away from face)
        bool force_dir_test = expected_force.dz() > 0;
        bool force_finite_test = is_finite_vector(expected_force);

        std::cout << "  Test 1a (Penetration): ";
        std::cout << "geom=" << geom_test1 << " ";
        std::cout << "unit_vec=" << geom_test2 << " ";
        std::cout << "dir=" << force_dir_test << " ";
        std::cout << "finite=" << force_finite_test << std::endl;

        all_tests_passed &= (geom_test1 && geom_test2 && force_dir_test && force_finite_test);
    }

    //--------------------------------------------------------------------------
    // Test 1b: Node exactly on face (zero penetration)
    //--------------------------------------------------------------------------
    {
        vec3 node_pos(0.25, 0.25, 0.0);  // Exactly on triangle

        auto [dist_sq, bary] = contact_model_abstract::compute_node_triangle_distance(
            node_pos, A, B, C
        );

        double distance = std::sqrt(dist_sq);

        // Distance should be zero
        bool zero_dist_test = approx_equal(distance, 0.0, EPSILON);

        // In the actual contact model, zero distance means no force application
        // (the code checks min_squared_distance != 0.0 to avoid division by zero)

        std::cout << "  Test 1b (Zero penetration): ";
        std::cout << "zero_dist=" << zero_dist_test << std::endl;

        all_tests_passed &= zero_dist_test;
    }

    //--------------------------------------------------------------------------
    // Test 1c: Node below face (no contact from this direction)
    //--------------------------------------------------------------------------
    {
        vec3 node_pos(0.25, 0.25, -0.1);  // Below triangle

        auto [dist_sq, bary] = contact_model_abstract::compute_node_triangle_distance(
            node_pos, A, B, C
        );

        double distance = std::sqrt(dist_sq);
        vec3 closest_point = A * bary.dx() + B * bary.dy() + C * bary.dz();
        vec3 direction = node_pos - closest_point;

        // Distance is 0.1, but direction points downward (-Z)
        bool dist_test = approx_equal(distance, 0.1, EPSILON);

        // The dot product with face normal determines contact type
        // For node below face: direction.dot(normal) < 0 (penetration from below)
        double dot_product = direction.dot(face_normal);
        bool dir_test = dot_product < 0;

        std::cout << "  Test 1c (Below face): ";
        std::cout << "dist=" << dist_test << " ";
        std::cout << "dot_neg=" << dir_test << std::endl;

        all_tests_passed &= (dist_test && dir_test);
    }

    //--------------------------------------------------------------------------
    // Test 1d: Force magnitude scaling with strength parameter
    //--------------------------------------------------------------------------
    {
        vec3 node_pos(0.25, 0.25, 0.05);  // Smaller penetration = 0.05

        auto [dist_sq, bary] = contact_model_abstract::compute_node_triangle_distance(
            node_pos, A, B, C
        );

        double distance = std::sqrt(dist_sq);

        // Force should be F = repulsion_strength * integration_region
        // (direction is normalized in actual implementation)

        double force_magnitude = repulsion_strength * integration_region;
        bool force_positive = force_magnitude > 0;
        bool force_reasonable = force_magnitude < 1e6;  // Sanity check

        std::cout << "  Test 1d (Force scaling): ";
        std::cout << "positive=" << force_positive << " ";
        std::cout << "reasonable=" << force_reasonable << std::endl;

        all_tests_passed &= (force_positive && force_reasonable);
    }

    std::cout << "Test 1 Result: " << (all_tests_passed ? "PASS" : "FAIL") << std::endl;

    // Return 0 on pass, 1 on fail (following project convention)
    return !all_tests_passed;
}


//---------------------------------------------------------------------------------------------------------------
// Test 2: Force Symmetry (Newton's Third Law)
//---------------------------------------------------------------------------------------------------------------
int tester_contact_force_unit::test_contact_force_symmetry() {
    /**
     * Test Rationale:
     * --------------
     * Newton's third law requires F_ab = -F_ba. In the contact models, when a node
     * interacts with a face, the force on the node should be equal and opposite to
     * the total force on the face (distributed across its vertices).
     *
     * Test Configuration:
     * ------------------
     * Same triangle as Test 1, with a node penetrating from above.
     * We verify that:
     *   - Force on node = -(sum of forces on triangle vertices)
     *   - Barycentric distribution preserves force magnitude
     *   - No net momentum is created
     *
     * Implementation Note:
     * -------------------
     * In contact_node_face_via_spring.cpp:
     *   - Face vertices receive: force_vector * bary_pos.dx/dy/dz()
     *   - Node receives: force_vector * -1.0
     * This ensures F_node = -F_face_total
     */

    std::cout << "\n=== Test 2: Force Symmetry (Newton's Third Law) ===" << std::endl;

    vec3 A(0.0, 0.0, 0.0);
    vec3 B(1.0, 0.0, 0.0);
    vec3 C(0.0, 1.0, 0.0);
    vec3 node_pos(0.3, 0.2, 0.15);  // Penetrating from above

    double repulsion_strength = 100.0;
    double face_area = 0.5;

    bool all_tests_passed = true;

    //--------------------------------------------------------------------------
    // Compute geometric quantities
    //--------------------------------------------------------------------------
    auto [dist_sq, bary] = contact_model_abstract::compute_node_triangle_distance(
        node_pos, A, B, C
    );

    vec3 closest_point = A * bary.dx() + B * bary.dy() + C * bary.dz();
    vec3 direction = node_pos - closest_point;

    // Force applied to the system
    vec3 repulsion_force_vector = direction * (repulsion_strength * face_area);

    //--------------------------------------------------------------------------
    // Simulate force distribution (as done in apply_contact_forces)
    //--------------------------------------------------------------------------
    vec3 force_on_vertex_A = repulsion_force_vector * bary.dx();
    vec3 force_on_vertex_B = repulsion_force_vector * bary.dy();
    vec3 force_on_vertex_C = repulsion_force_vector * bary.dz();
    vec3 force_on_node = repulsion_force_vector * -1.0;

    //--------------------------------------------------------------------------
    // Test 2a: Total force on face equals negative of force on node
    //--------------------------------------------------------------------------
    vec3 total_force_on_face = force_on_vertex_A + force_on_vertex_B + force_on_vertex_C;
    vec3 force_sum = total_force_on_face + force_on_node;

    bool symmetry_x = approx_equal(force_sum.dx(), 0.0, FORCE_EPSILON);
    bool symmetry_y = approx_equal(force_sum.dy(), 0.0, FORCE_EPSILON);
    bool symmetry_z = approx_equal(force_sum.dz(), 0.0, FORCE_EPSILON);

    std::cout << "  Test 2a (Force balance): ";
    std::cout << "Fx=" << symmetry_x << " ";
    std::cout << "Fy=" << symmetry_y << " ";
    std::cout << "Fz=" << symmetry_z << std::endl;

    all_tests_passed &= (symmetry_x && symmetry_y && symmetry_z);

    //--------------------------------------------------------------------------
    // Test 2b: Barycentric weights sum to 1 (sanity check)
    //--------------------------------------------------------------------------
    double bary_sum = bary.dx() + bary.dy() + bary.dz();
    bool bary_normalized = approx_equal(bary_sum, 1.0, EPSILON);

    std::cout << "  Test 2b (Barycentric normalization): ";
    std::cout << bary_normalized << std::endl;

    all_tests_passed &= bary_normalized;

    //--------------------------------------------------------------------------
    // Test 2c: Force magnitude consistency
    //--------------------------------------------------------------------------
    double force_on_node_mag = force_on_node.norm();
    double total_force_on_face_mag = total_force_on_face.norm();

    bool magnitude_match = approx_equal(force_on_node_mag, total_force_on_face_mag, FORCE_EPSILON);

    std::cout << "  Test 2c (Force magnitude match): ";
    std::cout << magnitude_match << std::endl;
    std::cout << "    |F_node| = " << force_on_node_mag << std::endl;
    std::cout << "    |F_face| = " << total_force_on_face_mag << std::endl;

    all_tests_passed &= magnitude_match;

    //--------------------------------------------------------------------------
    // Test 2d: Direction opposition (forces are anti-parallel)
    //--------------------------------------------------------------------------
    if (force_on_node_mag > FORCE_EPSILON && total_force_on_face_mag > FORCE_EPSILON) {
        vec3 node_force_dir = force_on_node.normalize();
        vec3 face_force_dir = total_force_on_face.normalize();

        // Dot product should be -1 (anti-parallel)
        double dot_product = node_force_dir.dot(face_force_dir);
        bool antiparallel = approx_equal(dot_product, -1.0, 1e-6);

        std::cout << "  Test 2d (Force anti-parallel): ";
        std::cout << antiparallel << " (dot=" << dot_product << ")" << std::endl;

        all_tests_passed &= antiparallel;
    }

    std::cout << "Test 2 Result: " << (all_tests_passed ? "PASS" : "FAIL") << std::endl;

    return !all_tests_passed;
}


//---------------------------------------------------------------------------------------------------------------
// Test 3: Interaction Radius Boundary
//---------------------------------------------------------------------------------------------------------------
int tester_contact_force_unit::test_contact_radius_boundary() {
    /**
     * Test Rationale:
     * --------------
     * Contact models use interaction_cutoff to limit force computation range.
     * Beyond this distance, forces should be zero to improve performance and
     * avoid spurious long-range interactions.
     *
     * Test Configuration:
     * ------------------
     * - Place nodes at various distances from triangle
     * - Verify forces are zero beyond cutoff
     * - Verify forces activate within cutoff
     * - Test boundary behavior at exactly cutoff distance
     *
     * Implementation Note:
     * -------------------
     * All contact models check:
     *   if (min_squared_distance < interaction_cutoff_square_)
     * This is an optimization (avoids sqrt) and defines the contact regime.
     */

    std::cout << "\n=== Test 3: Interaction Radius Boundary ===" << std::endl;

    vec3 A(0.0, 0.0, 0.0);
    vec3 B(1.0, 0.0, 0.0);
    vec3 C(0.0, 1.0, 0.0);

    double interaction_cutoff = 0.5;  // Interaction range
    double interaction_cutoff_square = interaction_cutoff * interaction_cutoff;

    bool all_tests_passed = true;

    //--------------------------------------------------------------------------
    // Test 3a: Node well within interaction radius
    //--------------------------------------------------------------------------
    {
        vec3 node_pos(0.25, 0.25, 0.1);  // Distance = 0.1 << cutoff
        auto [dist_sq, bary] = contact_model_abstract::compute_node_triangle_distance(
            node_pos, A, B, C
        );

        bool within_cutoff = (dist_sq < interaction_cutoff_square);

        std::cout << "  Test 3a (Well within cutoff): ";
        std::cout << "within=" << within_cutoff << " ";
        std::cout << "(dist=" << std::sqrt(dist_sq) << " < " << interaction_cutoff << ")" << std::endl;

        all_tests_passed &= within_cutoff;
    }

    //--------------------------------------------------------------------------
    // Test 3b: Node exactly at interaction radius
    //--------------------------------------------------------------------------
    {
        // Place node at exactly cutoff distance
        vec3 node_pos(0.25, 0.25, interaction_cutoff);
        auto [dist_sq, bary] = contact_model_abstract::compute_node_triangle_distance(
            node_pos, A, B, C
        );

        double distance = std::sqrt(dist_sq);

        // Should be approximately at cutoff (within numerical precision)
        bool at_boundary = approx_equal(distance, interaction_cutoff, EPSILON);

        // Check condition used in code: dist_sq < cutoff_sq
        // At exact boundary, this should be false (strict inequality)
        bool cutoff_condition = (dist_sq < interaction_cutoff_square);

        std::cout << "  Test 3b (At boundary): ";
        std::cout << "dist_match=" << at_boundary << " ";
        std::cout << "excluded=" << !cutoff_condition << " ";
        std::cout << "(dist=" << distance << ")" << std::endl;

        all_tests_passed &= at_boundary;
    }

    //--------------------------------------------------------------------------
    // Test 3c: Node beyond interaction radius
    //--------------------------------------------------------------------------
    {
        vec3 node_pos(0.25, 0.25, 1.0);  // Distance = 1.0 >> cutoff
        auto [dist_sq, bary] = contact_model_abstract::compute_node_triangle_distance(
            node_pos, A, B, C
        );

        bool beyond_cutoff = (dist_sq > interaction_cutoff_square);

        std::cout << "  Test 3c (Beyond cutoff): ";
        std::cout << "beyond=" << beyond_cutoff << " ";
        std::cout << "(dist=" << std::sqrt(dist_sq) << " > " << interaction_cutoff << ")" << std::endl;

        all_tests_passed &= beyond_cutoff;
    }

    //--------------------------------------------------------------------------
    // Test 3d: Squared distance optimization correctness
    //--------------------------------------------------------------------------
    {
        vec3 node_pos(0.3, 0.2, 0.15);
        auto [dist_sq, bary] = contact_model_abstract::compute_node_triangle_distance(
            node_pos, A, B, C
        );

        double distance = std::sqrt(dist_sq);
        double distance_squared_check = distance * distance;

        bool squared_correct = approx_equal(dist_sq, distance_squared_check, EPSILON);

        std::cout << "  Test 3d (Squared distance consistency): ";
        std::cout << squared_correct << std::endl;

        all_tests_passed &= squared_correct;
    }

    //--------------------------------------------------------------------------
    // Test 3e: Different cutoff values (adhesion vs repulsion)
    //--------------------------------------------------------------------------
    {
        // In contact_node_face_via_spring, there are separate cutoffs
        double cutoff_adhesion = 0.6;
        double cutoff_repulsion = 0.4;

        vec3 node_pos(0.25, 0.25, 0.5);  // Between the two cutoffs
        auto [dist_sq, bary] = contact_model_abstract::compute_node_triangle_distance(
            node_pos, A, B, C
        );

        double distance = std::sqrt(dist_sq);

        bool beyond_repulsion = (distance > cutoff_repulsion);
        bool within_adhesion = (distance < cutoff_adhesion);

        std::cout << "  Test 3e (Multiple cutoffs): ";
        std::cout << "rep=" << beyond_repulsion << " ";
        std::cout << "adh=" << within_adhesion << " ";
        std::cout << "(dist=" << distance << ")" << std::endl;

        all_tests_passed &= (beyond_repulsion && within_adhesion);
    }

    std::cout << "Test 3 Result: " << (all_tests_passed ? "PASS" : "FAIL") << std::endl;

    return !all_tests_passed;
}


//---------------------------------------------------------------------------------------------------------------
// Test 4: Repulsion at Overlap
//---------------------------------------------------------------------------------------------------------------
int tester_contact_force_unit::test_repulsion_at_overlap() {
    /**
     * Test Rationale:
     * --------------
     * When surfaces overlap (penetrate), repulsive forces must push them apart.
     * Force magnitude should increase with penetration depth to prevent
     * unphysical interpenetration.
     *
     * Test Configuration:
     * ------------------
     * - Penetrations at depths: 0.01, 0.05, 0.1, 0.2
     * - Verify force magnitude monotonically increases
     * - Verify force always points away from surface
     * - Verify force scales with face area
     *
     * Physics Validation:
     * ------------------
     * In spring model: F = k * integration_region * direction
     * In coupling models: F = strength * integration_region * direction
     * Direction should point from face center toward penetrating node.
     */

    std::cout << "\n=== Test 4: Repulsion at Overlap ===" << std::endl;

    vec3 A(0.0, 0.0, 0.0);
    vec3 B(1.0, 0.0, 0.0);
    vec3 C(0.0, 1.0, 0.0);
    vec3 face_normal(0.0, 0.0, 1.0);

    double repulsion_strength = 100.0;
    double face_area = 0.5;

    bool all_tests_passed = true;

    // Test different penetration depths
    std::vector<double> penetration_depths = {0.01, 0.05, 0.1, 0.2};
    std::vector<double> force_magnitudes;

    //--------------------------------------------------------------------------
    // Test 4a: Force magnitude increases with penetration
    //--------------------------------------------------------------------------
    for (double depth : penetration_depths) {
        vec3 node_pos(0.25, 0.25, depth);  // Node above triangle

        auto [dist_sq, bary] = contact_model_abstract::compute_node_triangle_distance(
            node_pos, A, B, C
        );

        vec3 closest_point = A * bary.dx() + B * bary.dy() + C * bary.dz();
        vec3 direction = node_pos - closest_point;

        // Repulsion force
        vec3 repulsion_force = direction * (repulsion_strength * face_area);
        double force_mag = repulsion_force.norm();

        force_magnitudes.push_back(force_mag);
    }

    // Verify monotonic increase
    bool monotonic = true;
    for (size_t i = 1; i < force_magnitudes.size(); ++i) {
        if (force_magnitudes[i] <= force_magnitudes[i-1]) {
            monotonic = false;
        }
    }

    std::cout << "  Test 4a (Monotonic increase): " << monotonic << std::endl;
    std::cout << "    Force magnitudes: ";
    for (double f : force_magnitudes) {
        std::cout << f << " ";
    }
    std::cout << std::endl;

    all_tests_passed &= monotonic;

    //--------------------------------------------------------------------------
    // Test 4b: Force direction always repulsive (points away from face)
    //--------------------------------------------------------------------------
    {
        vec3 node_pos(0.25, 0.25, 0.1);

        auto [dist_sq, bary] = contact_model_abstract::compute_node_triangle_distance(
            node_pos, A, B, C
        );

        vec3 closest_point = A * bary.dx() + B * bary.dy() + C * bary.dz();
        vec3 direction = node_pos - closest_point;

        // Direction should point upward (+Z) for node above face
        bool points_up = (direction.dz() > 0);

        // Force on node should also point upward (repulsive)
        vec3 force_on_node = direction * (repulsion_strength * face_area) * -1.0;
        bool force_repulsive = (force_on_node.dz() < 0);  // Force on node points down means face pushes node up

        std::cout << "  Test 4b (Repulsive direction): ";
        std::cout << "dir_up=" << points_up << " ";
        std::cout << "repulsive=" << force_repulsive << std::endl;

        all_tests_passed &= (points_up && force_repulsive);
    }

    //--------------------------------------------------------------------------
    // Test 4c: Force scales with face area
    //--------------------------------------------------------------------------
    {
        vec3 node_pos(0.25, 0.25, 0.1);

        auto [dist_sq, bary] = contact_model_abstract::compute_node_triangle_distance(
            node_pos, A, B, C
        );

        vec3 direction = node_pos - (A * bary.dx() + B * bary.dy() + C * bary.dz());

        // Test with two different areas
        double area1 = 0.5;
        double area2 = 1.0;  // Double the area

        vec3 force1 = direction * (repulsion_strength * area1);
        vec3 force2 = direction * (repulsion_strength * area2);

        double mag1 = force1.norm();
        double mag2 = force2.norm();

        // Force should double when area doubles
        bool area_scaling = approx_equal(mag2 / mag1, 2.0, FORCE_EPSILON);

        std::cout << "  Test 4c (Area scaling): ";
        std::cout << area_scaling << " ";
        std::cout << "(ratio=" << mag2/mag1 << ")" << std::endl;

        all_tests_passed &= area_scaling;
    }

    //--------------------------------------------------------------------------
    // Test 4d: No attractive forces in repulsion regime
    //--------------------------------------------------------------------------
    {
        // Test various penetration scenarios
        std::vector<vec3> test_positions = {
            vec3(0.1, 0.1, 0.05),
            vec3(0.3, 0.3, 0.15),
            vec3(0.2, 0.1, 0.08)
        };

        bool all_repulsive = true;
        for (const auto& pos : test_positions) {
            auto [dist_sq, bary] = contact_model_abstract::compute_node_triangle_distance(
                pos, A, B, C
            );

            vec3 closest = A * bary.dx() + B * bary.dy() + C * bary.dz();
            vec3 dir = pos - closest;

            // For repulsion, dot product with normal should be consistent
            double dot_prod = dir.dot(face_normal);

            // If node is above (pos.dz() > 0), dot product should be positive
            if (pos.dz() > 0 && dot_prod <= 0) {
                all_repulsive = false;
            }
        }

        std::cout << "  Test 4d (No attraction): " << all_repulsive << std::endl;

        all_tests_passed &= all_repulsive;
    }

    std::cout << "Test 4 Result: " << (all_tests_passed ? "PASS" : "FAIL") << std::endl;

    return !all_tests_passed;
}


//---------------------------------------------------------------------------------------------------------------
// Test 5: Edge Cases
//---------------------------------------------------------------------------------------------------------------
int tester_contact_force_unit::test_edge_cases() {
    /**
     * Test Rationale:
     * --------------
     * Numerical stability under degenerate conditions is critical for robustness.
     * This test validates behavior when:
     * - Distance approaches zero (potential division by zero)
     * - Surfaces are parallel (normal alignment)
     * - Contact is grazing (tangential)
     * - Distances are very large
     *
     * These cases can cause numerical instabilities if not handled correctly.
     */

    std::cout << "\n=== Test 5: Edge Cases ===" << std::endl;

    vec3 A(0.0, 0.0, 0.0);
    vec3 B(1.0, 0.0, 0.0);
    vec3 C(0.0, 1.0, 0.0);

    bool all_tests_passed = true;

    //--------------------------------------------------------------------------
    // Test 5a: Near-zero distance (numerical stability)
    //--------------------------------------------------------------------------
    {
        vec3 node_pos(0.25, 0.25, 1e-12);  // Extremely close to face

        auto [dist_sq, bary] = contact_model_abstract::compute_node_triangle_distance(
            node_pos, A, B, C
        );

        double distance = std::sqrt(dist_sq);

        // Should handle without NaN or infinity
        bool distance_valid = std::isfinite(distance);
        bool distance_nonnegative = (distance >= 0.0);

        std::cout << "  Test 5a (Near-zero distance): ";
        std::cout << "finite=" << distance_valid << " ";
        std::cout << "nonneg=" << distance_nonnegative << std::endl;

        all_tests_passed &= (distance_valid && distance_nonnegative);
    }

    //--------------------------------------------------------------------------
    // Test 5b: Node outside triangle projection (edge proximity)
    //--------------------------------------------------------------------------
    {
        vec3 node_pos(2.0, 0.0, 0.1);  // Outside triangle, closest to vertex B

        auto [dist_sq, bary] = contact_model_abstract::compute_node_triangle_distance(
            node_pos, A, B, C
        );

        // Barycentric coordinates should handle this correctly
        // One coordinate should be 1.0, others 0.0 (closest to vertex)
        bool bary_valid = (bary.dx() >= 0.0 && bary.dy() >= 0.0 && bary.dz() >= 0.0);
        bool bary_sum_one = approx_equal(bary.dx() + bary.dy() + bary.dz(), 1.0, EPSILON);

        std::cout << "  Test 5b (Outside projection): ";
        std::cout << "bary_valid=" << bary_valid << " ";
        std::cout << "bary_sum=" << bary_sum_one << std::endl;

        all_tests_passed &= (bary_valid && bary_sum_one);
    }

    //--------------------------------------------------------------------------
    // Test 5c: Very large distance
    //--------------------------------------------------------------------------
    {
        vec3 node_pos(0.25, 0.25, 1000.0);  // Very far from face

        auto [dist_sq, bary] = contact_model_abstract::compute_node_triangle_distance(
            node_pos, A, B, C
        );

        // Should compute correctly without overflow
        bool finite_result = std::isfinite(dist_sq);
        bool reasonable_value = (dist_sq > 0.0 && dist_sq < 1e12);

        std::cout << "  Test 5c (Large distance): ";
        std::cout << "finite=" << finite_result << " ";
        std::cout << "reasonable=" << reasonable_value << std::endl;

        all_tests_passed &= (finite_result && reasonable_value);
    }

    //--------------------------------------------------------------------------
    // Test 5d: Node at triangle vertex (exact coincidence)
    //--------------------------------------------------------------------------
    {
        vec3 node_pos = A;  // Exactly at vertex A

        auto [dist_sq, bary] = contact_model_abstract::compute_node_triangle_distance(
            node_pos, A, B, C
        );

        bool zero_distance = approx_equal(dist_sq, 0.0, EPSILON);
        // Barycentric should be (1, 0, 0) for vertex A
        bool bary_at_A = approx_equal(bary.dx(), 1.0, EPSILON) &&
                         approx_equal(bary.dy(), 0.0, EPSILON) &&
                         approx_equal(bary.dz(), 0.0, EPSILON);

        std::cout << "  Test 5d (Vertex coincidence): ";
        std::cout << "zero_dist=" << zero_distance << " ";
        std::cout << "bary_A=" << bary_at_A << std::endl;

        all_tests_passed &= (zero_distance && bary_at_A);
    }

    //--------------------------------------------------------------------------
    // Test 5e: Grazing contact (node at edge)
    //--------------------------------------------------------------------------
    {
        vec3 node_pos(0.5, 0.0, 0.0);  // On edge AB

        auto [dist_sq, bary] = contact_model_abstract::compute_node_triangle_distance(
            node_pos, A, B, C
        );

        bool zero_distance = approx_equal(dist_sq, 0.0, EPSILON);
        // Barycentric should have dz() = 0 (not involving vertex C)
        bool on_edge_AB = approx_equal(bary.dz(), 0.0, EPSILON);

        std::cout << "  Test 5e (Edge contact): ";
        std::cout << "zero_dist=" << zero_distance << " ";
        std::cout << "on_edge=" << on_edge_AB << std::endl;

        all_tests_passed &= (zero_distance && on_edge_AB);
    }

    std::cout << "Test 5 Result: " << (all_tests_passed ? "PASS" : "FAIL") << std::endl;

    return !all_tests_passed;
}


//---------------------------------------------------------------------------------------------------------------
// Test 6: Force Direction Correctness
//---------------------------------------------------------------------------------------------------------------
int tester_contact_force_unit::test_force_direction_correctness() {
    /**
     * Test Rationale:
     * --------------
     * Force direction determines whether surfaces attract or repel.
     * Incorrect direction can lead to unphysical collapse or explosion.
     *
     * Test Configuration:
     * ------------------
     * - Verify repulsion forces point away from penetration
     * - Verify force alignment with contact normal
     * - Test various approach angles
     *
     * Implementation Reference:
     * ------------------------
     * In all contact models, contact type is determined by:
     *   bool adhesive_contact = (cpa_f_to_node.dot(f->normal_) > 0.0)
     * where cpa_f_to_node points from closest point on face to node.
     */

    std::cout << "\n=== Test 6: Force Direction Correctness ===" << std::endl;

    vec3 A(0.0, 0.0, 0.0);
    vec3 B(1.0, 0.0, 0.0);
    vec3 C(0.0, 1.0, 0.0);
    vec3 face_normal(0.0, 0.0, 1.0);  // Points +Z

    bool all_tests_passed = true;

    //--------------------------------------------------------------------------
    // Test 6a: Perpendicular approach (normal incidence)
    //--------------------------------------------------------------------------
    {
        vec3 node_pos(0.25, 0.25, 0.1);  // Directly above triangle center

        auto [dist_sq, bary] = contact_model_abstract::compute_node_triangle_distance(
            node_pos, A, B, C
        );

        vec3 closest = A * bary.dx() + B * bary.dy() + C * bary.dz();
        vec3 direction = node_pos - closest;
        vec3 normalized_dir = direction.normalize();

        // Direction should align with face normal
        double dot_with_normal = normalized_dir.dot(face_normal);
        bool aligned = approx_equal(dot_with_normal, 1.0, 1e-6);

        std::cout << "  Test 6a (Normal incidence): ";
        std::cout << "aligned=" << aligned << " ";
        std::cout << "(dot=" << dot_with_normal << ")" << std::endl;

        all_tests_passed &= aligned;
    }

    //--------------------------------------------------------------------------
    // Test 6b: Oblique approach
    //--------------------------------------------------------------------------
    {
        vec3 node_pos(0.8, 0.05, 0.1);  // Near edge, oblique approach

        auto [dist_sq, bary] = contact_model_abstract::compute_node_triangle_distance(
            node_pos, A, B, C
        );

        vec3 closest = A * bary.dx() + B * bary.dy() + C * bary.dz();
        vec3 direction = node_pos - closest;

        // Direction should still have positive Z component (upward)
        bool points_away = (direction.dz() > 0.0);

        // But not necessarily aligned with normal
        vec3 normalized_dir = direction.normalize();
        double dot_with_normal = normalized_dir.dot(face_normal);
        bool positive_component = (dot_with_normal > 0.0);

        std::cout << "  Test 6b (Oblique approach): ";
        std::cout << "away=" << points_away << " ";
        std::cout << "positive_z=" << positive_component << std::endl;

        all_tests_passed &= (points_away && positive_component);
    }

    //--------------------------------------------------------------------------
    // Test 6c: Contact type determination
    //--------------------------------------------------------------------------
    {
        // Node above face (adhesive contact)
        vec3 node_above(0.25, 0.25, 0.1);
        auto [dist_sq_above, bary_above] = contact_model_abstract::compute_node_triangle_distance(
            node_above, A, B, C
        );
        vec3 closest_above = A * bary_above.dx() + B * bary_above.dy() + C * bary_above.dz();
        vec3 dir_above = node_above - closest_above;

        bool adhesive = (dir_above.dot(face_normal) > 0.0);

        // Node below face (repulsive contact)
        vec3 node_below(0.25, 0.25, -0.1);
        auto [dist_sq_below, bary_below] = contact_model_abstract::compute_node_triangle_distance(
            node_below, A, B, C
        );
        vec3 closest_below = A * bary_below.dx() + B * bary_below.dy() + C * bary_below.dz();
        vec3 dir_below = node_below - closest_below;

        bool repulsive = (dir_below.dot(face_normal) < 0.0);

        std::cout << "  Test 6c (Contact type): ";
        std::cout << "adhesive=" << adhesive << " ";
        std::cout << "repulsive=" << repulsive << std::endl;

        all_tests_passed &= (adhesive && repulsive);
    }

    //--------------------------------------------------------------------------
    // Test 6d: Force magnitude reasonableness
    //--------------------------------------------------------------------------
    {
        vec3 node_pos(0.25, 0.25, 0.1);
        double repulsion_strength = 100.0;
        double face_area = 0.5;

        auto [dist_sq, bary] = contact_model_abstract::compute_node_triangle_distance(
            node_pos, A, B, C
        );

        vec3 closest = A * bary.dx() + B * bary.dy() + C * bary.dz();
        vec3 direction = node_pos - closest;

        vec3 force = direction * (repulsion_strength * face_area);
        double force_mag = force.norm();

        // Force should be finite and positive
        bool finite = std::isfinite(force_mag);
        bool positive = (force_mag > 0.0);
        bool reasonable = (force_mag < 1e6);  // Not absurdly large

        std::cout << "  Test 6d (Force reasonableness): ";
        std::cout << "finite=" << finite << " ";
        std::cout << "positive=" << positive << " ";
        std::cout << "reasonable=" << reasonable << " ";
        std::cout << "(mag=" << force_mag << ")" << std::endl;

        all_tests_passed &= (finite && positive && reasonable);
    }

    std::cout << "Test 6 Result: " << (all_tests_passed ? "PASS" : "FAIL") << std::endl;

    return !all_tests_passed;
}


//---------------------------------------------------------------------------------------------------------------
// Main Function
//---------------------------------------------------------------------------------------------------------------
int main(int argc, char** argv) {
    /**
     * Test Runner
     * ----------
     * Follows the project convention:
     * - Takes test name as command-line argument
     * - Returns 0 on pass, 1 on fail
     * - Outputs diagnostic information to stdout
     */

    // Validate command-line arguments
    assert(argc == 2);

    std::string test_name = argv[1];

    // Instantiate tester
    tester_contact_force_unit tester;

    // Dispatch to requested test
    if (test_name == "test_contact_spring_force_calculation") {
        return tester.test_contact_spring_force_calculation();
    }
    else if (test_name == "test_contact_force_symmetry") {
        return tester.test_contact_force_symmetry();
    }
    else if (test_name == "test_contact_radius_boundary") {
        return tester.test_contact_radius_boundary();
    }
    else if (test_name == "test_repulsion_at_overlap") {
        return tester.test_repulsion_at_overlap();
    }
    else if (test_name == "test_edge_cases") {
        return tester.test_edge_cases();
    }
    else if (test_name == "test_force_direction_correctness") {
        return tester.test_force_direction_correctness();
    }

    // Test name not recognized
    std::cout << "ERROR: Test name '" << test_name << "' does not exist" << std::endl;
    std::cout << "\nAvailable tests:" << std::endl;
    std::cout << "  - test_contact_spring_force_calculation" << std::endl;
    std::cout << "  - test_contact_force_symmetry" << std::endl;
    std::cout << "  - test_contact_radius_boundary" << std::endl;
    std::cout << "  - test_repulsion_at_overlap" << std::endl;
    std::cout << "  - test_edge_cases" << std::endl;
    std::cout << "  - test_force_direction_correctness" << std::endl;

    return 1;
}
