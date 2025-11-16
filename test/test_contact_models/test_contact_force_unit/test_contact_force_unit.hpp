#ifndef DEF_TESTER_CONTACT_FORCE_UNIT
#define DEF_TESTER_CONTACT_FORCE_UNIT

#include <cassert>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>

#include "custom_structures.hpp"
#include "contact_model_abstract.hpp"
#include "vec3.hpp"

/**
 * @brief Unit test class for contact force calculations
 *
 * This test suite validates the numerical correctness of contact force computations
 * across all three contact models (node-face springs, node-node coupling, face-face coupling).
 * Tests focus on isolated force calculations, not integrated cell interactions.
 */
class tester_contact_force_unit {

    private:
        // Numerical tolerance for floating-point comparisons (geometric computations)
        static constexpr double EPSILON = 1e-10;
        static constexpr double FORCE_EPSILON = 1e-8;  // Slightly relaxed for force magnitudes

        /**
         * @brief Helper function to compare floating-point values with tolerance
         */
        bool approx_equal(double a, double b, double tolerance = EPSILON) const {
            return std::abs(a - b) < tolerance;
        }

        /**
         * @brief Helper function to compare vec3 objects with tolerance
         */
        bool approx_equal_vec3(const vec3& a, const vec3& b, double tolerance = EPSILON) const {
            return approx_equal(a.dx(), b.dx(), tolerance) &&
                   approx_equal(a.dy(), b.dy(), tolerance) &&
                   approx_equal(a.dz(), b.dz(), tolerance);
        }

        /**
         * @brief Verify a vector is normalized (unit length)
         */
        bool is_unit_vector(const vec3& v) const {
            return approx_equal(v.squared_norm(), 1.0);
        }

        /**
         * @brief Verify a vector has finite components (not NaN or infinite)
         */
        bool is_finite_vector(const vec3& v) const {
            return std::isfinite(v.dx()) && std::isfinite(v.dy()) && std::isfinite(v.dz());
        }

    public:
        /**
         * @brief Test 1: Spring force calculation in node-face contact model
         *
         * Validates:
         * - Force magnitude proportional to penetration depth (F = k * d)
         * - Force direction along surface normal
         * - Zero force when no penetration
         * - Correct spring constant application
         *
         * Physics: In contact_node_face_via_spring, repulsion force is:
         *   F_repulsion = repulsion_strength * integration_region * direction_vector
         * where direction_vector points from closest point on face to node
         *
         * @return 0 on success, 1 on failure
         */
        int test_contact_spring_force_calculation();

        /**
         * @brief Test 2: Force symmetry verification (Newton's third law)
         *
         * Validates:
         * - F_ab = -F_ba for interacting surfaces
         * - Energy conservation in force application
         * - Proper force distribution across triangle vertices
         *
         * Critical for simulation stability and physical correctness.
         *
         * @return 0 on success, 1 on failure
         */
        int test_contact_force_symmetry();

        /**
         * @brief Test 3: Interaction radius boundary conditions
         *
         * Validates:
         * - No forces computed when separation > interaction_cutoff
         * - Forces activate when separation <= interaction_cutoff
         * - Smooth transition at boundary (no discontinuities)
         * - Squared distance optimization correctness
         *
         * @return 0 on success, 1 on failure
         */
        int test_contact_radius_boundary();

        /**
         * @brief Test 4: Repulsive force at surface overlap
         *
         * Validates:
         * - Force magnitude increases with penetration depth
         * - Force direction pushes surfaces apart (anti-parallel to penetration)
         * - Force scales with face area (integration_region)
         * - No attractive forces in repulsion regime
         *
         * @return 0 on success, 1 on failure
         */
        int test_repulsion_at_overlap();

        /**
         * @brief Test 5: Edge case validation
         *
         * Validates behavior under degenerate conditions:
         * - Zero separation distance
         * - Parallel faces (zero normal component)
         * - Grazing contacts (tangential approach)
         * - Very large separations
         * - Numerical stability near boundaries
         *
         * @return 0 on success, 1 on failure
         */
        int test_edge_cases();

        /**
         * @brief Test 6: Force direction correctness
         *
         * Validates:
         * - Repulsion forces point away from surface
         * - Adhesion forces point toward surface
         * - Force vectors perpendicular to face when appropriate
         * - Correct handling of contact normal orientation
         *
         * @return 0 on success, 1 on failure
         */
        int test_force_direction_correctness();
};

#endif
