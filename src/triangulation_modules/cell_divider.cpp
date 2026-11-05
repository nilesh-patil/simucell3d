#include "cell_divider.hpp"
#include <cmath>
#include <iostream>
#include <set>
#include <map>
#include <array>
#include <algorithm>
#include <numeric>


namespace {

//-----------------------------------------------------------------------------------------------
//Triangulate a simple 2D polygon (z ignored) by ear clipping. Returns triplets of indices into the
//polygon, or an empty vector if no ear can be found (the polygon is not simple)
std::vector<std::array<unsigned, 3>> triangulate_polygon_by_ear_clipping(const std::vector<vec3>& polygon){

    auto cross_z = [](const vec3& a, const vec3& b, const vec3& c){
        return (b.dx() - a.dx()) * (c.dy() - a.dy()) - (b.dy() - a.dy()) * (c.dx() - a.dx());
    };

    //Orientation of the polygon, from the sign of its area
    double twice_area = 0.;
    for(size_t i = 0; i < polygon.size(); i++){
        const vec3& a = polygon[i];
        const vec3& b = polygon[(i + 1) % polygon.size()];
        twice_area += a.dx() * b.dy() - b.dx() * a.dy();
    }
    const double orientation = twice_area > 0. ? 1. : -1.;

    std::vector<unsigned> remaining(polygon.size());
    std::iota(remaining.begin(), remaining.end(), 0);

    std::vector<std::array<unsigned, 3>> triangle_lst;
    while(remaining.size() > 3){
        bool ear_found = false;
        for(size_t k = 0; k < remaining.size() && !ear_found; k++){
            const unsigned i_prev = remaining[(k + remaining.size() - 1) % remaining.size()];
            const unsigned i_curr = remaining[k];
            const unsigned i_next = remaining[(k + 1) % remaining.size()];
            const vec3& a = polygon[i_prev];
            const vec3& b = polygon[i_curr];
            const vec3& c = polygon[i_next];

            //The corner must be convex and the triangle must not be degenerate
            if(orientation * cross_z(a, b, c) <= 0.) continue;

            //No other corner of the polygon may lie in the triangle
            const bool triangle_is_empty = std::none_of(remaining.begin(), remaining.end(), [&](const unsigned i){
                if(i == i_prev || i == i_curr || i == i_next) return false;
                const vec3& q = polygon[i];
                return orientation * cross_z(a, b, q) >= 0. && orientation * cross_z(b, c, q) >= 0. && orientation * cross_z(c, a, q) >= 0.;
            });
            if(!triangle_is_empty) continue;

            triangle_lst.push_back({i_prev, i_curr, i_next});
            remaining.erase(remaining.begin() + k);
            ear_found = true;
        }
        if(!ear_found) return {};
    }
    triangle_lst.push_back({remaining[0], remaining[1], remaining[2]});
    return triangle_lst;
}
//-----------------------------------------------------------------------------------------------

}





//-----------------------------------------------------------------------------------------------
//If a cell is ready to be divided, this function will divide it into two daughter cells
void cell_divider::run(
    std::vector<cell_ptr>& cell_lst,
    const double l_min, //The min edge length
    const local_mesh_refiner& lmr, // Will be used to refine the meshes of the daughter cells
    unsigned& max_cell_id_, //Use this number to assign unique ids to the daughter cells
    bool verbose
) noexcept{


    //Keep track of the ids of the cells to delete
    std::vector<unsigned> cells_to_delete_lst;

    //The daughter cells are collected here and appended to cell_lst only after the parallel
    //loop: pushing them into cell_lst from inside the loop can reallocate its storage while
    //other threads are still reading cell_lst[i].
    std::vector<cell_ptr> daughter_lst;

    //Loop over the cells
    #pragma omp parallel for
    for(size_t i = 0; i < cell_lst.size(); i++){

        //Check if the cell is ready to be divided (use result to avoid redundant call)
        const bool is_ready_to_divide = cell_lst[i]->is_ready_to_divide();

        //If the cell is ready to be divided
        if(is_ready_to_divide){


            auto division_result  = divide_cell(cell_lst[i], l_min, lmr);

            //If the division was successful
            if(division_result.has_value()){

                #pragma omp critical
                {
                    auto [daughter_1, daughter_2] = division_result.value();

                    //Erase all the faces and nodes of the mother cell to avoid cyclic dependencies
                    cell_lst[i]->clear_data();

                    cells_to_delete_lst.push_back(i);

                    daughter_1->cell_id_ = max_cell_id_++;
                    daughter_2->cell_id_ = max_cell_id_++;

                    daughter_lst.push_back(daughter_1);
                    daughter_lst.push_back(daughter_2);
                }
            }
        }
    }

    cell_lst.insert(cell_lst.end(), daughter_lst.begin(), daughter_lst.end());

    //Remove the mother cells at the end of the parallel loop
    if(cells_to_delete_lst.size() > 0){
        std::sort(cells_to_delete_lst.begin(), cells_to_delete_lst.end(), std::less<unsigned>());
        remove_index(cell_lst, cells_to_delete_lst);

        for(size_t local_cell_id = 0; local_cell_id < cell_lst.size(); local_cell_id++){
            cell_lst[local_cell_id]->set_local_id(local_cell_id);
        }
    }




}
//-----------------------------------------------------------------------------------------------



//-----------------------------------------------------------------------------------------------
//Run the whole division pipeline, and cut the mother cell into 2 daughter cells
//The mother cell is not affected by the division operation, if anything goes wrong during the division
//operation the mother cell will continue to exist
std::optional<std::pair<cell_ptr, cell_ptr>>  cell_divider::divide_cell(
    cell_ptr c, 
    const double l_min, //The min edge length
    const local_mesh_refiner& lmr // Will be used to refine the meshes of the daughter cells
) noexcept{


    try{
        //Remove the unused nodes or faces in the cell
        c->rebase();
        
        //Get the origin and the normal of the division plane
        const vec3 centroid = c->compute_centroid();
        const vec3 division_plane_normal = c->get_cell_division_axis();

        //All the points that will be part of the division interface will have ids >= than this value
        const unsigned division_point_ids_threshold = c->get_node_lst().size();

        //Add the points where the plane intersects with the edges of the cell
        mesh m = cell_divider::add_intersection_points(c, centroid, division_plane_normal);

        //At this stage some faces are not triangles anymore and must be triangulated before the division can be performed
        cell_divider::divide_faces(m, division_point_ids_threshold);

        //All the faces that will be part of the division interface will have an id greater than this value
        const unsigned division_face_ids_threshold  = m.face_point_ids.size();

        //Form a polygon by connecting all the points where the plane intersects with the edges of the cell
        // and then divide this polygon in triangles
        const unsigned nb_division_face_points = m.node_pos_lst.size() / 3 - division_point_ids_threshold;
        std::vector<unsigned> division_face_point_ids(nb_division_face_points);
        std::iota(division_face_point_ids.begin(), division_face_point_ids.end(), division_point_ids_threshold);
        m.face_point_ids.push_back(division_face_point_ids);
        initial_triangulation::coarse_triangulation(m);
    
        //Map the points of the division face to the xy plane. This is done such that we can use the 2D
        //Delaunay triangulation and it also facilitates subsequent tests
        auto [translation, rotation] = cell_divider::map_points_to_xy_plane(m, division_point_ids_threshold, division_plane_normal);

        //Triangulate the the division interface in a way that respects the constraints on the edge lengths
        cell_divider::triangulate_division_interface(
            l_min, 
            m, 
            division_point_ids_threshold, 
            division_face_ids_threshold, 
            division_plane_normal
        );

        //Map the points of the division interface back to their original positions
        cell_divider::map_points_to_division_plane(m, division_point_ids_threshold, translation, rotation);

        //Split the mesh of the mother cell in 2 and create the daughter cells
        auto [daughter_cell_1, daughter_cell_2] = cell_divider::create_daughter_cells(
            c, 
            m, 
            division_point_ids_threshold, 
            division_face_ids_threshold, 
            division_plane_normal,
            centroid
        );




        //Refine the meshes of the daughter cells to make sure their edge lengths are in the range [l_min, l_max]
        lmr.refine_mesh(daughter_cell_1);
        lmr.refine_mesh(daughter_cell_2);

        //If this is not done there will be a sudden drop in the daughter cells pressures
        daughter_cell_1->target_volume_ = c->target_volume_ / 2;
        daughter_cell_2->target_volume_ = c->target_volume_ / 2;

        daughter_cell_1->rebase();
        daughter_cell_2->rebase();

        //Draw the growth rate and division volume from a normal distribution
        daughter_cell_1->initialize_random_properties();
        daughter_cell_2->initialize_random_properties();

        return std::make_pair(daughter_cell_1, daughter_cell_2);
    }

    catch(const std::exception& e){
        // Log the exception details for debugging before returning nullopt
        std::cerr << "Cell division failed: " << e.what() << std::endl;
        return std::nullopt;
    }
}
//-----------------------------------------------------------------------------------------------






//-----------------------------------------------------------------------------------------------
//Finds where the division plane intersects with the edges of the cell and return a mesh of the 
//cell with the intersection points added.
mesh cell_divider::add_intersection_points(
    cell_ptr c, 
    const vec3& p,  //Position of a point on the plane
    const vec3& n   //Normal vector of the plane
)noexcept(false){

    assert(c->free_face_queue_.empty());
    assert(c->free_node_queue_.empty());


    //Get a mesh object of the cell, all the modifications are going to be done on this mesh object
    //and the mother cell will be left untouched, if anything goes wrong during the division operation
    //the mother cell will continue to exist
    mesh m = c->get_mesh();

    //The plane can meet the surface along several closed curves, e.g. when it also slices a thin
    //protrusion of the cell. Trace all of them, then cut the mother along the longest one only.
    //The cut used to follow whichever curve contained the first intersected edge found in the edge
    //set, which could slice a sub-micron protrusion off instead of dividing the cell body.
    std::vector<std::vector<std::pair<edge, vec3>>> loop_lst;
    std::set<std::pair<unsigned, unsigned>> traced_edges;
    auto edge_key = [](const edge& e){return std::make_pair(std::min(e.n1(), e.n2()), std::max(e.n1(), e.n2()));};

    for(const edge& e : c->get_edge_set()){
        if(traced_edges.count(edge_key(e)) != 0) continue;

        //Get the nodes of the edge
        const vec3& n1_pos = c->get_node(e.n1()).pos();
        const vec3& n2_pos = c->get_node(e.n2()).pos();
        if(!find_edge_plane_intersection(n1_pos, n2_pos, p, n).has_value()) continue;

        traced_edges.insert(edge_key(e));
        try{
            std::vector<std::pair<edge, vec3>> loop = trace_cut_loop(c, e, p, n);
            for(const auto& [loop_edge, intersection_point] : loop) traced_edges.insert(edge_key(loop_edge));
            loop_lst.push_back(std::move(loop));
        }
        //A curve that cannot be followed (e.g. the plane goes exactly through a node) only matters if
        //it is the one the cell must be cut along: skip it, and fail below if no curve is left
        catch(const division_exception&){}
    }
    if(loop_lst.empty()){throw division_exception("The plane does not intersect with the cell");}

    auto loop_length = [](const std::vector<std::pair<edge, vec3>>& loop){
        double length = 0.;
        for(size_t i = 0; i < loop.size(); i++) length += (loop[(i + 1) % loop.size()].second - loop[i].second).norm();
        return length;
    };
    const auto& loop_to_cut = *std::max_element(loop_lst.begin(), loop_lst.end(),
        [&loop_length](const auto& l1, const auto& l2){return loop_length(l1) < loop_length(l2);});

    //Add the intersection points of that curve to the mesh and to the 2 faces sharing each cut edge
    for(const auto& [edge_to_cut, intersection_point] : loop_to_cut){
        m.node_pos_lst.insert(m.node_pos_lst.end(), {intersection_point.dx(), intersection_point.dy(), intersection_point.dz()});

        //Get the id of the intersection point (size must be >= 3 for at least one node)
        if (m.node_pos_lst.size() < 3) {
            throw division_exception("Mesh has no nodes when computing intersection point id");
        }
        const unsigned intersection_point_id = static_cast<unsigned>(m.node_pos_lst.size() / 3) - 1;
        add_point_to_face(m, edge_to_cut.f1(), edge_to_cut.n1(), edge_to_cut.n2(), intersection_point_id);
        add_point_to_face(m, edge_to_cut.f2(), edge_to_cut.n1(), edge_to_cut.n2(), intersection_point_id);
    }

    return m;
}
//-----------------------------------------------------------------------------------------------




//-----------------------------------------------------------------------------------------------
//Follow the closed curve along which the plane cuts the surface of the cell, starting from an edge
//that the plane intersects. Returns the cut edges in order, each with its intersection point.
std::vector<std::pair<edge, vec3>> cell_divider::trace_cut_loop(
    cell_ptr c,
    const edge& seed_edge,  //An edge intersected by the plane
    const vec3& p,          //Position of a point on the plane
    const vec3& n           //Normal vector of the plane
) noexcept(false){

    std::optional<vec3> seed_point = find_edge_plane_intersection(c->get_node(seed_edge.n1()).pos(), c->get_node(seed_edge.n2()).pos(), p, n);
    if(!seed_point.has_value()){throw division_exception("The plane does not intersect with the cell");}

    std::vector<std::pair<edge, vec3>> loop{{seed_edge, seed_point.value()}};
    edge edge_to_cut = seed_edge;

    //We cut the cell starting from f2 and when the cut reaches f1 we stop
    const face stop_face   = c->get_face(edge_to_cut.f1());
    face face_to_cut       = c->get_face(edge_to_cut.f2());

    //This loop goes through all the edges of the curve that are intersected by the plane
    unsigned iteration = 0;
    while(face_to_cut != stop_face){

        //Get the ids of the nodes of the edge that has already been cut
        const unsigned n_a_id = edge_to_cut.n1();
        const unsigned n_b_id = edge_to_cut.n2();

        //Get the node opposite to the edge that has already been cut in the face
        const unsigned n_c_id = face_to_cut.get_opposite_node(edge_to_cut.n1(), edge_to_cut.n2());

        //Get the nodes a, b, and c
        const node& n_a = c->get_node(n_a_id);
        const node& n_b = c->get_node(n_b_id);
        const node& n_c = c->get_node(n_c_id);

        //The plane either cuts the edge ac or the edge bc
        std::optional<vec3> intersection_ac = find_edge_plane_intersection(n_a.pos(), n_c.pos(), p, n);
        std::optional<vec3> intersection_bc = find_edge_plane_intersection(n_b.pos(), n_c.pos(), p, n);

        assert((intersection_ac.has_value() && intersection_bc.has_value()) == false);
        vec3 intersection_point;
        //If the plane cuts the edge ac
        if(intersection_ac.has_value()){
            auto edge_to_cut_opt = c->get_edge(n_a_id, n_c_id);
            assert(edge_to_cut_opt.has_value());
            edge_to_cut = edge_to_cut_opt.value();
            intersection_point = intersection_ac.value();
        }

        //If the plane cuts the edge bc
        else if(intersection_bc.has_value()){
            auto edge_to_cut_opt = c->get_edge(n_b_id, n_c_id);
            assert(edge_to_cut_opt.has_value());
            edge_to_cut = edge_to_cut_opt.value();
            intersection_point = intersection_bc.value();
        }
        else{throw division_exception("The plane does not intersect with the cell");}

        loop.emplace_back(edge_to_cut, intersection_point);

        //Get the next face which is cut by the plane
        const unsigned face_to_cut_id = (edge_to_cut.f1() == face_to_cut.get_local_id()) ? edge_to_cut.f2() : edge_to_cut.f1();
        face_to_cut = c->get_face(face_to_cut_id);

        if(iteration == c->get_edge_set().size() - 1){throw division_exception("Infinite loop stopped during cell division");}
        iteration++;
    }

    return loop;
}

//-----------------------------------------------------------------------------------------------




//-----------------------------------------------------------------------------------------------
//At this stage of the division process, some faces of the mesh have 5 nodes. This is because the plane
//has intersected with 2 of their edges. This function splits those faces in 3 triangular subfaces. 
//The argument "intersection_point_ids_threshold" corresponds to the id of the first intersection point
//that was added to the mesh. The other intersection points that were added to the mesh have ids greater
void cell_divider::divide_faces(mesh& m, const unsigned intersection_point_ids_threshold) noexcept(false){
    assert(intersection_point_ids_threshold > 0 && intersection_point_ids_threshold < m.node_pos_lst.size()/3);
    assert(m.face_point_ids.size() > 0);

    //Keep track of the faces to delete
    std::vector<unsigned> faces_to_delete;
    std::vector<std::vector<unsigned>> faces_to_add;

    //Loop through all the faces of the mesh
    for(unsigned k = 0; k < m.face_point_ids.size(); k++){
        auto& f = m.face_point_ids[k];
        assert(f.size() == 3 || f.size() == 5);

        //If the face has 5 nodes, it means that the plane has intersected with 2 of its edges
        if(f.size() == 5){
            faces_to_delete.push_back(k);

            //Get the ids of the intersection points
            auto it1 = std::find_if(f.begin(), f.end(), [intersection_point_ids_threshold](const unsigned& id){return id >= intersection_point_ids_threshold;});
            assert(it1 != f.end());

            auto it2 = std::find_if(it1 + 1, f.end(), [intersection_point_ids_threshold](const unsigned& id){return id >= intersection_point_ids_threshold;});
            assert(it2 != f.end());
            assert(*it1 != *it2);

            //Get the local ids of the 2 intesection pointspoints in the face
            const unsigned local_point_1_id = std::distance(f.begin(), it1);
            const unsigned local_point_2_id = std::distance(f.begin(), it2);
            assert(local_point_2_id - local_point_1_id == 3 || local_point_2_id - local_point_1_id == 2);

            //Based on where the face has been divided there are 2 ways to create the new triangular subfaces
            if(local_point_2_id - local_point_1_id  == 3){

                //Add the new triangular subfaces
                faces_to_add.insert(faces_to_add.end(), {f[local_point_1_id],  f[local_point_2_id],              f[(local_point_2_id + 1) % 5]});
                faces_to_add.insert(faces_to_add.end(), {f[local_point_1_id],  f[local_point_1_id + 1],          f[local_point_1_id + 2]});
                faces_to_add.insert(faces_to_add.end(), {f[local_point_1_id],  f[(local_point_1_id + 2) % 5],    f[local_point_2_id]});

            }
            else{
                faces_to_add.insert(faces_to_add.end(), {f[local_point_1_id],            f[local_point_1_id + 1],    f[local_point_2_id]});
                faces_to_add.insert(faces_to_add.end(), {f[(local_point_2_id + 2) % 5],  f[local_point_1_id],        f[local_point_2_id]});
                faces_to_add.insert(faces_to_add.end(), {f[(local_point_2_id + 2) % 5],  f[local_point_2_id],        f[(local_point_2_id + 1) % 5]});
            }
        }
    }

    
    //Delete the faces that have been divided
    std::sort(faces_to_delete.begin(), faces_to_delete.end(), std::less<unsigned>());
    remove_index(m.face_point_ids, faces_to_delete);

    //Add the faces that have been created 
    std::move(faces_to_add.begin(), faces_to_add.end(), std::back_inserter(m.face_point_ids));
}
//-----------------------------------------------------------------------------------------------






//-----------------------------------------------------------------------------------------------
//Find the intersection point between an edge (line segment) and a plane
//If there is no intersection returns an empty optional
std::optional<vec3> cell_divider::find_edge_plane_intersection(
    const vec3& e1, //Position of the 1st point of the edge
    const vec3& e2, //Position of the 2nd point of the edge

    const vec3& p,  //Position of a point on the plane
    const vec3& n   //Normal vector of the plane
) noexcept(false){


    //Find where a plane cuts an edge, returns nullptr if it doesn't cut it
    // Compute the t value for the directed line ab intersecting the plane
    const double dot1 = n.dot(p  - e1);
    const double dot2 = n.dot(e2 - e1);

    double t;
    //Use epsilon comparison for floating-point safety (edge nearly parallel to plane)
    constexpr double epsilon = 1e-12;
    if(std::abs(dot2) < epsilon){return std::nullopt;} //The edge is colinear with the plane
    else{t = dot1 / dot2;}
    if(t < 0.0 || t > 1.0){return std::nullopt;}
    
    vec3 intersection_point = e1 + (e2 - e1) * t;
    return intersection_point;
}
//-----------------------------------------------------------------------------------------------







//-----------------------------------------------------------------------------------------------
//Based on the side of a face wrt the plane, return true or false based on the side
bool cell_divider::face_side_wrt_plane(
    const face& f,
    const cell_ptr c,

    const vec3& p,  //Position of a point on the plane
    const vec3& n   //Normal vector of the plane
    
) noexcept{

    //Get the position of the 3 nodes of the face
    auto [n1_id, n2_id, n3_id] = f.get_node_ids();

    const vec3& p1 = c->get_node(n1_id).pos();
    const vec3& p2 = c->get_node(n2_id).pos();
    const vec3& p3 = c->get_node(n3_id).pos();

    //Compute the centroid of the face
    const vec3 centroid = (p1 + p2 + p3) / 3.0;

    return (centroid - p).dot(n) > 0;
}


//-----------------------------------------------------------------------------------------------



//-----------------------------------------------------------------------------------------------
//Add the id of the point p to the given face and make sure that the point p is inserted between the nodes n_a and n_b
void cell_divider::add_point_to_face(
    mesh& m, 
    const unsigned face_id, 
    const unsigned n_a_id, 
    const unsigned n_b_id,
    const unsigned n_p_id) noexcept(false){

    //Runtime validation (assert is disabled in release builds)
    if (face_id >= m.face_point_ids.size()) {
        throw division_exception("Invalid face_id in add_point_to_face");
    }

    //Get the face
    auto& f = m.face_point_ids[face_id];

    //Runtime validation - face must have at least 3 nodes to form edges
    if (f.size() < 3) {
        throw division_exception("Face has fewer than 3 nodes in add_point_to_face");
    }

    //Loop over the edges of the face (f.size() >= 3 guaranteed above, so f.size()-1 is safe)
    for(unsigned i = static_cast<unsigned>(f.size() - 1), j = 0; j < f.size(); i = j++){
        //If the edge is the one between n_a and n_b
        if((f[i] == n_a_id && f[j] == n_b_id) || (f[i] == n_b_id && f[j] == n_a_id)){
            //Insert the point between the two nodes
            f.insert(f.begin() + j, n_p_id);
            return;
        }
    }
    throw division_exception("The point could not be inserted between the two nodes");
}
//-----------------------------------------------------------------------------------------------





//-----------------------------------------------------------------------------------------------
//As it names indicates, this function maps the points in intersection_points to the xy plane
//Returns the translation and the rotation that were applied to the points
std ::pair<vec3, mat33> cell_divider::map_points_to_xy_plane(
    mesh& m, 
    const unsigned division_point_ids_threshold,
    const vec3& division_plane_normal 
) noexcept{

    assert(division_point_ids_threshold < m.node_pos_lst.size() / 3);

    //Compute the number of points that are on the division interface
    const double nb_points_division_interface = (static_cast<double>(m.node_pos_lst.size()) / 3.) - static_cast<double>(division_point_ids_threshold);

    //First compute the centroid of the intersection points
    vec3 centroid(0.0, 0.0, 0.0);
    for(unsigned p_id = division_point_ids_threshold; p_id < m.node_pos_lst.size() / 3; p_id++){
        centroid.translate(m.node_pos_lst[p_id *3    ], m.node_pos_lst[p_id *3 + 1], m.node_pos_lst[p_id *3 + 2]);
    }
    const vec3 translation = centroid / (-1 * nb_points_division_interface);

    //Move all the points so that the centroid is at the origin
    for(unsigned p_id = division_point_ids_threshold; p_id < m.node_pos_lst.size() / 3; p_id++){

        m.node_pos_lst[p_id *3    ] += translation.dx();
        m.node_pos_lst[p_id *3 + 1] += translation.dy();
        m.node_pos_lst[p_id *3 + 2] += translation.dz();
    }

    //Compute the rotation matrix that maps the plane to the xy plane
    const vec3 xy_plane_normal(0., 0., 1.);
    mat33 rotation_matrix;

    //If the division plane is already the xy plane, then the rotation matrix is the identity matrix
    if(xy_plane_normal.dot(division_plane_normal) == 1.0){rotation_matrix = mat33::identity();}

    //Otherwise we need to create a rotation matrix from a quaternion
    else{
        const vec3 a = division_plane_normal.cross(xy_plane_normal);
        const double w = 1.0 + division_plane_normal.dot(xy_plane_normal);

        //Use a quaternion to make sure that the rotation matrix is orthogonal
        quaternion q(w, a.dx(), a.dy(), a.dz());
        q = q.normalize();
        rotation_matrix = q.to_matrix();
    }


    //Rotate all the intesections points
    for(unsigned p_id = division_point_ids_threshold; p_id < m.node_pos_lst.size() / 3; p_id++){
        assert(p_id * 3 + 2 < m.node_pos_lst.size());

        vec3 pos_after_rotation = rotation_matrix.dot(vec3(
            m.node_pos_lst[p_id *3    ],
            m.node_pos_lst[p_id *3 + 1],
            m.node_pos_lst[p_id *3 + 2]
        ));

        m.node_pos_lst[p_id *3    ] = pos_after_rotation.dx();
        m.node_pos_lst[p_id *3 + 1] = pos_after_rotation.dy();
        m.node_pos_lst[p_id *3 + 2] = 0.;
    }
    


    return std::make_pair(translation, rotation_matrix);
}
//-----------------------------------------------------------------------------------------------

//-----------------------------------------------------------------------------------------------
//Maps back the points of the division face to their original plane
void cell_divider::map_points_to_division_plane(
    mesh& m, 
    const unsigned division_point_ids_threshold,
    const vec3& translation,  
    const mat33& rotation_matrix
) noexcept{
    
    assert(division_point_ids_threshold < m.node_pos_lst.size() / 3);

    //Map back the nodes of the division face to their original positions
    const mat33 rotation_inv = rotation_matrix.transpose();

    for(unsigned p_id  = division_point_ids_threshold; p_id < m.node_pos_lst.size() / 3; p_id++){

        //Get the position of the point as a vector
        vec3 p(m.node_pos_lst[p_id * 3], m.node_pos_lst[p_id * 3 + 1], m.node_pos_lst[p_id * 3 + 2]);

        //Position back this point to the correct position of the division interface
        p = rotation_inv.dot(p) - translation;
        m.node_pos_lst[p_id * 3    ] = p.dx();
        m.node_pos_lst[p_id * 3 + 1] = p.dy();
        m.node_pos_lst[p_id * 3 + 2] = p.dz();
    }    
   
}
//-----------------------------------------------------------------------------------------------



//-----------------------------------------------------------------------------------------------
//Triangulate the interface between the 2 daughter cells in a way that respects the constraints on the edge lengths.
void cell_divider::triangulate_division_interface(
    const double l_min,                             //The minimum edge length
    mesh& m,                                        //The mesh containing all the faces and nodes
    const unsigned division_point_ids_threshold,    //All the nodes that belong to the division interface have an id greater than this threshold
    const unsigned division_face_ids_threshold,     //All the faces that belong to the division interface have an id greater than this threshold
    const vec3& division_face_normal               //The normal vector of the division interface
) noexcept(false){


    assert(l_min > 0.0);
    assert(m.node_pos_lst.size() % 3 == 0 && m.node_pos_lst.size() > 0);
    assert(m.face_point_ids.size() > 0);
    assert(division_point_ids_threshold < m.node_pos_lst.size() / 3);
    assert(division_face_ids_threshold < m.face_point_ids.size());
    
    //Compute the number of points  that are part of the division interface at this stage
    unsigned nb_points_division_interface = static_cast<unsigned>(m.node_pos_lst.size() / 3.) - division_point_ids_threshold;

    //Get a list of the coordinates of all the points located on the division interface
    std::vector<vec3> division_face_points_coords;

    //The last point is the centroid of the division interface and should not be added
    division_face_points_coords.reserve(nb_points_division_interface - 1); 

    //The last point at this stage is the centroid of the division interaface and should not be added
    for(unsigned p_id = division_point_ids_threshold; p_id < m.node_pos_lst.size() / 3 - 1; p_id++){
        division_face_points_coords.emplace_back(m.node_pos_lst[p_id * 3    ], m.node_pos_lst[p_id * 3 + 1], 0.);
    }

    //Use the poisson point cloud algorithm to triangulate the division interface
    std::vector<oriented_point> poisson_point_lst = poisson_sampling::compute_poisson_point_cloud(
        l_min, 
        m, 
        division_point_ids_threshold, 
        division_face_ids_threshold, 
        division_face_normal
    );
    
    //Store the points that have been created by the poisson sampling in the mesh
    for(const auto& poisson_point: poisson_point_lst){

        if(poisson_point.created_by_poisson_sampling_){
            //Add the point to the mesh
            m.node_pos_lst.insert(m.node_pos_lst.end(), {poisson_point.position_.dx(), poisson_point.position_.dy(), 0.});
        }
    }

    //Update the number of points  that are part of the division interface at this stage
    nb_points_division_interface = static_cast<unsigned>(m.node_pos_lst.size() / 3.) - division_point_ids_threshold;
  
    //Remove all the triangles of the division interface, they will be replaced by 
    //triangles respecting the constraints on the edge lengths
    m.face_point_ids.erase(m.face_point_ids.begin() + division_face_ids_threshold, m.face_point_ids.end());

    //Store in 2D the coordinates of the points of the division interface
    std::vector<double> division_interface_points_2d_coords;
    division_interface_points_2d_coords.reserve(nb_points_division_interface * 2);

    //Convert to 2D the positions of all the nodes located on the division interface
    for(unsigned p_id = division_point_ids_threshold; p_id < m.node_pos_lst.size() / 3; p_id++){
        division_interface_points_2d_coords.push_back(m.node_pos_lst[p_id * 3    ]);
        division_interface_points_2d_coords.push_back(m.node_pos_lst[p_id * 3 + 1]);
    }
    
    //Run the 2D delaunay triangulation
    try{
        delaunator::Delaunator d(division_interface_points_2d_coords);

        //Saves the faces created by the Delaunay algorithm into the mesh
        for(std::size_t i = 0; i < d.triangles.size(); i+=3) {
            assert(d.triangles[i + 2] < d.coords.size() / 2);

            //The Delaunay triangulation can create some triangles outside of the division interface. 
            //The following code checks if the triangle is inside the division interface
            const vec3 p0(d.coords[d.triangles[i    ] * 2], d.coords[d.triangles[i    ] * 2 + 1], 0.);
            const vec3 p1(d.coords[d.triangles[i + 1] * 2], d.coords[d.triangles[i + 1] * 2 + 1], 0.);
            const vec3 p2(d.coords[d.triangles[i + 2] * 2], d.coords[d.triangles[i + 2] * 2 + 1], 0.);

            //Compute the centroid of the triangle
            const vec3 face_centroid = (p0 + p1 + p2) / 3.;

            //If the triangle is in the division interface, then add it to the mesh
            if(cell_divider::point_is_in_polygon(division_face_points_coords, face_centroid)){

                assert(d.triangles[i]     < nb_points_division_interface);
                assert(d.triangles[i + 1] < nb_points_division_interface);
                assert(d.triangles[i + 2] < nb_points_division_interface);  

                //Get the ids of the 3 nodes in the mesh
                const unsigned n1_id =  division_point_ids_threshold + d.triangles[i    ];
                const unsigned n2_id =  division_point_ids_threshold + d.triangles[i + 1];
                const unsigned n3_id =  division_point_ids_threshold + d.triangles[i + 2];

                //Add the face to the mesh
                m.face_point_ids.insert(m.face_point_ids.end(),{n1_id, n2_id, n3_id});
            }
        }

    } catch (const std::exception& e) {
        // Include original error message for better diagnostics
        throw division_exception(std::string("Delaunay triangulation failed: ") + e.what());
    }

    //The Delaunay triangulation is not constrained by the boundary of the interface: when the boundary
    //polygon is not convex, some of its sides can be missing from the triangles kept above, which leaves
    //a hole between the interface and the rest of the surface of each daughter cell. In that case the
    //boundary polygon alone is triangulated by ear clipping; its long edges are split afterwards, when
    //the meshes of the daughter cells are refined.
    const unsigned nb_boundary_points = static_cast<unsigned>(division_face_points_coords.size());
    std::set<std::pair<unsigned, unsigned>> interface_edges;
    for(unsigned f_id = division_face_ids_threshold; f_id < m.face_point_ids.size(); f_id++){
        const auto& f = m.face_point_ids[f_id];
        for(unsigned k = 0; k < 3; k++){
            interface_edges.insert(std::make_pair(std::min(f[k], f[(k + 1) % 3]), std::max(f[k], f[(k + 1) % 3])));
        }
    }
    bool boundary_is_covered = true;
    for(unsigned i = 0; i < nb_boundary_points && boundary_is_covered; i++){
        const unsigned a = division_point_ids_threshold + i;
        const unsigned b = division_point_ids_threshold + (i + 1) % nb_boundary_points;
        boundary_is_covered = interface_edges.count(std::make_pair(std::min(a, b), std::max(a, b))) != 0;
    }

    if(!boundary_is_covered){
        const auto triangle_lst = triangulate_polygon_by_ear_clipping(division_face_points_coords);
        if(triangle_lst.empty()){throw division_exception("The division interface could not be triangulated");}

        m.face_point_ids.erase(m.face_point_ids.begin() + division_face_ids_threshold, m.face_point_ids.end());
        for(const auto& [i1, i2, i3] : triangle_lst){
            m.face_point_ids.insert(m.face_point_ids.end(), {division_point_ids_threshold + i1, division_point_ids_threshold + i2, division_point_ids_threshold + i3});
        }
    }
    //catch (const std::runtime_error& error) {
    //    throw division_exception("The Delaunay algorithm failed to triangulate the division interface.");
    //}


}
//-----------------------------------------------------------------------------------------------


//-----------------------------------------------------------------------------------------------
//Check if a point is inside a polygon
bool cell_divider::point_is_in_polygon(
    const std::vector<vec3>& polygon, 
    const vec3& point
    ) noexcept{

    bool c = false;
    const double x = point.dx();
    const double y = point.dy();
    unsigned i, j;
    const unsigned nvert =  polygon.size();

    for (i = 0, j = nvert-1; i < nvert; j = i++) {
        const double vert_i_x = polygon[i].dx();
        const double vert_i_y = polygon[i].dy();
        const double vert_j_x = polygon[j].dx();
        const double vert_j_y = polygon[j].dy();

        //The point is part of the division face
        if((vert_i_x == x && vert_i_y == y)){return true;}
        if (((vert_i_y>y) != (vert_j_y>y)) &&
            (x < (vert_j_x-vert_i_x) * (y-vert_i_y) / (vert_j_y-vert_i_y) + vert_i_x)){
            c = !c;
        }
    }
  return c;
}
//-----------------------------------------------------------------------------------------------





//-----------------------------------------------------------------------------------------------




//-----------------------------------------------------------------------------------------------
//Split the mesh of the mother cell into 2 meshes, one for each daughter cell. And create the daughter cells
std::pair<cell_ptr, cell_ptr> cell_divider::create_daughter_cells(
    cell_ptr mother_cell, 
    mesh& mother_cell_mesh,                             //The mesh containing all the faces and nodes
    const unsigned division_point_ids_threshold,        //All the nodes that belong to the division interface have an id greater than this threshold
    const unsigned division_face_ids_threshold,         //All the faces that belong to the division interface have an id greater than this threshold
    const vec3& division_plane_normal,                  //The normal vector of the division interface
    const vec3& division_plane_origin                   //A point located on the division plane
) noexcept(false){


    assert(mother_cell->get_cell_type()!= nullptr);
    assert(division_point_ids_threshold < mother_cell_mesh.node_pos_lst.size() / 3);
    assert(division_face_ids_threshold < mother_cell_mesh.face_point_ids.size());

    //Create a mesh of the mother cell which does not contain the division interface
    mesh mother_cell_mesh_no_division_interface;
    mother_cell_mesh_no_division_interface.node_pos_lst = mother_cell_mesh.node_pos_lst;
    std::copy(mother_cell_mesh.face_point_ids.begin(), mother_cell_mesh.face_point_ids.begin() + division_face_ids_threshold, std::back_inserter(mother_cell_mesh_no_division_interface.face_point_ids));

    
    //Create the two daughter cells
    cell_ptr daughter_cell_1 = mother_cell->get_cell_same_type(mother_cell_mesh_no_division_interface);
    cell_ptr daughter_cell_2 = mother_cell->get_cell_same_type(mother_cell_mesh_no_division_interface);

    //Get a reference of the daughter cells node_lst
    auto& mother_cell_node_lst = mother_cell->node_lst_;
    auto& daughter_cell_1_node_lst = daughter_cell_1->node_lst_;
    auto& daughter_cell_2_node_lst = daughter_cell_2->node_lst_;

    //Map the attributes of the nodes of the mother cell to the nodes of the daughter cell
    for(unsigned i = 0; i < mother_cell_node_lst.size(); i++){
        daughter_cell_1_node_lst[i] = mother_cell_node_lst[i];
        daughter_cell_2_node_lst[i] = mother_cell_node_lst[i];
    }

    //Hand each face of the mother to one daughter by walking over the surface from face to face without
    //crossing the cut (the edges joining two division points): one closed cut splits the surface into
    //exactly two pieces. Faces used to be handed out by the side of the plane they lie on, which broke
    //every cell that the plane meets along more than one curve: the faces around the curves that were
    //not cut straddle the plane, and splitting them between the daughters left a hole in each.
    const auto& mother_face_lst = daughter_cell_1->face_lst_;
    const unsigned nb_mother_faces = static_cast<unsigned>(mother_face_lst.size());

    std::map<std::pair<unsigned, unsigned>, std::vector<unsigned>> edge_face_map;
    for(unsigned f_id = 0; f_id < nb_mother_faces; f_id++){
        auto [n1_id, n2_id, n3_id] = mother_face_lst[f_id].get_node_ids();
        for(auto [a, b] : {std::make_pair(n1_id, n2_id), std::make_pair(n2_id, n3_id), std::make_pair(n3_id, n1_id)}){
            edge_face_map[std::make_pair(std::min(a, b), std::max(a, b))].push_back(f_id);
        }
    }

    std::vector<int> piece_lst(nb_mother_faces, -1);
    int nb_pieces = 0;
    for(unsigned seed_face_id = 0; seed_face_id < nb_mother_faces; seed_face_id++){
        if(piece_lst[seed_face_id] != -1) continue;
        if(nb_pieces == 2){throw division_exception("The division cut does not split the surface of the cell in two");}

        std::vector<unsigned> face_stack{seed_face_id};
        piece_lst[seed_face_id] = nb_pieces;
        while(!face_stack.empty()){
            const unsigned f_id = face_stack.back();
            face_stack.pop_back();

            auto [n1_id, n2_id, n3_id] = mother_face_lst[f_id].get_node_ids();
            for(auto [a, b] : {std::make_pair(n1_id, n2_id), std::make_pair(n2_id, n3_id), std::make_pair(n3_id, n1_id)}){
                //Do not cross the cut
                if(a >= division_point_ids_threshold && b >= division_point_ids_threshold) continue;

                for(const unsigned neighbor_face_id : edge_face_map[std::make_pair(std::min(a, b), std::max(a, b))]){
                    if(piece_lst[neighbor_face_id] == -1){
                        piece_lst[neighbor_face_id] = nb_pieces;
                        face_stack.push_back(neighbor_face_id);
                    }
                }
            }
        }
        nb_pieces++;
    }
    if(nb_pieces != 2){throw division_exception("The division cut does not split the surface of the cell in two");}

    //As before, daughter 1 keeps the piece lying on the negative side of the plane
    std::array<double, 2> nb_faces_in_piece{0., 0.};
    std::array<double, 2> nb_positive_faces_in_piece{0., 0.};
    for(unsigned f_id = 0; f_id < nb_mother_faces; f_id++){
        nb_faces_in_piece[piece_lst[f_id]] += 1.;
        if(cell_divider::face_side_wrt_plane(mother_face_lst[f_id], daughter_cell_1, division_plane_origin, division_plane_normal)){
            nb_positive_faces_in_piece[piece_lst[f_id]] += 1.;
        }
    }
    const int positive_piece = (nb_positive_faces_in_piece[0] / nb_faces_in_piece[0] > nb_positive_faces_in_piece[1] / nb_faces_in_piece[1]) ? 0 : 1;

    std::vector<unsigned> faces_to_remove_1;
    std::vector<unsigned> faces_to_remove_2;
    for(unsigned f_id = 0; f_id < nb_mother_faces; f_id++){
        if(piece_lst[f_id] == positive_piece) faces_to_remove_1.push_back(f_id);
        else                                  faces_to_remove_2.push_back(f_id);
    }
    assert(faces_to_remove_1.size() != 0 && faces_to_remove_2.size() != 0);
    
    //Remove the faces that are on the wrong side of the division plane
    remove_index(daughter_cell_1->face_lst_, faces_to_remove_1);
    remove_index(daughter_cell_2->face_lst_, faces_to_remove_2);

    //We add all the faces of the division interface
    for(unsigned f_id = division_face_ids_threshold; f_id < mother_cell_mesh.face_point_ids.size(); f_id++){
        
        //Transform the mesh face into a cell face
        auto& f_mesh = mother_cell_mesh.face_point_ids[f_id];
        assert(f_mesh.size() == 3); // Make sure the face is a triangle

        //Create the cell face
        face f1(f_mesh[0], f_mesh[2], f_mesh[1], f_id);
        face f2(f_mesh[0], f_mesh[1], f_mesh[2], f_id);


        //Add the face to the daughter cell 1
        daughter_cell_1->face_lst_.push_back(f1);

        //Add the face to the daughter cell 2
        daughter_cell_2->face_lst_.push_back(f2);
    }

    //Initialize the properties of the 2 daughter cells
    daughter_cell_1->initialize_cell_properties();  
    daughter_cell_2->initialize_cell_properties(); 

    //Check that the daughter cells have the same type as the mother cell
    assert(daughter_cell_1->get_cell_type()->global_type_id_ == mother_cell->get_cell_type()->global_type_id_);
    assert(daughter_cell_2->get_cell_type()->global_type_id_ == mother_cell->get_cell_type()->global_type_id_);

    return std::make_pair(daughter_cell_1, daughter_cell_2);
}
//-----------------------------------------------------------------------------------------------


