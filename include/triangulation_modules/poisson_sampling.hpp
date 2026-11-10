#ifndef DEF_POISSON_SAMPLING
#define DEF_POISSON_SAMPLING

#define _USE_MATH_DEFINES


#include <cassert>
#include <chrono> 
#include <cmath>  
#include <random> 
#include <vector>
#include <forward_list>

#include "custom_exception.hpp"
#include "custom_structures.hpp"

#include "utils.hpp"
#include "vec3.hpp"

//Why the linker fails if you don't give the uspg absolute path? This is a mistery
//beyond the reach of human knowledge
#include "../uspg/uspg_3d.hpp"
#include "../uspg/uspg_4d.hpp"

#include "cell.hpp"
#include "face.hpp"





/**
 * @file
 * @brief Poisson-disk sampling of triangulated surfaces.
 * @ingroup triangulation
 */

/**
 * @brief Poisson-disk sampling of a triangulated surface; all methods are static.
 *
 * Returns points on the surface that are at least l_min apart. The surface is first sampled
 * densely and uniformly; points are then accepted voxel by voxel in a uniform spatial
 * partitioning grid if they are at least l_min from the points already accepted. The method
 * follows Bowers et al., "Parallel Poisson disk sampling with spectrum analysis on surfaces",
 * SIGGRAPH Asia 2010.
 *
 * @ingroup triangulation
 */
class poisson_sampling
{



    public:
        //Make sure the poisson_sampling cannot be default instantiated
        poisson_sampling() = delete;                                           //default constructor
        poisson_sampling(const poisson_sampling& c) = delete;           //copy constructor
        poisson_sampling(poisson_sampling&& c) = delete;                //move constructor
        poisson_sampling& operator=(const poisson_sampling& c) = delete;//copy assignment operator
        poisson_sampling& operator=(poisson_sampling&& c) = delete;     //move assignment operator 

        /**
         * @brief Poisson-disk point cloud of the surface of a cell.
         * @param l_min Minimum distance between points, m.
         * @param input_cell Triangulated cell with outward face normals.
         * @return Points on the surface, each with the normal of the face it was sampled on.
         */
        static std::vector<oriented_point> compute_poisson_point_cloud(double const l_min, const cell_ptr input_cell) noexcept(false); 

        /**
         * @brief Poisson-disk point cloud of the division interface, used by cell_divider.
         *
         * The interface nodes (ids from @p division_point_ids_threshold) are always kept,
         * marked with oriented_point::created_by_poisson_sampling_ = false; new points are
         * sampled on the faces with ids from @p division_face_ids_threshold.
         *
         * @param l_min Minimum distance between points, m.
         * @param m Triangulated mesh containing the interface.
         * @param division_point_ids_threshold Id of the first interface node.
         * @param division_face_ids_threshold Id of the first interface face.
         * @param division_plane_normal Normal given to the points.
         * @throws division_exception if a face would need more uniform samples than an unsigned
         *         index can hold.
         */
        static std::vector<oriented_point> compute_poisson_point_cloud(
            double const l_min, 
            mesh& m,
            const unsigned division_point_ids_threshold,  // All the points with an id equal or greater than this value should be added to the sampling
            const unsigned division_face_ids_threshold,   //All the faces with an id equal or greater than this value will be sampled
            const vec3 division_plane_normal //The normal of the division plane
        ) noexcept(false);

        /**
         * @brief Dense uniform random sampling of the faces of a cell (about 1000 points per
         *        disk of radius l_min).
         */
        static std::vector<oriented_point> uniform_sampling(cell_ptr input_cell, const double l_min) noexcept(false); 
        /** @brief Dense uniform random sampling of the faces of @p m with ids from @p division_face_ids_threshold. */
        static std::vector<oriented_point> uniform_sampling(const mesh& m, const unsigned division_face_ids_threshold, const double l_min) noexcept(false);

        /**
         * @brief Select from the uniform samples in @p grid_1 the points that are at least
         *        @p l_min (m) from all points already in @p grid_2, at most one new point per
         *        voxel, trying at most 30 candidates per voxel.
         * @return All points in @p grid_2 after the selection.
         */
        static std::vector<oriented_point> poisson_disk_sampling(
            const uspg_4d<oriented_point>& grid_1, 
            uspg_4d<oriented_point>& grid_2,
            const double l_min
        ) noexcept;

        /** @brief Axis-aligned bounding box {min_x, min_y, min_z, max_x, max_y, max_z} of the given faces, m. */
        static std::array<double, 6> get_surface_aabb(
            const mesh& m, 
            const std::vector<unsigned> face_to_sample_ids
        ) noexcept;

        /** @brief Axis-aligned bounding box of the faces with ids from @p division_face_ids_threshold, m. */
        static std::array<double, 6> get_surface_aabb(
            const mesh& m, 
            const unsigned division_face_ids_threshold // All the faces with id >= division_face_ids_threshold will be included in the AABB
        ) noexcept;
};

#endif