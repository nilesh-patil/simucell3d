#include "contact_face_face_via_coupling.hpp"
#include <omp.h>
#include <algorithm>
#include <array>

/*
Couple pairs of faces between adjacent cells. This is the most computationaly expensive contact model.
*/


#if CONTACT_MODEL_INDEX == 2

//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//The trivial constructor of the contact model
contact_face_face_via_coupling::contact_face_face_via_coupling(const global_simulation_parameters& sim_parameters) noexcept(false): contact_model_abstract(sim_parameters){
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------


//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//Find the contacts between the faces of the mesh and apply the contact forces
void contact_face_face_via_coupling::run(const std::vector<cell_ptr>& cell_lst) noexcept{

        //Give to each face a global id which is the index of the face in the face_lst_ vector. The ids follow
        //the (cell, face) order of a serial traversal; an exclusive scan over the per-cell counts lets the
        //cells be processed in parallel with the same numbering.
        const size_t nb_cells = cell_lst.size();
        std::vector<size_t> face_offset(nb_cells + 1, 0);
        #pragma omp parallel for schedule(static)
        for(size_t i = 0; i < nb_cells; i++){
            size_t nb_used = 0;
            for(const auto& f : cell_lst[i]->face_lst_){ if(f.is_used()) nb_used++; }
            face_offset[i + 1] = nb_used;
        }
        for(size_t i = 0; i < nb_cells; i++){ face_offset[i + 1] += face_offset[i]; }
        const size_t nb_faces = face_offset[nb_cells];

        face_lst_.resize(nb_faces);
        face_owner_gid_.resize(nb_faces);

        #pragma omp parallel for schedule(static)
        for(size_t i = 0; i < nb_cells; i++){
            const cell_ptr& c = cell_lst[i];
            const unsigned owner_gid = c->get_id();
            size_t face_global_id = face_offset[i];
            for(auto& f : c->face_lst_){
                if(f.is_used()){
                    face_lst_[face_global_id] = &f;
                    face_owner_gid_[face_global_id] = owner_gid;

                    //Set the global id of all the faces
                    f.global_face_id_ = face_global_id++;

                    //Only compiles this section if the faces store their contact energies
                    #if FACE_STORE_CONTACT_ENERGY
                        //Reset the adhesion and repulsion energies of the face
                        f.adhesion_energy_ = 0.0;
                        f.repulsion_energy_ = 0.0;
                    #endif
                }
            }

            for(node& n: c->node_lst_){
                if(n.is_used()){
                    //Reset the coupling of the node
                    n.coupled_nodes_map_.clear();
                }
            }
        }

        //Compute the axis aligned bounding box of ech face and store it in the face_aabb_lst_ vector and update the grid dimensions
        update_face_aabbs(cell_lst);

        //Bin the faces in the broad-phase grid (the SAP strategy builds its own structure in resolve_all_contacts)
        const bool use_sap = (detection_strategy_ &&
                              select_algorithm_for_cell_count(cell_lst.size()) == ContactDetectionAlgorithm::SWEEP_AND_PRUNE);
        if(!use_sap){
            build_face_grid();
            build_work_units(cell_lst);
        }

        //Find the faces that are within a distance below the contact cutoff and apply adhesive or repulsive forces
        resolve_all_contacts(cell_lst);
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------



//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//Voxel of a point in the CSR grid. Same formula as the one used to bin the face AABBs, so a node that lies in
//the AABB of a face always falls in one of the voxels the face was stored in (floor((x - min) / v) is monotonic).
size_t contact_face_face_via_coupling::node_voxel(const vec3& p) const noexcept{
    auto axis = [this](const double x, const double lo, const size_t n) -> size_t {
        const double q = std::floor((x - lo) / csr_voxel_size_);
        if(!(q > 0.)) return 0;                                   //also catches NaN
        return std::min(static_cast<size_t>(q), n - 1);
    };
    const size_t ix = axis(p.dx(), csr_min_x_, csr_nx_);
    const size_t iy = axis(p.dy(), csr_min_y_, csr_ny_);
    const size_t iz = axis(p.dz(), csr_min_z_, csr_nz_);
    return (iz * csr_ny_ + iy) * csr_nx_ + ix;
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------



//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//Bin all the face AABBs in a CSR uniform grid whose voxel edge is the mean face AABB extent
void contact_face_face_via_coupling::build_face_grid() noexcept(false){

    const size_t nb_faces = face_lst_.size();
    if(nb_faces == 0){
        csr_nx_ = csr_ny_ = csr_nz_ = 1;
        csr_voxel_size_ = 1.;
        csr_min_x_ = csr_min_y_ = csr_min_z_ = 0.;
        csr_start_.assign(2, 0);
        csr_faces_.clear();
        return;
    }

    //Mean extent of the (padded) face AABBs
    double sum_extent = 0.;
    #pragma omp parallel for schedule(static) reduction(+:sum_extent)
    for(size_t i = 0; i < nb_faces; i++){
        const double* bb = &face_aabb_lst_[i * 6];
        sum_extent += (bb[3] - bb[0]) + (bb[4] - bb[1]) + (bb[5] - bb[2]);
    }
    const double mean_extent = sum_extent / (3. * nb_faces);

    const double ext_x = global_max_x_ - global_min_x_;
    const double ext_y = global_max_y_ - global_min_y_;
    const double ext_z = global_max_z_ - global_min_z_;
    if(!std::isfinite(mean_extent) || !std::isfinite(ext_x) || !std::isfinite(ext_y) || !std::isfinite(ext_z)){
        throw unstable_simulation_exception("The bounding box of the simulation is not finite: at least one cell has diverged.");
    }

    //Voxel edge = mean face AABB extent (~8 voxels per face, a few tens of faces per voxel). Never coarser than the
    //legacy 3*l_min + 2*c voxel, and coarsened if the dense grid would have more than 4 voxels per face (sparse or
    //diverging domain), so the offsets array stays O(nb_faces).
    double v = std::min(std::max(mean_extent, 1e-12), grid_.voxel_size_);
    auto dims = [&](const double vs){
        return std::array<double, 3>{std::floor(ext_x / vs) + 1., std::floor(ext_y / vs) + 1., std::floor(ext_z / vs) + 1.};
    };
    const double max_voxels = std::max(64., 4. * static_cast<double>(nb_faces));
    for(int k = 0; k < 64; k++){
        const auto d = dims(v);
        if(d[0] * d[1] * d[2] <= max_voxels) break;
        v *= std::max(1.05, std::cbrt(d[0] * d[1] * d[2] / max_voxels));
    }
    const auto d = dims(v);
    if(d[0] * d[1] * d[2] > 1e10){
        throw unstable_simulation_exception("The contact grid would need more than 1e10 voxels: at least one cell has diverged.");
    }
    csr_voxel_size_ = v;
    csr_min_x_ = global_min_x_; csr_min_y_ = global_min_y_; csr_min_z_ = global_min_z_;
    csr_nx_ = static_cast<size_t>(d[0]); csr_ny_ = static_cast<size_t>(d[1]); csr_nz_ = static_cast<size_t>(d[2]);
    const size_t nb_voxels = csr_nx_ * csr_ny_ * csr_nz_;

    //Pass 1: voxel range of each face, and number of faces per voxel
    face_voxel_box_.resize(nb_faces * 6);
    csr_start_.assign(nb_voxels + 1, 0);
    uint32_t* const count = csr_start_.data() + 1;
    size_t nb_entries = 0;
    #pragma omp parallel for schedule(static) reduction(+:nb_entries)
    for(size_t i = 0; i < nb_faces; i++){
        const double* bb = &face_aabb_lst_[i * 6];
        uint32_t* box = &face_voxel_box_[i * 6];
        const double lo[3]  = {csr_min_x_, csr_min_y_, csr_min_z_};
        const size_t  n[3]  = {csr_nx_, csr_ny_, csr_nz_};
        for(int a = 0; a < 3; a++){
            const double q0 = std::floor((bb[a]     - lo[a]) / v);
            const double q1 = std::floor((bb[a + 3] - lo[a]) / v);
            box[a]     = static_cast<uint32_t>(std::min<double>(std::max(q0, 0.), n[a] - 1));
            box[a + 3] = static_cast<uint32_t>(std::min<double>(std::max(q1, 0.), n[a] - 1));
        }
        for(size_t z = box[2]; z <= box[5]; z++)
        for(size_t y = box[1]; y <= box[4]; y++)
        for(size_t x = box[0]; x <= box[3]; x++){
            #pragma omp atomic
            count[(z * csr_ny_ + y) * csr_nx_ + x]++;
        }
        nb_entries += static_cast<size_t>(box[3] - box[0] + 1) * (box[4] - box[1] + 1) * (box[5] - box[2] + 1);
    }
    if(nb_entries >= std::numeric_limits<uint32_t>::max()){
        throw unstable_simulation_exception("Too many face/voxel entries in the contact grid: the simulation is probably unstable.");
    }

    //Exclusive scan
    for(size_t i = 0; i < nb_voxels; i++){ csr_start_[i + 1] += csr_start_[i]; }

    //Pass 2: scatter the face ids (unordered within a voxel)
    csr_faces_.resize(nb_entries);
    csr_cursor_.assign(csr_start_.begin(), csr_start_.end() - 1);
    #pragma omp parallel for schedule(static)
    for(size_t i = 0; i < nb_faces; i++){
        const uint32_t* box = &face_voxel_box_[i * 6];
        for(size_t z = box[2]; z <= box[5]; z++)
        for(size_t y = box[1]; y <= box[4]; y++)
        for(size_t x = box[0]; x <= box[3]; x++){
            uint32_t slot;
            #pragma omp atomic capture
            slot = csr_cursor_[(z * csr_ny_ + y) * csr_nx_ + x]++;
            csr_faces_[slot] = static_cast<uint32_t>(i);
        }
    }

    //Pass 3: ascending face ids within each voxel = the visiting order of the uspg path
    #pragma omp parallel for schedule(dynamic, 1024)
    for(size_t vox = 0; vox < nb_voxels; vox++){
        const uint32_t b = csr_start_[vox], e = csr_start_[vox + 1];
        if(e - b > 1) std::sort(csr_faces_.begin() + b, csr_faces_.begin() + e);
    }
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------



//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//Split the contact work into units. The work of a node is the number of faces stored in its voxel. A cell whose
//work exceeds total / (16 * nb_threads) is cut into node ranges of about that work, so that one swollen or
//crumpled cell (thousands of faces per voxel) is shared by all the threads instead of serialising the loop.
//Normal cells stay a single unit, processed in the original cell order.
void contact_face_face_via_coupling::build_work_units(const std::vector<cell_ptr>& cell_lst) noexcept{

    const size_t nb_cells = cell_lst.size();
    cell_cost_.assign(nb_cells, 0.);

    double total = 0.;
    #pragma omp parallel for schedule(static) reduction(+:total)
    for(size_t i = 0; i < nb_cells; i++){
        const cell_ptr& c = cell_lst[i];
        const double curv_max = c->get_cell_type()->surface_coupling_max_curvature_;
        double w = 0.;
        for(const node& n : c->node_lst_){
            if(n.is_used() && n.curvature_ < curv_max){
                const size_t vox = node_voxel(n.pos());
                w += 1. + (csr_start_[vox + 1] - csr_start_[vox]);
            }
        }
        cell_cost_[i] = w;
        total += w;
    }

    const int nb_threads = std::max(1, omp_get_max_threads());
    const double budget = std::max(1., total / (16. * nb_threads));

    work_units_.clear();
    work_units_.reserve(nb_cells + 64);
    std::vector<work_unit> light;
    light.reserve(nb_cells);
    for(size_t i = 0; i < nb_cells; i++){
        const auto& nodes = cell_lst[i]->node_lst_;
        const uint32_t nb_nodes = static_cast<uint32_t>(nodes.size());
        if(cell_cost_[i] <= budget || nb_threads == 1){
            light.push_back({static_cast<uint32_t>(i), 0u, nb_nodes});
            continue;
        }
        //Heavy cell: cut the node list into pieces of ~budget work, scheduled first
        const double curv_max = cell_lst[i]->get_cell_type()->surface_coupling_max_curvature_;
        double acc = 0.;
        uint32_t begin = 0;
        for(uint32_t k = 0; k < nb_nodes; k++){
            const node& n = nodes[k];
            if(n.is_used() && n.curvature_ < curv_max){
                const size_t vox = node_voxel(n.pos());
                acc += 1. + (csr_start_[vox + 1] - csr_start_[vox]);
            }
            if(acc >= budget){
                work_units_.push_back({static_cast<uint32_t>(i), begin, k + 1});
                begin = k + 1;
                acc = 0.;
            }
        }
        if(begin < nb_nodes) work_units_.push_back({static_cast<uint32_t>(i), begin, nb_nodes});
    }
    work_units_.insert(work_units_.end(), light.begin(), light.end());
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------





//-----------------------------------------------------------------------------------------------
void contact_face_face_via_coupling::resolve_all_contacts(const std::vector<cell_ptr>& cell_lst) noexcept{

    // ADAPTIVE mode: dynamically select algorithm based on cell count
    const size_t cell_count = cell_lst.size();
    const ContactDetectionAlgorithm selected_algo = select_algorithm_for_cell_count(cell_count);

    // Check if we need to switch strategies (only in ADAPTIVE mode)
    if (configured_algorithm_ == ContactDetectionAlgorithm::ADAPTIVE) {
        const ContactDetectionAlgorithm current_algo = detection_strategy_ ?
            detection_strategy_->algorithm_type() : ContactDetectionAlgorithm::USPG;

        if (current_algo != selected_algo) {
            // Recreate strategy with the new algorithm
            detection_strategy_ = contact_detection_strategy::create(selected_algo, sim_parameters_);
        }
    }

    // Determine if we should use SAP (Sweep-and-Prune) or USPG (Uniform Space Partitioning Grid)
    // USPG uses optimized direct grid access; SAP uses the strategy interface
    const bool use_sap = (detection_strategy_ &&
                          detection_strategy_->algorithm_type() == ContactDetectionAlgorithm::SWEEP_AND_PRUNE);

    // If using SAP, prepare the strategy with current face data
    if (use_sap) {
        // Convert face_aabb_lst_ (flat vector of 6 doubles per face) to vector<aabb> format
        std::vector<aabb> aabbs;
        aabbs.reserve(face_lst_.size());
        for (size_t i = 0; i < face_lst_.size(); i++) {
            const size_t pos = i * 6;
            aabbs.emplace_back(
                face_aabb_lst_[pos], face_aabb_lst_[pos + 1], face_aabb_lst_[pos + 2],
                face_aabb_lst_[pos + 3], face_aabb_lst_[pos + 4], face_aabb_lst_[pos + 5]
            );
        }

        // Build cell pointer vector for the strategy
        std::vector<cell*> cells;
        cells.reserve(cell_lst.size());
        for (const auto& c : cell_lst) {
            cells.push_back(c.get());
        }

        // Prepare the SAP strategy with current simulation state
        vec3 bounds(global_max_x_, global_max_y_, global_max_z_);
        detection_strategy_->prepare(cells, face_lst_, aabbs, bounds);
    }

    if(use_sap){
        // Loop over the cells in parallel (SAP path unchanged)
        #pragma omp parallel for schedule(runtime)
        for(size_t cell_id = 0; cell_id < cell_lst.size(); cell_id++){
            const cell_ptr& c1 = cell_lst[cell_id];
            const double surface_coupling_max_curvature = c1->get_cell_type()->surface_coupling_max_curvature_;
            for(node& n: c1->node_lst_){
                if(n.is_used() && n.curvature_ < surface_coupling_max_curvature){
                    std::vector<face*> candidates = detection_strategy_->get_candidate_faces(n.pos(), c1.get());
                    for (face* f : candidates) {
                        assert(f != nullptr);
                        if (aabb_intersection_check(f->global_face_id_ * 6, n.pos()) &&
                            n.normal_.dot(f->normal_) < max_dot_product_repulsion_) {
                            resolve_contact(c1, f->owner_cell_, n, f);
                        }
                    }
                }
            }
        }
    }
    else{
        resolve_grid_contacts(cell_lst);
    }

    for(unsigned c1_local_id = 0; c1_local_id < cell_lst.size(); c1_local_id++){

        //Get the cell c1
        cell_ptr c1 = cell_lst[c1_local_id];


        //Loop over the nodes of c1
        for(node& n1: c1->node_lst_){

            if(n1.is_used() == false || !n1.is_coupled()){continue;}
                    
            //If this node is coupled to other nodes on other cells, and this cell has the greatest id
            bool c1_has_greatest_id = std::all_of(n1.coupled_nodes_map_.begin(), n1.coupled_nodes_map_.end(), 
                [&](const auto& coupled_node_data){
                    return c1->get_local_id() > coupled_node_data.first;
                }
            );

            //If this node is coupled to other nodes on other cells, and one of them has a greater id than this cell
            if(c1_has_greatest_id == false){continue;}

            //Loop over the coupled nodes and calculate the average position to which all the nodes should be translated
            vec3 avg_pos = n1.pos_;
            for(auto it = n1.coupled_nodes_map_.begin() ; it != n1.coupled_nodes_map_.end(); it++){

                const unsigned c2_local_id = it->first;
                const unsigned n2_local_id = it->second.first;

                //Get the adjacent cell
                assert(c2_local_id < cell_lst.size());
                cell_ptr c2 = cell_lst[c2_local_id];

                //Get the coupled node in the adjacent cell
                assert(n2_local_id < c2->node_lst_.size());
                node& n2 = c2->node_lst_[n2_local_id];

                //Add the force of the second node to the average force
                avg_pos = avg_pos + n2.pos_;
            }

            //Compute the average position
            avg_pos = avg_pos / (n1.get_nb_coupled_nodes() + 1.0);

            //Move all the points to the average position
            n1.pos_.reset(avg_pos);

            for(auto it = n1.coupled_nodes_map_.begin() ; it != n1.coupled_nodes_map_.end(); it++){

                const unsigned c2_local_id = it->first;
                const unsigned n2_local_id = it->second.first;

                //Get the adjacent cell
                assert(c2_local_id < cell_lst.size());
                cell_ptr c2 = cell_lst[c2_local_id];

                //Get the coupled node in the adjacent cell
                assert(n2_local_id < c2->node_lst_.size());
                node& n2 = c2->node_lst_[n2_local_id];

                //Add the force of the second node to the average force
                n2.pos_.reset(avg_pos);
            }
        }
    }
}
//-----------------------------------------------------------------------------------------------



//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//Prevent the surfaces of the cells to interpenetrate by applying repulsive forces on the surfaces
void contact_face_face_via_coupling::resolve_contact(const cell_ptr& c1, const cell_ptr& c2, node& n1, face* f) const noexcept{

    //Make sure that the nodes of the face are part of the cell number 2
    assert(f->n1_id_ < c2->node_lst_.size() && f->n2_id_ < c2->node_lst_.size() && f->n3_id_ < c2->node_lst_.size());

    //Get the node of the face
    node& f_n1 = c2->node_lst_[f->n1_id_];
    node& f_n2 = c2->node_lst_[f->n2_id_];
    node& f_n3 = c2->node_lst_[f->n3_id_];

    assert(f_n1.is_used() && f_n2.is_used() && f_n3.is_used());
    
    //Extract the position of the nodes of the face
    const vec3& f_n1_pos = f_n1.pos();
    const vec3& f_n2_pos = f_n2.pos();
    const vec3& f_n3_pos = f_n3.pos();


    //If the 2 cells are epithelial cell
    if(
        c1->get_cell_type_id() == 0 && c2->get_cell_type_id() == 0
    ){

        //Compute the squared distances between the node n1 and the nodes of the face
        double squared_distance_f_n1 = (n1.pos() - f_n1_pos).squared_norm();
        double squared_distance_f_n2 = (n1.pos() - f_n2_pos).squared_norm();
        double squared_distance_f_n3 = (n1.pos() - f_n3_pos).squared_norm();

        //Check if the node of c1 has already been coupled to a node of c2.
        //GitHub issue #113: coupled_nodes_map_ is keyed by the coupled cell's LOCAL id
        //(see set_coupled_node_and_min_distance() below, and run()/time_integration which
        //index cell_lst with the key). Looking it up by the GLOBAL id missed on every cell
        //whose global id had drifted from its local id - i.e. on every cell once the tissue
        //started dividing - so previous_min_squared_distance stayed DBL_MAX, the
        //"keep the closest partner" test at the bottom of this block always passed, and the
        //coupling partner became whichever candidate the threads happened to visit last.
        //Coupled nodes are then teleported onto their common average position by run() and
        //by the integrator, so a non-nearest partner shears the mesh by up to the adhesion
        //cutoff every iteration - five orders of magnitude more than the per-step motion the
        //forces produce - which is how sub-l_min edges and sliver triangles are seeded.
        auto it_n1 = n1.coupled_nodes_map_.find(c2->get_local_id());

        //Save the previous shortest distance between the node n1 and the nodes of the cell
        const double previous_min_squared_distance = (it_n1 != n1.coupled_nodes_map_.end()) ? it_n1->second.second : std::numeric_limits<double>::max();

        //If the node of c1 has already been coupled to a node of c2
        if(it_n1 != n1.coupled_nodes_map_.end()){

            //Get the distance between the 

            //If the node of c1 is coupled with a node of c2 whose distance is smaller than the distance with the node n1
            //we set the squared distance to infinity  
            if(previous_min_squared_distance < squared_distance_f_n1){squared_distance_f_n1 = std::numeric_limits<double>::max();}
            if(previous_min_squared_distance < squared_distance_f_n2){squared_distance_f_n2 = std::numeric_limits<double>::max();}
            if(previous_min_squared_distance < squared_distance_f_n3){squared_distance_f_n3 = std::numeric_limits<double>::max();}
        }


        //If the normal at the node n1 is pointing in an opposite direction than the normal of the face's nodes, we set the squared distance to infinity
        if(n1.normal_.dot(f_n1.normal_) > max_dot_product_adhesion_){squared_distance_f_n1 = std::numeric_limits<double>::max();}
        if(n1.normal_.dot(f_n2.normal_) > max_dot_product_adhesion_){squared_distance_f_n2 = std::numeric_limits<double>::max();}
        if(n1.normal_.dot(f_n3.normal_) > max_dot_product_adhesion_){squared_distance_f_n3 = std::numeric_limits<double>::max();}

        //Check if the nodes of the face are already coupled to other nodes on c1
        //Same key mismatch as above (issue #113): the map is keyed by local, not global, cell id
        auto it_f_n1 = f_n1.coupled_nodes_map_.find(c1->get_local_id());
        auto it_f_n2 = f_n2.coupled_nodes_map_.find(c1->get_local_id());
        auto it_f_n3 = f_n3.coupled_nodes_map_.find(c1->get_local_id());

        //If the nodes of the face are already coupled to other nodes on c1, and if the distance with these nodes is smaller than the distance with the node n1
        if(it_f_n1 !=  f_n1.coupled_nodes_map_.end() && it_f_n1->second.second < squared_distance_f_n1){squared_distance_f_n1 = std::numeric_limits<double>::max();}
        if(it_f_n2 !=  f_n2.coupled_nodes_map_.end() && it_f_n2->second.second < squared_distance_f_n2){squared_distance_f_n2 = std::numeric_limits<double>::max();}
        if(it_f_n3 !=  f_n3.coupled_nodes_map_.end() && it_f_n3->second.second < squared_distance_f_n3){squared_distance_f_n3 = std::numeric_limits<double>::max();}

        //Make sure the curvature at the other point is below the threshold
        const double surface_coupling_max_curvature = c2->get_cell_type()->surface_coupling_max_curvature_;
        if(f_n1.curvature_ > surface_coupling_max_curvature){squared_distance_f_n1 = std::numeric_limits<double>::max();}
        if(f_n2.curvature_ > surface_coupling_max_curvature){squared_distance_f_n2 = std::numeric_limits<double>::max();}
        if(f_n3.curvature_ > surface_coupling_max_curvature){squared_distance_f_n3 = std::numeric_limits<double>::max();}

        //Find the node of the face that is the closest to the node n1
        double min_squared_distance_to_node_face;
        node* n2 = nullptr;

        if(squared_distance_f_n1 < squared_distance_f_n2 && squared_distance_f_n1 < squared_distance_f_n3){
            min_squared_distance_to_node_face = squared_distance_f_n1;
            n2 = &f_n1;
        }
        else if(squared_distance_f_n2 < squared_distance_f_n1 && squared_distance_f_n2 < squared_distance_f_n3){
            min_squared_distance_to_node_face = squared_distance_f_n2;
            n2 = &f_n2;
        }
        else{
            min_squared_distance_to_node_face = squared_distance_f_n3;
            n2 = &f_n3;
        }

        //If this closesst node in the face (n2) is closer to the node n1 than any previous closest node
        //and if the distance is below the adhesion cutoff, then we mechanically couple the node n1 to the node n2
        if(                                                                              
            min_squared_distance_to_node_face < interaction_cutoff_square_adhesion_ &&      //If the distance is below the adhesion cutoff
            min_squared_distance_to_node_face < previous_min_squared_distance      //If the distance is smaller than the previous closest distance
        ){

            assert(n2->get_local_id() < c2->get_node_lst().size());
            assert(n2->is_used());
            assert(c2->get_node_lst()[n2->get_local_id()].is_used());

            //This section is thread safe
            n1.set_coupled_node_and_min_distance( c2->get_local_id(), n2->get_local_id(), min_squared_distance_to_node_face);
            n2->set_coupled_node_and_min_distance(c1->get_local_id(), n1.get_local_id(),  min_squared_distance_to_node_face);

            //The contact has been resolved, we can stop here
            return;
        }
    }


    //If the 2 cells are not epithelial cells, or if the node n1 has not been coupled to one of the node of the face
    //We compute the minimum distance between the node and the face

    //Get the minimal squared distance between the node and the face
    const auto [min_squared_distance, bary_pos] = compute_node_triangle_distance(n1.pos(), f_n1_pos, f_n2_pos, f_n3_pos);

    //If the node is located within the interaction cutoff distance of the face, we have to check if a repulsion force need to be applied.
    if(
        min_squared_distance < max_interaction_cutoff_square_
    ){

        //The contact force vector goes from the closest point of approach on the face to the node
        const vec3 cpa_pos = f_n1_pos * bary_pos.dx() + f_n2_pos * bary_pos.dy() + f_n3_pos * bary_pos.dz();
        const vec3 cpa_f_to_node = n1.pos() - cpa_pos;

        //Get a reference to the types of the face
        const auto& face_type = c2->get_face_type(f->local_face_id_);

        //Determine if the contact is of adhesive or repulsive type based on the normal of the face
        bool contact_is_repulsive = (cpa_f_to_node.dot(f->normal_) < 0.0);

        //Reverse the direction of contact if the cell to which the face belongs is an ECM cell
        if (c1->get_cell_type_id() == 0 && c2->get_cell_type_id() == 1){contact_is_repulsive = !contact_is_repulsive;}

        //Do the same of the node belongs to a nucleus and the face to an epithelial cell
        if (c1->get_cell_type_id() == 3 && c2->get_cell_type_id() == 0){contact_is_repulsive = !contact_is_repulsive;}


        //If the 2 membranes are interpenetrating, we apply a repulsive force
        if(contact_is_repulsive){
            
            //Compute the distance
            const double min_distance = std::sqrt(min_squared_distance);
            const double integration_region = f->get_area();

            //Get a reference to the types of the face
            const auto& face_type = c2->get_face_type(f->local_face_id_);

            #if FACE_STORE_CONTACT_ENERGY
                const double repulsion_energy = 0.5 * face_type.repulsion_strength_ * integration_region * min_distance * min_distance;
                f->add_repulsion_energy(repulsion_energy);
            #endif

            //The force that will be distributed on the nodes of the 2 faces
            const vec3 repulsion_force_vector =  cpa_f_to_node  *  face_type.repulsion_strength_ * integration_region;

            //Apply the repulsion forces on the nodes of the 2 faces
            f_n1.add_force(repulsion_force_vector * bary_pos.dx());
            f_n2.add_force(repulsion_force_vector * bary_pos.dy());
            f_n3.add_force(repulsion_force_vector * bary_pos.dz());
            n1.add_force(repulsion_force_vector * -1.0);   
        }
    }
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------







//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void contact_face_face_via_coupling::resolve_grid_contacts(const std::vector<cell_ptr>& cell_lst) noexcept{
        //Grid path: dynamic schedule over balanced work units (whole cells, or node ranges of heavy cells).
        //For every node the candidate faces are visited in ascending global face id, exactly as in the uspg path.
        const uint32_t* const start = csr_start_.data();
        const uint32_t* const faces = csr_faces_.data();
        const unsigned* const owner = face_owner_gid_.data();
        const double*   const aabb  = face_aabb_lst_.data();

        #pragma omp parallel for schedule(dynamic, 1)
        for(size_t u = 0; u < work_units_.size(); u++){
            const work_unit wu = work_units_[u];
            const cell_ptr& c1 = cell_lst[wu.cell_id];
            const unsigned c1_gid = c1->get_id();

            //Get the maximum mean curvature of the cell, above which one of its point cannot be coupled to a face
            //on another cell surface
            const double surface_coupling_max_curvature = c1->get_cell_type()->surface_coupling_max_curvature_;

            for(uint32_t k = wu.node_begin; k < wu.node_end; k++){
                node& n = c1->node_lst_[k];
                if(!(n.is_used() && n.curvature_ < surface_coupling_max_curvature)) continue;

                const double px = n.pos().dx(), py = n.pos().dy(), pz = n.pos().dz();
                const size_t vox = node_voxel(n.pos());

                for(uint32_t e = start[vox]; e < start[vox + 1]; e++){
                    const uint32_t fid = faces[e];
                    if(owner[fid] == c1_gid) continue;

                    //Check if the node is located in the AABB of the face (same test as aabb_intersection_check)
                    const double* bb = aabb + static_cast<size_t>(fid) * 6;
                    if(px < bb[0] || px > bb[3] || py < bb[1] || py > bb[4] || pz < bb[2] || pz > bb[5]) continue;

                    face* f = face_lst_[fid];
                    if(n.normal_.dot(f->normal_) < max_dot_product_repulsion_){
                        resolve_contact(c1, f->owner_cell_, n, f);
                    }
                }
            }
        }
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------


#endif