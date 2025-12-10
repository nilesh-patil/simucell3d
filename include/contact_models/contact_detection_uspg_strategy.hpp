#ifndef DEF_CONTACT_DETECTION_USPG_STRATEGY
#define DEF_CONTACT_DETECTION_USPG_STRATEGY

#include "contact_detection_strategy.hpp"
#include "uspg_4d.hpp"
#include "face.hpp"
#include "cell.hpp"

//---------------------------------------------------------------------------------------------
/**
 * Uniform Spatial Partitioning Grid (USPG) contact detection strategy.
 *
 * This strategy wraps the existing USPG implementation to provide
 * contact detection through the polymorphic strategy interface.
 *
 * Algorithm Overview:
 * - Space is divided into uniform voxels of size determined by
 *   min_edge_len and contact cutoff parameters
 * - Faces are inserted into all voxels their AABB overlaps
 * - Queries return faces from the voxel containing the query point
 *   plus all 26 neighboring voxels (3x3x3 neighborhood)
 *
 * Complexity:
 * - prepare(): O(F * V) where F is face count and V is average voxels per face
 * - get_candidate_faces(): O(N) where N is average faces in 27-voxel neighborhood
 *
 * Memory: O(F * V) for face storage in voxels (faces may appear in multiple voxels)
 *
 * Best For:
 * - Uniform cell distributions
 * - Dense packing where most voxels contain faces
 * - Simulations with consistent mesh resolution
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
     * Construct USPG strategy from simulation parameters.
     *
     * Derives voxel size from min_edge_len and contact cutoff distances
     * to ensure efficient spatial queries.
     *
     * @param params Simulation parameters containing edge length and cutoff values
     */
    explicit contact_detection_uspg_strategy(const global_simulation_parameters& params) noexcept;

    // Implement the strategy interface
    void prepare(
        const std::vector<cell*>& cells,
        const std::vector<face*>& faces,
        const std::vector<aabb>& aabbs,
        const vec3& bounds
    ) noexcept override;

    std::vector<face*> get_candidate_faces(
        const vec3& query_pos,
        cell* query_cell
    ) const noexcept override;

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
    void store_faces_in_grid() noexcept;

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
