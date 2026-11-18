#ifndef DEF_TIME_INTEGRATION
#define DEF_TIME_INTEGRATION


#include <cassert>

#include "global_configuration.hpp"

#include "node.hpp"
#include "cell.hpp"


/**
 * @file
 * @brief Integration of the equations of motion of the nodes.
 * @ingroup time_integration
 */


//------------------------------------------------------------------------------------------------------------------------
/**
 * @brief Moves the nodes of all cells from the forces accumulated on them, one time step at a time.
 *
 * The equations are selected at compile time by DYNAMIC_MODEL_INDEX (global_configuration.hpp):
 * - 0: full equations of motion, semi-implicit Euler scheme: the momentum p of a node of mass m is updated
 *   first, p += (F - (zeta/m) p) dt, then the position, x += (p/m) dt;
 * - 1: overdamped equations of motion, forward Euler scheme: x += (F/zeta) dt.
 *
 * Here zeta is the damping coefficient (kg/s) and dt the time step (s). Static cells are not moved. With
 * CONTACT_MODEL_INDEX 1 or 2, nodes coupled across a cell-cell contact are moved together with their averaged
 * force (and momentum). The forces of the nodes are reset after the update.
 * @ingroup time_integration
 */
class time_integration_scheme{

   protected:
      double dt_; //Time step
      double damping_coeff_; //Damping coefficient
      double simulation_time_ = 0.0; //Simulation time
      bool verbose_;

      //True if the time step is a temporary (intermediate) one. No available scheme uses temporary steps, so it stays false
      bool tmp_step_ = false; 

   public:
      time_integration_scheme() = default;                                           //default constructor
      time_integration_scheme(const time_integration_scheme& v) = delete;            //copy constructor
      time_integration_scheme(time_integration_scheme&& v) = delete;                 //move constructor
      time_integration_scheme& operator=(const time_integration_scheme& v) = delete; //copy assignment operator
      time_integration_scheme& operator=(time_integration_scheme&& v) = default;     //move assignment operator

      /**
       * @brief Create the scheme from the simulation parameters.
       * @param sim_parameters Provides the time step (s) and the damping coefficient (kg/s).
       * @param verbose Print progress information.
       */
      time_integration_scheme(const global_simulation_parameters& sim_parameters, const bool verbose) noexcept: 
         dt_(sim_parameters.time_step_), 
         damping_coeff_(sim_parameters.damping_coefficient_),
         verbose_(verbose)
      {};

      /// Return the simulated time, s.
      double get_simulation_time() const noexcept {return simulation_time_;}

      /// Return whether the current step is a temporary (intermediate) step. Always false with the two
      /// available dynamic models; the solver skips output and division on temporary steps.
      bool is_step_tmp() const noexcept {return tmp_step_;}

      /**
       * @brief Move the nodes of all cells by one time step and advance the simulated time.
       *
       * Also recomputes the kinetic energy of each cell and resets the node forces.
       * @param cell_lst Cells of the simulation; their positions in the list must equal their local ids.
       */
      void update_nodes_positions(const std::vector<cell_ptr>& cell_lst);



};
//------------------------------------------------------------------------------------------------------------------------





#endif