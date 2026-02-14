#include "contact_detection_sap_strategy.hpp"
#include "face.hpp"
#include "cell.hpp"

#include <limits>
#include <cmath>

#ifdef _OPENMP
#include <omp.h>
#endif


//---------------------------------------------------------------------------------------------
// Constructor: Initialize SAP strategy with contact cutoff from parameters
//---------------------------------------------------------------------------------------------
contact_detection_sap_strategy::contact_detection_sap_strategy(
    const global_simulation_parameters& params
) noexcept {
    // AABB padding = max(adhesion_cutoff, repulsion_cutoff)
    // This ensures conservative detection at contact distance
    aabb_padding_ = std::max(
        params.contact_cutoff_adhesion_,
        params.contact_cutoff_repulsion_
    );
}
//---------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------
// Compute cell AABBs from face AABBs
//
// Algorithm:
// 1. Build face-to-index map for O(1) AABB lookup
// 2. Build cell-to-faces mapping (O(F) total) - parallelized with thread-local buffers
// 3. Store face AABBs parallel to cell_faces_ for O(1) indexed access during queries
// 4. For each cell, iterate only its own faces (O(faces_per_cell), not O(F)!)
// 5. Expand by padding for conservative detection
//
// Complexity: O(F) for mapping + O(C * avg_faces_per_cell) = O(F) total
// Previous implementation was O(C * F) due to nested loop bug
//
// OpenMP Parallelization:
// - Cell-to-faces mapping: thread-local vectors merged sequentially to avoid race conditions
// - Cell AABB computation: fully parallel with schedule(runtime)
//---------------------------------------------------------------------------------------------
void contact_detection_sap_strategy::compute_cell_aabbs(
    const std::vector<cell*>& cells,
    const std::vector<face*>& faces,
    const std::vector<aabb>& face_aabbs
) noexcept {
    // Clear previous data
    cell_aabbs_.clear();
    cell_to_index_.clear();
    cell_faces_.clear();
    cell_face_aabbs_.clear();

    if (cells.empty()) {
        return;
    }

    const size_t num_cells = cells.size();
    const size_t num_faces = faces.size();

    // Reserve capacity to avoid rehashing
    cell_faces_.resize(num_cells);
    cell_face_aabbs_.resize(num_cells);
    cell_to_index_.reserve(num_cells);

    // Build cell-to-index mapping (sequential - hash map insertion not thread-safe)
    for (size_t i = 0; i < num_cells; ++i) {
        cell_to_index_[cells[i]] = i;
    }

    // Build face-to-index map for O(1) AABB lookup
    // This allows us to find the AABB for any face pointer in constant time
    std::unordered_map<face*, size_t> face_to_index;
    face_to_index.reserve(num_faces);
    for (size_t f_idx = 0; f_idx < num_faces; ++f_idx) {
        if (faces[f_idx] != nullptr) {
            face_to_index[faces[f_idx]] = f_idx;
        }
    }

    //-----------------------------------------------------------------------------------------
    // Build cell-to-faces mapping using thread-local vectors to avoid race conditions
    //
    // Problem: Multiple faces can belong to the same cell, so parallel writes to
    // cell_faces_[c_idx].push_back() would cause race conditions.
    //
    // Solution: Each thread builds its own local vectors, then we merge them sequentially.
    // This approach has O(num_threads * num_cells) merge overhead but scales well for
    // large face counts where the parallel face processing dominates.
    //-----------------------------------------------------------------------------------------
#ifdef _OPENMP
    const int max_threads = omp_get_max_threads();
#else
    const int max_threads = 1;
#endif

    // Thread-local storage for face data
    // Each thread t stores: thread_local_faces[t][cell_idx] = vector of (face*, face_aabb)
    using FaceWithAABB = std::pair<face*, std::array<double, 6>>;
    std::vector<std::vector<std::vector<FaceWithAABB>>> thread_local_faces(
        max_threads, std::vector<std::vector<FaceWithAABB>>(num_cells)
    );

    // Parallel face-to-cell assignment with thread-local buffers
    #pragma omp parallel for schedule(runtime)
    for (size_t f_idx = 0; f_idx < num_faces; ++f_idx) {
        face* f = faces[f_idx];
        if (f == nullptr) continue;

        // Get the owner cell of this face
        cell* owner = f->get_owner_cell().get();
        if (owner == nullptr) continue;

        // Find cell index (read-only access to cell_to_index_ is thread-safe)
        auto it = cell_to_index_.find(owner);
        if (it != cell_to_index_.end()) {
            const size_t c_idx = it->second;
            const aabb& fa = face_aabbs[f_idx];

            // Compute padded face AABB
            std::array<double, 6> padded_aabb = {{
                fa.min_corner.dx() - aabb_padding_,
                fa.min_corner.dy() - aabb_padding_,
                fa.min_corner.dz() - aabb_padding_,
                fa.max_corner.dx() + aabb_padding_,
                fa.max_corner.dy() + aabb_padding_,
                fa.max_corner.dz() + aabb_padding_
            }};

            // Store in thread-local buffer (no synchronization needed)
#ifdef _OPENMP
            const int tid = omp_get_thread_num();
#else
            const int tid = 0;
#endif
            thread_local_faces[tid][c_idx].emplace_back(f, padded_aabb);
        }
    }

    // Merge thread-local buffers into final cell_faces_ and cell_face_aabbs_
    // This is sequential but fast (just moving/copying pointers and small arrays)
    for (int t = 0; t < max_threads; ++t) {
        for (size_t c_idx = 0; c_idx < num_cells; ++c_idx) {
            for (const auto& face_data : thread_local_faces[t][c_idx]) {
                cell_faces_[c_idx].push_back(face_data.first);
                cell_face_aabbs_[c_idx].push_back(face_data.second);
            }
        }
    }

    //-----------------------------------------------------------------------------------------
    // Compute cell AABBs in parallel
    //
    // Each cell's AABB computation is independent - we just need to find the bounding box
    // of all faces belonging to that cell. This is embarrassingly parallel.
    //-----------------------------------------------------------------------------------------

    // Pre-allocate cell_aabbs_ to enable parallel writes to known indices
    cell_aabbs_.resize(num_cells);

    #pragma omp parallel for schedule(runtime)
    for (size_t c_idx = 0; c_idx < num_cells; ++c_idx) {
        cell* c = cells[c_idx];

        // Initialize with extreme values
        double min_x = std::numeric_limits<double>::max();
        double min_y = std::numeric_limits<double>::max();
        double min_z = std::numeric_limits<double>::max();
        double max_x = std::numeric_limits<double>::lowest();
        double max_y = std::numeric_limits<double>::lowest();
        double max_z = std::numeric_limits<double>::lowest();

        // Use stored cell face AABBs (without padding) to compute cell AABB
        // Note: cell_face_aabbs_ includes padding, so we access original face_aabbs
        for (face* f : cell_faces_[c_idx]) {
            // Look up face index in O(1) time (read-only, thread-safe)
            auto fit = face_to_index.find(f);
            if (fit == face_to_index.end()) continue;

            // Get AABB for this face (without padding for cell AABB computation)
            const aabb& fa = face_aabbs[fit->second];
            min_x = std::min(min_x, fa.min_corner.dx());
            min_y = std::min(min_y, fa.min_corner.dy());
            min_z = std::min(min_z, fa.min_corner.dz());
            max_x = std::max(max_x, fa.max_corner.dx());
            max_y = std::max(max_y, fa.max_corner.dy());
            max_z = std::max(max_z, fa.max_corner.dz());
        }

        // If no faces found for this cell, create degenerate AABB
        if (min_x > max_x) {
            cell_aabbs_[c_idx] = CellAABB(0, 0, 0, 0, 0, 0, c, c_idx);
            continue;
        }

        // Apply padding for conservative detection
        min_x -= aabb_padding_;
        min_y -= aabb_padding_;
        min_z -= aabb_padding_;
        max_x += aabb_padding_;
        max_y += aabb_padding_;
        max_z += aabb_padding_;

        cell_aabbs_[c_idx] = CellAABB(min_x, min_y, min_z, max_x, max_y, max_z, c, c_idx);
    }
}
//---------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------
// Perform 3-axis Sweep-and-Prune
//
// Algorithm:
// 1. Build endpoint list for X-axis (min and max for each cell) - parallelized
// 2. Sort endpoints by position
// 3. Sweep through X endpoints, maintaining active set (sequential - dependency on active set)
//    - When entering a cell (min endpoint), check FULL 3-axis overlap with all active cells
//    - When exiting a cell (max endpoint), remove from active set
//
// OPTIMIZATION: Instead of creating intermediate x_overlapping and xy_overlapping sets,
// we immediately check Y and Z overlap during the X sweep. This eliminates two temporary
// sets and reduces memory allocation overhead.
//
// OpenMP Parallelization:
// - Endpoint construction: parallel writes to pre-allocated array (each cell writes to unique indices)
// - Sorting: std::sort is already efficient, no parallelization needed
// - Sweep phase: NOT parallelized due to sequential dependency on active set
//
// Complexity: O(C log C) for sorting + O(C * avg_active_cells) for sweep
//---------------------------------------------------------------------------------------------
void contact_detection_sap_strategy::sweep_and_prune() noexcept {
    overlapping_pairs_.clear();

    const size_t num_cells = cell_aabbs_.size();
    if (num_cells < 2) {
        // No pairs possible with fewer than 2 cells
        build_overlap_lookup();
        return;
    }

    //-----------------------------------------------------------------------------------------
    // Build X-axis endpoints in parallel
    //
    // Each cell contributes exactly 2 endpoints (min and max) at indices [2*i] and [2*i+1].
    // Pre-allocating and writing to known indices allows safe parallel execution.
    //-----------------------------------------------------------------------------------------
    std::vector<Endpoint> x_endpoints(num_cells * 2);

    #pragma omp parallel for schedule(runtime)
    for (size_t i = 0; i < num_cells; ++i) {
        const CellAABB& box = cell_aabbs_[i];
        x_endpoints[i * 2] = Endpoint(box.min_x, i, true);       // min endpoint
        x_endpoints[i * 2 + 1] = Endpoint(box.max_x, i, false);  // max endpoint
    }

    // Sort endpoints (std::sort is already efficient; parallel sort would add complexity
    // for marginal gains at typical cell counts)
    std::sort(x_endpoints.begin(), x_endpoints.end());

    // OPTIMIZATION: Use unordered_set for O(1) amortized operations instead of O(log n)
    std::unordered_set<size_t> active_cells;
    active_cells.reserve(num_cells / 4);  // Estimate: ~25% of cells active at any time

    // Reserve estimated capacity for overlapping pairs
    overlapping_pairs_.reserve(num_cells);  // Estimate: ~1 overlap per cell on average

    // Sweep X-axis and immediately check Y/Z overlap
    // OPTIMIZATION: Combined 3-axis check eliminates intermediate sets
    for (const Endpoint& ep : x_endpoints) {
        if (ep.is_min) {
            // Entering a cell - check FULL 3-axis overlap with all active cells
            const CellAABB& a = cell_aabbs_[ep.cell_index];

            for (size_t active_idx : active_cells) {
                const CellAABB& b = cell_aabbs_[active_idx];

                // Check Y and Z overlap immediately instead of storing in intermediate sets
                if (overlaps_on_axis(a, b, 1) && overlaps_on_axis(a, b, 2)) {
                    // Store pair as (smaller, larger) to avoid duplicates
                    size_t smaller = std::min(ep.cell_index, active_idx);
                    size_t larger = std::max(ep.cell_index, active_idx);
                    overlapping_pairs_.emplace(smaller, larger);
                }
            }
            active_cells.insert(ep.cell_index);
        } else {
            // Exiting a cell - remove from active set
            active_cells.erase(ep.cell_index);
        }
    }

    // Build lookup structure for fast queries
    build_overlap_lookup();
}
//---------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------
// Check if two AABBs overlap on a specific axis
//---------------------------------------------------------------------------------------------
bool contact_detection_sap_strategy::overlaps_on_axis(
    const CellAABB& a, const CellAABB& b,
    int axis
) noexcept {
    // Small epsilon for numerical tolerance
    constexpr double EPS = 1e-10;

    switch (axis) {
        case 0:  // X-axis
            return (a.max_x >= b.min_x - EPS) && (a.min_x <= b.max_x + EPS);
        case 1:  // Y-axis
            return (a.max_y >= b.min_y - EPS) && (a.min_y <= b.max_y + EPS);
        case 2:  // Z-axis
            return (a.max_z >= b.min_z - EPS) && (a.min_z <= b.max_z + EPS);
        default:
            return false;
    }
}
//---------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------
// Check if two AABBs overlap on all 3 axes
//---------------------------------------------------------------------------------------------
bool contact_detection_sap_strategy::aabbs_overlap(
    const CellAABB& a, const CellAABB& b
) noexcept {
    return overlaps_on_axis(a, b, 0) &&  // X
           overlaps_on_axis(a, b, 1) &&  // Y
           overlaps_on_axis(a, b, 2);    // Z
}
//---------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------
// Check if a point is inside a cell AABB
//---------------------------------------------------------------------------------------------
bool contact_detection_sap_strategy::point_in_aabb(
    const vec3& point, const CellAABB& box
) noexcept {
    constexpr double EPS = 1e-10;

    return (point.dx() >= box.min_x - EPS) && (point.dx() <= box.max_x + EPS) &&
           (point.dy() >= box.min_y - EPS) && (point.dy() <= box.max_y + EPS) &&
           (point.dz() >= box.min_z - EPS) && (point.dz() <= box.max_z + EPS);
}
//---------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------
// Build the overlaps_for_cell_ lookup structure
//
// Converts the set of overlapping pairs into per-cell adjacency lists
// for O(1) lookup during queries.
//---------------------------------------------------------------------------------------------
void contact_detection_sap_strategy::build_overlap_lookup() noexcept {
    const size_t num_cells = cell_aabbs_.size();
    const size_t num_pairs = overlapping_pairs_.size();

    // Clear and resize
    overlaps_for_cell_.clear();
    overlaps_for_cell_.resize(num_cells);

    // Pre-reserve estimated capacity for each cell's overlap list
    // Average overlaps per cell = 2 * num_pairs / num_cells (each pair adds to 2 cells)
    if (num_cells > 0 && num_pairs > 0) {
        const size_t avg_overlaps = std::max(size_t(1), (2 * num_pairs) / num_cells);
        for (size_t i = 0; i < num_cells; ++i) {
            overlaps_for_cell_[i].reserve(avg_overlaps);
        }
    }

    // Build adjacency lists from pairs
    for (const auto& pair : overlapping_pairs_) {
        // Both cells overlap with each other
        overlaps_for_cell_[pair.first].push_back(pair.second);
        overlaps_for_cell_[pair.second].push_back(pair.first);
    }
}
//---------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------
// Prepare the SAP structure
//---------------------------------------------------------------------------------------------
void contact_detection_sap_strategy::prepare(
    const std::vector<cell*>& cells,
    const std::vector<face*>& faces,
    const std::vector<aabb>& aabbs,
    const vec3& bounds
) noexcept {
    (void)bounds;  // Not used in SAP, but part of interface

    // Handle empty inputs gracefully
    if (cells.empty() || faces.empty()) {
        cell_aabbs_.clear();
        cell_to_index_.clear();
        cell_faces_.clear();
        cell_face_aabbs_.clear();
        overlapping_pairs_.clear();
        overlaps_for_cell_.clear();
        return;
    }

    // Step 1-2: Build cell-to-faces mapping and compute cell AABBs
    compute_cell_aabbs(cells, faces, aabbs);

    // Step 3-6: Perform 3-axis sweep-and-prune
    sweep_and_prune();
}
//---------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------
// Get candidate faces for a query
//
// Algorithm:
// 1. Find the query cell's index (if provided)
// 2. Get all cells that overlap with query cell (precomputed during SAP)
// 3. For each overlapping cell, filter faces to only those whose AABB contains query_pos
// 4. Return filtered faces (excluding query cell's faces)
//
// OPTIMIZATION: Instead of returning ALL faces from overlapping cells (which caused
// 15x slowdown vs USPG), we filter faces by position using stored face AABBs.
// Face AABBs are stored parallel to cell_faces_ for O(1) indexed access,
// avoiding hash map lookups in the hot path.
//
// Conservative Property:
// We return faces from cells that overlap with query cell AND whose individual
// AABBs contain the query position. This guarantees no false negatives.
//---------------------------------------------------------------------------------------------
std::vector<face*> contact_detection_sap_strategy::get_candidate_faces(
    const vec3& query_pos,
    cell* query_cell
) const noexcept {
    std::vector<face*> candidates;

    // Handle empty state
    if (cell_aabbs_.empty() || cell_faces_.empty()) {
        return candidates;
    }

    // Small epsilon for numerical tolerance in AABB containment tests
    constexpr double EPS = 1e-10;

    // Extract query position components once for efficiency
    const double qx = query_pos.dx();
    const double qy = query_pos.dy();
    const double qz = query_pos.dz();

    // Helper lambda to check if query position is within a face's AABB using indexed access
    // OPTIMIZATION: Uses O(1) array indexing instead of hash map lookup
    auto point_in_face_aabb_indexed = [&](size_t cell_idx, size_t face_idx) -> bool {
        const std::array<double, 6>& box = cell_face_aabbs_[cell_idx][face_idx];
        return (qx >= box[0] - EPS) && (qx <= box[3] + EPS) &&
               (qy >= box[1] - EPS) && (qy <= box[4] + EPS) &&
               (qz >= box[2] - EPS) && (qz <= box[5] + EPS);
    };

    // If no query cell, find faces from all cells whose AABB contains the query position
    if (query_cell == nullptr) {
        for (size_t i = 0; i < cell_aabbs_.size(); ++i) {
            if (point_in_aabb(query_pos, cell_aabbs_[i])) {
                // Only add faces whose AABB actually contains the query position
                const std::vector<face*>& faces = cell_faces_[i];
                for (size_t f_idx = 0; f_idx < faces.size(); ++f_idx) {
                    if (point_in_face_aabb_indexed(i, f_idx)) {
                        candidates.push_back(faces[f_idx]);
                    }
                }
            }
        }
        return candidates;
    }

    // Find query cell's index
    auto it = cell_to_index_.find(query_cell);
    if (it == cell_to_index_.end()) {
        // Query cell not in our index - fall back to checking all cells
        for (size_t i = 0; i < cell_aabbs_.size(); ++i) {
            if (point_in_aabb(query_pos, cell_aabbs_[i])) {
                const std::vector<face*>& faces = cell_faces_[i];
                for (size_t f_idx = 0; f_idx < faces.size(); ++f_idx) {
                    if (point_in_face_aabb_indexed(i, f_idx)) {
                        candidates.push_back(faces[f_idx]);
                    }
                }
            }
        }
        return candidates;
    }

    const size_t query_idx = it->second;

    // Get all cells that overlap with query cell (precomputed during sweep_and_prune)
    if (query_idx < overlaps_for_cell_.size()) {
        const std::vector<size_t>& overlapping_cells = overlaps_for_cell_[query_idx];

        // Reserve estimated capacity (will be filtered down significantly)
        candidates.reserve(overlapping_cells.size() * 10);  // Estimate ~10 faces per cell near query

        // For each overlapping cell, add only faces whose AABB contains query_pos
        // OPTIMIZATION: Uses O(1) indexed access to face AABBs instead of hash map lookups
        for (size_t other_idx : overlapping_cells) {
            if (other_idx >= cell_faces_.size()) continue;

            const std::vector<face*>& faces = cell_faces_[other_idx];
            for (size_t f_idx = 0; f_idx < faces.size(); ++f_idx) {
                if (point_in_face_aabb_indexed(other_idx, f_idx)) {
                    candidates.push_back(faces[f_idx]);
                }
            }
        }
    }

    return candidates;
}
//---------------------------------------------------------------------------------------------
