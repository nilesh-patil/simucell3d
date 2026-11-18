#ifndef DEF_EDGE
#define DEF_EDGE

/**
 * @file edge.hpp
 * @brief Edge of a cell surface mesh: two nodes and the (at most two) faces that share them.
 * @ingroup mesh
 */

#include <cassert>
#include <iostream>
#include <optional>
#include <memory>
#include <unordered_set>


class node;
class face;


#include "node.hpp"
#include "face.hpp"
#include "custom_exception.hpp"



/**
 * @brief Edge of a cell surface mesh, defined by two node ids and up to two face ids.
 *
 * The node ids are stored in increasing order, so that an edge does not depend on the
 * direction in which it was created. All ids are local to the owning cell. An edge with
 * two faces is manifold (see is_manifold()).
 *
 * @ingroup mesh
 */

class edge 
{

    private: 

        //The ids of the 2 nodes owning this edge
        unsigned n1_id_;
        unsigned n2_id_;

        //The ids of the 2 edges owning this edge
        std::optional<unsigned> f1_id_ = std::nullopt;
        std::optional<unsigned> f2_id_ = std::nullopt;


    public:

        //Make sure a edge cannot be instantiated without its nodes
        edge() = default; // Have to exist in order to create empty arrayrs of edges
        edge(const edge& e) = default;           //copy constructor
        edge(edge&& e) = default;                //move constructor
        edge& operator=(const edge& e) = default;//copy assignment operator
        edge& operator=(edge&& e) = default;     //move assignment operator 

        /** @brief Create the edge between two nodes, using their local ids. */
        edge(const node& n1, const node& n2) noexcept;

        /** @brief Create the edge between the nodes with local ids @p n1 and @p n2, without faces. */
        explicit edge(unsigned n1, unsigned n2) noexcept;

        /** @brief Create the edge between nodes @p n1 and @p n2, shared by faces @p f1 and @p f2 (which must differ). */
        explicit edge(unsigned n1, unsigned n2, unsigned f1, unsigned f2) noexcept;

        /** @brief Return the local ids of the two nodes, smallest first. */
        std::pair<unsigned, unsigned> get_node_ids() const noexcept;

        /** @brief Return the local ids of the two faces. The edge must be manifold. */
        std::pair<unsigned, unsigned> get_face_ids() const noexcept;

        /** @brief Exchange the first and second face ids. The edge must be manifold. */
        void swap_face_ids() noexcept;

        /**
         * @brief Remove the face @p f_id from the edge; a remaining second face becomes the first.
         * @throws mesh_integrity_exception If the edge does not reference @p f_id.
         */
        void delete_face(const unsigned f_id) noexcept(false);

        /// @brief Local id of the first face (must be set).
        unsigned f1() const noexcept {assert(f1_id_); return f1_id_.value();}
        /// @brief Local id of the second face (must be set).
        unsigned f2() const noexcept {assert(f2_id_); return f2_id_.value();}
        /// @brief Local id of the first node (the smaller id).
        unsigned n1() const noexcept {return n1_id_;}
        /// @brief Local id of the second node (the larger id).
        unsigned n2() const noexcept {return n2_id_;}

        /**
         * @brief Attach the face @p f_id to the edge, in the first free slot.
         * @throws mesh_integrity_exception If the edge already has two faces.
         */
        void add_face(const unsigned f_id) noexcept(false);
        
        /** @brief Return true if the edge is shared by exactly two faces. */
        bool is_manifold() const noexcept;

        /** @brief Cantor pairing of the two node ids; unique for each pair of nodes. */
        size_t hash() const noexcept{
            return 0.5 * (n1_id_ + n2_id_) * (n1_id_ + n2_id_ + 1) + n2_id_;
        }

        /** @brief Two edges are equal if they join the same two nodes; faces are ignored. */
        bool operator==(const edge& e) const noexcept;
        bool operator!=(const edge& e) const noexcept;

        /** @brief Return true if @p f_id is one of the faces of the edge. */
        bool has_face(const unsigned f_id) const noexcept{
            return (f1_id_ && f1_id_.value() == f_id) || (f2_id_ && f2_id_.value() == f_id);
        }
        
        /** @brief Return true if @p n_id is one of the nodes of the edge. */
        bool has_node(const unsigned n_id) const noexcept{
            return n1_id_ == n_id || n2_id_ == n_id;
        }

        /**
         * @brief Replace the face @p old_f_id by @p new_f_id. The hash is unchanged.
         *
         * The edge must be manifold and reference @p old_f_id (checked by assertions only).
         */
        void replace_face(const unsigned old_f_id, const unsigned new_f_id) noexcept(false){
            assert(f1_id_ && f2_id_);
            assert(old_f_id == f1_id_ || old_f_id == f2_id_);
            if(f1_id_ == old_f_id){f1_id_ = new_f_id;}
            else{f2_id_ = new_f_id;}
        }

        /** @brief Order edges by hash(), for use in std::set. */
        bool operator<(const edge& e2) const{
            return hash() < e2.hash();
        }

};







//-----------------------------------------------------------------------------------------------------
/**
 * @brief Hash functor for edges (Cantor pairing of the node ids), for unordered containers.
 * @ingroup mesh
 */
struct edge_hasher{
    int operator()(const edge& e) const{
        
        //Create a hash function for pairs of edges with the Cantor pairing function
        //The created hash will always be unique
        const auto [e_n1, e_n2] = e.get_node_ids();
        return 0.5 * (e_n1 + e_n2) * (e_n1 + e_n2 + 1) + e_n2;
    }
};
//-----------------------------------------------------------------------------------------------------


//-----------------------------------------------------------------------------------------------------
/**
 * @brief Equality functor for edges, for unordered containers.
 * @ingroup mesh
 */
struct edge_comparator{
    bool operator()(const edge& e1,const edge& e2) const{
        return e1 == e2;
    }
};
//----------------------------------------------------------------------------------------------------



#endif