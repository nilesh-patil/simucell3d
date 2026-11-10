#ifndef DEF_CONTACT_FACE_FACE_VIA_COUPLING 
#define DEF_CONTACT_FACE_FACE_VIA_COUPLING

/**
 * @file contact_face_face_via_coupling.hpp
 * @brief Contact model 2: coupling of the surfaces of adjacent epithelial cells.
 * @ingroup contact_models
 */


#include "global_configuration.hpp"

#define _USE_MATH_DEFINES

#include "contact_model_abstract.hpp"
#include <math.h>
#include <cstdint>


/**
 * @brief Contact model 2 (`CONTACT_MODEL_INDEX 2`, default): face-face coupling.
 *
 * For each node of a cell, candidate faces of other cells are those whose padded AABB contains
 * the node. Between two epithelial cells (cell class 0), the node is coupled to the closest
 * node of the candidate face if their distance is below the adhesion cutoff, the angle between
 * the two node normals is at least 90 degrees, the curvature of both nodes is below `surface_coupling_max_curvature`, and
 * neither node already has a closer partner on that cell. A node keeps at most one partner per
 * adjacent cell. After the search, every group of coupled nodes is moved to its mean position.
 * In all other cases (other cell classes, or no coupling), a node that has penetrated the face
 * receives a repulsion force of magnitude `repulsion_strength` x face area x distance, shared
 * with the face nodes by barycentric weights. Nodes whose curvature is at or above
 * `surface_coupling_max_curvature` are skipped.
 *
 * Candidate faces come from an internal grid binned by face AABB, or from the sweep-and-prune
 * strategy when `contact_detection_algorithm` selects it.
 *
 * @note Thread-safe: contact forces are added with atomic updates and the coupling maps of the
 *       nodes are written under the node locks (node::couple_if_closest()).
 * @ingroup contact_models
 */
class contact_face_face_via_coupling: public contact_model_abstract{

    private:
        static constexpr double max_dot_product_adhesion_  = std::cos(90 * M_PI / 180.0); 
        static constexpr double max_dot_product_repulsion_ = std::cos(90 * M_PI / 180.0); 


        //---------------------------------------------------------------------------------------------
        //Compact broad phase (replaces uspg_4d<face*> for this model). Faces are binned by their padded
        //AABB into a CSR grid (one offsets array + one flat array of face ids, ascending within a voxel,
        //i.e. the same order as the push_back order of the uspg). The voxel edge follows the mean face
        //AABB extent instead of the fixed 3*l_min + 2*c, so the number of faces scanned per node stays
        //bounded when the face density grows. The node/face pairs that pass the AABB test, and the
        //order in which they are visited per node, are identical to the uspg path.
        std::vector<uint32_t> csr_start_;       //nb_voxels + 1 offsets into csr_faces_
        std::vector<uint32_t> csr_faces_;       //face global ids
        std::vector<uint32_t> csr_cursor_;      //scratch: per-voxel fill cursor
        std::vector<uint32_t> face_voxel_box_;  //scratch: 6 voxel bounds per face
        std::vector<unsigned> face_owner_gid_;  //global id of the cell owning each face (avoids a shared_ptr copy per candidate)
        double csr_voxel_size_ = 0.;
        double csr_min_x_ = 0., csr_min_y_ = 0., csr_min_z_ = 0.;
        size_t csr_nx_ = 1, csr_ny_ = 1, csr_nz_ = 1;

        //Load balancing: (cell index, first node, last node) work units, heavy cells split by node ranges
        struct work_unit { uint32_t cell_id; uint32_t node_begin; uint32_t node_end; };
        std::vector<work_unit> work_units_;
        std::vector<double> cell_cost_;

        void build_face_grid() noexcept(false);
        void build_work_units(const std::vector<cell_ptr>& cell_lst) noexcept;
        size_t node_voxel(const vec3& p) const noexcept;
        void resolve_grid_contacts(const std::vector<cell_ptr>& cell_lst) noexcept;

    public:
        contact_face_face_via_coupling() = default;                                          //default constructor
        contact_face_face_via_coupling(const contact_face_face_via_coupling& v) = delete;           //copy constructor
        contact_face_face_via_coupling(contact_face_face_via_coupling&& v) = delete;                //move constructor
        contact_face_face_via_coupling& operator=(const contact_face_face_via_coupling& v) = delete;//copy assignment operator
        contact_face_face_via_coupling& operator=(contact_face_face_via_coupling&& v) = default;     //move assignment operator 

        /**
         * @brief Construct the model; see contact_model_abstract::contact_model_abstract().
         * @param sim_parameters Numerical parameters (contact cutoffs in m, minimum edge length in m,
         *        contact detection algorithm).
         */
        contact_face_face_via_coupling(const global_simulation_parameters& sim_parameters) noexcept(false);

        /**
         * @brief Number the faces, reset the couplings, build the broad phase and resolve all contacts.
         * @param cell_lst All cells of the simulation.
         * @throws unstable_simulation_exception If the bounding box of the domain is not finite or
         *         the contact grid would be too large (a cell has diverged).
         */
        void run(const std::vector<cell_ptr>& cell_lst) noexcept(false) override;

        /**
         * @brief Visit every node/candidate-face pair, couple or repel them, then move each group of
         *        coupled nodes to its mean position.
         *
         * In adaptive mode, this is where the detection strategy is switched when the number of cells
         * crosses the threshold.
         *
         * @param cell_lst All cells of the simulation.
         */
        void resolve_all_contacts(const std::vector<cell_ptr>& cell_lst) noexcept(false);


        /**
         * @brief Resolve the contact between one node and one face of another cell.
         *
         * Between two epithelial cells, first try to couple the node to the closest node of the face;
         * if no coupling is stored, apply repulsion when the node lies on the inner side of the face
         * (the side is inverted for an epithelial node against an ECM face and for a nucleus node
         * against an epithelial face) and within the larger contact cutoff.
         *
         * @param c1 Cell that owns the node.
         * @param c2 Cell that owns the face.
         * @param n  Node of `c1`.
         * @param f  Face of `c2`.
         */
        void resolve_contact(const cell_ptr& c1, const cell_ptr& c2, node& n, face* f) const noexcept;


};
#endif
