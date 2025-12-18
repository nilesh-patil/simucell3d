#ifndef DEF_INTEGRATION_TEST_FIXTURES
#define DEF_INTEGRATION_TEST_FIXTURES

#include <vector>
#include <memory>
#include <set>
#include <cmath>
#include <algorithm>

#include "cell.hpp"
#include "face.hpp"
#include "node.hpp"
#include "vec3.hpp"
#include "custom_structures.hpp"

//---------------------------------------------------------------------------------------------
/**
 * Test fixture for two cells in contact configuration.
 *
 * Creates two cubic cells separated by a configurable distance.
 * Used for testing basic contact detection and force calculation.
 *
 * Geometry:
 *   Cell 1: centered at (0, 0, 0), side length = 1.0
 *   Cell 2: centered at (separation, 0, 0), side length = 1.0
 *
 * When separation < 1.0, cells overlap and should produce contacts.
 */
struct TwoCellContactFixture {
    std::vector<cell_ptr> cells;
    double separation;
    double contact_cutoff_adhesion;
    double contact_cutoff_repulsion;

    /**
     * Create the fixture with specified separation and cutoffs.
     *
     * @param sep Cell separation distance
     * @param cutoff_adhesion Adhesion contact cutoff
     * @param cutoff_repulsion Repulsion contact cutoff
     */
    TwoCellContactFixture(
        double sep = 0.5,
        double cutoff_adhesion = 0.3,
        double cutoff_repulsion = 0.3
    );

    /**
     * Get simulation parameters configured for this fixture.
     */
    global_simulation_parameters get_parameters(
        ContactDetectionAlgorithm algorithm = ContactDetectionAlgorithm::USPG
    ) const;

    /**
     * Count expected contacts based on geometry and cutoffs.
     * Returns number of node-face pairs within cutoff distance.
     */
    size_t count_expected_contacts() const;
};
//---------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------
/**
 * Test fixture for 8-cell cubic cluster.
 *
 * Creates a 2x2x2 arrangement of cubic cells with configurable spacing.
 * Used for testing dense contact scenarios and neighbor topology.
 *
 * Geometry:
 *   8 cells arranged in a cube pattern
 *   Cell side length = 1.0
 *   Spacing between cell centers = cell_spacing
 *
 * When cell_spacing = 1.0, cells touch at faces.
 * When cell_spacing < 1.0, cells overlap.
 */
struct EightCellClusterFixture {
    std::vector<cell_ptr> cells;
    double cell_spacing;
    double contact_cutoff;

    /**
     * Create the fixture with specified spacing and cutoff.
     *
     * @param spacing Distance between cell centers
     * @param cutoff Contact detection cutoff distance
     */
    EightCellClusterFixture(
        double spacing = 0.9,
        double cutoff = 0.3
    );

    /**
     * Get simulation parameters for this fixture.
     */
    global_simulation_parameters get_parameters(
        ContactDetectionAlgorithm algorithm = ContactDetectionAlgorithm::USPG
    ) const;

    /**
     * Get expected neighbor topology (which cells should contact which).
     * Returns pairs of (cell_index_i, cell_index_j) for i < j.
     */
    std::set<std::pair<size_t, size_t>> get_expected_neighbor_pairs() const;
};
//---------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------
/**
 * Test fixture for heterogeneous cell types.
 *
 * Creates a mixture of epithelial cells, ECM, and nuclei.
 * Used for testing cell-type-specific contact logic.
 *
 * Configuration:
 *   - 2 epithelial cells (type_id = 0)
 *   - 1 ECM cell (type_id = 1)
 *   - 1 nucleus (type_id = 3)
 */
struct HeterogeneousCellFixture {
    std::vector<cell_ptr> cells;
    std::vector<cell_type_parameters> cell_types;

    /**
     * Create the fixture with default heterogeneous configuration.
     */
    HeterogeneousCellFixture();

    /**
     * Get simulation parameters for this fixture.
     */
    global_simulation_parameters get_parameters(
        ContactDetectionAlgorithm algorithm = ContactDetectionAlgorithm::USPG
    ) const;

    /**
     * Verify that cell types are assigned correctly.
     */
    bool verify_cell_types() const;
};
//---------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------
/**
 * Contact pair representation for comparison.
 *
 * Stores a node-face contact pair with normalized ordering to enable
 * order-independent comparison between USPG and SAP results.
 */
struct ContactPair {
    size_t node_cell_id;
    size_t node_id;
    size_t face_cell_id;
    size_t face_id;

    ContactPair(size_t n_cell, size_t n_id, size_t f_cell, size_t f_id)
        : node_cell_id(n_cell), node_id(n_id),
          face_cell_id(f_cell), face_id(f_id) {}

    // For set operations
    bool operator<(const ContactPair& other) const {
        if (node_cell_id != other.node_cell_id) return node_cell_id < other.node_cell_id;
        if (node_id != other.node_id) return node_id < other.node_id;
        if (face_cell_id != other.face_cell_id) return face_cell_id < other.face_cell_id;
        return face_id < other.face_id;
    }

    bool operator==(const ContactPair& other) const {
        return node_cell_id == other.node_cell_id &&
               node_id == other.node_id &&
               face_cell_id == other.face_cell_id &&
               face_id == other.face_id;
    }
};
//---------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------
/**
 * Utility for comparing sets of contact pairs.
 *
 * Provides order-independent comparison with proper handling of
 * floating-point tolerances in contact distances.
 */
class ContactSetComparator {
public:
    /**
     * Check if two contact sets are equivalent.
     *
     * @param set1 First contact set
     * @param set2 Second contact set
     * @param allow_superset If true, set2 can be a superset of set1 (conservative SAP)
     * @return true if sets are equivalent (or set2 ⊇ set1 if allow_superset)
     */
    static bool equivalent(
        const std::set<ContactPair>& set1,
        const std::set<ContactPair>& set2,
        bool allow_superset = false
    );

    /**
     * Extract contact pairs from simulation state.
     *
     * Scans all cells and identifies node-face pairs that are coupled
     * or have non-zero contact forces.
     *
     * @param cells Cell list from simulation
     * @return Set of detected contact pairs
     */
    static std::set<ContactPair> extract_contacts(
        const std::vector<cell_ptr>& cells
    );
};
//---------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------
/**
 * Utility for comparing force vectors with proper numerical tolerances.
 *
 * Handles both relative and absolute tolerance checks for force comparisons.
 */
class ForceVectorComparator {
public:
    /**
     * Check if two force vectors are equivalent.
     *
     * Uses hybrid tolerance: max(abs_tol, rel_tol * magnitude)
     *
     * @param f1 First force vector
     * @param f2 Second force vector
     * @param rel_tol Relative tolerance (default: 1e-13)
     * @param abs_tol Absolute tolerance (default: 1e-14)
     * @return true if forces are equivalent
     */
    static bool equivalent(
        const vec3& f1,
        const vec3& f2,
        double rel_tol = 1e-13,
        double abs_tol = 1e-14
    );

    /**
     * Compute total force on all nodes in a cell.
     *
     * @param cell Cell to compute total force for
     * @return Sum of all node forces
     */
    static vec3 total_force(const cell_ptr& cell);

    /**
     * Compare force states between two cell configurations.
     *
     * @param cells1 First cell configuration
     * @param cells2 Second cell configuration
     * @param rel_tol Relative tolerance
     * @param abs_tol Absolute tolerance
     * @return true if all forces match
     */
    static bool compare_force_states(
        const std::vector<cell_ptr>& cells1,
        const std::vector<cell_ptr>& cells2,
        double rel_tol = 1e-13,
        double abs_tol = 1e-14
    );
};
//---------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------
/**
 * Utility for capturing and comparing full simulation states.
 *
 * Captures node positions, velocities, forces, and coupling state
 * for comprehensive equivalence testing.
 */
class SimulationStateCapture {
public:
    struct NodeState {
        vec3 position;
        vec3 velocity;
        vec3 force;
        size_t num_coupled_nodes;
    };

    struct CellState {
        size_t cell_id;
        std::vector<NodeState> nodes;
    };

    std::vector<CellState> cell_states;

    /**
     * Capture current simulation state.
     *
     * @param cells Cell list to capture
     */
    void capture(const std::vector<cell_ptr>& cells);

    /**
     * Compare two simulation states for equivalence.
     *
     * @param other Other state to compare to
     * @param pos_tol Position tolerance
     * @param vel_tol Velocity tolerance
     * @param force_tol Force tolerance
     * @return true if states are equivalent
     */
    bool equivalent(
        const SimulationStateCapture& other,
        double pos_tol = 1e-13,
        double vel_tol = 1e-13,
        double force_tol = 1e-13
    ) const;

    /**
     * Print state summary for debugging.
     */
    void print_summary() const;
};
//---------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------
/**
 * Helper functions for creating test geometries.
 */
namespace TestGeometryHelpers {
    /**
     * Create a cubic cell with specified center and side length.
     *
     * @param center Cell center position
     * @param side_length Length of cube sides
     * @param cell_id Cell identifier
     * @return Shared pointer to created cell
     */
    cell_ptr create_cubic_cell(
        const vec3& center,
        double side_length,
        size_t cell_id
    );

    /**
     * Create default simulation parameters for testing.
     *
     * @param algorithm Contact detection algorithm to use
     * @param cutoff_adhesion Adhesion contact cutoff
     * @param cutoff_repulsion Repulsion contact cutoff
     * @return Configured parameters
     */
    global_simulation_parameters create_default_test_parameters(
        ContactDetectionAlgorithm algorithm = ContactDetectionAlgorithm::USPG,
        double cutoff_adhesion = 0.3,
        double cutoff_repulsion = 0.3
    );

    /**
     * Deep copy cell list for state comparison.
     *
     * Creates independent copies of cells to enable before/after comparison.
     *
     * @param cells Cells to copy
     * @return Copied cell list
     */
    std::vector<cell_ptr> deep_copy_cells(const std::vector<cell_ptr>& cells);

    /**
     * Compute distance between a node and a face.
     *
     * @param node_pos Node position
     * @param face Face to compute distance to
     * @param face_cell Cell owning the face
     * @return Minimum distance from node to face
     */
    double compute_node_face_distance(
        const vec3& node_pos,
        const face& f,
        const cell_ptr& face_cell
    );

    /**
     * Check if two doubles are approximately equal.
     *
     * @param a First value
     * @param b Second value
     * @param rel_tol Relative tolerance
     * @param abs_tol Absolute tolerance
     * @return true if approximately equal
     */
    inline bool approx_equal(
        double a,
        double b,
        double rel_tol = 1e-12,
        double abs_tol = 1e-14
    ) {
        if (std::isnan(a) || std::isnan(b)) return false;
        if (std::isinf(a) || std::isinf(b)) return a == b;

        const double diff = std::abs(a - b);
        const double mag = std::max(std::abs(a), std::abs(b));

        return diff <= abs_tol || diff <= rel_tol * mag;
    }

    /**
     * Check if two vec3 are approximately equal.
     */
    inline bool approx_equal_vec3(
        const vec3& a,
        const vec3& b,
        double rel_tol = 1e-12,
        double abs_tol = 1e-14
    ) {
        return approx_equal(a.dx(), b.dx(), rel_tol, abs_tol) &&
               approx_equal(a.dy(), b.dy(), rel_tol, abs_tol) &&
               approx_equal(a.dz(), b.dz(), rel_tol, abs_tol);
    }
}
//---------------------------------------------------------------------------------------------

#endif // DEF_INTEGRATION_TEST_FIXTURES
