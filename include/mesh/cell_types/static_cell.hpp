#ifndef DEF_STATIC_CELL
#define DEF_STATIC_CELL

#include <cassert>

#include "../cell.hpp"


/**
 * @file static_cell.hpp
 * @brief Static cell class (global_cell_id 4).
 * @ingroup cell_classes
 */


/**
 * @brief Static cell, selected by `global_cell_id` 4.
 *
 * A static cell does not move: the time integration skips it. Unlike ecm_cell, its internal
 * forces are still computed, and it takes part in contact detection. Static cells do not
 * divide and are not checked for inside-out or self-intersecting surfaces by
 * cell::check_numerical_stability().
 *
 * @ingroup cell_classes
 */
class static_cell: public cell {

    private: 
        friend class cell_divider;

    protected:


    public:

        //Make sure the static_cell cannot be default instantiated
        static_cell() = delete;                        //default constructor
        static_cell(const static_cell& c) = default;           //copy constructor
        static_cell(static_cell&& c) = delete;                //move constructor
        static_cell& operator=(const static_cell& c) = delete;//copy assignment operator
        static_cell& operator=(static_cell&& c) = delete;     //move assignment operator 


        
        /** @brief Create the cell from its nodes and faces; see the corresponding cell constructor. */
        static_cell(
            const std::vector<node>& node_lst,
            const std::vector<face>& face_lst,
            unsigned cell_id,
            cell_type_param_ptr cell_type_parameters
        ) noexcept : cell(node_lst, face_lst, cell_id, cell_type_parameters){

            //This cell should not move
            is_static_ = true;
        }

        /** @brief Create the cell from flat node coordinates (m) and face node ids; see the corresponding cell constructor. */
        static_cell(
            const std::vector<double>&  node_position,
            const std::vector<unsigned>& face_node_ids,
            unsigned cell_id,
            cell_type_param_ptr cell_type_parameters
        ) noexcept : cell(node_position, face_node_ids, cell_id, cell_type_parameters){
                
                //This cell should not move
                is_static_ = true;
        }


        /** @brief Create the cell from a mesh; see the corresponding cell constructor. */
        static_cell(
            const mesh& m,
            unsigned cell_id,
            cell_type_param_ptr cell_type_parameters
        ) noexcept : cell(m, cell_id, cell_type_parameters){
                
                //This cell should not move
                is_static_ = true;
        }


        /** @brief Return a new static_cell built from @p m, with the same id and cell type. */
        cell_ptr get_cell_same_type(const mesh& m) noexcept(false) override{
            //Return a copy of the cell
            return std::make_shared<static_cell>(m, cell_id_, cell_type_);
        }
};

#endif