#ifndef DEF_CONTACT_DETECTION_STRATEGY
#define DEF_CONTACT_DETECTION_STRATEGY

#include <vector>
#include <memory>
#include "vec3.hpp"
#include "custom_structures.hpp"

// Forward declarations
class cell;
class face;

//---------------------------------------------------------------------------------------------
// AABB structure for axis-aligned bounding boxes
// Used by contact detection strategies for spatial acceleration
//---------------------------------------------------------------------------------------------
struct aabb {
    vec3 min_corner;
    vec3 max_corner;

    // Default constructor
    aabb() : min_corner(0., 0., 0.), max_corner(0., 0., 0.) {}

    // Constructor with corners
    aabb(const vec3& min_pt, const vec3& max_pt)
        : min_corner(min_pt), max_corner(max_pt) {}

    // Constructor from 6 doubles (min_x, min_y, min_z, max_x, max_y, max_z)
    aabb(double min_x, double min_y, double min_z,
         double max_x, double max_y, double max_z)
        : min_corner(min_x, min_y, min_z), max_corner(max_x, max_y, max_z) {}
};
//---------------------------------------------------------------------------------------------



//---------------------------------------------------------------------------------------------
/**
 * Abstract base class for contact detection strategies.
 *
 * This interface defines the contract for contact detection algorithms
 * used in SimuCell3D. The Strategy pattern allows runtime selection
 * between different spatial acceleration structures:
 *
 * - USPG (Uniform Spatial Partitioning Grid): The established baseline
 *   algorithm that partitions space into uniform voxels. Provides O(1)
 *   neighbor queries but may have redundant checks for sparse systems.
 *
 * - Sweep-and-Prune (SAP): Alternative algorithm that sorts AABBs along
 *   axes and uses interval overlap tests. More efficient for systems
 *   with varying density or clustering.
 *
 * Design Philosophy:
 * - prepare() is called once per simulation iteration to build/update
 *   the acceleration structure with current face positions
 * - get_candidate_faces() is called many times (once per node) to retrieve
 *   faces that may be in contact with a query node
 * - Both methods are noexcept to ensure exception safety in tight loops
 *
 * Usage Pattern:
 *   auto strategy = contact_detection_strategy::create(algorithm, params);
 *   strategy->prepare(cells, faces, aabbs, bounds);
 *   for (node : nodes) {
 *       auto candidates = strategy->get_candidate_faces(node.pos(), node.cell());
 *       // ... process candidates ...
 *   }
 */
class contact_detection_strategy {
public:
    virtual ~contact_detection_strategy() = default;

    /**
     * Prepare the acceleration structure with current simulation data.
     *
     * This method is called once per iteration to build/update the
     * spatial acceleration structure (grid, sorted lists, etc.).
     * The implementation should be optimized for incremental updates
     * when possible.
     *
     * Thread Safety: This method should be called from a single thread
     * before parallel contact detection begins.
     *
     * @param cells   All cells in the simulation (for cell exclusion context)
     * @param faces   All faces from all cells (for contact detection)
     * @param aabbs   Axis-aligned bounding boxes for each face (parallel to faces vector)
     * @param bounds  Simulation domain bounds (max corner - used for grid sizing)
     *
     * @note The aabbs vector must have the same size as the faces vector
     * @note bounds represents the maximum extents of the simulation domain
     */
    virtual void prepare(
        const std::vector<cell*>& cells,
        const std::vector<face*>& faces,
        const std::vector<aabb>& aabbs,
        const vec3& bounds
    ) noexcept = 0;

    /**
     * Get candidate faces for contact detection at a query position.
     *
     * Returns faces that may be in contact with a node at query_pos.
     * The query_cell parameter is used to exclude faces belonging to
     * the same cell (to avoid self-contact false positives).
     *
     * Performance: This method is called in the innermost loop of contact
     * detection (once per node). Implementations should minimize allocations
     * and optimize for cache efficiency.
     *
     * @param query_pos  Position to query (typically a node position)
     * @param query_cell Cell to exclude from results (nullptr for no exclusion)
     * @return Vector of candidate face pointers (may be empty)
     *
     * @note Returned faces are candidates only - distance checks still required
     * @note The returned vector is a fresh copy; caller owns it
     */
    virtual std::vector<face*> get_candidate_faces(
        const vec3& query_pos,
        cell* query_cell
    ) const noexcept = 0;

    /**
     * Return the algorithm type for runtime identification.
     *
     * Used for diagnostics, logging, performance monitoring, and
     * to verify correct strategy instantiation in tests.
     *
     * @return The ContactDetectionAlgorithm enum value
     */
    virtual ContactDetectionAlgorithm algorithm_type() const noexcept = 0;

    /**
     * Factory method to create strategy instances.
     *
     * Creates and returns a concrete strategy implementation based on
     * the specified algorithm type. This factory ensures proper
     * encapsulation and allows runtime algorithm selection.
     *
     * Thread Safety: This method is thread-safe and can be called
     * from any thread.
     *
     * @param algorithm The algorithm to instantiate (USPG or SWEEP_AND_PRUNE)
     * @param params    Simulation parameters (may contain algorithm-specific settings)
     * @return Unique pointer to the created strategy (never null)
     *
     * @note The factory never returns null - unknown algorithms default to USPG
     */
    static std::unique_ptr<contact_detection_strategy> create(
        ContactDetectionAlgorithm algorithm,
        const global_simulation_parameters& params
    );
};
//---------------------------------------------------------------------------------------------

#endif // DEF_CONTACT_DETECTION_STRATEGY
