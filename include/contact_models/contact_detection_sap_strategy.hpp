#ifndef DEF_CONTACT_DETECTION_SAP_STRATEGY
#define DEF_CONTACT_DETECTION_SAP_STRATEGY

/**
 * @file contact_detection_sap_strategy.hpp
 * @brief Contact detection by sweep and prune over cell bounding boxes (`sweep_and_prune`).
 * @ingroup contact_models
 */

#include <vector>
#include <array>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>

#include "contact_detection_strategy.hpp"

//---------------------------------------------------------------------------------------------
/**
 * @brief Sweep-and-prune (SAP) contact detection strategy.
 *
 * Broad phase on cell bounding boxes along the three axes:
 * 1. Build cell-to-faces map (which faces belong to which cell)
 * 2. Compute cell AABBs (union of all face AABBs for each cell, with padding)
 * 3. Build sorted endpoint lists for X, Y, Z axes
 * 4. Sweep X-axis to find pairs overlapping on X
 * 5. Filter by Y-axis overlap
 * 6. Filter by Z-axis overlap
 * 7. Store the overlapping cell pairs, used by the queries
 *
 * Complexity:
 * - prepare(): O(C log C + F + P) for C cells, F faces and P overlapping cell pairs
 * - get_candidate_faces(): linear in the number of faces of the cells that overlap the query cell
 *
 * The result is conservative: every face that can be within the contact cutoffs of the query
 * point is returned, possibly with extra faces.
 *
 * @note prepare() must be called from a single thread; get_candidate_faces() may then be
 *       called concurrently.
 * @ingroup contact_models
 */
class contact_detection_sap_strategy : public contact_detection_strategy {
private:
    //-----------------------------------------------------------------------------------------
    // Data Structures
    //-----------------------------------------------------------------------------------------

    /**
     * AABB for each cell - union of all face AABBs plus padding.
     * The padding ensures conservative detection at contact distance.
     */
    struct CellAABB {
        double min_x, min_y, min_z;
        double max_x, max_y, max_z;
        cell* cell_ptr;
        size_t cell_index;

        CellAABB() : min_x(0), min_y(0), min_z(0),
                     max_x(0), max_y(0), max_z(0),
                     cell_ptr(nullptr), cell_index(0) {}

        CellAABB(double minx, double miny, double minz,
                 double maxx, double maxy, double maxz,
                 cell* c, size_t idx)
            : min_x(minx), min_y(miny), min_z(minz),
              max_x(maxx), max_y(maxy), max_z(maxz),
              cell_ptr(c), cell_index(idx) {}
    };

    /**
     * Endpoint for axis sweep.
     * Represents either the min or max boundary of a cell's AABB on an axis.
     */
    struct Endpoint {
        double value;
        size_t cell_index;
        bool is_min;  // true for min endpoint, false for max

        Endpoint() : value(0), cell_index(0), is_min(true) {}
        Endpoint(double v, size_t idx, bool min)
            : value(v), cell_index(idx), is_min(min) {}

        // Sort by position, with min endpoints before max at same position
        bool operator<(const Endpoint& other) const {
            if (value != other.value) return value < other.value;
            // At same position, min comes before max (conservative)
            return is_min && !other.is_min;
        }
    };

    /**
     * Hash function for size_t pairs used in unordered_set.
     * Uses XOR with bit shift for reasonable distribution.
     */
    struct PairHash {
        size_t operator()(const std::pair<size_t, size_t>& p) const noexcept {
            // Combine hashes using XOR with shift for better distribution
            return std::hash<size_t>()(p.first) ^ (std::hash<size_t>()(p.second) << 1);
        }
    };

    //-----------------------------------------------------------------------------------------
    // Member Variables
    //-----------------------------------------------------------------------------------------

    // AABB padding = max(contact_cutoff_adhesion, contact_cutoff_repulsion)
    double aabb_padding_;

    // Cell AABBs computed during prepare()
    std::vector<CellAABB> cell_aabbs_;

    // Maps cell pointer to index in cell_aabbs_
    std::unordered_map<cell*, size_t> cell_to_index_;

    // Faces for each cell: cell_faces_[i] = faces belonging to cell i
    std::vector<std::vector<face*>> cell_faces_;

    // Face AABBs stored per-cell parallel to cell_faces_ for O(1) indexed access
    // cell_face_aabbs_[cell_idx][face_idx] = {min_x, min_y, min_z, max_x, max_y, max_z}
    // This avoids hash map lookups in the hot path of get_candidate_faces()
    std::vector<std::vector<std::array<double, 6>>> cell_face_aabbs_;

    // Overlapping cell pairs after 3-axis sweep
    // Stored as (smaller_index, larger_index) to avoid duplicates
    // Using unordered_set for O(1) insertion instead of O(log n) with std::set
    std::unordered_set<std::pair<size_t, size_t>, PairHash> overlapping_pairs_;

    // For each cell, which other cells overlap with it
    // overlaps_for_cell_[i] = indices of cells overlapping with cell i
    std::vector<std::vector<size_t>> overlaps_for_cell_;

    //-----------------------------------------------------------------------------------------
    // Helper Methods
    //-----------------------------------------------------------------------------------------

    /**
     * Compute cell AABBs from face AABBs.
     * Each cell's AABB is the union of all its faces' AABBs, plus padding.
     */
    void compute_cell_aabbs(
        const std::vector<cell*>& cells,
        const std::vector<face*>& faces,
        const std::vector<aabb>& face_aabbs
    ) noexcept;

    /**
     * Perform 3-axis sweep-and-prune to find overlapping cell pairs.
     * Sweeps X first, then filters by Y and Z overlap.
     */
    void sweep_and_prune() noexcept;

    /**
     * Check if two cell AABBs overlap on a specific axis.
     */
    static bool overlaps_on_axis(
        const CellAABB& a, const CellAABB& b,
        int axis  // 0=X, 1=Y, 2=Z
    ) noexcept;

    /**
     * Check if two cell AABBs overlap on all 3 axes.
     */
    static bool aabbs_overlap(const CellAABB& a, const CellAABB& b) noexcept;

    /**
     * Check if a point is inside a cell AABB.
     */
    static bool point_in_aabb(const vec3& point, const CellAABB& box) noexcept;

    /**
     * Build the overlaps_for_cell_ lookup structure from overlapping_pairs_.
     */
    void build_overlap_lookup() noexcept;

public:
    /**
     * @brief Set the AABB padding to the larger of the two contact cutoffs.
     * @param params Numerical parameters; uses `contact_cutoff_adhesion_` and
     *        `contact_cutoff_repulsion_` (m).
     */
    explicit contact_detection_sap_strategy(const global_simulation_parameters& params) noexcept;

    /**
     * @brief Group the faces by cell, compute the padded cell AABBs and find the overlapping cell pairs.
     *
     * Must be called from a single thread before the parallel queries. Does not throw.
     *
     * @param cells  All cells of the simulation.
     * @param faces  All faces of all cells.
     * @param aabbs  Bounding box of each face, in the same order as `faces`, m.
     * @param bounds Maximum corner of the simulation domain, m.
     */
    void prepare(
        const std::vector<cell*>& cells,
        const std::vector<face*>& faces,
        const std::vector<aabb>& aabbs,
        const vec3& bounds
    ) noexcept override;

    /**
     * @brief Return the faces of the cells overlapping `query_cell` whose AABB contains the point.
     *
     * The faces of `query_cell` itself are excluded. If `query_cell` is nullptr or unknown, every
     * cell whose AABB contains the point is searched.
     *
     * @param query_pos  Position to query (usually a node position), m.
     * @param query_cell Cell whose own faces are excluded, or nullptr.
     * @return A new vector of candidate faces.
     * @note Thread-safe after prepare().
     */
    std::vector<face*> get_candidate_faces(
        const vec3& query_pos,
        cell* query_cell
    ) const noexcept override;

    /** @brief Return ContactDetectionAlgorithm::SWEEP_AND_PRUNE. */
    ContactDetectionAlgorithm algorithm_type() const noexcept override {
        return ContactDetectionAlgorithm::SWEEP_AND_PRUNE;
    }
};
//---------------------------------------------------------------------------------------------

#endif // DEF_CONTACT_DETECTION_SAP_STRATEGY
