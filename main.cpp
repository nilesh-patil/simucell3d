
#include <iostream>

#include "simulation_initializer.hpp"
#include "solver.hpp"
#include "mesh_writer.hpp"




void print_usage() {
    std::cout << "Usage: simucell3d <parameter_file.xml> [options]\n";
    std::cout << "\nOptions:\n";
    std::cout << "  --checkpoint-interval <N>  Save checkpoint every N iterations\n";
    std::cout << "  --checkpoint-dir <path>    Directory for checkpoint files (default: ./checkpoints/)\n";
    std::cout << "  --restart <file>           Resume from checkpoint file\n";
    std::cout << "\nExamples:\n";
    std::cout << "  simucell3d params.xml --checkpoint-interval 100\n";
    std::cout << "  simucell3d params.xml --restart checkpoints/checkpoint_00000500.simchk\n";
}

int main (int argc, char** argv){

    //Get the path to the input parameter file
    std::string path_parameter_file;
    std::string checkpoint_restart_file;
    unsigned int checkpoint_interval = 0;
    std::string checkpoint_dir = "./checkpoints/";

    // Parse command-line arguments
    if (argc < 2){
        std::cout << "No input file given in the command line" << std::endl;
        print_usage();
        return 1;
    }

    // Check for help first
    if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")){
        print_usage();
        return 0;
    }

    path_parameter_file = argv[1];

    // Parse optional arguments
    for (int i = 2; i < argc; i++){
        std::string arg = argv[i];

        if (arg == "--checkpoint-interval" && i + 1 < argc){
            checkpoint_interval = std::stoul(argv[++i]);
        }
        else if (arg == "--checkpoint-dir" && i + 1 < argc){
            checkpoint_dir = argv[++i];
        }
        else if (arg == "--restart" && i + 1 < argc){
            checkpoint_restart_file = argv[++i];
        }
        else if (arg == "--help" || arg == "-h"){
            print_usage();
            return 0;
        }
        else{
            std::cout << "Unknown argument: " << arg << std::endl;
            print_usage();
            return 1;
        }
    }

    //Catch any exception and print error msg
    solver solver_;
    try{
        // Check if restarting from checkpoint
        if (!checkpoint_restart_file.empty()){
            std::cout << "Restarting from checkpoint: " << checkpoint_restart_file << std::endl;

            //Load parameters (needed for solver configuration)
            simulation_initializer sim_init(path_parameter_file);

            //Initialize solver with parameters
            solver_ = solver(sim_init.get_simulation_parameters(), sim_init.get_cell_lst());

            //Load checkpoint state (replaces cells and restores iteration/time)
            solver_.load_checkpoint(checkpoint_restart_file);
        }
        else {
            //Load all the parameters and geometrical information of the tissue
            simulation_initializer sim_init(path_parameter_file);

            //Initialize the solver
            solver_ =  solver(sim_init.get_simulation_parameters(), sim_init.get_cell_lst());
        }

        // Configure checkpointing
        solver_.set_parameter_file_path(path_parameter_file);
        solver_.set_checkpoint_interval(checkpoint_interval);
        solver_.set_checkpoint_directory(checkpoint_dir);

        if (checkpoint_interval > 0){
            std::cout << "Checkpointing enabled: saving every " << checkpoint_interval
                      << " iterations to " << checkpoint_dir << std::endl;
        }

        //Run the simulation
        solver_.run();
    }
    
    //Catch and print any exception
    catch(std::exception const& e){
        std::cerr << e.what() << std::endl;

        return 1;
    }

    return 0;
}