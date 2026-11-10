#ifndef DEF_SIMUCELL3D_WRAPPER
#define DEF_SIMUCELL3D_WRAPPER


#include <string>
#include <cassert>
#include <vector>
#include <filesystem>
#include <omp.h>

#include <iostream>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

/**
 * @file
 * @brief Entry point of the Python module `simucell3d_python_wrapper`.
 * @ingroup core
 */


#include "custom_structures.hpp"
#include "custom_exception.hpp"
#include "solver.hpp"
#include "solver_python_wrapper.hpp"
#include "simulation_initializer.hpp"

/**
 * @brief Runs a complete simulation in its constructor; exposed to Python as `simucell3d_wrapper`.
 *
 * Built only with `-DENABLE_PYTHON_BINDINGS=TRUE`. The per-cell statistics are collected in a string instead
 * of `simulation_statistics.csv`; the output meshes are still written. The Python reference is in the
 * documentation page "Python module".
 * @ingroup core
 */
class simucell3d_wrapper{
    private:
        //Return the state of the simulation in this structure, which can be used in python
        simulation_outputs simulation_outputs_;


    public:
        simucell3d_wrapper() = default;                          //default constructor
        simucell3d_wrapper(const simucell3d_wrapper& v) = delete;            //copy constructor
        simucell3d_wrapper(simucell3d_wrapper&& v) = delete;                 //move constructor
        simucell3d_wrapper& operator=(const simucell3d_wrapper& v) = delete; //copy assignment operator
        simucell3d_wrapper& operator=(simucell3d_wrapper&& v) = default;     //move assignment operator 

        /**
         * @brief Create the cells, run the simulation and store the result.
         *
         * Exceptions are not propagated: any std::exception, including a Ctrl-C from Python, sets
         * `RETURN_CODE_` to 1 and `error_message_` in the result.
         * @param sim_parameters Numerical parameters of the simulation.
         * @param cell_type_param_lst Cell types, indexed by the `cell_type_id` of the input mesh.
         * @param verbose Print the progress of the simulation.
         * @param nb_threads Number of OpenMP threads; 0 or less uses one thread per physical core.
         */
        simucell3d_wrapper(
            const global_simulation_parameters& sim_parameters,
            const std::vector<cell_type_parameters>& cell_type_param_lst,
            const bool verbose, //If set to true, the progress of the simulation will be printed
            const int nb_threads //Number of threads to use in the simulatiions
        ) noexcept(false);

        /// Return a copy of the result: return code, error message, per-cell statistics and final cells.
        simulation_outputs get_simulation_outputs() const noexcept(false){return simulation_outputs_;}
};

#endif