#ifndef DEF_CONTACT_DETECTION_USPG_STRATEGY
#define DEF_CONTACT_DETECTION_USPG_STRATEGY

/**
 * @file contact_detection_uspg_strategy.hpp
 * @brief Contact detection with a uniform spatial partitioning grid (`uspg`).
 * @ingroup contact_models
 */

#include "contact_detection_strategy.hpp"
#include "uspg_4d.hpp"
#include "face.hpp"
#include "cell.hpp"

//---------------------------------------------------------------------------------------------
/**
 * @brief Contact detection strategy based on a uniform spatial partitioning grid (USPG).
 *
 * Space is divided into cubic voxels of edge 3 x `min_edge_len_` + 2 x padding, where the
 * padding is the larger contact cutoff. Each face is inserted, in Morton (Z-order) order of its
 * AABB centre, into every voxel its padded AABB overlaps. A query returns the faces stored in
 * the voxel of the point and its 26 neighbours, excluding faces of the query cell.
 *
 * @note contact_face_face_via_coupling bins faces in its own grid when this algorithm is
 *       selected; this class is the strategy-interface implementation of the same idea.
 * @ingroup contact_models
 */
class contact_detection_uspg_strategy : public contact_detection_strategy {
private:
    // The USPG grid storing face pointers (managed via unique_ptr for easy recreation)
    std::unique_ptr<uspg_4d<face*>> grid_;

    // Store face AABBs for intersection tests
    // Layout: [min_x, min_y, min_z, max_x, max_y, max_z] per face
    std::vector<double> face_aabb_lst_;

    // Store face pointers for indexed access
    std::vector<face*> face_lst_;

    // Grid parameters derived from simulation parameters
    double voxel_size_;
    double aabb_padding_;

    // Global bounds of the simulation domain
    double global_min_x_, global_min_y_, global_min_z_;
    double global_max_x_, global_max_y_, global_max_z_;

public:
    /**
     * @brief Set the padding and the voxel edge from the parameters.
     * @param params Numerical parameters; uses `min_edge_len_` and the two contact cutoffs (m).
     */
    explicit contact_detection_uspg_strategy(const global_simulation_parameters& params) noexcept;

    /**
     * @brief Pad the face AABBs, size the grid to the domain and insert the faces.
     * @copydetails contact_detection_strategy::prepare()
     * @note `bounds` is not used; the grid is sized from the face AABBs.
     */
    void prepare(
        const std::vector<cell*>& cells,
        const std::vector<face*>& faces,
        const std::vector<aabb>& aabbs,
        const vec3& bounds
    ) noexcept(false) override;

    /**
     * @brief Return the faces in the 3 x 3 x 3 voxels around a point, excluding those of `query_cell`.
     * @param query_pos  Position to query, m. A point outside the grid returns no faces.
     * @param query_cell Cell whose own faces are excluded, or nullptr.
     * @return A new vector of candidate faces.
     */
    std::vector<face*> get_candidate_faces(
        const vec3& query_pos,
        cell* query_cell
    ) const noexcept override;

    /** @brief Return ContactDetectionAlgorithm::USPG. */
    ContactDetectionAlgorithm algorithm_type() const noexcept override {
        return ContactDetectionAlgorithm::USPG;
    }

private:
    /**
     * Store all faces in the USPG grid.
     *
     * Each face is stored in all voxels that its AABB overlaps.
     * This ensures that neighbor queries always find nearby faces.
     */
    void store_faces_in_grid() noexcept(false);  //throws unstable_simulation_exception on a diverged domain

    /**
     * Check if a point is within a face's AABB.
     *
     * @param face_aabb_pos Position in face_aabb_lst_ (face_index * 6)
     * @param point         Point to test
     * @return true if point is within the AABB
     */
    bool point_in_aabb(size_t face_aabb_pos, const vec3& point) const noexcept;
};
//---------------------------------------------------------------------------------------------

#endif // DEF_CONTACT_DETECTION_USPG_STRATEGY
