#ifndef DEF_FACE
#define DEF_FACE

/**
 * @file face.hpp
 * @brief Triangular face of a cell surface mesh.
 * @ingroup mesh
 */

#include <cassert>
#include <map>
#include <iostream>
#include <optional>
#include <memory>
#include <omp.h>

class node;
class cell;

#include "global_configuration.hpp"

#include "vec3.hpp"
#include "node.hpp"



typedef std::shared_ptr<cell> cell_ptr;

/**
 * @brief Triangular face of a cell surface mesh.
 *
 * A face stores the local ids of its three nodes, its unit outward normal, its area (m^2)
 * and its face type, given as the index of the face type in the cell type's list of face
 * types. Normals and areas are computed by the owning cell (cell::update_all_face_normals_and_areas()).
 * Faces live in the face list of their cell; a face whose slot is free has is_used() false.
 *
 * @ingroup mesh
 */

class face final
{

    private: 
        vec3 normal_;                    // Face outward normal
        double area_ = -1.;              // Face area

        //The id of the face in the cell lst
        unsigned local_face_id_;

        //The id of the face in the mesh. This global id is only used
        //by the contact detection modules
        unsigned global_face_id_;

        //A pointer toward the cell that owns this face
        cell_ptr owner_cell_ = nullptr;

        //The type of the face
        unsigned short type_id_ = 0;

        //Only compiles this section if the faces store their contact energies
        #if FACE_STORE_CONTACT_ENERGY
            //Store the adhesion and repulsion energies associated to this face, float are good enough
            //since these values are only used for plotting
            float adhesion_energy_ = 0.;
            float repulsion_energy_ = 0.;
        #endif

        //cell is a friend class because its the only one that can reset the face id
        friend class cell;
        friend class local_mesh_refiner;

        //All the contact model classes have direct access to the face attributes 
        //to compute more efficiently the contact forces
        friend class contact_model_abstract;
        friend class contact_node_node_via_coupling;
        friend class contact_face_face_via_coupling;
        friend class contact_node_face_via_spring;

        friend class cell_divider;
        friend class face_tester;
        friend class cell_tester;



    //All this functions are protected to make sure that only the cell owning this face
    //can call them.
    protected:

        //The id of the node composing the face
        //A face might be left in a state where it is not connected to 
        //any node.
        unsigned n1_id_ = 0;
        unsigned n2_id_ = 0;
        unsigned n3_id_ = 0; 


        //Indicate if the face is used by the cell to store nodes
        bool is_used_ = true;

        //The ids of the nodes might change, so we need to update them in each face
        void update_node_ids(std::map<unsigned, unsigned>& node_id_correspondence) noexcept;

        //Only the cell should modify the local ids of its faces
        void set_local_id(const unsigned id) noexcept {local_face_id_ = id;}; 
   
        //Empties the face content
        void reset() noexcept;

        //Swap the second and third node to correctly orient the face normal
        void swap_nodes() noexcept;

        //Only the cell can set the area of a face
        void set_area(double area) noexcept;
        void set_normal(const vec3& normal) noexcept{normal_ = normal;}


        //Only compiles this section if the faces store their contact energies
        #if FACE_STORE_CONTACT_ENERGY
            void add_adhesion_energy(float adhesion_energy) noexcept {
                #pragma omp atomic update
                adhesion_energy_ += adhesion_energy;
            }

            void add_repulsion_energy(float repulsion_energy) noexcept {
                #pragma omp atomic update
                repulsion_energy_ += repulsion_energy;
            }
        #endif




    public:

        //Make sure a face cannot be instantiated without its nodes
        face() = default;
        face(const face& f) = default;            //copy constructor
        face(face&& f) = default;                  //move constructor
        face& operator=(const face& f) = default; //copy assignment operator
        face& operator=(face&& f) = default;      //move assignment operator 

        /** @brief Create a face from the local ids of its three nodes and its own local id @p face_id. */
        explicit face(const unsigned node_id_1, const unsigned node_id_2, const unsigned node_id_3, const unsigned face_id) noexcept;

        /** @brief Create a face from its three nodes and its own local id @p face_id. */
        explicit face(const node& n1, const node& n2, const node& n3, const unsigned face_id) noexcept;

        /** @brief Return the id of the face in the face list of its cell. */
        const unsigned get_local_id() const noexcept {return local_face_id_;};
 
        /** @brief Return the area of the face, m^2, as last computed by the owning cell. */
        inline double get_area() const noexcept {return area_;}

        /**
         * @brief Set the face type, as an index into the face types of the cell type.
         * @note The write is atomic (OpenMP), so several threads may set it.
         */
        void set_face_type_id(const unsigned short type_id) noexcept {

            //Protect against race conditions
            #pragma omp atomic write
            type_id_ = type_id;
        };

        /** @brief Return the cell that owns the face. May be a null pointer. */
        cell_ptr get_owner_cell() const noexcept {return owner_cell_;}

        /** @brief Return the unit outward normal (the zero vector for a degenerate face). */
        inline const vec3& get_normal() const noexcept {return normal_;}

        /** @brief Return true if this slot of the face list holds a face of the mesh. */
        bool is_used() const noexcept {return is_used_;}
        /** @brief Mark this slot of the face list as used or free. */
        void set_is_used(bool is_used) noexcept {is_used_ = is_used;}


        /** @brief Return true if the node with local id @p node_id is a vertex of the face. */
        bool has_node(const unsigned node_id) const noexcept{
            return (node_id == n1_id_ || node_id == n2_id_ || node_id == n3_id_);
        }

        /** @brief Return the local ids of the three nodes, in winding order. */
        std::array<unsigned, 3> get_node_ids() const noexcept; 

        /** @name Node ids
         *  Local ids of the first, second and third node (the face must be used). */
        ///@{
        unsigned n1_id() const noexcept {assert(is_used_); return n1_id_;}
        unsigned n2_id() const noexcept {assert(is_used_); return n2_id_;}
        unsigned n3_id() const noexcept {assert(is_used_); return n3_id_;}
        ///@}


        /** @brief Replace the node @p old_node_id by @p new_node_id, keeping the winding order. */
        void replace_node(const unsigned old_node_id, const unsigned new_node_id) noexcept;

        /** @brief Return the node of the face that is not on the edge (@p n1, @p n2). */
        unsigned get_opposite_node(const unsigned n1, const unsigned n2) const noexcept;

        /** @brief Return the face type, as an index into cell_type_parameters::face_types_. */
        unsigned short get_local_face_type_id() const noexcept {return type_id_;}
      
        /** @name Contact energies
         *  Adhesion and repulsion energies accumulated on the face, J.
         *  @note Present only when FACE_STORE_CONTACT_ENERGY is true. */
        ///@{
        #if FACE_STORE_CONTACT_ENERGY
            float get_adhesion_energy()  const noexcept {return adhesion_energy_;}
            float get_repulsion_energy() const noexcept {return repulsion_energy_;}
        #endif
        ///@}

        /** @brief Two faces are equal if they have the same local id. */
        bool operator==(const face& f) const noexcept; 
        bool operator!=(const face& f) const noexcept;

};

#endif