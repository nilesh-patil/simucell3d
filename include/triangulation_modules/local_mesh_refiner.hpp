#ifndef DEF_local_mesh_refiner
#define DEF_local_mesh_refiner

#define _USE_MATH_DEFINES


#include <cstdint>
#include <cassert>
#include <cmath>
#include <set>
#include <functional>
#include <string>
#include <vector>


#include "utils.hpp"


#include "custom_exception.hpp"
#include "custom_structures.hpp"

#include "mesh_writer.hpp"
#include "cell.hpp"
#include "face.hpp"
#include "edge.hpp"



/**
 * @file
 * @brief Local remeshing that keeps the edge lengths of the cell surfaces within bounds.
 * @ingroup triangulation
 */

/**
 * @brief Keeps the edge lengths of every cell surface in [l_min, l_max].
 *
 * Called by the solver at every iteration. An edge longer than l_max is split at its
 * midpoint; an edge shorter than l_min is collapsed into a node at its midpoint, unless the
 * collapse would make an edge shared by more than two faces. If edge swaps are enabled,
 * triangles whose quality score (36/sqrt(3) * area / perimeter^2, 1 for an equilateral
 * triangle) is below 0.2 first have their longest edge swapped. With the full equations of
 * motion (DYNAMIC_MODEL_INDEX 0), the momentum of the nodes is redistributed so that it is
 * conserved by each operation.
 *
 * The solver uses l_max = 3 * l_min, with l_min the min_edge_length of the parameter file.
 *
 * @ingroup triangulation
 */
class local_mesh_refiner
{

    private:

        //The minimum and maximum authorized edge lengths
        const double l_min_, l_max_, l_min_squared_, l_max_squared_;

        //If set to true, the local mesh refiner will remove the triangles with high aspect ratio
        //by swapping their longest edge
        const bool enable_edge_swap_operation_;

        //The isoperimetric ratio of an equliateral triangle, which
        //is the smmallest possible isoperimetric ratio
        static constexpr double q_min_ = 36. / std::sqrt(3.);

        //The minimum allowed triangle score, below that, the edge swap operation is triggered
        static constexpr double triangle_score_min_ = 0.2;

        friend class local_mesh_refiner_tester;


    public:
        /** @brief A cell whose refinement threw an ordinary error during refine_meshes(). */
        struct refinement_failure {
            cell_ptr cell;       ///< The cell; its mesh may be damaged.
            std::string reason;  ///< The what() of the error.
        };


    private:
        //The body of refine_meshes(), with the refinement of one cell given as a parameter so that the tests
        //(local_mesh_refiner_tester) can make chosen cells throw chosen exceptions. refine_one is called once
        //per cell, from several threads at the same time
        [[nodiscard]] std::vector<refinement_failure> refine_meshes_impl(
            const std::vector<cell_ptr>& cell_lst,
            const std::function<void(const cell_ptr&)>& refine_one
        ) const noexcept(false);


    public:
        //Make sure the local_mesh_refiner cannot be default instantiated
        local_mesh_refiner() = default;                                     //default constructor
        local_mesh_refiner(const local_mesh_refiner& c) = delete;           //copy constructor
        local_mesh_refiner(local_mesh_refiner&& c) = delete;                //move constructor
        local_mesh_refiner& operator=(const local_mesh_refiner& c) = default;//copy assignment operator
        local_mesh_refiner& operator=(local_mesh_refiner&& c) = default;     //move assignment operator 

        double get_l_min() const noexcept {return l_min_;};  ///< Minimum edge length, m.
        double get_l_max() const noexcept {return l_max_;};  ///< Maximum edge length, m.

        double get_l_min_squared() const noexcept {return l_min_squared_;};  ///< Square of the minimum edge length, m^2.
        double get_l_max_squared() const noexcept {return l_max_squared_;};  ///< Square of the maximum edge length, m^2.

        /**
         * @brief Construct the refiner.
         * @param l_min Minimum edge length, m; must be positive.
         * @param l_max Maximum edge length, m; must be greater than @p l_min.
         * @param enable_edge_swap_operation Swap the longest edge of low-quality triangles
         *        (enable_edge_swap_operation in the parameter file).
         */
        local_mesh_refiner(const double l_min, const double l_max, const bool enable_edge_swap_operation = true) noexcept;

        /**
         * @brief Refine the meshes of all cells in parallel.
         *
         * A cell whose refinement throws a std::exception is left as it is and returned in the
         * list of failures (in no particular order); the other cells are refined as if alone.
         * The solver removes the failed cells, or stops the run if there are too many
         * (solver::handle_refinement_failures()).
         *
         * @param cell_lst The cells.
         * @return The cells whose refinement failed, with the error message.
         * @throws unstable_simulation_exception, std::bad_alloc, or any exception not derived
         *         from std::exception thrown for a cell; rethrown after the parallel loop ends.
         *         If several are thrown, the first one stored is rethrown.
         */
        [[nodiscard]] std::vector<refinement_failure> refine_meshes(const std::vector<cell_ptr>& cell_lst) const noexcept(false);

        /**
         * @brief Refine the mesh of one cell.
         *
         * Swaps edges of low-quality triangles (if enabled), then splits and collapses edges
         * until every edge is in [l_min, l_max]. If more than 50 split or collapse operations
         * fall in the same cube of side 1e-7 m (a split/collapse cycle), the refinement stops
         * early for this call, leaving some edges out of range, and a line is written to
         * standard error (the first 20 occurrences, then every 1000th). Finally the face
         * winding is checked and repaired (repair_inconsistent_winding()).
         *
         * @param c The cell.
         * @throws mesh_integrity_exception if 500000 operations are reached without convergence.
         */
        void refine_mesh(cell_ptr c) const noexcept(false);

        /**
         * @brief Swap the longest edge of every face whose quality score is below 0.2.
         * @param c The cell.
         */
        void remove_elongated_triangles(cell_ptr c) const noexcept(false);

        /**
         * @brief Quality score of a triangle and its longest edge.
         * @param c The cell that owns the face.
         * @param f A used face of @p c.
         * @return The score 36/sqrt(3) * area / perimeter^2 (1 for an equilateral triangle,
         *         towards 0 for a degenerate one) and the longest edge.
         */
        std::pair<double, edge> get_triangle_score(cell_ptr c, const face& f) const noexcept;

        /**
         * @brief Whether an edge can be collapsed into a node.
         * @return false if the two end nodes share more than two neighbours, in which case the
         *         collapse would create an edge shared by more than two faces.
         */
        bool can_be_merged(edge& e_ab, cell_ptr c) const noexcept(false);
 
        /**
         * @brief Replace the edge AB shared by faces ABC and ABD by the edge CD.
         *
         * Nothing is done if the edge CD already exists or the configuration is degenerate.
         */
        void swap_edge(edge& e_ab, cell_ptr c) const noexcept(false);

        /**
         * @brief Split an edge at its midpoint, replacing its two faces by four.
         *
         * The new faces inherit the winding and the face type of the faces they replace. The
         * new and modified edges are added to or updated in @p edge_to_check_set.
         */
        void split_edge(edge& e_ab, cell_ptr c, edge_set& edge_to_check_set) const noexcept(false);

        /**
         * @brief Collapse an edge into a new node at its midpoint and delete its two faces.
         *
         * Deleted edges are removed from, and new edges added to, @p edge_to_check_set.
         */
        void merge_edge(edge& e_ab, cell_ptr c, edge_set& edge_to_check_set) const noexcept(false);

        /**
         * @brief Number of edges whose two faces traverse them in the same direction.
         * @return 0 for a consistently wound (oriented) surface.
         */
        static unsigned count_inconsistently_wound_edges(const cell_ptr& c) noexcept;

        /**
         * @brief Fast check of the face winding in one pass over the faces.
         *
         * Sums an antisymmetric hash g(a, b) = -g(b, a) over all directed edges (a, b) of the
         * faces, modulo 2^64.
         *
         * @return 0 for a consistently wound closed surface; non-zero, except with a
         *         probability of about 2^-64, if any edge is inconsistent.
         */
        static uint64_t winding_checksum(const cell_ptr& c) noexcept;

        /**
         * @brief Detect and repair an inconsistent face winding.
         *
         * If the winding is inconsistent, a line is written to standard error (the first 20
         * occurrences, then every 1000th) and, if the surface is manifold, all faces are
         * re-oriented from one seed face.
         *
         * @param c The cell.
         * @return The number of inconsistently wound edges found (0 if none).
         */
        unsigned repair_inconsistent_winding(cell_ptr c) const noexcept;


        




};

#endif