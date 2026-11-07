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



/*
    This class makes sure that all the edges of the cell meshes have lengths comprised in the range [l_min, l_max]. 
    When an edge is too long, it is subdivided into two edges. When an edge is too short, it is merged with into one node.
    Triangles with very high isoperimetric ratio are also subdivided into two triangles.
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
        //A cell whose mesh refinement threw an ordinary error during refine_meshes(), with the what() of that error
        struct refinement_failure {
            cell_ptr cell;
            std::string reason;
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

        double get_l_min() const noexcept {return l_min_;};
        double get_l_max() const noexcept {return l_max_;};

        double get_l_min_squared() const noexcept {return l_min_squared_;};
        double get_l_max_squared() const noexcept {return l_max_squared_;};

        //Trivial constructor
        local_mesh_refiner(const double l_min, const double l_max, const bool enable_edge_swap_operation = true) noexcept;

        //Refine the meshes of all the cells in the vector, in parallel. A cell whose refinement throws a
        //std::exception is left as it is and returned in the list of failures (in no particular order), the other
        //cells are refined as if alone. unstable_simulation_exception, std::bad_alloc and the exceptions that do
        //not derive from std::exception still leave the method, once the parallel loop is over
        [[nodiscard]] std::vector<refinement_failure> refine_meshes(const std::vector<cell_ptr>& cell_lst) const noexcept(false);

        //Refine the mesh of a given cell
        void refine_mesh(cell_ptr c) const noexcept(false);

        //Remove all the elongated triangles
        void remove_elongated_triangles(cell_ptr c) const noexcept(false);

        //Return the triangle score and also the longest edge of the triangle
        std::pair<double, edge> get_triangle_score(cell_ptr c, const face& f) const noexcept;

        //Check if an edge can be merged into a node
        bool can_be_merged(edge& e_ab, cell_ptr c) const noexcept(false);
 
        void swap_edge(edge& e_ab, cell_ptr c) const noexcept(false);

        void split_edge(edge& e_ab, cell_ptr c, edge_set& edge_to_check_set) const noexcept(false);

        void merge_edge(edge& e_ab, cell_ptr c, edge_set& edge_to_check_set) const noexcept(false);

        //Return the number of edges of the cell whose 2 faces run through them in the same direction.
        //It is 0 for a consistently wound (oriented) surface, where every directed edge belongs to exactly one face
        static unsigned count_inconsistently_wound_edges(const cell_ptr& c) noexcept;

        //Fast screen for the same property, one pass over the faces: the sum over all the directed edges
        //(a, b) of the faces of an antisymmetric hash g(a, b) = -g(b, a). It is exactly 0 for a consistently
        //wound closed surface, and non-zero (but for a ~2^-64 chance) as soon as one edge is not
        static uint64_t winding_checksum(const cell_ptr& c) noexcept;

        //If the winding of the faces of the cell is inconsistent, report it and re-orient the faces.
        //Returns the number of inconsistently wound edges that were found
        unsigned repair_inconsistent_winding(cell_ptr c) const noexcept;


        




};

#endif