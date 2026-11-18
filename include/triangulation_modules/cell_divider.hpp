#ifndef DEF_CELL_DIVIDER
#define DEF_CELL_DIVIDER


#include <cassert>
#include <iostream>
#include <vector>
#include <optional>

#include "utils.hpp"
#include "custom_structures.hpp"
#include "custom_exception.hpp"

#include "vec3.hpp"
#include "mat33.hpp"
#include "quaternion.hpp"

#include "cell.hpp"
#include "face.hpp"

#include "epithelial_cell.hpp"


#include "initial_triangulation.hpp"
#include "poisson_sampling.hpp"
#include "ball_pivoting_algorithm.hpp"
#include "local_mesh_refiner.hpp"


#include "delaunator.hpp" //Defined in lib/delaunator/include/delaunator.hpp


/**
 * @file
 * @brief Division of a cell into two daughter cells.
 * @ingroup triangulation
 */

/**
 * @brief Divides cells that have reached their division volume.
 *
 * All methods are static; the class cannot be instantiated. The solver calls run() every 5
 * iterations. A cell is cut by the plane through its centroid normal to its longest axis
 * (cell::get_cell_division_axis()). The cut interface is triangulated with Poisson-disk
 * sampling and a 2D Delaunay triangulation, each daughter receives one side of the surface
 * plus the interface, and both daughters are refined by the local mesh refiner.
 *
 * A division can be refused. The mother cell is then left unchanged, a line
 * "Cell division failed for cell <id>: <reason>" is written to standard error, and the
 * division is tried again at the next check. Reasons include: no closed cut curve can be
 * traced; the Delaunay triangulation does not cover every side of the interface outline;
 * the cut does not split the surface into exactly two pieces. When the plane meets the
 * surface along several closed curves, the cell is cut along the longest one.
 *
 * @ingroup triangulation
 */


class cell_divider
{

    public:
        //Make sure the cell_divider cannot be default instantiated
        cell_divider() = delete;                                           //default constructor
        cell_divider(const cell_divider& c) = delete;           //copy constructor
        cell_divider(cell_divider&& c) = delete;                //move constructor
        cell_divider& operator=(const cell_divider& c) = delete;//copy assignment operator
        cell_divider& operator=(cell_divider&& c) = delete;     //move assignment operator 

        /**
         * @brief Divide every cell that is ready to divide (cell::is_ready_to_divide()).
         *
         * Cells are processed in an OpenMP parallel loop. Each successful division replaces
         * the mother in @p cell_lst by two daughters, appended at the end of the list with
         * new ids; local ids are then renumbered. Refused divisions leave the mother in place.
         *
         * @param cell_lst All cells; modified in place.
         * @param l_min Minimum edge length, m.
         * @param lmr Mesh refiner applied to the daughter cells.
         * @param max_cell_id_ Next unused cell id; incremented by 2 per division.
         * @param verbose Not used by the current implementation.
         */
        static void run(
            std::vector<cell_ptr>& cell_lst,
            const double l_min, //The min edge length
            const local_mesh_refiner& lmr, // Will be used to refine the meshes of the daughter cells
            unsigned& max_cell_id_, //Use this number to assign unique ids to the daughter cells
            bool verbose = true
        ) noexcept;

        /**
         * @brief Divide one cell into two daughter cells.
         *
         * The mother cell is not modified, apart from cell::rebase(). Each daughter gets half
         * of the mother's target volume and draws a new growth rate and division volume.
         *
         * @param c The mother cell.
         * @param l_min Minimum edge length, m.
         * @param lmr Mesh refiner applied to the daughter cells.
         * @return The two daughters, or an empty optional if the division was refused (any
         *         std::exception is caught, logged to standard error and turned into a refusal).
         */
        static std::optional<std::pair<cell_ptr, cell_ptr>> divide_cell(
            cell_ptr c, 
            const double l_min, //The min edge length
            const local_mesh_refiner& lmr // Will be used to refine the meshes of the daughter cells
        ) noexcept; 

        /**
         * @brief Mesh of the cell with the points where the cut curve crosses its edges.
         *
         * All closed curves along which the plane meets the surface are traced; only the
         * longest is inserted into the mesh.
         *
         * @param c The cell; it is not modified.
         * @param p A point of the plane, m.
         * @param n Normal of the plane.
         * @throws division_exception if no closed curve can be traced.
         */
        static mesh add_intersection_points(
            cell_ptr c, 
            const vec3& p,  //Position of a point on the plane
            const vec3& n   //Normal vector of the plane
        )noexcept(false);


        /**
         * @brief Intersection of a line segment with a plane.
         * @return The intersection point, or an empty optional if the segment does not reach
         *         the plane or is parallel to it.
         */
        static std::optional<vec3> find_edge_plane_intersection(
            const vec3& e1, //Position of the 1st point of the edge
            const vec3& e2, //Position of the 2nd point of the edge

            const vec3& p,  //Position of a point on the plane
            const vec3& n   //Normal vector of the plane
        ) noexcept(false);


        /**
         * @brief Follow the closed curve along which the plane cuts the cell surface.
         * @param c The cell.
         * @param seed_edge An edge intersected by the plane.
         * @param p A point of the plane, m.
         * @param n Normal of the plane.
         * @return The cut edges in order, each with its intersection point.
         * @throws division_exception if the curve cannot be followed (for example when the
         *         plane passes through a node) or does not close.
         */
        static std::vector<std::pair<edge, vec3>> trace_cut_loop(
            cell_ptr c,
            const edge& seed_edge,  //An edge intersected by the plane
            const vec3& p,          //Position of a point on the plane
            const vec3& n           //Normal vector of the plane
        ) noexcept(false);

        /**
         * @brief Side of the plane on which a face lies.
         * @return true if the face centroid is on the side the normal @p n points to.
         */
        static bool face_side_wrt_plane(
            const face& f,
            const cell_ptr c,
            const vec3& p,  //Position of a point on the plane
            const vec3& n   //Normal vector of the plane
        ) noexcept;

        /**
         * @brief Insert node @p n_p_id into face @p face_id of @p m between nodes @p n_a_id
         *        and @p n_b_id.
         * @throws division_exception if the face does not exist, has fewer than 3 nodes, or does
         *         not contain the edge (n_a, n_b).
         */
        static void add_point_to_face(
            mesh& m, 
            const unsigned face_id, 
            const unsigned n_a_id, 
            const unsigned n_b_id,
            const unsigned n_p_id
        ) noexcept(false);


        /**
         * @brief Split the faces crossed by the cut into triangles.
         *
         * After add_intersection_points(), a face crossed by the cut has 5 nodes (two of its
         * edges were cut); it is replaced by 3 triangles.
         *
         * @param m The mesh.
         * @param intersection_point_ids_threshold Id of the first intersection point; all
         *        intersection points have ids greater than or equal to it.
         */
        static void divide_faces(mesh& m, const unsigned intersection_point_ids_threshold) noexcept(false);

        /**
         * @brief Move the interface points (ids from @p division_point_ids_threshold) into the
         *        xy plane: centroid to the origin, then a rotation of @p n onto the z axis.
         * @return The translation and the rotation that were applied.
         */
        static std ::pair<vec3, mat33> map_points_to_xy_plane(
            mesh& m, 
            const unsigned division_point_ids_threshold,
            const vec3& n
        ) noexcept;

        /**
         * @brief Triangulate the interface between the two daughter cells.
         *
         * Adds interior points by Poisson-disk sampling with spacing l_min, then triangulates
         * all interface points with a 2D Delaunay triangulation, keeping the triangles whose
         * centroid lies inside the interface outline.
         *
         * @throws division_exception if the Delaunay triangulation fails or does not cover
         *         every side of the interface outline (possible when the outline is not
         *         convex); the division is then refused.
         */
        static void triangulate_division_interface(
            const double l_min,                             //The minimum edge length
            mesh& m,                                        //The mesh containing all the faces and nodes
            const unsigned division_point_ids_threshold,    //All the nodes that belong to the division interface have an id greater than this threshold
            const unsigned division_face_ids_threshold,     //All the faces that belong to the division interface have an id greater than this threshold
            const vec3& division_face_normal                //The normal vector of the division interface
        ) noexcept(false);


        /** @brief Undo map_points_to_xy_plane() for the interface points. */
        static void map_points_to_division_plane(
            mesh& m, 
            const unsigned division_point_ids_threshold,
            const vec3& translation,  
            const mat33& rotation_matrix
        ) noexcept;

        /**
         * @brief Whether a point lies inside a polygon, using the x and y coordinates only
         *        (crossing-number test).
         */
        static bool point_is_in_polygon(
            const std::vector<vec3>& polygon, 
            const vec3& point
        ) noexcept;

        /**
         * @brief Build the two daughter cells from the cut mesh of the mother cell.
         *
         * The faces of the mother are split into the two pieces separated by the cut; each
         * daughter gets one piece plus the interface triangles, with opposite orientation.
         * Daughter 1 gets the piece on the negative side of the plane.
         *
         * @throws division_exception if the cut does not split the surface into exactly two
         *         pieces.
         */
        static std::pair<cell_ptr, cell_ptr> create_daughter_cells(
            cell_ptr mother_cell, 
            mesh& mother_cell_mesh,                           //The mesh containing all the faces and nodes
            const unsigned division_point_ids_threshold,        //All the nodes that belong to the division interface have an id greater than this threshold
            const unsigned division_face_ids_threshold,         //All the faces that belong to the division interface have an id greater than this threshold
            const vec3& division_plane_normal,                  //The normal vector of the division interface
            const vec3& division_plane_origin                   // A point located on the division plane
        ) noexcept(false);




};

#endif