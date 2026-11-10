#ifndef DEF_CONTACT_DETECTION_STRATEGY
#define DEF_CONTACT_DETECTION_STRATEGY

/**
 * @file contact_detection_strategy.hpp
 * @brief Interface of the broad-phase contact detection strategies and the aabb box type.
 * @ingroup contact_models
 */

#include <vector>
#include <memory>
#include "vec3.hpp"
#include "custom_structures.hpp"

// Forward declarations
class cell;
class face;

//---------------------------------------------------------------------------------------------
/**
 * @brief Axis-aligned bounding box, passed to the contact detection strategies.
 * @ingroup contact_models
 */
struct aabb {
    vec3 min_corner;    ///< Corner with the smallest coordinates, m.
    vec3 max_corner;    ///< Corner with the largest coordinates, m.

    /** @brief Box with both corners at the origin. */
    aabb() : min_corner(0., 0., 0.), max_corner(0., 0., 0.) {}

    /** @brief Box from its two corners (m). */
    aabb(const vec3& min_pt, const vec3& max_pt)
        : min_corner(min_pt), max_corner(max_pt) {}

    /** @brief Box from six coordinates (min_x, min_y, min_z, max_x, max_y, max_z), m. */
    aabb(double min_x, double min_y, double min_z,
         double max_x, double max_y, double max_z)
        : min_corner(min_x, min_y, min_z), max_corner(max_x, max_y, max_z) {}
};
//---------------------------------------------------------------------------------------------



//---------------------------------------------------------------------------------------------
/**
 * @brief Interface of the broad-phase contact detection strategies.
 *
 * A strategy returns, for a query position, the faces of other cells that may be in contact
 * with it; the contact model then performs the exact distance test. The strategy is selected
 * at run time by the `contact_detection_algorithm` parameter:
 *
 * - contact_detection_uspg_strategy (`uspg`): uniform spatial partitioning grid (USPG) of
 *   face AABBs; a query visits the voxel of the point and its 26 neighbours.
 * - contact_detection_sap_strategy (`sweep_and_prune`): sweep and prune over the AABBs of the
 *   cells; a query visits the faces of the cells whose AABB overlaps that of the query cell.
 *
 * prepare() is called once per iteration, serially, before the parallel contact loop; it may
 * throw unstable_simulation_exception when the domain has diverged. get_candidate_faces() is
 * called once per node inside the parallel loop and does not throw.
 *
 * Usage:
 * @code
 * auto strategy = contact_detection_strategy::create(algorithm, params);
 * strategy->prepare(cells, faces, aabbs, bounds);
 * // for each node n of cell c:
 * std::vector<face*> candidates = strategy->get_candidate_faces(n.pos(), c);
 * @endcode
 *
 * @ingroup contact_models
 */
class contact_detection_strategy {
public:
    virtual ~contact_detection_strategy() = default;

    /**
     * @brief Build the acceleration structure from the current face positions.
     *
     * Called once per iteration from a single thread, before the parallel contact loop.
     *
     * @param cells   All cells of the simulation.
     * @param faces   All faces of all cells.
     * @param aabbs   Bounding box of each face, in the same order as `faces`, m.
     * @param bounds  Maximum corner of the simulation domain, m (not used by every strategy).
     * @throws unstable_simulation_exception If the domain has diverged (USPG strategy).
     */
    virtual void prepare(
        const std::vector<cell*>& cells,
        const std::vector<face*>& faces,
        const std::vector<aabb>& aabbs,
        const vec3& bounds
    ) noexcept(false) = 0;

    /**
     * @brief Return the faces that may be in contact with a point.
     *
     * The result is a superset of the faces in contact: the caller still performs the AABB
     * and distance tests.
     *
     * @param query_pos  Position to query (usually a node position), m.
     * @param query_cell Cell whose own faces are excluded, or nullptr for no exclusion.
     * @return A new vector of candidate faces (possibly empty).
     * @note Safe to call concurrently from several threads after prepare().
     */
    virtual std::vector<face*> get_candidate_faces(
        const vec3& query_pos,
        cell* query_cell
    ) const noexcept = 0;

    /**
     * @brief Return the algorithm implemented by this strategy.
     * @return ContactDetectionAlgorithm::USPG or ContactDetectionAlgorithm::SWEEP_AND_PRUNE.
     */
    virtual ContactDetectionAlgorithm algorithm_type() const noexcept = 0;

    /**
     * @brief Create the strategy for an algorithm.
     * @param algorithm USPG or SWEEP_AND_PRUNE. Any other value, including ADAPTIVE, creates the
     *        USPG strategy; the adaptive switch is made by the contact model.
     * @param params    Numerical parameters (contact cutoffs and minimum edge length, m).
     * @return The new strategy; never null.
     */
    static std::unique_ptr<contact_detection_strategy> create(
        ContactDetectionAlgorithm algorithm,
        const global_simulation_parameters& params
    );
};
//---------------------------------------------------------------------------------------------

#endif // DEF_CONTACT_DETECTION_STRATEGY
