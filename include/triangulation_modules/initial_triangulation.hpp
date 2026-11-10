#ifndef DEF_INITIAL_TRIANGULATION
#define DEF_INITIAL_TRIANGULATION


#include <cassert>
#include <iostream>
#include <vector>

#include "utils.hpp"
#include "custom_structures.hpp"
#include "custom_exception.hpp"
#include "vec3.hpp"
#include "cell.hpp"

#include "poisson_sampling.hpp"
#include "ball_pivoting_algorithm.hpp"

/**
 * @file
 * @brief Re-triangulation of an input cell surface with edge lengths close to l_min.
 * @ingroup triangulation
 */

/**
 * @brief Re-triangulates a closed input surface; all methods are static.
 *
 * Used by simulation_initializer when perform_initial_triangulation is enabled. The
 * procedure is:
 *
 * 1. Coarsely triangulate the surface by connecting the edges of each non-triangular face
 *    to the face centre.
 * 2. Convert it to a cell, which orients the faces outward and checks that the surface is
 *    closed and manifold.
 * 3. Sample the surface with Poisson-disk sampling (points at least l_min apart).
 * 4. Reconstruct a triangulated surface from the points with the ball pivoting algorithm.
 *
 * The edge lengths of the result are brought into [l_min, l_max] later by the local mesh
 * refiner.
 *
 * @ingroup triangulation
 */


class initial_triangulation
{

    public:
        //Make sure the initial_triangulation cannot be default instantiated
        initial_triangulation() = delete;                                           //default constructor
        initial_triangulation(const initial_triangulation& c) = delete;           //copy constructor
        initial_triangulation(initial_triangulation&& c) = delete;                //move constructor
        initial_triangulation& operator=(const initial_triangulation& c) = delete;//copy assignment operator
        initial_triangulation& operator=(initial_triangulation&& c) = delete;     //move assignment operator 

        /**
         * @brief Re-triangulate a closed surface.
         * @param l_min Minimum edge length, m.
         * @param l_max Maximum edge length, m (used to bound the number of sampled points).
         * @param surface Input surface; faces may have more than 3 nodes.
         * @param cell_id Id of the cell (not used by the current implementation).
         * @return The triangulated surface.
         * @throws initial_triangulation_exception if the surface has fewer than 4 nodes, a
         *         face with fewer than 3 nodes, is not a valid closed surface, or would need more
         *         points than an unsigned index can hold.
         * @throws bpa_exception if the ball pivoting algorithm fails.
         */
        static mesh triangulate_surface(
            double const l_min, 
            double const l_max, 
            const mesh& surface, 
            const unsigned cell_id
        ) noexcept(false); 

        /**
         * @brief Triangulate, in place, every face with more than 3 nodes by connecting its
         *        edges to a new node at the face centre.
         */
        static void coarse_triangulation(mesh& surface) noexcept; 

        /**
         * @brief Convert a triangulated surface to a cell with outward-oriented faces.
         * @throws initial_triangulation_exception if the cell initialization reports a mesh
         *         integrity error.
         */
        static cell_ptr convert_mesh_to_cell(const mesh& surface) noexcept(false); //throw mesh_integrity_exception

        /**
         * @brief Poisson-disk point cloud of the surface of a cell, points at least @p l_min
         *        (m) apart; see poisson_sampling::compute_poisson_point_cloud().
         */
        static std::vector<oriented_point> generate_poisson_point_cloud(const double l_min, const cell_ptr c) noexcept;

};

#endif