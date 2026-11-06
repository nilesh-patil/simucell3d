#ifndef DEF_CONTACT_FACE_FACE_VIA_COUPLING 
#define DEF_CONTACT_FACE_FACE_VIA_COUPLING


#include "global_configuration.hpp"

#define _USE_MATH_DEFINES

#include "contact_model_abstract.hpp"
#include <math.h>
#include <cstdint>


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

        //The trivial constructor of the contact model
        contact_face_face_via_coupling(const global_simulation_parameters& sim_parameters) noexcept(false);
            
        void run(const std::vector<cell_ptr>& cell_lst) noexcept(false) override;

        //Find the faces that are within a distance below the contact cutoff and apply adhesive or repulsive forces 
        void resolve_all_contacts(const std::vector<cell_ptr>& cell_lst) noexcept(false);


        //Prevent the surfaces of the cells to interpenetrate by applying repulsive forces on the surfaces.
        //These forces are only applied if the 2 cells are not epithelial
        void resolve_contact(const cell_ptr& c1, const cell_ptr& c2, node& n, face* f) const noexcept;


};
#endif
