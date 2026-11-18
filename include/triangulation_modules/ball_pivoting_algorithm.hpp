#ifndef DEF_BALL_PIVOTING_ALGORITHM
#define DEF_BALL_PIVOTING_ALGORITHM

#define _USE_MATH_DEFINES

#include <map>
#include <random>
#include <optional>
#include <unordered_set>

#include "custom_exception.hpp"
#include "custom_structures.hpp"

#include "utils.hpp"
#include "vec3.hpp"
#include "../uspg/uspg_4d.hpp"

#include "cell.hpp"
#include "face.hpp"
#include "edge.hpp"
#include "node.hpp"

#include "mesh_writer.hpp"





/**
 * @file
 * @brief Surface reconstruction from an oriented point cloud with the ball pivoting algorithm.
 * @ingroup triangulation
 */

/**
 * @brief Reconstructs a closed triangulated surface from a Poisson-disk point cloud.
 *
 * Implements the ball pivoting algorithm of Bernardini et al., "The ball-pivoting algorithm
 * for surface reconstruction", IEEE TVCG 5(4), 1999, doi:10.1109/2945.817351, with a ball of
 * radius 1.7 * l_min. After the front has been expanded, holes of up to about 10 nodes are closed
 * by fans around a new centre node. Used by initial_triangulation.
 *
 * @ingroup triangulation
 */


class ball_pivoting_algorithm
{
    private:
        double ball_radius_;
        double ball_radius_squared_;

        std::vector<oriented_point> node_lst_;

        //Store all the points in a uspg_4D
        uspg_4d<oriented_point> grid_;

        //All the created edges and faces are stored in vectors
        std::vector<edge> edge_lst_;
        std::vector<face> face_lst_;

        //Some faces will have to be deleted during the hole filling process. The indexes of the faces
        //to be deleted are stored in this vector. This faces can only be deleted at the end of the triagulation
        //std::vector<unsigned> face_to_delete_lst_;
        //std::vector<unsigned> edge_to_delete_lst_;


        //The nodes keep track of the edges or faces in which they are involved. All this tracking is 
        //needed to determine if a node is already in the triangulated region of the cell surface or not.
        //The key of the map is the node_id, the value is the list of the indexes of the faces using the node
        std::map<unsigned, std::vector<unsigned>> node_face_lst_; 
        std::map<unsigned, std::vector<unsigned>> node_edge_lst_;

        //Keep track of the edges at the front of the trangulation
        std::vector<unsigned> edge_id_front_lst_;

        //The tester class used to test the methods of this bpa class
        friend class bpa_tester;

        //This constructor is only for testing purposes
        ball_pivoting_algorithm(const double l_min): ball_radius_(1.2 * l_min), ball_radius_squared_(1.44 * l_min * l_min){};
        

    public:
        //Make sure the ball_pivoting_algorithm cannot be default instantiated                                        
        ball_pivoting_algorithm() = delete;                                           //default constructor
        ball_pivoting_algorithm(const ball_pivoting_algorithm& c) = delete;           //copy constructor
        ball_pivoting_algorithm(ball_pivoting_algorithm&& c) = delete;                //move constructor
        ball_pivoting_algorithm& operator=(const ball_pivoting_algorithm& c) = delete;//copy assignment operator
        ball_pivoting_algorithm& operator=(ball_pivoting_algorithm&& c) = delete;     //move assignment operator 

        /**
         * @brief Prepare the reconstruction.
         * @param poisson_point_cloud At least 4 oriented points, typically at least l_min apart.
         * @param l_min Minimum edge length, m; the ball radius is 1.7 * l_min.
         * @param min_x,min_y,min_z,max_x,max_y,max_z Bounding box of the points, m.
         */
        ball_pivoting_algorithm(
            const std::vector<oriented_point>& poisson_point_cloud, 
            const double l_min,
            const double min_x, const double min_y, const double min_z, 
            const double max_x, const double max_y, const double max_z 
        ) noexcept(false);

        /**
         * @brief Run the reconstruction: seed triangle, front expansion, hole filling.
         * @throws bpa_exception if no seed triangle is found, a hole is too large (more than about 10 nodes) or has fewer
         *         than 3 nodes, or the surface is not watertight.
         */
        void run() noexcept(false){
            find_seed_triangle();
            expand_triangulation();
            fill_surface_holes();
        }

        /**
         * @brief Find a first triangle whose ball contains no other point.
         * @throws bpa_exception if none is found.
         */
        void find_seed_triangle() noexcept(false);

        std::vector<face> get_face_lst() const noexcept{return face_lst_;}  ///< Copy of the created faces.
        std::vector<edge> get_edge_lst() const noexcept{return edge_lst_;}  ///< Copy of the created edges.
        std::vector<oriented_point> get_node_lst() const noexcept{return node_lst_;}  ///< Copy of the points, including hole centres.

        /** @brief Pivot the ball around the front edges until no new triangle can be added. */
        void expand_triangulation() noexcept;

        /** @brief Whether the normals of the three points point in the same direction. */
        static bool points_coherently_oriented(
            const oriented_point& a, 
            const oriented_point& b,
            const oriented_point& c
        ) noexcept;
        
        /**
         * @brief Unit normal of the plane through the three points, oriented like the points'
         *        normals.
         */
        static vec3 compute_oriented_normal(
            const oriented_point& point_a, 
            const oriented_point& point_b,
            const oriented_point& point_c
        ) noexcept;

        /**
         * @brief Centre of the ball through the three points, on the side of their normals.
         * @return The centre, or an empty optional if the ball is too small to touch all three.
         */
        std::optional<vec3> compute_circum_sphere_center(
            const oriented_point& point_a, 
            const oriented_point& point_b,
            const oriented_point& point_c
        ) const noexcept;

        /** @brief compute_circum_sphere_center() for the three nodes of a face. */
        std::optional<vec3> compute_circum_sphere_center(
            const face& f
        ) const noexcept;

        /** @brief Whether the ball at @p circum_sphere_center contains no point other than a, b and c. */
        bool circum_sphere_is_empty(
            const vec3& circum_sphere_center, 
            const oriented_point& point_a, 
            const oriented_point& point_b, 
            const oriented_point& point_c
        ) const noexcept;

        /** @brief Id of the edge between two points. */
        unsigned get_edge(const unsigned point_1_id, const unsigned point_2_id) noexcept;
        /** @brief Id of the edge between two points. */
        unsigned get_edge(const oriented_point& point_a, const oriented_point& point_b) noexcept;

        /** @brief The point that the ball reaches first when pivoting around @p edge, if any. */
        std::optional<oriented_point> find_candidate_node(const edge& edge) const noexcept;

        /** @brief Whether the point is already inside the triangulated region. */
        bool node_is_inner_vertex(const unsigned point_id)  const noexcept;
        /** @brief Whether the point is already inside the triangulated region. */
        bool node_is_inner_vertex(const oriented_point& p) const noexcept;

        /**
         * @brief Whether the face {f1_a, f1_b, p} has a normal pointing in the same direction
         *        as the face {f1_a, f1_b, f1_c}.
         */
        static bool node_is_compatible_with_edge(
            const oriented_point& p, 
            const oriented_point& f1_a,
            const oriented_point& f1_b,
            const oriented_point& f1_c
        ) noexcept;

        /**
         * @brief Pivoting angle, in [0, 2 pi], between two ball centres seen from the edge
         *        centre; the candidate with the smallest angle is chosen.
         */
        static double compute_angle_between_circumspheres(
                const vec3& face_a_circum_center,
                const vec3& face_b_circum_center,
                const vec3& edge_center_vec
            ) noexcept;



        /**
         * @brief Close the remaining holes with fans of triangles around a new centre node.
         * @throws bpa_exception if a hole is too large (more than about 10 nodes) or has fewer than 3 nodes.
         */
        void fill_surface_holes() noexcept(false);

        /**
         * @brief Next node along the boundary of a hole.
         * @throws bpa_exception if the boundary cannot be followed (surface not watertight).
         */
        unsigned get_next_node_of_the_hole(const unsigned previous_node, const unsigned current_node) const noexcept(false);





};

#endif