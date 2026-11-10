#ifndef DEF_SIMULATION_INITIALIZER
#define DEF_SIMULATION_INITIALIZER


#include <string>
#include <iostream>
#include <cassert>
#include <vector>


#include "custom_structures.hpp"
#include "custom_exception.hpp"
#include "parameter_reader.hpp"
#include "mesh_reader.hpp"
#include "mesh_writer.hpp"
#include "initial_triangulation.hpp"


#include "cell.hpp"
#include "epithelial_cell.hpp"
#include "static_cell.hpp"
#include "nucleus_cell.hpp"
#include "lumen_cell.hpp"
#include "ecm_cell.hpp"



/**
 * @file
 * @brief Creation of the cells of a simulation from the parameters and the input mesh.
 * @ingroup io
 */




/**
 * @brief Builds the list of cells and the numerical parameters passed to the solver.
 *
 * Reads the parameter file (or takes the parameter structures directly), reads the input mesh, triangulates
 * each cell surface in parallel and creates, for each cell, an object of the class selected by the
 * `global_type_id_` of its cell type: 0 epithelial_cell, 1 ecm_cell, 2 lumen_cell, 3 nucleus_cell,
 * 4 static_cell. The `cell_type_id` of a cell in the input mesh is the position of its cell type in the
 * parameter file.
 * @ingroup io
 */
class simulation_initializer{
   private:

      //The parameters that should be passed to the simulation_solver
      global_simulation_parameters sim_parameters_; 

      //The list of cells
      std::vector<cell_ptr> cell_lst_; 

      //If set to true all the cells will get the attribute of the 
      //epithelial cell type. However, the different cells can still possess 
      //different parameters
      bool make_all_cell_epithelial_ = false;

      //If set to false, the cells will not be triangulated
      bool perform_initial_triangulation_ = true;

      bool verbose_;
      

   public:
      simulation_initializer() = delete;                               //default constructor
      simulation_initializer(const simulation_initializer& v) = delete;           //copy constructor
      simulation_initializer(simulation_initializer&& v) = delete;                //move constructor
      simulation_initializer& operator=(const simulation_initializer& v) = delete;//copy assignment operator
      simulation_initializer& operator=(simulation_initializer&& v) = delete;     //move assignment operator 
      

      /**
       * @brief Create the cells from parameter structures (used by the Python module).
       * @param sim_parameters Numerical parameters, including the input mesh path.
       * @param cell_type_param_lst Cell types, indexed by the `cell_type_id` of the input mesh.
       * @param verbose Print progress information.
       * @throws intialization_exception If a cell type has no face type, the mesh and its cell types are
       *         inconsistent, or a cell cannot be triangulated.
       * @throws mesh_reader_exception If the input mesh cannot be read.
       */
      simulation_initializer(
         const global_simulation_parameters& sim_parameters,
         const std::vector<cell_type_param_ptr>& cell_type_param_lst, 
         const bool verbose = true
      ) noexcept(false);

      /**
       * @brief Create the cells from an XML parameter file.
       * @param parameter_file_path Path of the parameter file, absolute or relative to the working directory.
       * @param verbose Print progress information.
       * @throws parameter_reader_exception If the parameter file cannot be read or is invalid.
       * @throws intialization_exception See the other constructor.
       * @throws mesh_reader_exception If the input mesh cannot be read.
       */
      simulation_initializer(
         const std::string& parameter_file_path,
         const bool verbose = true
      ) noexcept(false);

      /**
       * @brief Read the input mesh and create the cells; called by the constructors.
       * @param cell_type_param_lst Cell types, indexed by the `cell_type_id` of the input mesh.
       * @throws intialization_exception If a cell type has no face type, the numbers of cells and cell type ids
       *         in the mesh differ, a cell type id is out of range, or a cell cannot be triangulated.
       * @throws mesh_reader_exception If the input mesh cannot be read.
       */
      void run(const std::vector<cell_type_param_ptr>& cell_type_param_lst) noexcept(false); 

      /// Return the cells created from the input mesh.
      std::vector<cell_ptr> get_cell_lst() const noexcept {return cell_lst_;}

      /// Return the numerical parameters of the simulation.
      global_simulation_parameters get_simulation_parameters() const noexcept {return sim_parameters_;}

      /**
       * @brief Create one cell from its input mesh, retrying up to 10 times.
       *
       * With `perform_initial_triangulation` the surface is re-triangulated with edge lengths between the
       * minimum edge length and three times that value; otherwise the input faces must already be triangles.
       * @param cell_mesh Input mesh of the cell.
       * @param cell_id Index of the cell in the input mesh.
       * @param cell_type Cell type of the cell; its `global_type_id_` selects the cell class.
       * @return The new cell.
       * @throws intialization_exception If all 10 attempts fail, or `global_type_id_` is not in 0-4.
       */
      cell_ptr triangulate_surface(
        const mesh& cell_mesh, 
        const size_t cell_id,
        cell_type_param_ptr cell_type
      ) noexcept(false);

};

#endif