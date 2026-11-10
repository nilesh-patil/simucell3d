#ifndef DEF_CONTACT_NODE_FACE_VIA_SPRING
#define DEF_CONTACT_NODE_FACE_VIA_SPRING

/**
 * @file contact_node_face_via_spring.hpp
 * @brief Contact model 0: springs between nodes and faces of adjacent cells.
 * @note The class is compiled only when `CONTACT_MODEL_INDEX` is 0.
 * @ingroup contact_models
 */


#include "global_configuration.hpp"


#if CONTACT_MODEL_INDEX == 0

     #include "contact_model_abstract.hpp"


     /**
      * @brief Contact model 0 (`CONTACT_MODEL_INDEX 0`): node-face springs.
      *
      * The padded AABBs of all faces are stored in the uniform spatial partitioning grid of the
      * base class. For every node, each face of another cell whose AABB contains the node is
      * tested: the distance d between the node and the closest point of the face decides the force,
      * which acts along the line between the two points and is shared with the face nodes by
      * barycentric weights. With A the face area, c the adhesion cutoff, omega the
      * `adherence_strength` and xi the `repulsion_strength` of the face type (N/m^3):
      *
      * - node on the outer side of the face and d < c: adhesion of magnitude omega A d for
      *   d < c/2, and omega A (c - d) for d >= c/2;
      * - node on the inner side and d below the repulsion cutoff: repulsion of magnitude xi A d.
      *
      * The side is inverted for an epithelial node against an ECM face and for a nucleus node
      * against an epithelial face. With `POLARIZATION_MODE_INDEX 1`, each face in contact is
      * reported to its cell for polarization. The `contact_detection_algorithm` parameter has no
      * effect on this model.
      *
      * @note Forces are added without atomic operations while several threads may update the
      *       same node.
      * @ingroup contact_models
      */


     class contact_node_face_via_spring: public contact_model_abstract{

     private:
          double interaction_cutoff_, interaction_cutoff_square_; 

          //The distance between 2 surfaces where the adhesion force is maximal, by default it's equal to interaction_cutoff_ / 2
          double hardening_distance_;

     public:
          contact_node_face_via_spring() = default;                                          //default constructor
          contact_node_face_via_spring(const contact_node_face_via_spring& v) = delete;           //copy constructor
          contact_node_face_via_spring(contact_node_face_via_spring&& v) = delete;                //move constructor
          contact_node_face_via_spring& operator=(const contact_node_face_via_spring& v) = delete;//copy assignment operator
          contact_node_face_via_spring& operator=(contact_node_face_via_spring&& v) = default;     //move assignment operator 

          /**
           * @brief Construct the model; see contact_model_abstract::contact_model_abstract().
           * @param sim_parameters Numerical parameters (contact cutoffs in m, minimum edge length in m).
           */
          contact_node_face_via_spring(const global_simulation_parameters& sim_parameters) noexcept(false);

          /**
           * @brief Number the faces, fill the face grid and apply the contact forces.
           * @param cell_lst All cells of the simulation.
           * @throws unstable_simulation_exception If the domain has diverged and the face grid cannot
           *         be built.
           */
          void run(const std::vector<cell_ptr>& cell_lst) noexcept(false) override;

          /**
           * @brief For every node, apply the contact forces with the faces of other cells whose AABB
           *        contains it.
           * @param cell_lst All cells of the simulation.
           */
          void resolve_contacts(const std::vector<cell_ptr>& cell_lst) noexcept;

          /**
           * @brief Compute the node-face distance and apply the adhesion or repulsion force.
           * @param c1 Cell that owns the node.
           * @param n  Node of `c1`.
           * @param f  Face of another cell.
           */
          void apply_contact_forces(cell_ptr c1, node& n, face* f) const noexcept;

     };

     #endif

#endif