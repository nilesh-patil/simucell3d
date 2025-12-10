#include "contact_detection_uspg_strategy.hpp"

#include <algorithm>
#include <limits>
#include <cmath>
#include "morton_code.hpp"


//---------------------------------------------------------------------------------------------
// Constructor: Initialize USPG strategy from simulation parameters
//---------------------------------------------------------------------------------------------
contact_detection_uspg_strategy::contact_detection_uspg_strategy(
    const global_simulation_parameters& params
) noexcept
    : grid_(nullptr)
    , voxel_size_(0.0)
    , aabb_padding_(0.0)
    , global_min_x_(0.0), global_min_y_(0.0), global_min_z_(0.0)
    , global_max_x_(0.0), global_max_y_(0.0), global_max_z_(0.0)
{
    // Compute AABB padding from contact cutoffs
    // This ensures faces within interaction distance are found
    aabb_padding_ = std::max(params.contact_cutoff_repulsion_, params.contact_cutoff_adhesion_);

    // Compute voxel size to balance query efficiency vs. memory usage
    // Voxel should be large enough to contain typical face AABBs
    // Formula: 3 * min_edge_len (typical face span) + 2 * padding (for neighbor inclusion)
    voxel_size_ = params.min_edge_len_ * 3.0 + 2.0 * aabb_padding_;
}
//---------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------
// Prepare the USPG grid with current face data
//---------------------------------------------------------------------------------------------
void contact_detection_uspg_strategy::prepare(
    const std::vector<cell*>& cells,
    const std::vector<face*>& faces,
    const std::vector<aabb>& aabbs,
    const vec3& bounds
) noexcept {
    // Handle empty input gracefully
    if (faces.empty()) {
        face_lst_.clear();
        face_aabb_lst_.clear();
        grid_.reset();
        return;
    }

    // Store face pointers for later reference
    face_lst_ = faces;

    // Convert AABBs to flat storage format (6 doubles per face)
    // and compute global bounds
    face_aabb_lst_.clear();
    face_aabb_lst_.reserve(faces.size() * 6);

    global_min_x_ = std::numeric_limits<double>::infinity();
    global_min_y_ = std::numeric_limits<double>::infinity();
    global_min_z_ = std::numeric_limits<double>::infinity();

    global_max_x_ = -std::numeric_limits<double>::infinity();
    global_max_y_ = -std::numeric_limits<double>::infinity();
    global_max_z_ = -std::numeric_limits<double>::infinity();

    for (size_t i = 0; i < faces.size(); ++i) {
        const aabb& box = aabbs[i];

        // Extract AABB corners with padding
        const double face_min_x = box.min_corner.dx() - aabb_padding_;
        const double face_min_y = box.min_corner.dy() - aabb_padding_;
        const double face_min_z = box.min_corner.dz() - aabb_padding_;
        const double face_max_x = box.max_corner.dx() + aabb_padding_;
        const double face_max_y = box.max_corner.dy() + aabb_padding_;
        const double face_max_z = box.max_corner.dz() + aabb_padding_;

        // Update global bounds
        if (face_min_x < global_min_x_) global_min_x_ = face_min_x;
        if (face_min_y < global_min_y_) global_min_y_ = face_min_y;
        if (face_min_z < global_min_z_) global_min_z_ = face_min_z;

        if (face_max_x > global_max_x_) global_max_x_ = face_max_x;
        if (face_max_y > global_max_y_) global_max_y_ = face_max_y;
        if (face_max_z > global_max_z_) global_max_z_ = face_max_z;

        // Store AABB in flat format
        face_aabb_lst_.insert(face_aabb_lst_.end(),
            {face_min_x, face_min_y, face_min_z, face_max_x, face_max_y, face_max_z});
    }

    // Add padding to global bounds to ensure boundary faces are handled correctly
    global_min_x_ -= aabb_padding_;
    global_min_y_ -= aabb_padding_;
    global_min_z_ -= aabb_padding_;

    // Store faces in the USPG grid
    store_faces_in_grid();
}
//---------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------
// Store all faces in the USPG grid with Morton code sorting for cache locality
//---------------------------------------------------------------------------------------------
void contact_detection_uspg_strategy::store_faces_in_grid() noexcept {
    // Create a new grid with proper dimensions using unique_ptr
    grid_ = std::make_unique<uspg_4d<face*>>(
        global_min_x_, global_min_y_, global_min_z_,
        global_max_x_, global_max_y_, global_max_z_,
        voxel_size_, face_lst_.size()
    );

    // Get grid parameters via public accessors
    const auto [grid_min_x, grid_min_y, grid_min_z] = grid_->get_min_corner();
    const double grid_voxel_size = grid_->get_voxel_size();

    // Create Morton-sorted index for cache-friendly insertion order
    // Compute Morton code for each face centroid and sort
    struct face_morton {
        size_t index;
        uint64_t code;
    };
    std::vector<face_morton> sorted_faces;
    sorted_faces.reserve(face_lst_.size());

    // Compute global bounds for Morton encoding
    const double bounds[6] = {
        global_min_x_, global_min_y_, global_min_z_,
        global_max_x_, global_max_y_, global_max_z_
    };

    for (size_t i = 0; i < face_lst_.size(); ++i) {
        face* f = face_lst_[i];
        if (f == nullptr || !f->is_used()) continue;

        // Compute face centroid from AABB center
        const size_t aabb_pos = i * 6;
        const double cx = (face_aabb_lst_[aabb_pos] + face_aabb_lst_[aabb_pos + 3]) * 0.5;
        const double cy = (face_aabb_lst_[aabb_pos + 1] + face_aabb_lst_[aabb_pos + 4]) * 0.5;
        const double cz = (face_aabb_lst_[aabb_pos + 2] + face_aabb_lst_[aabb_pos + 5]) * 0.5;

        // Compute Morton code for centroid
        const vec3 centroid(cx, cy, cz);
        const uint64_t morton = morton_code::encode(centroid, bounds);

        sorted_faces.push_back({i, morton});
    }

    // Sort by Morton code for spatial locality
    std::sort(sorted_faces.begin(), sorted_faces.end(),
              [](const face_morton& a, const face_morton& b) { return a.code < b.code; });

    // Insert faces in Morton-sorted order for better cache locality
    for (const auto& fm : sorted_faces) {
        const size_t i = fm.index;
        face* f = face_lst_[i];

        // Get AABB position in flat storage
        const size_t aabb_pos = i * 6;

        // Extract AABB corners
        const double face_min_x = face_aabb_lst_[aabb_pos];
        const double face_min_y = face_aabb_lst_[aabb_pos + 1];
        const double face_min_z = face_aabb_lst_[aabb_pos + 2];
        const double face_max_x = face_aabb_lst_[aabb_pos + 3];
        const double face_max_y = face_aabb_lst_[aabb_pos + 4];
        const double face_max_z = face_aabb_lst_[aabb_pos + 5];

        // Compute voxel range for this face's AABB
        const unsigned voxel_x_start = static_cast<unsigned>(
            std::floor((face_min_x - grid_min_x) / grid_voxel_size));
        const unsigned voxel_y_start = static_cast<unsigned>(
            std::floor((face_min_y - grid_min_y) / grid_voxel_size));
        const unsigned voxel_z_start = static_cast<unsigned>(
            std::floor((face_min_z - grid_min_z) / grid_voxel_size));

        const unsigned voxel_x_stop = static_cast<unsigned>(
            std::floor((face_max_x - grid_min_x) / grid_voxel_size));
        const unsigned voxel_y_stop = static_cast<unsigned>(
            std::floor((face_max_y - grid_min_y) / grid_voxel_size));
        const unsigned voxel_z_stop = static_cast<unsigned>(
            std::floor((face_max_z - grid_min_z) / grid_voxel_size));

        // Insert face into all overlapping voxels using public place_object method
        for (unsigned voxel_x = voxel_x_start; voxel_x <= voxel_x_stop; ++voxel_x) {
            for (unsigned voxel_y = voxel_y_start; voxel_y <= voxel_y_stop; ++voxel_y) {
                for (unsigned voxel_z = voxel_z_start; voxel_z <= voxel_z_stop; ++voxel_z) {
                    const size_t voxel_id = grid_->get_voxel_index(voxel_x, voxel_y, voxel_z);
                    grid_->place_object(f, voxel_id);
                }
            }
        }
    }
}
//---------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------
// Get candidate faces near a query position
//---------------------------------------------------------------------------------------------
std::vector<face*> contact_detection_uspg_strategy::get_candidate_faces(
    const vec3& query_pos,
    cell* query_cell
) const noexcept {
    std::vector<face*> candidates;
    candidates.reserve(64);  // Preallocate for typical neighborhood size

    // Return empty if no faces or grid not initialized
    if (face_lst_.empty() || !grid_) {
        return candidates;
    }

    // Get grid bounds using public accessors
    const auto [grid_min_x, grid_min_y, grid_min_z] = grid_->get_min_corner();
    const auto [grid_max_x, grid_max_y, grid_max_z] = grid_->get_max_corner();

    // Check if query position is within grid bounds
    if (query_pos.dx() < grid_min_x || query_pos.dx() > grid_max_x ||
        query_pos.dy() < grid_min_y || query_pos.dy() > grid_max_y ||
        query_pos.dz() < grid_min_z || query_pos.dz() > grid_max_z) {
        return candidates;
    }

    // Get neighboring faces from USPG (includes 27-voxel neighborhood)
    // Returns vector for cache-efficient iteration
    std::vector<face*> neighbors = grid_->get_neighborhood(query_pos);

    // Filter to exclude self-contact (faces from the same cell)
    for (face* f : neighbors) {
        if (f == nullptr || !f->is_used()) continue;

        // Skip faces belonging to the query cell (no self-contact)
        if (query_cell != nullptr) {
            cell_ptr owner = f->get_owner_cell();
            if (owner && owner.get() == query_cell) {
                continue;
            }
        }

        candidates.push_back(f);
    }

    return candidates;
}
//---------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------
// Check if a point is within a face's AABB
//---------------------------------------------------------------------------------------------
bool contact_detection_uspg_strategy::point_in_aabb(
    size_t face_aabb_pos,
    const vec3& point
) const noexcept {
    if (point.dx() < face_aabb_lst_[face_aabb_pos] ||
        point.dx() > face_aabb_lst_[face_aabb_pos + 3]) return false;
    if (point.dy() < face_aabb_lst_[face_aabb_pos + 1] ||
        point.dy() > face_aabb_lst_[face_aabb_pos + 4]) return false;
    if (point.dz() < face_aabb_lst_[face_aabb_pos + 2] ||
        point.dz() > face_aabb_lst_[face_aabb_pos + 5]) return false;
    return true;
}
//---------------------------------------------------------------------------------------------
