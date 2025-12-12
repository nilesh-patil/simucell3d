#ifndef DEF_CONTACT_DETECTION_SAP_STRATEGY
#define DEF_CONTACT_DETECTION_SAP_STRATEGY

#include <vector>
#include <array>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>

#include "contact_detection_strategy.hpp"

//---------------------------------------------------------------------------------------------
/**
 * Sweep-and-Prune (SAP) contact detection strategy.
 *
 * This strategy implements a 3-axis Sweep-and-Prune algorithm for broad-phase
 * contact detection. It is optimized for systems with non-uniform cell distributions,
 * clustering, and varying mesh resolution.
 *
 * Algorithm Overview:
 * 1. Build cell-to-faces map (which faces belong to which cell)
 * 2. Compute cell AABBs (union of all face AABBs for each cell, with padding)
 * 3. Build sorted endpoint lists for X, Y, Z axes
 * 4. Sweep X-axis to find pairs overlapping on X
 * 5. Filter by Y-axis overlap
 * 6. Filter by Z-axis overlap
 * 7. Store overlapping cell pairs for fast queries
 *
 * Complexity:
 * - prepare(): O(C log C + F) where C = cells, F = faces
 * - get_candidate_faces(): O(K) where K = overlapping cells
 *
 * Conservative Property:
 * SAP MUST be conservative - it returns ALL faces that could potentially contact.
 * False positives are acceptable (extra faces), but false negatives (missing faces)
 * are NOT allowed as they break physics simulation correctness.
 *
 * Thread Safety:
 * - prepare() must be called from a single thread before parallel queries
 * - get_candidate_faces() is const and thread-safe for concurrent queries
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
     * Construct SAP strategy from simulation parameters.
     *
     * Extracts contact_cutoff_adhesion and contact_cutoff_repulsion from params
     * to compute AABB padding = max(adhesion, repulsion).
     *
     * @param params Simulation parameters containing cutoff distances
     */
    explicit contact_detection_sap_strategy(const global_simulation_parameters& params) noexcept;

    /**
     * Prepare the SAP structure with current simulation data.
     *
     * Implementation:
     * 1. Build cell-to-faces mapping
     * 2. Compute cell AABBs (union of face AABBs + padding)
     * 3. Perform 3-axis sweep-and-prune
     * 4. Store overlapping pairs for fast queries
     *
     * Thread Safety: Must be called from single thread before parallel queries.
     *
     * @param cells All cells in the simulation
     * @param faces All faces from all cells
     * @param aabbs AABBs for each face (parallel to faces vector)
     * @param bounds Simulation domain bounds
     */
    void prepare(
        const std::vector<cell*>& cells,
        const std::vector<face*>& faces,
        const std::vector<aabb>& aabbs,
        const vec3& bounds
    ) noexcept override;

    /**
     * Get candidate faces for contact detection.
     *
     * Returns faces from all cells that overlap with query_cell's AABB.
     * The query_cell's own faces are excluded to prevent self-contact.
     *
     * Conservative Property: Returns ALL potentially contacting faces.
     * May include false positives (extra faces) but NEVER false negatives.
     *
     * Thread Safety: This method is const and thread-safe.
     *
     * @param query_pos Position being queried (typically a node position)
     * @param query_cell Cell to exclude from results (nullptr for no exclusion)
     * @return Vector of candidate face pointers
     */
    std::vector<face*> get_candidate_faces(
        const vec3& query_pos,
        cell* query_cell
    ) const noexcept override;

    ContactDetectionAlgorithm algorithm_type() const noexcept override {
        return ContactDetectionAlgorithm::SWEEP_AND_PRUNE;
    }
};
//---------------------------------------------------------------------------------------------

#endif // DEF_CONTACT_DETECTION_SAP_STRATEGY
