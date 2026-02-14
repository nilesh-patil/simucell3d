#include "node.hpp"


//---------------------------------------------------------------------------------------------------------
// Default constructor
//---------------------------------------------------------------------------------------------------------
node::node() noexcept : node_id_(0), is_used_(true) {
    // std::mutex auto-initializes - no explicit initialization needed
}

//---------------------------------------------------------------------------------------------------------
// Custom copy constructor - CRITICAL: Initialize NEW lock, do NOT copy the old one
// Copying omp_lock_t is undefined behavior (it contains pthread_mutex_t internal state)
//---------------------------------------------------------------------------------------------------------
node::node(const node& n) noexcept
    : node_id_(n.node_id_)
    , is_used_(n.is_used_)
    , pos_(n.pos_)
    , force_(n.force_)
#if DYNAMIC_MODEL_INDEX == 0
    , momentum_(n.momentum_)
#endif
{
    #if CONTACT_MODEL_INDEX == 1 || CONTACT_MODEL_INDEX == 2
        // std::mutex auto-initializes - each node gets its own fresh mutex
        normal_ = n.normal_;
        curvature_ = n.curvature_;
    #endif

    #if CONTACT_MODEL_INDEX == 1
        coupled_node_ = n.coupled_node_;
        squared_distance_to_closest_node_ = n.squared_distance_to_closest_node_;
    #elif CONTACT_MODEL_INDEX == 2
        coupled_nodes_map_ = n.coupled_nodes_map_;
    #endif
}

//---------------------------------------------------------------------------------------------------------
// Custom move constructor - Initialize NEW lock, move other members
//---------------------------------------------------------------------------------------------------------
node::node(node&& n) noexcept
    : node_id_(n.node_id_)
    , is_used_(n.is_used_)
    , pos_(std::move(n.pos_))
    , force_(std::move(n.force_))
#if DYNAMIC_MODEL_INDEX == 0
    , momentum_(std::move(n.momentum_))
#endif
{
    #if CONTACT_MODEL_INDEX == 1 || CONTACT_MODEL_INDEX == 2
        // std::mutex auto-initializes - each node gets its own fresh mutex
        normal_ = std::move(n.normal_);
        curvature_ = n.curvature_;
    #endif

    #if CONTACT_MODEL_INDEX == 1
        coupled_node_ = std::move(n.coupled_node_);
        squared_distance_to_closest_node_ = n.squared_distance_to_closest_node_;
    #elif CONTACT_MODEL_INDEX == 2
        coupled_nodes_map_ = std::move(n.coupled_nodes_map_);
    #endif
}

//---------------------------------------------------------------------------------------------------------
// Custom copy assignment operator
//---------------------------------------------------------------------------------------------------------
node& node::operator=(const node& n) noexcept {
    if (this != &n) {
        node_id_ = n.node_id_;
        is_used_ = n.is_used_;
        pos_ = n.pos_;
        force_ = n.force_;

        #if DYNAMIC_MODEL_INDEX == 0
            momentum_ = n.momentum_;
        #endif

        #if CONTACT_MODEL_INDEX == 1 || CONTACT_MODEL_INDEX == 2
            // Keep our own lock, just copy other members
            // lock_ is NOT copied - each node keeps its own initialized lock
            normal_ = n.normal_;
            curvature_ = n.curvature_;
        #endif

        #if CONTACT_MODEL_INDEX == 1
            coupled_node_ = n.coupled_node_;
            squared_distance_to_closest_node_ = n.squared_distance_to_closest_node_;
        #elif CONTACT_MODEL_INDEX == 2
            coupled_nodes_map_ = n.coupled_nodes_map_;
        #endif
    }
    return *this;
}

//---------------------------------------------------------------------------------------------------------
// Custom move assignment operator
//---------------------------------------------------------------------------------------------------------
node& node::operator=(node&& n) noexcept {
    if (this != &n) {
        node_id_ = n.node_id_;
        is_used_ = n.is_used_;
        pos_ = std::move(n.pos_);
        force_ = std::move(n.force_);

        #if DYNAMIC_MODEL_INDEX == 0
            momentum_ = std::move(n.momentum_);
        #endif

        #if CONTACT_MODEL_INDEX == 1 || CONTACT_MODEL_INDEX == 2
            // Keep our own lock, just move other members
            // lock_ is NOT moved - each node keeps its own initialized lock
            normal_ = std::move(n.normal_);
            curvature_ = n.curvature_;
        #endif

        #if CONTACT_MODEL_INDEX == 1
            coupled_node_ = std::move(n.coupled_node_);
            squared_distance_to_closest_node_ = n.squared_distance_to_closest_node_;
        #elif CONTACT_MODEL_INDEX == 2
            coupled_nodes_map_ = std::move(n.coupled_nodes_map_);
        #endif
    }
    return *this;
}

//---------------------------------------------------------------------------------------------------------
// Constructor with node_id only
//---------------------------------------------------------------------------------------------------------
node::node(const unsigned node_id) noexcept : node_id_(node_id), is_used_(true) {
    // std::mutex auto-initializes - no explicit initialization needed
}

//Trivial constructor
//---------------------------------------------------------------------------------------------------------
node::node(const double dx, const double dy, const double dz, const unsigned node_id) noexcept :node_id_(node_id){

    //Check the coordinates of the node
    assert(std::isfinite(dx) && std::isfinite(dy) && std::isfinite(dz));

    //Set the position of the node
    pos_.translate(dx, dy, dz);


    //Overdamped equations of motion solved with the improved euler scheme
    #if DYNAMIC_MODEL_INDEX == 2

        //The previous position is the starting position
        previous_pos_.translate(dx, dy, dz);
    #endif 

    // std::mutex auto-initializes - no explicit initialization needed
}

node::node(const vec3& pos, const unsigned node_id) noexcept{
    node_id_ = node_id;
    pos_ = pos;

    //Overdamped equations of motion solved with the improved euler scheme
    #if DYNAMIC_MODEL_INDEX == 2
     
        //The previous position is the starting position
        previous_pos_ = pos;
    #endif 

    // std::mutex auto-initializes - no explicit initialization needed
}

//---------------------------------------------------------------------------------------------------------
//Reset the vectors owned by the node
void node::reset() noexcept {
    assert(is_used_);  // BUG-003 fix: was `= true` (assignment), now proper check

    pos_.reset();
    force_.reset();


    #if CONTACT_MODEL_INDEX == 1
        //Remove the coupling to the face
        coupled_node_.reset();

    #endif



    #if CONTACT_MODEL_INDEX == 2
        //Remove the coupling to the face
        coupled_nodes_map_.clear();

    #endif

    //Overdamped equations of motion solved with the improved euler scheme
    #if DYNAMIC_MODEL_INDEX == 0
        momentum_.reset();
    
    #elif DYNAMIC_MODEL_INDEX == 2
    
        previous_pos_.reset();
        previous_force_.reset();
    #endif 

    //Indicate that the node is not used by the cell anymore
    is_used_ = false;
}
//---------------------------------------------------------------------------------------------------------



//---------------------------------------------------------------------------------------------------------
//Check if the 2 nodes have the same id
bool node::operator==(const node& n) const noexcept {return node_id_ == n.get_local_id();}

bool node::operator!=(const node& n) const noexcept{return node_id_ != n.get_local_id();}
//---------------------------------------------------------------------------------------------------------

//The substraction of 2 nodes returns a vector
vec3 node::operator-(const node& n) const noexcept{return pos_ - n.pos();}

