
#ifndef DEF_AUTOMATIC_POLARIZER
#define DEF_AUTOMATIC_POLARIZER

/**
 * @file automatic_polarizer.hpp
 * @brief Face polarization from a discretization of the simulation space (`POLARIZATION_MODE_INDEX 2`).
 * @ingroup polarization
 */


#include <iostream>
#include <vector>
#include <set>
#include <array>
#include <functional>

#include <omp.h>


#include "global_configuration.hpp"

#include "utils.hpp"

#include "custom_structures.hpp"
#include "custom_exception.hpp"

#include "cell.hpp"
#include "face.hpp"
#include "node.hpp"
#include "uspg_3d.hpp"




/// Region label of a voxel that contains a node (a cell surface).
constexpr unsigned short boundary_region_id  = 0;
/// Region label of a voxel inside a cell.
constexpr unsigned short cytoplasm_region_id = 1;
/// Region label of a voxel connected to the outside of the tissue.
constexpr unsigned short exterior_region_id  = 2;
/// Region label of a voxel enclosed by cells but outside every cell (a lumen).
constexpr unsigned short lumen_region_id     = 3;



/**
 * @brief Assigns face types to epithelial cells from a voxel discretization of the space.
 *
 * Used by the solver when `POLARIZATION_MODE_INDEX` is 2. At each call of polarize_faces():
 *
 * 1. A uspg_3d grid of voxel edge `max_edge_length` covers the nodes plus two voxels of margin.
 * 2. Voxels that contain a node are labelled boundary.
 * 3. From the voxel of each cell centroid, the cytoplasm label is flood-filled (6-connected)
 *    up to the boundary voxels.
 * 4. From the corner voxel (0, 0, 0), the exterior label is flood-filled the same way.
 * 5. The remaining unlabelled voxels are labelled lumen.
 * 6. For each face of each epithelial cell (cell class 0), a ray is cast from its first node
 *    along the face normal; the first non-boundary voxel gives the region in contact. Faces
 *    facing the exterior get face type 0; faces facing a lumen or another cell get face type 1
 *    (indices into the face types of the cell type).
 *
 * @note The cytoplasm fill starts at the centroid, so it fails for a cell whose centroid lies
 *       outside its volume (strongly concave cells).
 * @ingroup polarization
 */
class automatic_polarizer{

        protected:

                //The unifom space partitionning grid is used to discretize the simulation space in voxels
                uspg_3d<unsigned short> grid_;
                const double grid_voxel_size_;

                //The maximum length of a ray used to detect with which region a face is in contact
                double ray_maximum_length_;
                


        public:
                automatic_polarizer() = delete;                                         //default constructor
                automatic_polarizer(const automatic_polarizer& v) = delete;           //copy constructor
                automatic_polarizer(automatic_polarizer&& v) = delete;                //move constructor
                automatic_polarizer& operator=(const automatic_polarizer& v) = delete;//copy assignment operator
                automatic_polarizer& operator=(automatic_polarizer&& v) = delete;    //move assignment operator 


                /**
                 * @brief Construct a polarizer with a given voxel edge.
                 * @param max_edge_length Voxel edge length, m. The solver passes the maximum edge
                 *        length (3 x `min_edge_length`).
                 */
                automatic_polarizer(const double max_edge_length) noexcept: grid_voxel_size_(max_edge_length){};


                /** @brief Return the labelled voxel grid of the last call to polarize_faces(). */
                inline const uspg_3d<unsigned short>& get_grid() const noexcept {return grid_;};

                /**
                 * @brief Rebuild and label the voxel grid, then set the face types of all epithelial cells.
                 * @param cell_lst All cells of the simulation.
                 * @throws automatic_polarization_exception If the ray cast from a face finds no
                 *         non-boundary voxel.
                 */
                void polarize_faces(const std::vector<cell_ptr>& cell_lst) noexcept(false);

                /**
                 * @brief Resize the grid to the bounding box of all nodes plus two voxels on each side.
                 * @param cell_lst All cells of the simulation.
                 */
                void update_grid_dimensions(const std::vector<cell_ptr>& cell_lst) noexcept;

                /** @brief Label as boundary every voxel that contains a node. */
                void mark_boundary_voxels(const std::vector<cell_ptr>& cell_lst) noexcept;

                /** @brief Label the cytoplasm of each cell by flood fill from the voxel of its centroid. */
                void mark_cytoplasm_voxels(const std::vector<cell_ptr>& cell_lst) noexcept;

                /** @brief Label the exterior space by flood fill from the voxel (0, 0, 0). */
                void mark_exterior_space_voxels(const std::vector<cell_ptr>& cell_lst);

                /** @brief Label as lumen every voxel still unlabelled after the other steps. */
                void mark_luminal_voxels(const std::vector<cell_ptr>& cell_lst);

                /**
                 * @brief Flood-fill a label from a voxel through its 6-connected unlabelled neighbours.
                 *
                 * The fill stops at labelled voxels and at the edge of the grid.
                 *
                 * @param voxel_index Index (i, j, k) of the start voxel.
                 * @param label       Region label to propagate.
                 */
                void expand_voxel_label(const std::array<unsigned,3>& voxel_index,const unsigned short label) noexcept;
                
                /**
                 * @brief Return the indices of the face-adjacent (6-connected) neighbours of a voxel.
                 *
                 * At the edge of the grid the voxel itself is returned in place of a missing neighbour.
                 */
                std::set<std::array<unsigned,3>> get_neighbor_voxels(std::array<unsigned, 3> voxel_index) const noexcept;

                /**
                 * @brief Set the face type of every face of an epithelial cell from the region it faces.
                 *
                 * Exterior space gives face type 0; lumen or cytoplasm of another cell gives face type 1.
                 * Cells of other classes are left unchanged.
                 *
                 * @param c Cell to polarize.
                 * @throws automatic_polarization_exception See get_region_in_contact_with_face().
                 */
                void polarize_faces_based_on_contacts_with_discretized_space(cell_ptr c) const noexcept(false);

                /**
                 * @brief Return the region label of the first non-boundary voxel along a ray from the face.
                 *
                 * The ray starts at the first node of the face and follows the face normal (voxel
                 * traversal after C. Ericson, *Real-Time Collision Detection*, section 7.4.2).
                 *
                 * @param c Cell that owns the face.
                 * @param f Face of `c`.
                 * @return cytoplasm_region_id, exterior_region_id or lumen_region_id.
                 * @throws automatic_polarization_exception If no non-boundary voxel is found within
                 *         3 x the largest number of voxels along an axis.
                 */
                unsigned short get_region_in_contact_with_face(cell_ptr c, const face& f) const noexcept(false);



};

#endif