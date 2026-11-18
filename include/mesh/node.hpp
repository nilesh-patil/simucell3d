#ifndef DEF_NODE
#define DEF_NODE

/**
 * @file node.hpp
 * @brief Node (vertex) of a cell surface mesh.
 * @ingroup mesh
 */


#include <iostream>
#include <memory>
#include <optional>
#include <map>
#include <mutex>
#include <limits>
#include <cassert>

#include <omp.h>

#include "global_configuration.hpp"


class face;
class cell;

#include "vec3.hpp"


typedef std::shared_ptr<cell> cell_ptr;

/**
 * @brief Node (vertex) of a cell surface mesh.
 *
 * A node stores its position (m) and the force acting on it (N). With the full equations of
 * motion (DYNAMIC_MODEL_INDEX 0) it also stores its momentum (kg m/s). With the coupling
 * contact models (CONTACT_MODEL_INDEX 1 or 2) it also stores its normal, its curvature and
 * its coupling partners on neighbouring cells, protected by a per-node std::mutex.
 *
 * Nodes are owned by their cell and identified by their index in the cell's node list
 * (the local id). A node whose slot is free has is_used() false.
 *
 * @note Members inside `#if CONTACT_MODEL_INDEX` or `#if DYNAMIC_MODEL_INDEX` blocks exist
 * only for those values of the macros in global_configuration.hpp.
 *
 * @ingroup mesh
 */
class node final 
{

    private: 



        //Each node has a unique identifier
        unsigned node_id_;

        //Wether or not it is used by a cell
        bool is_used_ = true;

        //cell is a friend class because its the only one that can reset the node id
        friend class cell;
        friend class epithelial_cell;
        friend class local_mesh_refiner;
        friend class time_integration_scheme;
        friend class dynamic_motion_semi_implicit_euler;       //The time integration scheme
        friend class overdamped_motion_forward_euler;          //The time integration scheme
        friend class overdamped_motion_improved_euler;         //The time integration scheme
        
        //The class that computes the contact forces between the cells
        friend class contact_model_abstract;
        friend class contact_node_node_via_coupling;
        friend class contact_face_face_via_coupling;
        friend class contact_node_face_via_spring;

        friend class cell_divider;
        friend class node_tester;
        friend class cell_tester;


        
    protected:

        ///If we use a contact model that mechanically link the nodes or the faces of adjacent cells
        #if CONTACT_MODEL_INDEX == 1 ||  CONTACT_MODEL_INDEX == 2
            //The normal of the node
            vec3 normal_;

            //Curvature of the cell surface at the node
            double curvature_;

            // Lock that serialises updates of the coupling data by several threads
            mutable std::mutex mutex_;


        #endif


        //If we link pairs of adjacent nodes
        #if CONTACT_MODEL_INDEX == 1

            //This the node that is coupled to this node. A node can only be coupled to one other node
            std::optional<std::pair<unsigned, unsigned>> coupled_node_;

            //The squared distance to the closest node on another cell
            double squared_distance_to_closest_node_ = 0.;
        
        //If we link pairs of adjacent faces
        #elif CONTACT_MODEL_INDEX == 2

            //Each node can be coupled to one node on each adjacent cell. I.E. a node can be coupled to multiple nodes
            //if they belong to different cells

            //The key of this map is the local id of the adjacent cell
            //The value is the local id of the coupled node as well as the distance to this coupled node
            std::map<unsigned, std::pair<unsigned, double>> coupled_nodes_map_;

        #endif


        //Based on the dynamic model chosen to run the simulation the nodes need to store different state variables
        vec3 pos_, force_;

        #if DYNAMIC_MODEL_INDEX == 0 //Full equations of motion solved with the semi implicit euler scheme
            vec3 momentum_;
        #endif 


        void set_local_id(const unsigned node_id) noexcept {node_id_ = node_id;}

        //Reset the vectors owned by the node
        void reset() noexcept;

        //Indicate wether or not the node is used by a cell
        void set_is_used(const bool is_used) noexcept {is_used_  = is_used;}

    public:

        /** @brief Create a used node with local id 0. */
        node() noexcept;

        /**
         * @brief Copy all node data except the lock; the copy has its own unlocked mutex.
         */
        node(const node& n) noexcept;

        /** @brief Move all node data except the lock; the new node has its own unlocked mutex. */
        node(node&& n) noexcept;

        /** @brief Copy all node data except the lock, which stays the node's own. */
        node& operator=(const node& n) noexcept;

        /** @brief Move all node data except the lock, which stays the node's own. */
        node& operator=(node&& n) noexcept;

        /** @brief Create a node at the origin with local id @p node_id. */
        explicit node(const unsigned node_id) noexcept;

        /** @brief Create a node at (@p dx, @p dy, @p dz), m, with local id @p node_id. */
        explicit node(const double dx, const double dy, const double dz, const unsigned node_id) noexcept;

        /** @brief Create a node at @p pos, m, with local id @p node_id. */
        explicit node(const vec3& pos, const unsigned node_id) noexcept;
        
        /** @brief Return the index of the node in the node list of its cell. */
        unsigned get_local_id() const noexcept {return node_id_;}

        /** @brief Return the position of the node, m. */
        inline const vec3& pos()       const noexcept {return pos_;}
        /** @brief Return the force accumulated on the node in the current iteration, N. */
        inline const vec3& force()     const noexcept {return force_;}
        /** @brief Return true if this slot of the node list holds a node of the mesh. */
        inline const bool  is_used()   const noexcept {return is_used_;}

        //If we use a contact model that mechanically link the nodes or the faces of adjacent cells
        #if CONTACT_MODEL_INDEX == 1 ||  CONTACT_MODEL_INDEX == 2

            /**
             * @brief Return the unsigned discrete mean curvature at the node, 1/m.
             * @note Only with CONTACT_MODEL_INDEX 1 or 2. Computed by cell::compute_node_curvature_and_normals().
             */
            double get_curvature() const noexcept {return curvature_;}

            /** @brief Return the unit outward normal at the node. @note Only with CONTACT_MODEL_INDEX 1 or 2. */
            vec3 get_normal() const noexcept {return normal_;}

            /** @brief Destructor. */
            ~node() = default;

        #endif

        //If we link pairs of adjacent nodes
        #if CONTACT_MODEL_INDEX == 1

            /// @cond SIMUCELL3D_DOXYGEN_SKIP
            // Documented once, in the CONTACT_MODEL_INDEX 2 branch below (same declaration).
            bool is_coupled() const noexcept {return coupled_node_.has_value();}
            /// @endcond

            /**
             * @brief Return the partner as (cell local id, node local id). The node must be coupled.
             * @note Only with CONTACT_MODEL_INDEX 1.
             */
            std::pair<unsigned, unsigned> get_coupled_node() const noexcept {return coupled_node_.value();}

            /**
             * @brief Store the coupling partner and its squared distance (m^2), under the node lock.
             * @note Only with CONTACT_MODEL_INDEX 1. Thread-safe.
             */
            void set_coupled_node_and_min_distance(const std::pair<unsigned, unsigned>& coupled_node, const double min_dist) noexcept {
                //Make sure that one thread at a time can access this function
                std::lock_guard<std::mutex> guard(mutex_);
                coupled_node_ = coupled_node;
                squared_distance_to_closest_node_ = min_dist;
            }

        //If we link pairs of adjacent faces
        #elif CONTACT_MODEL_INDEX == 2

            /**
             * @brief Return true if the node is coupled to at least one node on another cell.
             * @note With CONTACT_MODEL_INDEX 2. Not locked; read outside the contact phase. With
             *       CONTACT_MODEL_INDEX 1 the same function returns true if the node has its one partner.
             */
            bool is_coupled() const noexcept {return !coupled_nodes_map_.empty();}

            /** @brief Return the number of coupled nodes on other cells. @note Only with CONTACT_MODEL_INDEX 2. */
            int get_nb_coupled_nodes() const noexcept {return coupled_nodes_map_.size();}

            //The contact model couples nodes from several threads at once: a node is written by the thread that
            //processes its own cell AND by every thread that processes a node facing it (see
            //contact_face_face_via_coupling::resolve_contact()). Every access to coupled_nodes_map_ during the
            //contact phase must therefore go through the locked methods below.

            /**
             * @brief Return the squared distance (m^2) to the partner on cell @p coupled_cell_local_id.
             *
             * Returns std::numeric_limits<double>::max() if the node has no partner on that cell.
             * @note Only with CONTACT_MODEL_INDEX 2. Thread-safe (locked read).
             */
            double get_coupled_squared_distance(const unsigned coupled_cell_local_id) const noexcept {
                std::lock_guard<std::mutex> guard(mutex_);
                const auto it = coupled_nodes_map_.find(coupled_cell_local_id);
                return (it != coupled_nodes_map_.end()) ? it->second.second : std::numeric_limits<double>::max();
            }

            /**
             * @brief Couple this node to node @p coupled_node_local_id of cell @p coupled_cell_local_id.
             *
             * A node has at most one partner per adjacent cell, the closest one found: the partner is
             * stored unless the node already has a strictly closer partner on that cell. The compare
             * and the store happen under the node lock, so a farther candidate found by another
             * thread never replaces a closer one.
             * @param squared_distance Squared distance between the two nodes, m^2.
             * @return True if the partner was stored.
             * @note Only with CONTACT_MODEL_INDEX 2. Thread-safe.
             */
            bool set_coupled_node_and_min_distance(const unsigned coupled_cell_local_id, const unsigned coupled_node_local_id,  const double squared_distance) noexcept {
                std::lock_guard<std::mutex> guard(mutex_);
                return store_partner_unless_closer_one(coupled_cell_local_id, coupled_node_local_id, squared_distance);
            }

            /**
             * @brief Couple node @p a of cell @p a_cell_local_id and node @p b of cell @p b_cell_local_id to each other.
             *
             * Both node locks are held (std::scoped_lock, deadlock-free) and the pair is stored only
             * if neither node already has a strictly closer partner on the other node's cell. Either
             * both directions are stored or neither.
             * @param squared_distance Squared distance between the two nodes, m^2.
             * @return True if the pair was stored.
             * @note Only with CONTACT_MODEL_INDEX 2. Thread-safe.
             */
            static bool couple_if_closest(node& a, const unsigned a_cell_local_id, node& b, const unsigned b_cell_local_id, const double squared_distance) noexcept {
                assert(&a != &b);
                std::scoped_lock guard(a.mutex_, b.mutex_);
                if(a.has_closer_partner(b_cell_local_id, squared_distance) || b.has_closer_partner(a_cell_local_id, squared_distance)) return false;
                a.coupled_nodes_map_[b_cell_local_id] = std::make_pair(b.node_id_, squared_distance);
                b.coupled_nodes_map_[a_cell_local_id] = std::make_pair(a.node_id_, squared_distance);
                return true;
            }

        private:
            //These two must be called with mutex_ held
            bool has_closer_partner(const unsigned coupled_cell_local_id, const double squared_distance) const noexcept {
                const auto it = coupled_nodes_map_.find(coupled_cell_local_id);
                return it != coupled_nodes_map_.end() && it->second.second < squared_distance;
            }

            bool store_partner_unless_closer_one(const unsigned coupled_cell_local_id, const unsigned coupled_node_local_id, const double squared_distance) noexcept {
                if(has_closer_partner(coupled_cell_local_id, squared_distance)) return false;
                coupled_nodes_map_[coupled_cell_local_id] = std::make_pair(coupled_node_local_id, squared_distance);
                return true;
            }

        public:
        #endif




        /** @brief Replace the force on the node, N. Not thread-safe. */
        void set_force(const vec3& force) noexcept {force_.reset(force);}
        /** @brief Add @p force (N) to the force on the node. Not thread-safe. */
        void add_force(const vec3& force) noexcept {force_.translate(force);}

        /**
         * @brief Add @p force (N) to the force on the node with atomic updates of each component.
         *
         * Used for contact forces: a node receives forces from the thread that processes its own
         * cell and from the threads that process the cells facing it.
         * @note Thread-safe.
         */
        void add_force_atomic(const vec3& force) noexcept {force_.atomic_translate(force);}

        /** @brief Return the vector from node @p n to this node, m. */
        vec3 operator-(const node& n) const noexcept; 
        /** @brief Two nodes are equal if they have the same local id. */
        bool operator==(const node& n) const noexcept; 
        bool operator!=(const node& n) const noexcept;

        //Full equations of motion solved with the semi implicit euler scheme
        #if DYNAMIC_MODEL_INDEX == 0 

            /** @brief Return the momentum of the node, kg m/s. @note Only with DYNAMIC_MODEL_INDEX 0. */
            inline const vec3& momentum()  const noexcept {return momentum_;}
            /** @brief Set the momentum of the node, kg m/s. @note Only with DYNAMIC_MODEL_INDEX 0. */
            void set_momentum(const vec3& momentum) noexcept {momentum_ = momentum;}
        #endif 


};

#endif