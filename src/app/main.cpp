
#include <iostream>

#include "simulation_initializer.hpp"
#include "solver.hpp"
#include "mesh_writer.hpp"

#ifdef __linux__
#include <sched.h>  // For CPU affinity detection
#endif

// External reference to global variable defined in solver.cpp
// We capture the original CPU count VERY EARLY in main() before OpenMP initialization
extern int g_original_cpu_count;

int main (int argc, char** argv){

    // Capture CPU affinity early in main() for CPU restriction detection
    // The static initializer in solver.cpp also captures this, but we update here
    // in case the affinity changes between static init and main()
    #ifdef __linux__
    cpu_set_t original_mask;
    CPU_ZERO(&original_mask);
    if (sched_getaffinity(0, sizeof(original_mask), &original_mask) == 0) {
        int current_count = CPU_COUNT(&original_mask);
        // Use the maximum of what static initializer captured vs what we see now
        // This handles the case where OpenMP modifies affinity after static init
        if (g_original_cpu_count < current_count) {
            g_original_cpu_count = current_count;
        }
    }
    #endif

    //Get the path to the input parameter file and optional scheduling mode
	std::string path_parameter_file;
    std::string schedule_mode = "adaptive"; // default to adaptive (intelligent per-loop) scheduling
    std::string diagnostics_csv_path = ""; // default: no custom diagnostics path
    std::string custom_output_dir = ""; // default: use XML output_mesh_folder_path

    if (argc >= 2){
        // Parse all arguments - flags can appear anywhere, parameter file is the non-flag argument
        for(int i = 1; i < argc; i++){
            std::string arg = argv[i];

            // Skip if it's a flag (starts with --)
            if(arg.find("--") == 0){
                // Parse the flag below
            }
            else{
                // Non-flag argument is the parameter file
                if(path_parameter_file.empty()){
                    path_parameter_file = arg;
                }
                else{
                    std::cerr << "Error: Multiple parameter files specified" << std::endl;
                    std::cerr << "       First:  " << path_parameter_file << std::endl;
                    std::cerr << "       Second: " << arg << std::endl;
                    return 1;
                }
                continue;  // Skip flag parsing for non-flag arguments
            }

            if(arg.find("--schedule=") == 0){
                schedule_mode = arg.substr(11); // Extract value after "--schedule="
                // Check for removed --schedule=auto
                if(schedule_mode == "auto"){
                    std::cerr << "Error: --schedule=auto has been removed." << std::endl;
                    std::cerr << "       Use --schedule=adaptive instead (now the default)." << std::endl;
                    return 1;
                }
                if(schedule_mode != "static" && schedule_mode != "dynamic" &&
                   schedule_mode != "guided" && schedule_mode != "adaptive"){
                    std::cerr << "Error: --schedule must be 'static', 'dynamic', 'guided', or 'adaptive'" << std::endl;
                    std::cout << "Usage: ./simucell3d path/to/parameter_file.xml [OPTIONS]" << std::endl;
                    std::cout << "  --schedule=MODE        : adaptive (default), static, dynamic, guided" << std::endl;
                    std::cout << "  --output-dir=PATH      : Override output directory" << std::endl;
                    std::cout << "  --diagnostics-csv=PATH : Export diagnostics to CSV" << std::endl;
                    return 1;
                }
            }
            else if(arg.find("--diagnostics-csv=") == 0){
                diagnostics_csv_path = arg.substr(18); // Extract value after "--diagnostics-csv="
                if(diagnostics_csv_path.empty()){
                    std::cerr << "Error: --diagnostics-csv requires a file path" << std::endl;
                    return 1;
                }
            }
            else if(arg == "--enable-per-loop-scheduling"){
                std::cerr << "Error: --enable-per-loop-scheduling has been removed." << std::endl;
                std::cerr << "       Per-loop scheduling is now automatic with --schedule=adaptive (the default)." << std::endl;
                return 1;
            }
            else if(arg.find("--output-dir=") == 0){
                custom_output_dir = arg.substr(13); // Extract value after "--output-dir="
                if(custom_output_dir.empty()){
                    std::cerr << "Error: --output-dir requires a directory path" << std::endl;
                    return 1;
                }

                // Trim whitespace
                custom_output_dir.erase(0, custom_output_dir.find_first_not_of(" \t\n\r"));
                custom_output_dir.erase(custom_output_dir.find_last_not_of(" \t\n\r") + 1);

                if(custom_output_dir.empty()){
                    std::cerr << "Error: --output-dir path is whitespace-only" << std::endl;
                    return 1;
                }

                if(custom_output_dir.length() > 4096){
                    std::cerr << "Error: --output-dir path too long (max 4096 characters)" << std::endl;
                    return 1;
                }

                // Check if path is existing file
                namespace fs = std::filesystem;
                if(fs::exists(custom_output_dir) && !fs::is_directory(custom_output_dir)){
                    std::cerr << "Error: --output-dir points to existing file, not directory: " << custom_output_dir << std::endl;
                    return 1;
                }

                // Warn on relative paths
                if(custom_output_dir[0] != '/'){
                    std::cerr << "Warning: --output-dir uses relative path: " << custom_output_dir << std::endl;
                    std::cerr << "         Resolved relative to: " << fs::current_path() << std::endl;
                }
            }
        }

        // Verify we got a parameter file
        if(path_parameter_file.empty()){
            std::cout << "No input parameter file given in the command line" << std::endl;
            std::cout << "Usage: ./simucell3d path/to/parameter_file.xml [OPTIONS]" << std::endl;
            std::cout << "\nScheduling modes:" << std::endl;
            std::cout << "  --schedule=adaptive : Intelligent per-loop scheduling [DEFAULT]" << std::endl;
            std::cout << "  --schedule=static   : Fixed static scheduling" << std::endl;
            std::cout << "  --schedule=dynamic  : Fixed dynamic scheduling" << std::endl;
            std::cout << "  --schedule=guided   : Fixed guided scheduling" << std::endl;
            return 1;
        }
    }
    else{
        std::cout << "No input file given in the command line" << std::endl;
        std::cout << "Usage: ./simucell3d path/to/parameter_file.xml [OPTIONS]" << std::endl;
        std::cout << "\nScheduling modes:" << std::endl;
        std::cout << "  --schedule=adaptive : Intelligent per-loop scheduling [DEFAULT]" << std::endl;
        std::cout << "                        Automatically optimizes each computational phase:" << std::endl;
        std::cout << "                        - Contact detection: dynamic (heterogeneous workload)" << std::endl;
        std::cout << "                        - Force integration: guided (moderate variance)" << std::endl;
        std::cout << "                        - Mesh operations: static (uniform workload)" << std::endl;
        std::cout << "  --schedule=static   : Fixed static scheduling (large uniform workloads >2k cells)" << std::endl;
        std::cout << "  --schedule=dynamic  : Fixed dynamic scheduling (small or heterogeneous workloads)" << std::endl;
        std::cout << "  --schedule=guided   : Fixed guided scheduling (balanced approach)" << std::endl;
        std::cout << "\nOutput options:" << std::endl;
        std::cout << "  --output-dir=PATH      : Override output directory from XML" << std::endl;
        std::cout << "  --diagnostics-csv=PATH : Export metrics to CSV (every 100 iterations)" << std::endl;

        return 1;
    }

    //Catch any exception and print error msg
    solver solver_;
    try{
        //Load all the parameters and geometrical information of the tissue
        simulation_initializer sim_init(path_parameter_file);

        // Get simulation parameters and override output directory if flag provided
        auto sim_params = sim_init.get_simulation_parameters();
        if(!custom_output_dir.empty()){
            sim_params.output_folder_path_ = custom_output_dir;
            std::cout << "Output directory overridden: " << custom_output_dir << std::endl;
        }

        //Initialize the solver with scheduling mode and optional diagnostics path
        solver_ =  solver(sim_params, sim_init.get_cell_lst(), -1, false, true, schedule_mode, diagnostics_csv_path);

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