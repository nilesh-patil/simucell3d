#ifndef DEF_CONTACT_NODE_NODE_VIA_COUPLING
#define DEF_CONTACT_NODE_NODE_VIA_COUPLING

/**
 * @file contact_node_node_via_coupling.hpp
 * @brief Contact model 1: coupling of pairs of nodes of adjacent epithelial cells.
 * @ingroup contact_models
 */


#include "global_configuration.hpp"



#define _USE_MATH_DEFINES

#include "contact_model_abstract.hpp"
#include <math.h>


/**
 * @brief Contact model 1 (`CONTACT_MODEL_INDEX 1`): node-node coupling.
 *
 * Like contact_face_face_via_coupling, but each node has at most one partner in total (not one
 * per adjacent cell), the angle between the normals of the two nodes must be at least 45 degrees
 * (90 degrees in model 2), and after the search each coupled pair is moved to its midpoint. Faces are found with the uniform
 * spatial partitioning grid of the base class; the `contact_detection_algorithm` parameter has
 * no effect. Non-coupled contacts receive the same repulsion as in model 2.
 *
 * @note Repulsion forces are added without atomic operations while several threads may update
 *       the same node; the coupling itself is stored under the node lock.
 * @ingroup contact_models
 */
class contact_node_node_via_coupling: public contact_model_abstract{

    private:
        static constexpr double max_dot_product_adhesion_  = std::cos(45 * M_PI / 180.0); 
        static constexpr double max_dot_product_repulsion_ = std::cos(90 * M_PI / 180.0); 


    public:
        contact_node_node_via_coupling() = default;                                          //default constructor
        contact_node_node_via_coupling(const contact_node_node_via_coupling& v) = delete;           //copy constructor
        contact_node_node_via_coupling(contact_node_node_via_coupling&& v) = delete;                //move constructor
        contact_node_node_via_coupling& operator=(const contact_node_node_via_coupling& v) = delete;//copy assignment operator
        contact_node_node_via_coupling& operator=(contact_node_node_via_coupling&& v) = default;     //move assignment operator 

        /**
         * @brief Construct the model; see contact_model_abstract::contact_model_abstract().
         * @param sim_parameters Numerical parameters (contact cutoffs in m, minimum edge length in m).
         */
        contact_node_node_via_coupling(const global_simulation_parameters& sim_parameters) noexcept(false);

        /**
         * @brief Number the faces, reset the couplings, fill the face grid and resolve all contacts.
         * @param cell_lst All cells of the simulation.
         * @throws unstable_simulation_exception If the domain has diverged and the face grid cannot
         *         be built.
         */
        void run(const std::vector<cell_ptr>& cell_lst) noexcept(false) override;

        /**
         * @brief Visit every node/candidate-face pair, couple or repel them, then move each coupled pair
         *        to its midpoint.
         * @param cell_lst All cells of the simulation.
         */
        void resolve_all_contacts(const std::vector<cell_ptr>& cell_lst) noexcept;


        /**
         * @brief Resolve the contact between one node and one face of another cell.
         *
         * Between two epithelial cells, first try to couple the node to the closest node of the face;
         * otherwise apply repulsion as in contact_face_face_via_coupling::resolve_contact().
         *
         * @param c1 Cell that owns the node.
         * @param c2 Cell that owns the face.
         * @param n  Node of `c1`.
         * @param f  Face of `c2`.
         */
        void resolve_contact(cell_ptr c1, cell_ptr c2, node& n, face* f) const noexcept;


};
#endif
