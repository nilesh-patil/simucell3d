#ifndef DEF_EPITHELIAL_CELL
#define DEF_EPITHELIAL_CELL

#include <cassert>

#include "../cell.hpp"

#include "utils.hpp"


/**
 * @file epithelial_cell.hpp
 * @brief Epithelial cell class (global_cell_id 0): division and contact-based polarization.
 * @ingroup cell_classes
 */



typedef std::pair<unsigned, double> face_area_pair;

//-----------------------------------------------------------------------------------------------------
/**
 * @brief Hash functor for (face id, area) pairs: hashes the face id only.
 * @ingroup cell_classes
 */
struct face_area_pair_hasher{
    int operator()(const face_area_pair& fap) const{
        return static_cast<int>(fap.first);
    }
};
//-----------------------------------------------------------------------------------------------------


//-----------------------------------------------------------------------------------------------------
/**
 * @brief Equality functor for (face id, area) pairs: compares the face ids only.
 * @ingroup cell_classes
 */
struct face_area_pair_comparator{
    bool operator()(const face_area_pair& fap_1, const face_area_pair& fap_2) const{
        return fap_1.first == fap_2.first;
    }
};
//----------------------------------------------------------------------------------------------------

typedef std::unordered_set<face_area_pair, face_area_pair_hasher,  face_area_pair_comparator> face_area_pair_set;




/**
 * @brief Epithelial cell, selected by `global_cell_id` 0.
 *
 * The only cell class that divides: is_ready_to_divide() is true once the volume reaches the
 * division volume drawn at creation. Its faces are polarized by contacts. Face types are
 * indices into the cell type's face types and are used as 0 apical, 1 lateral, 2 basal, so
 * an epithelial cell type is expected to list its face types in that order.
 *
 * At every iteration update_face_types() marks all faces apical. Then, with
 * POLARIZATION_MODE_INDEX 1:
 * - with contact model 0, the contact model calls face_is_in_contact() for each face in
 *   contact (basal next to an ECM cell, apical next to a lumen, lateral otherwise);
 * - with contact models 1 and 2, special_polarization_update() marks a face lateral when its
 *   three nodes are coupled to nodes of one other cell that form an edge triangle (model 1)
 *   or a face (model 2) of that cell.
 *
 * @ingroup cell_classes
 */
class epithelial_cell: public cell {

    private: 
        friend class cell_divider;
        friend class cell_tester;

        
    protected:


    public:

        //------------------------------------------------------------------------------------------------------
        //Make sure the epithelial_cell cannot be default instantiated
        epithelial_cell() = delete;                        //default constructor
        epithelial_cell(const epithelial_cell& c) = default;           //copy constructor
        epithelial_cell(epithelial_cell&& c) = delete;                //move constructor
        epithelial_cell& operator=(const epithelial_cell& c) = delete;//copy assignment operator
        epithelial_cell& operator=(epithelial_cell&& c) = delete;     //move assignment operator 
        //------------------------------------------------------------------------------------------------------


        //------------------------------------------------------------------------------------------------------ 
        /** @brief Create the cell from its nodes and faces; see the corresponding cell constructor. */
        epithelial_cell(
            const std::vector<node>& node_lst,
            const std::vector<face>& face_lst,
            unsigned int cell_id,
            cell_type_param_ptr cell_type_parameters
        ) noexcept : cell(node_lst, face_lst, cell_id, cell_type_parameters){}




        /** @brief Create the cell from flat node coordinates (m) and face node ids; see the corresponding cell constructor. */
        epithelial_cell(
            const std::vector<double>&  node_position,
            const std::vector<unsigned>& face_node_ids,
            unsigned cell_id,
            cell_type_param_ptr cell_type_parameters
        ) noexcept : cell(node_position, face_node_ids, cell_id, cell_type_parameters){}


        /** @brief Create the cell from a mesh; see the corresponding cell constructor. */
        epithelial_cell(
            const mesh& m,
            unsigned cell_id,
            cell_type_param_ptr cell_type_parameters
        ) noexcept : cell(m, cell_id, cell_type_parameters){}
        //------------------------------------------------------------------------------------------------------


        //------------------------------------------------------------------------------------------------------
        /** @brief Return true once the volume (m^3) has reached the cell's division volume. */
        bool is_ready_to_divide() const noexcept override {
            assert(cell_type_ != nullptr);
            return volume_ >= division_volume_; 
        }
        //------------------------------------------------------------------------------------------------------

        //------------------------------------------------------------------------------------------------------
        /** @brief Return a new epithelial_cell built from @p m, with the same id and cell type (used for daughters). */
        cell_ptr get_cell_same_type(const mesh& m) noexcept(false) override{
            //Return a copy of the cell
            return std::make_shared<epithelial_cell>(m, cell_id_, cell_type_);
        }
        //------------------------------------------------------------------------------------------------------



        //------------------------------------------------------------------------------------------------------
        /**
         * @brief Set the type of face @p face_local_id from the global face type id of the face it touches.
         *
         * Global face type 3 (ECM) gives basal (2), 4 (lumen) gives apical (0), 5 (nucleus) leaves
         * the face unchanged, any other gives lateral (1). Not called by the 2.1 contact models.
         */
        void face_is_in_contact(const unsigned face_local_id, const cell_ptr other_cell, const face& other_face) noexcept override {
            assert(face_local_id < face_lst_.size());
            assert(other_cell != nullptr);
            
            //Ask to the other cell if the type of its face
            const auto& other_face_type = other_cell->get_face_type(other_face.get_local_id());

            //If the other face belongs to a nucleus we don't do anything
            if(other_face_type.face_type_global_id_ == 5) return;
            
            //If the other face is of type ECM, we set the given face as basal
            if(other_face_type.face_type_global_id_ == 3){
                face_lst_[face_local_id].set_face_type_id(2);
            }
            //If the other face is of type lumen, we set the given face as apical
            else if(other_face_type.face_type_global_id_ == 4){
                face_lst_[face_local_id].set_face_type_id(0);
            }
            //Otherwise, we set the given face as lateral
            else{
                face_lst_[face_local_id].set_face_type_id(1);
            }
        }
        //------------------------------------------------------------------------------------------------------




        //------------------------------------------------------------------------------------------------------
        /**
         * @brief Set the type of face @p face_local_id from the class of the cell it touches.
         *
         * ECM (`global_cell_id` 1) gives basal (2), lumen (2) gives apical (0), any other gives
         * lateral (1). Called by contact model 0 when POLARIZATION_MODE_INDEX is 1.
         */
        void face_is_in_contact(const unsigned face_local_id, const cell_ptr other_cell) noexcept override{
            assert(face_local_id < face_lst_.size());
            assert(other_cell != nullptr);

            //Get the type of the other cell
            const cell_type_param_ptr other_cell_type = other_cell->get_cell_type();

            //If the other cell is of type ECM, we set the given face as basal
            if(other_cell_type->global_type_id_ == 1){
                face_lst_[face_local_id].set_face_type_id(2);
            }
            //If the other cell is a lumen cell, we set the given face as apical
            else if(other_cell_type->global_type_id_ == 2){
                face_lst_[face_local_id].set_face_type_id(0);
            }
            //Otherwise, we set the given face as lateral
            else{
                face_lst_[face_local_id].set_face_type_id(1);
            }
        }
        //------------------------------------------------------------------------------------------------------

        //------------------------------------------------------------------------------------------------------
        /**
         * @brief Mark as lateral (1) the faces coupled to a face of one neighbouring cell, the others apical (0).
         *
         * A face is lateral when its three nodes are coupled to nodes of a single other cell and
         * those nodes form a face (contact model 2) or three edges (contact model 1) of that cell.
         * @note Does nothing unless POLARIZATION_MODE_INDEX is 1 and CONTACT_MODEL_INDEX is 1 or 2.
         * Reads the node couplings without locks; call it after the contact phase.
         */
        void special_polarization_update(const std::vector<cell_ptr>& cell_lst) noexcept override{

            //If the faces of epithelial cells are mechanically coupled and the polarization mode is based on the contacts
            #if  POLARIZATION_MODE_INDEX == 1

                #if CONTACT_MODEL_INDEX == 1 

        
                    //Loop over the faces
                    for(face& f: face_lst_){

                        if(f.is_used()){

                            //Get the nodes of the face
                            node& n1 = get_node(f.n1_id());
                            node& n2 = get_node(f.n2_id());
                            node& n3 = get_node(f.n3_id());

                            if(n1.is_coupled() && n2.is_coupled()  && n3.is_coupled()){  



                                //Make sure the normals of the 3 nodes are pointing in the same direction as the face normal
                                bool b1 = n1.get_normal().dot(f.get_normal()) > 0;
                                bool b2 = n2.get_normal().dot(f.get_normal()) > 0;
                                bool b3 = n3.get_normal().dot(f.get_normal()) > 0;

                                if(b1 || b2 || b3){


                                    //Check if the nodes are coupled to the same cell
                                    const auto [n1_c2_id, n1_coupled_node_id] = n1.get_coupled_node();
                                    const auto [n2_c2_id, n2_coupled_node_id] = n2.get_coupled_node();
                                    const auto [n3_c2_id, n3_coupled_node_id] = n3.get_coupled_node();

                                    //If the 3 nodes are coupled to nodes on the same cell
                                    if(n1_c2_id == n2_c2_id && n1_c2_id == n3_c2_id){

                                        //Get a pointer to the other cell
                                        cell_ptr c2 = cell_lst[n1_c2_id];


                                        //Check if the 3 edges exist on the other cell
                                        const bool b4 = c2->get_edge(n1_coupled_node_id, n2_coupled_node_id).has_value();
                                        const bool b5 = c2->get_edge(n2_coupled_node_id, n3_coupled_node_id).has_value();
                                        const bool b6 = c2->get_edge(n3_coupled_node_id, n1_coupled_node_id).has_value();

                                        if(b4 && b5 && b6){
                                            f.set_face_type_id(1);
                                        }
                                        else{
                                            f.set_face_type_id(0);
                                        }
                                    }
                                    else{
                                        f.set_face_type_id(1);
                                    }
                                }
                                else{
                                    f.set_face_type_id(0);
                                }
                            }
                        }    
                    }
                #elif CONTACT_MODEL_INDEX == 2


                //Loop over the faces
                for(face& f: face_lst_){

                    if(f.is_used()){

                        //Get the nodes of the face
                        node& n1 = get_node(f.n1_id());
                        node& n2 = get_node(f.n2_id());
                        node& n3 = get_node(f.n3_id());

                        //If one of the node is not coupled to any other nodes on another cell, we mark the cell as apical and continue
                        if(!n1.is_coupled()|| !n2.is_coupled() || !n3.is_coupled()){
                            f.set_face_type_id(0);
                            continue;
                        }

                        //Else we gather the ids of the cells to which the nodes are coupled
                        std::vector<unsigned> n1_coupled_cells_ids;
                        std::vector<unsigned> n2_coupled_cells_ids;
                        std::vector<unsigned> n3_coupled_cells_ids;

                        n1_coupled_cells_ids.reserve(n1.coupled_nodes_map_.size());
                        n2_coupled_cells_ids.reserve(n2.coupled_nodes_map_.size());
                        n3_coupled_cells_ids.reserve(n3.coupled_nodes_map_.size());

                        for(const auto& [c2_local_id, coupled_node_data] : n1.coupled_nodes_map_){n1_coupled_cells_ids.push_back(c2_local_id);}
                        for(const auto& [c2_local_id, coupled_node_data] : n2.coupled_nodes_map_){n2_coupled_cells_ids.push_back(c2_local_id);}
                        for(const auto& [c2_local_id, coupled_node_data] : n3.coupled_nodes_map_){n3_coupled_cells_ids.push_back(c2_local_id);}

                        //Sort the coupled cell ids
                        std::sort(n1_coupled_cells_ids.begin(), n1_coupled_cells_ids.end());
                        std::sort(n2_coupled_cells_ids.begin(), n2_coupled_cells_ids.end());
                        std::sort(n3_coupled_cells_ids.begin(), n3_coupled_cells_ids.end());

                        //Check the intersection of the 3 sets of ids
                        std::vector<unsigned> intersection_1;
                        std::set_intersection(
                            n1_coupled_cells_ids.begin(), n1_coupled_cells_ids.end(),
                            n2_coupled_cells_ids.begin(), n2_coupled_cells_ids.end(),
                            std::back_inserter(intersection_1)
                        );

                        std::vector<unsigned> intersection_2;
                        std::set_intersection(
                            intersection_1.begin(), intersection_1.end(),
                            n3_coupled_cells_ids.begin(), n3_coupled_cells_ids.end(),
                            std::back_inserter(intersection_2)
                        );

                        //If the 3 nodes of the cell are coupled to the same cell
                        if(!intersection_2.empty()){

                            //Get the id of the coupled cell
                            const unsigned c2_id = intersection_2[0];

                            //We get the id of the coupled nodes on the other cell
                            const unsigned c2_n1_id = n1.coupled_nodes_map_[c2_id].first;
                            const unsigned c2_n2_id = n2.coupled_nodes_map_[c2_id].first;
                            const unsigned c2_n3_id = n3.coupled_nodes_map_[c2_id].first;

                            //Get the pointer to the other cell
                            cell_ptr c2 = cell_lst[c2_id];

                            //Check if the 3 nodes form a face on the other cell
                            face* c2_f2 = c2->get_face(c2_n1_id, c2_n2_id, c2_n3_id);

                            //If the face exists on the other cell
                            if(c2_f2 != nullptr){
                                
                                f.set_face_type_id(1);
                            }
                            else{
                                f.set_face_type_id(0);
                            }
                        }

                        //If the 3 nodes are coupled to different cells
                        else{
                            f.set_face_type_id(0);
                        }
                    }
                }
                #endif
            #endif
        };
        //------------------------------------------------------------------------------------------------------



        //------------------------------------------------------------------------------------------------------
        /** @brief Reset every face to apical (0); called at the start of each iteration. */
        void update_face_types() noexcept override{
        
            //Reset all the faces to apical
            std::for_each(face_lst_.begin(), face_lst_.end(), [](face& f){f.set_face_type_id(0);});
        }
        //------------------------------------------------------------------------------------------------------


        //------------------------------------------------------------------------------------------------------
        /** @brief Return the area of the lateral faces (type 1) divided by the cell area. */
        double get_contact_area_fraction() override {


            const double contact_area = std::accumulate(face_lst_.begin(), face_lst_.end(), 0.0, 
                [](double acc, const face& f){
                    return acc + ((f.get_local_face_type_id() == 1) ? f.get_area() : 0.);
                }
            );

            //Count the number of lateral faces
            const double nb_lat_faces = std::count_if(face_lst_.begin(), face_lst_.end(), 
                [](const face& f) -> bool{
                    return f.get_local_face_type_id() == 1;
                } 
            );
            return contact_area / area_;
        }
        //------------------------------------------------------------------------------------------------------

};

#endif