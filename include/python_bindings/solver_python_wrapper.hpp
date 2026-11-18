#ifndef DEF_SOLVER_PYTHON_WRAPPER
#define DEF_SOLVER_PYTHON_WRAPPER


#include <string>
#include <cassert>
#include <vector>
#include <filesystem>
#include <omp.h>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "solver.hpp"

#include "custom_structures.hpp"
#include "custom_exception.hpp"
#include "local_mesh_refiner.hpp"
#include "time_integration.hpp"
#include "mesh_writer.hpp"
#include "cell_divider.hpp"


/**
 * @file
 * @brief Solver used by the Python module, interruptible with Ctrl-C.
 * @ingroup core
 */

//--------------------------------------------------------------------------------------------------------------------------
/**
 * @brief Solver used by simucell3d_wrapper: checks for a Python interrupt (Ctrl-C) before every iteration.
 * @ingroup core
 */
class solver_python_wrapper: public solver{

    public:

        solver_python_wrapper() = default;                                          //default constructor
        solver_python_wrapper(const solver_python_wrapper& v) = delete;            //copy constructor
        solver_python_wrapper(solver_python_wrapper&& v) = delete;                 //move constructor
        solver_python_wrapper& operator=(const solver_python_wrapper& v) = delete; //copy assignment operator
        solver_python_wrapper& operator=(solver_python_wrapper&& v) = default;     //move assignment operator 
      
        /// Construct the base solver with the default scheduling mode (`adaptive`) and diagnostics path.
        solver_python_wrapper(
            const global_simulation_parameters& sim_parameters, 
            const std::vector<cell_ptr>& cell_lst,
            int nb_threads = -1, //If set to -1, SimuCell3D will use the available cores
            bool write_cell_stats_in_string_ = false,  //If true, the program will write the cell statistics in a string instead of a file   
            bool verbose = true //If true, the program will print some information about the simulation progress
        )  noexcept(false) : solver(sim_parameters, cell_lst, nb_threads, write_cell_stats_in_string_, verbose){};

        /**
         * @brief Run one iteration unless Python has a pending signal.
         * @throws pybind11::error_already_set If the user pressed Ctrl-C.
         */
        void run_iteration() noexcept(false) override{

            //Check if the user has pressed ctrl-c
            if(PyErr_CheckSignals() != 0) throw pybind11::error_already_set();

            //Run the base class run_iteration method
            solver::run_iteration();
        };
};
//--------------------------------------------------------------------------------------------------------------------------


#endif