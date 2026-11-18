#ifndef DEF_LUMEN_CELL
#define DEF_LUMEN_CELL

#include <cassert>

#include "../cell.hpp"


/**
 * @file lumen_cell.hpp
 * @brief Lumen cell class (global_cell_id 2).
 * @ingroup cell_classes
 */



/**
 * @brief Lumen cell, selected by `global_cell_id` 2.
 *
 * A lumen is a deformable cell with the mechanics of the base class cell and no overrides
 * other than get_cell_same_type(). With contact model 0 and contact-based polarization
 * (POLARIZATION_MODE_INDEX 1), epithelial faces in contact with a lumen become apical.
 * Lumen cells do not divide.
 *
 * @ingroup cell_classes
 */
class lumen_cell: public cell {

    private: 
        friend class cell_divider;

    protected:


    public:

        //Make sure the lumen_cell cannot be default instantiated
        lumen_cell() = delete;                        //default constructor
        lumen_cell(const lumen_cell& c) = default;           //copy constructor
        lumen_cell(lumen_cell&& c) = delete;                //move constructor
        lumen_cell& operator=(const lumen_cell& c) = delete;//copy assignment operator
        lumen_cell& operator=(lumen_cell&& c) = delete;     //move assignment operator 


        /** @brief Create the cell from its nodes and faces; see the corresponding cell constructor. */
        lumen_cell(
            const std::vector<node>& node_lst,
            const std::vector<face>& face_lst,
            unsigned cell_id,
            cell_type_param_ptr cell_type_parameters
        ) noexcept : cell(node_lst, face_lst, cell_id, cell_type_parameters){}


        /** @brief Create the cell from flat node coordinates (m) and face node ids; see the corresponding cell constructor. */
        lumen_cell(
            const std::vector<double>&  node_position,
            const std::vector<unsigned>& face_node_ids,
            unsigned cell_id,
            cell_type_param_ptr cell_type_parameters
        ) noexcept : cell(node_position, face_node_ids, cell_id, cell_type_parameters){}


        /** @brief Create the cell from a mesh; see the corresponding cell constructor. */
        lumen_cell(
            const mesh& m,
            unsigned cell_id,
            cell_type_param_ptr cell_type_parameters
        ) noexcept : cell(m, cell_id, cell_type_parameters){}


        /** @brief Return a new lumen_cell built from @p m, with the same id and cell type. */
        cell_ptr get_cell_same_type(const mesh& m) noexcept(false) override{
            //Return a copy of the cell
            return std::make_shared<lumen_cell>(m, cell_id_, cell_type_);
        }
};

#endif