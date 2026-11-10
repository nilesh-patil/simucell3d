#ifndef DEF_SOLVER
#define DEF_SOLVER


#include <string>
#include <cassert>
#include <vector>
#include <filesystem>
#include <omp.h>

/**
 * @file
 * @brief The solver: owns the cells and runs the main loop of a simulation.
 * @ingroup core
 */


#include "custom_structures.hpp"
#include "custom_exception.hpp"
#include "local_mesh_refiner.hpp"
#include "time_integration.hpp"
#include "mesh_writer.hpp"
#include "cell_divider.hpp"

#include "automatic_polarizer.hpp"
#include "automatic_polarization_writer.hpp"

#include "contact_model_abstract.hpp"
#include "statistics_writer.hpp"
#include "cell.hpp"
#include "performance_monitor.hpp"


//Load the correct contact model depending on the value of the macro CONTACT_MODEL_INDEX
#include "contact_node_node_via_coupling.hpp"
#include "contact_node_face_via_spring.hpp"
#include "contact_face_face_via_coupling.hpp"


//--------------------------------------------------------------------------------------------------------------------------
/**
 * @brief Runs a simulation: owns the cells and the modules that update them at every iteration.
 *
 * The constructor sets up the thread count and placement, the OpenMP scheduling, the output folder and the
 * modules (local mesh refiner, contact model, time integration scheme, statistics writer). run() then calls
 * run_iteration() until the simulation duration is reached or no cell is left. One iteration writes the
 * output meshes when a new sampling period starts, divides cells (every 5 iterations), updates the face
 * types, refines the meshes, computes the contact forces, updates the polarization, computes the internal
 * forces of each cell (which also advances its target volume), moves the nodes, writes the statistics every
 * 50 iterations and the diagnostics every 100 iterations, and removes dead or numerically unstable cells.
 *
 * The contact model, dynamic model and polarization mode are fixed at compile time
 * (global_configuration.hpp).
 * @ingroup core
 */
class solver{
    
    protected:
        //The parameters used to run the simulation, they come from the input file
        //xml parameter file.
        global_simulation_parameters sim_parameters_;

        // Phase tracking for adaptive scheduling
        enum class SimulationPhase { INITIALIZATION, GROWTH, HOMEOSTASIS };

        // Interval, in iterations, between two updates of the workload coefficient of variation (CoV) and of
        // the scheduling; also the statistics write interval
        static constexpr unsigned COV_UPDATE_INTERVAL = 50;

        //Helper methods for the adaptive scheduling mode
        void update_workload_heterogeneity();  // Recompute CoV and update chunk size if needed
        void adaptive_schedule_update();  // Phase-aware scheduling mode adaptation
        SimulationPhase detect_simulation_phase();  // Classify current simulation phase
        void handle_division_event(unsigned num_divisions);  // Respond to cell division events

        //Helper methods for per-loop custom scheduling
        void initialize_per_loop_schedules();  // Set optimal schedules for each loop type
        void set_schedule_for_contact_detection();  // Switch to contact-optimized schedule
        void set_schedule_for_time_integration();   // Switch to integration-optimized schedule
        void set_schedule_for_mesh_updates();       // Switch to uniform-work schedule
        void set_schedule_for_cell_division();      // Switch to division-optimized schedule
        void restore_global_schedule();             // Restore global schedule_mode_

        //Append one row of biological and computational metrics to the diagnostics CSV
        void export_diagnostics(unsigned iteration, const std::string& csv_path);  // Export biological + computational metrics

        //The number of mesh refinement failures that one refine_meshes() call may report before the run is
        //stopped instead of removing the cells: max(10, 1% of the nb_cells cells, rounded down)
        static size_t refinement_failure_limit(const size_t nb_cells) noexcept;

        //The local mesh refiner is used to make sure that all the edges of the cell meshes have lengths
        std::unique_ptr<local_mesh_refiner> lmr_ptr_;

        //There are two types of statistic writers: one that writes the statistics in a file and one that writes in a string
        std::unique_ptr<abstract_statistics_writer> statistic_writer_ptr_;

        //There are also 2 types of contact models: one for face-face contacts and one for node-face contacts
        std::unique_ptr<contact_model_abstract> contact_model_ptr_;

        //There are also 2 types of time integrators: one for forward euler and one for semi-implicit euler
        std::unique_ptr<time_integration_scheme> time_integrator_ptr_;

        #if POLARIZATION_MODE_INDEX == 2
            std::unique_ptr<automatic_polarizer> cell_surface_polarizer_ptr_;
        #endif

        //The list of cells that will be simulated. Their geometries come from the
        //input mesh file.
        std::vector<cell_ptr> cell_lst_;

        //The number of iterations of the main loop
        unsigned int iteration_ = 0;
        unsigned int file_number_ = 0;

        //Use this number to assign a unique id to each cell
        unsigned max_cell_id_ = 0;

        //The time of the simulation
        double simulation_time_ = 0.;

        //Number of threads used to run the computation in parallel
        unsigned short int nb_threads_;

        //If true, the program will print some information about the simulation progress
        bool verbose_;

        //OpenMP scheduling mode: "static" or "dynamic"
        std::string schedule_mode_;

        //OpenMP schedule kind for runtime use (computed from schedule_mode_)
        omp_sched_t schedule_kind_;
        int schedule_chunk_size_;

        //Per-loop custom scheduling (enabled by the adaptive scheduling mode)
        bool enable_per_loop_scheduling_;  // If true, use specialized schedules for each loop type

        // Per-loop schedule settings (used when enable_per_loop_scheduling_ = true)
        struct PerLoopSchedule {
            omp_sched_t kind;
            int chunk_size;
        };
        PerLoopSchedule contact_detection_schedule_;   // Contact model loops (heterogeneous)
        PerLoopSchedule time_integration_schedule_;    // Time integration loops (moderate)
        PerLoopSchedule mesh_update_schedule_;         // Face updates, etc. (uniform)
        PerLoopSchedule cell_division_schedule_;       // Division checks (sparse, heterogeneous)

        //Performance monitoring for diagnostic and optimization
        PerformanceMonitor perf_monitor_;

        //Workload heterogeneity tracking for the adaptive scheduling mode
        double heterogeneity_cov_;  // Current coefficient of variation for workload
        unsigned int last_cov_update_iteration_;  // Last iteration when CoV was recomputed

        //Phase state tracking
        SimulationPhase current_phase_;
        unsigned int last_division_check_iteration_;
        unsigned int recent_division_count_;  // Divisions in last COV_UPDATE_INTERVAL iters (drives phase detection; reset every 50 iters)
        unsigned int diag_division_count_;    // Divisions in current diagnostics window (reset every 100 iters, after CSV export)
        unsigned int diag_unstable_cell_count_;  // Numerically unstable cells removed in the current diagnostics window
        unsigned int nb_unstable_cells_removed_; // ... and over the whole run

        //Optional custom path for diagnostics CSV export (for growth benchmarks)
        std::string diagnostics_csv_path_;  // If non-empty, use this path instead of default

        // Deferred schedule update after cell division.
        // When cell division modifies cell_lst_, we cannot immediately reconfigure the
        // OpenMP scheduler because the next parallel region may access stale pointers.
        // Instead, we set a flag and apply the update at the start of the next iteration.
        bool pending_division_update_ = false;
        unsigned pending_division_count_ = 0;


    public:
        /// Construct an empty solver; use the parameter constructor to run a simulation.
        solver() = default;                          //default constructor
        /// Release the modules of the solver in reverse order of creation.
        ~solver();                                   //destructor with proper cleanup
        solver(const solver& v) = delete;            //copy constructor
        solver(solver&& v) = delete;                 //move constructor
        solver& operator=(const solver& v) = delete; //copy assignment operator
        solver& operator=(solver&& v) = default;     //move assignment operator 
      
        /**
         * @brief Set up a simulation.
         *
         * Chooses the thread count, pins one thread per physical core (unless cpu_topology::pinning_requested()
         * is false), configures the OpenMP scheduling, deletes and
         * re-creates the output folder (with `cell_data/`, `face_data/` and, with POLARIZATION_MODE_INDEX 2,
         * `region_data/`), assigns cell ids, sets the initial target volumes and creates the modules.
         *
         * @param sim_parameters Numerical parameters of the simulation.
         * @param cell_lst Cells to simulate, as built by simulation_initializer.
         * @param nb_threads Number of OpenMP threads; 0 or less uses one thread per physical core.
         * @param write_cell_stats_in_string Write the per-cell statistics to a string (read with
         *        get_simulation_statistics()) instead of `simulation_statistics.csv`.
         * @param verbose Print the progress of the simulation.
         * @param schedule_mode OpenMP scheduling mode: `static`, `dynamic`, `guided` or `adaptive`.
         * @param diagnostics_csv_path Path of the diagnostics CSV; empty for
         *        `<output folder>/performance_diagnostics.csv`.
         * @throws intialization_exception If the scheduling mode is invalid or the output folders cannot be
         *         created.
         * @warning The output folder is deleted, with all its content, before it is re-created.
         */
        solver(
            const global_simulation_parameters& sim_parameters,
            const std::vector<cell_ptr>& cell_lst,
            int nb_threads = -1, //If set to -1, SimuCell3D will use the available cores
            bool write_cell_stats_in_string = false,  //If true, the program will write the cell statistics in a string instead of a file
            bool verbose = true, //If true, the program will print some information about the simulation progress
            const std::string& schedule_mode = "adaptive", //OpenMP scheduling mode: "static", "dynamic", "guided", or "adaptive"
            const std::string& diagnostics_csv_path = "" //Optional custom path for diagnostics CSV (for benchmarks)
        ) noexcept(false);

        /**
         * @brief Run the main loop until the simulation duration is reached or no cell is left.
         *
         * Calls run_iteration() repeatedly, then writes a last row of statistics and, if verbose, prints the
         * performance report.
         * @throws unstable_simulation_exception If the simulation diverges (see run_iteration()).
         * @throws intialization_exception If the statistics file cannot be written.
         */
        void run() noexcept(false);

        /**
         * @brief Run one iteration (one time step) of the simulation.
         *
         * Virtual so that derived solvers can add work to each iteration (solver_python_wrapper checks for
         * Ctrl-C here).
         * @throws unstable_simulation_exception If too many mesh refinements fail (handle_refinement_failures())
         *         or the contact grid cannot be built for a diverged tissue.
         * @throws intialization_exception If the statistics file cannot be written.
         * @throws mesh_writer_exception If an output mesh cannot be written.
         */
        virtual void run_iteration() noexcept(false);

        /**
         * @brief Write the cell and face meshes if a new sampling period has started.
         *
         * Files are `cell_data/result_<n>.vtk` and `face_data/result_<n>.vtk` with
         * n = floor(t / sampling period) + 1 (and `region_data/result_<n>.vtk` with POLARIZATION_MODE_INDEX 2).
         * @throws mesh_writer_exception If a file cannot be written.
         */
        void save_mesh() noexcept(false);

        /**
         * @brief Erase the cells below their minimum volume and the numerically unstable ones.
         *
         * Last step of run_iteration(). Each unstable cell is reported on standard error and counted in
         * get_nb_unstable_cells_removed().
         */
        void remove_dead_cells() noexcept;

        /**
         * @brief Erase the cells whose mesh refinement failed in this iteration.
         *
         * Called by run_iteration() right after the mesh refinement, before contact detection. Each removed cell
         * is reported on standard error and counted in get_nb_unstable_cells_removed().
         * @param failures Cells reported by local_mesh_refiner::refine_meshes(), with the reason of each failure.
         * @throws unstable_simulation_exception, without erasing anything, if there are more failures than
         *         refinement_failure_limit() allows (max(10, 1 % of the cells)) or if the failures cover every cell.
         */
        void handle_refinement_failures(const std::vector<local_mesh_refiner::refinement_failure>& failures) noexcept(false);

        /// Return the cells currently simulated.
        const std::vector<cell_ptr>& get_cell_lst() const noexcept{return cell_lst_;}
        /// Return the number of OpenMP threads used by the solver.
        unsigned short int get_nb_threads() const noexcept{return nb_threads_;}
        /// Return the number of cells removed as numerically unstable or after a failed mesh refinement since the start.
        unsigned int get_nb_unstable_cells_removed() const noexcept{return nb_unstable_cells_removed_;}
        /// Return the numerical parameters of the simulation.
        const global_simulation_parameters& get_sim_parameters() const noexcept{return sim_parameters_;}


        /**
         * @brief Return the per-cell statistics written to a string.
         * @return The statistics in CSV format.
         * @throws std::runtime_error If the solver was constructed with write_cell_stats_in_string = false.
         */
        std::string get_simulation_statistics() const noexcept(false);

};
//--------------------------------------------------------------------------------------------------------------------------


#endif