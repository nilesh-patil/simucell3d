#ifndef DEF_ECM_CELL
#define DEF_ECM_CELL

#include <cassert>

#include "../cell.hpp"


/**
 * @file ecm_cell.hpp
 * @brief ECM cell class (global_cell_id 1).
 * @ingroup cell_classes
 */



/**
 * @brief Extracellular matrix (ECM) cell, selected by `global_cell_id` 1.
 *
 * An ECM cell is static: it does not move (the time integration skips it) and its internal
 * forces are not computed (apply_internal_forces() does nothing). It still takes part in
 * contact detection. With contact model 0 and contact-based polarization
 * (POLARIZATION_MODE_INDEX 1), epithelial faces in contact with an ECM cell become basal.
 * ECM cells do not divide.
 *
 * @ingroup cell_classes
 */
class ecm_cell: public cell {

    private: 
        friend class cell_divider;

    protected:


    public:

        //Make sure the ecm_cell cannot be default instantiated
        ecm_cell() = delete;                        //default constructor
        ecm_cell(const ecm_cell& c) = default;           //copy constructor
        ecm_cell(ecm_cell&& c) = delete;                //move constructor
        ecm_cell& operator=(const ecm_cell& c) = delete;//copy assignment operator
        ecm_cell& operator=(ecm_cell&& c) = delete;     //move assignment operator 


        
        /** @brief Create the cell from its nodes and faces; see the corresponding cell constructor. */
        ecm_cell(
            const std::vector<node>& node_lst,
            const std::vector<face>& face_lst,
            unsigned cell_id,
            cell_type_param_ptr cell_type_parameters
        ) noexcept : cell(node_lst, face_lst, cell_id, cell_type_parameters){
            
            //This cell should not move
            is_static_ = true;
        }


        /** @brief Create the cell from flat node coordinates (m) and face node ids; see the corresponding cell constructor. */
        ecm_cell(
            const std::vector<double>&  node_position,
            const std::vector<unsigned>& face_node_ids,
            unsigned cell_id,
            cell_type_param_ptr cell_type_parameters
        ) noexcept : cell(node_position, face_node_ids, cell_id, cell_type_parameters){

            //This cell should not move
            is_static_ = true;
        }


        /** @brief Create the cell from a mesh; see the corresponding cell constructor. */
        ecm_cell(
            const mesh& m,
            unsigned cell_id,
            cell_type_param_ptr cell_type_parameters
        ) noexcept : cell(m, cell_id, cell_type_parameters){

            //This cell should not move
            is_static_ = true;
        }


        /** @brief Do nothing: ECM cells are static, so no internal forces are computed. */
        void apply_internal_forces(const double time_step) noexcept override{
            //Only if the cell is not static we apply the internal forces such as the cell pressure, sureface tension etc
            if(!is_static_){cell::apply_internal_forces(time_step);}
        }


        /** @brief Return a new ecm_cell built from @p m, with the same id and cell type. */
        cell_ptr get_cell_same_type(const mesh& m) noexcept(false) override{
            //Return a copy of the cell
            return std::make_shared<ecm_cell>(m, cell_id_, cell_type_);
        }

};

#endif