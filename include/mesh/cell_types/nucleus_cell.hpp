#ifndef DEF_NUCLEUS_CELL
#define DEF_NUCLEUS_CELL

#include <cassert>

#include "../cell.hpp"


/**
 * @file nucleus_cell.hpp
 * @brief Nucleus cell class (global_cell_id 3).
 * @ingroup cell_classes
 */



/**
 * @brief Nucleus, selected by `global_cell_id` 3.
 *
 * A nucleus is a deformable cell with the mechanics of the base class cell and no overrides
 * other than get_cell_same_type(). Nuclei do not divide.
 *
 * @ingroup cell_classes
 */
class nucleus_cell: public cell {

    private: 
        friend class cell_divider;

    protected:


    public:

        //Make sure the nucleus_cell cannot be default instantiated
        nucleus_cell() = delete;                        //default constructor
        nucleus_cell(const nucleus_cell& c) = default;           //copy constructor
        nucleus_cell(nucleus_cell&& c) = delete;                //move constructor
        nucleus_cell& operator=(const nucleus_cell& c) = delete;//copy assignment operator
        nucleus_cell& operator=(nucleus_cell&& c) = delete;     //move assignment operator 


        /** @brief Create the cell from its nodes and faces; see the corresponding cell constructor. */
        nucleus_cell(
            const std::vector<node>& node_lst,
            const std::vector<face>& face_lst,
            unsigned cell_id,
            cell_type_param_ptr cell_type_parameters
        ) noexcept : cell(node_lst, face_lst, cell_id, cell_type_parameters){}


        /** @brief Create the cell from flat node coordinates (m) and face node ids; see the corresponding cell constructor. */
        nucleus_cell(
            const std::vector<double>&  node_position,
            const std::vector<unsigned>& face_node_ids,
            unsigned cell_id,
            cell_type_param_ptr cell_type_parameters
        ) noexcept : cell(node_position, face_node_ids, cell_id, cell_type_parameters){}


        /** @brief Create the cell from a mesh; see the corresponding cell constructor. */
        nucleus_cell(
            const mesh& m,
            unsigned cell_id,
            cell_type_param_ptr cell_type_parameters
        ) noexcept : cell(m, cell_id, cell_type_parameters){}

        /** @brief Return a new nucleus_cell built from @p m, with the same id and cell type. */
        cell_ptr get_cell_same_type(const mesh& m) noexcept(false) override{
            //Return a copy of the cell
            return std::make_shared<nucleus_cell>(m, cell_id_, cell_type_);
        }


};

#endif