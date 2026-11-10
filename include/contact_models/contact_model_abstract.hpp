#ifndef DEF_CONTACT_MODEL_ABSTRACT
#define DEF_CONTACT_MODEL_ABSTRACT

/**
 * @file contact_model_abstract.hpp
 * @brief Base class of the contact models (adhesion and repulsion between cells).
 * @ingroup contact_models
 */


#include <iostream>
#include <vector>
#include <array>
#include <memory>
#include <omp.h>

#include "global_configuration.hpp"

#include "utils.hpp"
#include "custom_structures.hpp"
#include "custom_exception.hpp"

#include "cell.hpp"
#include "face.hpp"
#include "uspg_4d.hpp"
#include "contact_detection_strategy.hpp"





//--------------------------------------------------------------------------------------------------------------------------------
/**
 * @brief Base class of the contact models.
 *
 * A contact model finds the nodes of a cell that lie within the contact cutoffs of a face of
 * another cell and applies adhesion and repulsion to them. The solver owns exactly one model,
 * selected at compile time by `CONTACT_MODEL_INDEX` in `global_configuration.hpp`:
 * 0 contact_node_face_via_spring, 1 contact_node_node_via_coupling,
 * 2 contact_face_face_via_coupling (default).
 *
 * The base class holds the padded axis-aligned bounding boxes (AABB) of all faces, a uniform
 * spatial partitioning grid of faces (USPG), and the contact detection strategy chosen by the
 * `contact_detection_algorithm` parameter. With `adaptive`, the detection starts with the USPG
 * and switches to sweep and prune when the number of cells reaches
 * `ADAPTIVE_SAP_CELL_THRESHOLD` (500). Only contact_face_face_via_coupling consults the
 * detection strategy; models 0 and 1 always use the USPG.
 *
 * @ingroup contact_models
 */
class contact_model_abstract{

        private:
                friend class tester_contact_model_abstract;

        protected:

                //The length size of each voxel used to instantiate the grid
                double voxel_size_;
                
                //The cutoff distance above wich the interaction forces between 2 faces are not calculated anymore
                double interaction_cutoff_repulsion_, interaction_cutoff_square_repulsion_; 
                double interaction_cutoff_adhesion_, interaction_cutoff_square_adhesion_; 
                double max_interaction_cutoff_square_;

                
                //Keep track of the min and max coordinates of the mesh to update the partitionning grid
                double global_min_x_, global_min_y_, global_min_z_;
                double global_max_x_, global_max_y_, global_max_z_;

                //The uniform space partitionning grid that will store the faces of the mesh
                uspg_4d<face*> grid_;

                //Store the Axis Aligned Bounding Boxes of the face in this vector
                std::vector<double> face_aabb_lst_;

                //Store pointers to all the faces of the mesh in this vector
                std::vector<face*> face_lst_;

                //The extra padding added to the AABB of the faces, to check for potential contacts
                double aabb_padding_;

                // The contact detection strategy (USPG or SAP)
                // Initialized based on contact_detection_algorithm_ parameter
                std::unique_ptr<contact_detection_strategy> detection_strategy_;

                // Store the configured algorithm for dynamic switching support
                ContactDetectionAlgorithm configured_algorithm_;

                // Cached simulation parameters for strategy recreation in adaptive mode
                global_simulation_parameters sim_parameters_;

                // Select the appropriate algorithm based on cell count (for ADAPTIVE mode)
                ContactDetectionAlgorithm select_algorithm_for_cell_count(size_t cell_count) const noexcept;

        public:
                contact_model_abstract() = default;                                         //default constructor
                contact_model_abstract(const contact_model_abstract& v) = delete;           //copy constructor
                contact_model_abstract(contact_model_abstract&& v) = delete;                //move constructor
                contact_model_abstract& operator=(const contact_model_abstract& v) = delete;//copy assignment operator
                contact_model_abstract& operator=(contact_model_abstract&& v) = default;    //move assignment operator

                //The solver owns the model through a std::unique_ptr<contact_model_abstract>: without a virtual
                //destructor, deleting it skipped the derived destructor (UB; the face grid buffers leaked)
                virtual ~contact_model_abstract() = default;

                /**
                 * @brief Store the contact cutoffs and create the contact detection strategy.
                 *
                 * The AABB padding is the larger of the two contact cutoffs, and the voxel edge of the
                 * face grid is 3 x `min_edge_len_` + 2 x padding.
                 *
                 * @param sim_parameters Numerical parameters; uses `contact_cutoff_adhesion_` (m),
                 *        `contact_cutoff_repulsion_` (m), `min_edge_len_` (m) and
                 *        `contact_detection_algorithm_`.
                 */
                contact_model_abstract(const global_simulation_parameters& sim_parameters) noexcept(false);

                /**
                 * @brief Find the contacts between the cells and apply the contact forces to their nodes.
                 *
                 * Called once per iteration by the solver, outside any parallel region (the method
                 * opens its own).
                 *
                 * @param cell_lst All cells of the simulation.
                 * @throws unstable_simulation_exception If the domain has diverged and the contact grid
                 *         cannot be built.
                 */
                virtual void run(const std::vector<cell_ptr>& cell_lst) noexcept(false) = 0;

                /**
                 * @brief Recompute the padded AABB of every face in the face list and the bounds of the domain.
                 *
                 * Each face AABB is enlarged by the padding (the larger contact cutoff) on every side.
                 *
                 * @param cell_lst All cells of the simulation.
                 */
                void update_face_aabbs(
                        const std::vector<cell_ptr>& cell_lst
                ) noexcept;

                /**
                 * @brief Resize the face grid to the current domain and insert every face in all the voxels
                 *        its AABB overlaps.
                 * @throws unstable_simulation_exception If the domain has diverged
                 *         (see uspg_abstract::compute_nb_voxels()) or the grid cannot be allocated.
                 */
                void store_face_in_uspg() noexcept(false);

                /**
                 * @brief Test whether a point lies in the padded AABB of a face.
                 * @param face_aabb_pos Index of the AABB in the flat AABB list (6 x global face id).
                 * @param node_pos      Position of the point, m.
                 * @return True if the point is inside the box (boundary included).
                 */
                bool aabb_intersection_check(const size_t face_aabb_pos, const vec3& node_pos) const noexcept;

                /**
                 * @brief Find the point of a triangle closest to a given point.
                 * @param p Query point, m.
                 * @param a First vertex of the triangle, m.
                 * @param b Second vertex of the triangle, m.
                 * @param c Third vertex of the triangle, m.
                 * @return The squared distance between `p` and the closest point (m^2), and the
                 *         barycentric coordinates (u, v, w) of that point with respect to (a, b, c).
                 */
                static std::pair<double, vec3> compute_node_triangle_distance(
                        const vec3& p,
                        const vec3& a,
                        const vec3& b,
                        const vec3& c
                ) noexcept;

                /**
                 * @brief Return the current contact detection strategy (for tests and diagnostics).
                 * @return Non-owning pointer; may change between iterations in adaptive mode.
                 */
                const contact_detection_strategy* get_detection_strategy() const noexcept { return detection_strategy_.get(); }

};
//--------------------------------------------------------------------------------------------------------------------------------








#endif