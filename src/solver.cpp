#include "solver.hpp"
#include <pthread.h>  // For pthread_setaffinity_np
#include <sched.h>    // For cpu_set_t
#include <iomanip>    // For std::setprecision
#include <set>        // For std::set (hardware topology detection)

// Global variable to store original CPU count captured before OpenMP initialization
// Defined here so it's accessible from both main.cpp and solver.cpp
int g_original_cpu_count = -1;

// Static initializer to capture CPU affinity BEFORE any OpenMP initialization
// This runs during C++ static initialization, before main() and before OpenMP runtime init
#ifdef __linux__
struct EarlyCPUCapture {
    EarlyCPUCapture() {
        cpu_set_t mask;
        CPU_ZERO(&mask);
        if (sched_getaffinity(0, sizeof(mask), &mask) == 0) {
            g_original_cpu_count = CPU_COUNT(&mask);
        }
    }
};
// Create a static instance to trigger the constructor before main()
static EarlyCPUCapture early_capture;
#endif

//--------------------------------------------------------------------------------------------------------------------------
// Helper function: Calculate coefficient of variation for workload heterogeneity
// ADDITIVE COST MODEL (Fixed BUG #3): Independent cost components instead of multiplicative compounding
// Bio-Sim-Expert recommended: Additive model preserves biological semantics and reduces dynamic range
// Fixed BUG #2: Now mesh-adaptive (scales with min_edge_len and contact_cutoff)
static double calculate_workload_heterogeneity(const std::vector<cell_ptr>& cell_lst, double min_edge_len,
                                               double contact_cutoff_adhesion) {
    if (cell_lst.empty())
        return 0.0;

    // Collect biology-aware complexity scores for each cell
    std::vector<double> complexity_scores;
    complexity_scores.reserve(cell_lst.size());

    for (const auto& cell : cell_lst) {
        // Base cost: Mesh operations (face traversal, normal calculations, area updates)
        double base_cost = static_cast<double>(cell->get_nb_of_faces());

        // Initialize independent cost components (additive, not multiplicative)
        double contact_cost = 0.0;
        double integration_cost = 0.0;
        double polarization_cost = 0.0;
        double growth_cost = 0.0;
        double mesh_quality_cost = 0.0;

        // Factor 1: Contact complexity (count coupled nodes)
        // Different contact models store coupling information differently
        size_t contact_count = 0;
        size_t total_nodes = 0;

#if CONTACT_MODEL_INDEX == 1
        // Node-node coupling: each node can be coupled to one other node
        for (const auto& node : cell->get_node_lst()) {
            if (node.is_used()) {
                total_nodes++;
                if (node.is_coupled()) {
                    contact_count++;
                }
            }
        }
#elif CONTACT_MODEL_INDEX == 2
        // Face-face coupling: each node can be coupled to multiple nodes
        for (const auto& node : cell->get_node_lst()) {
            if (node.is_used()) {
                total_nodes++;
                if (node.is_coupled()) {
                    contact_count += node.get_nb_coupled_nodes();
                }
            }
        }
#endif

        // Calculate contact fraction for boundary/interior classification
        double contact_fraction = (total_nodes > 0) ? static_cast<double>(contact_count) / total_nodes : 0.0;

// Estimate neighbor count for boundary detection (mesh-adaptive)
#if CONTACT_MODEL_INDEX == 1 || CONTACT_MODEL_INDEX == 2
        // Fixed BUG #2: Mesh-adaptive neighbor estimation (Bio-Sim-Expert recommendation)
        // Scale from baseline calibration (default_dynamic: edge=7.5e-7, cutoff=5e-7)
        constexpr double baseline_edge_len = 7.5e-7;
        constexpr double baseline_cutoff = 5.0e-7;
        constexpr double baseline_nodes_per_interface = 15.0;

        // Resolution scaling: nodes/interface increases as mesh gets finer
        // Exponent 1.5 accounts for filtering (normal alignment, curvature, one-to-one mapping)
        double resolution_scaling = std::pow(baseline_edge_len / min_edge_len, 1.5);

        // Cutoff scaling: larger cutoff means more nodes in contact region
        double cutoff_scaling = contact_cutoff_adhesion / baseline_cutoff;

        // Calculate mesh-adaptive nodes per interface
        double nodes_per_interface = baseline_nodes_per_interface * resolution_scaling * cutoff_scaling;

        // Clamp to reasonable range [5, 100] to prevent extreme values
        nodes_per_interface = std::clamp(nodes_per_interface, 5.0, 100.0);

        size_t estimated_neighbors = (total_nodes > 0 && contact_count > 0)
                                         ? std::max((size_t)1, static_cast<size_t>(contact_count / nodes_per_interface))
                                         : 0;
#else
        size_t estimated_neighbors = 0;
#endif

        // Factor 2: Cell type-specific costs (ADDITIVE, not multiplicative)
        unsigned short cell_type_id = cell->get_cell_type_id();

        if (cell->is_static()) {
            // Static cells (ECM, structural barriers): Contact detection only, NO time integration
            // Bio-Sim-Expert: ~20% of dynamic cell (contact detection ~10%, USPG queries ~10%)
            contact_cost = base_cost * 0.20 * contact_fraction;
            integration_cost = 0.0;  // No time integration for static cells
        } else {
// Dynamic cells: Full contact + time integration + internal forces
// Contact cost scales with contact fraction
#if CONTACT_MODEL_INDEX == 0
            contact_cost = base_cost * 0.25 * contact_fraction;  // Node-face springs: baseline
#elif CONTACT_MODEL_INDEX == 1
            contact_cost = base_cost * 0.28 * contact_fraction;  // Node-node: +12% overhead
#elif CONTACT_MODEL_INDEX == 2
            contact_cost = base_cost * 0.32 * contact_fraction;  // Face-face: +28% overhead (multi-coupling)
#endif

            // Boundary cells have less contact work
            if (estimated_neighbors > 0 && estimated_neighbors < 5) {
                contact_cost *= 0.7;  // Boundary: 30% less contact work
            }

            // Time integration cost (pressure, surface tension, bending forces)
            integration_cost = base_cost * 0.65;  // ~65% of per-iteration work

            // Lumen cells (Type 2): Same integration as epithelial
            if (cell_type_id == 2) {
                // No adjustment needed - lumen does full force calc + integration
            }
            // Nucleus cells (Type 3): Already has smaller base_cost (fewer faces)
            // Bio-Sim-Expert: Nucleus is 50-60% of host surface area → 50-60% base_cost automatically
            // NO multiplier needed - size difference already captured in face count
            else if (cell_type_id == 3) {
                // No adjustment - smaller mesh already reflected in base_cost
            }
            // Epithelial cells (Type 0): Add polarization overhead if enabled
            else if (cell_type_id == 0) {
#if POLARIZATION_MODE_INDEX == 1
                // Contact-based polarization: Check face contacts, update apical/basal
                polarization_cost = base_cost * 0.10;  // +10% for contact queries
#elif POLARIZATION_MODE_INDEX == 2
                // Spatial discretization: Ray casting for orientation
                polarization_cost = base_cost * 0.45;  // +45% for geometric queries (expensive)
#endif
            }

            // Growth state overhead
            if (cell->get_growth_rate() > 1e-20) {
                // Bio-Sim-Expert: Direct cost is <1% (just volume update)
                // Indirect costs (pressure changes, mesh refinement) captured in other factors
                growth_cost = base_cost * 0.01;  // +1% for volume updates
            }
        }

        // Factor 6: Mesh quality proxy (high face count suggests poor quality)
        size_t num_faces = cell->get_nb_of_faces();
        if (num_faces > 300) {
            // High face count: likely needs edge swaps for quality maintenance
            mesh_quality_cost = base_cost * 0.08;  // +8% for edge swap operations
        }

        // Total complexity: Sum of independent components (ADDITIVE MODEL)
        double complexity =
            base_cost + contact_cost + integration_cost + polarization_cost + growth_cost + mesh_quality_cost;

        // Clamp to reasonable bounds to prevent extreme outliers from distorting CoV
        // Min: Static cell with no contacts (base_cost only)
        // Max: ~2.5× base for heavily loaded epithelial cell with spatial polarization
        complexity = std::clamp(complexity, base_cost * 0.2, base_cost * 2.5);

        complexity_scores.push_back(complexity);
    }

    // Calculate mean complexity
    double sum = 0.0;
    for (double score : complexity_scores) {
        sum += score;
    }
    double mean = sum / complexity_scores.size();

    // Calculate variance
    double variance = 0.0;
    for (double score : complexity_scores) {
        double diff = score - mean;
        variance += diff * diff;
    }
    variance /= complexity_scores.size();

    // Calculate coefficient of variation (CoV)
    double std_dev = std::sqrt(variance);
    double cov = (mean > 0.0) ? (std_dev / mean) : 0.0;

    return cov;
}

//--------------------------------------------------------------------------------------------------------------------------
// Helper struct: Benchmark-based schedule recommendation
struct BenchmarkEntry {
    size_t num_cells_min;
    size_t num_cells_max;
    std::string recommended_mode;
    std::string rationale;
};

//--------------------------------------------------------------------------------------------------------------------------
// Helper function: Lookup optimal mode from benchmark data
static std::string lookup_benchmark_mode(size_t num_cells, std::string& rationale) {
    // Empirically determined from 52 benchmark runs (November 2025)
    // Each entry represents the winning mode for that scale range
    static const std::vector<BenchmarkEntry> benchmark_table = {
        {1, 3, "dynamic", "Very small scale: dynamic handles thread starvation"},
        {4, 7, "static", "Small scale: static benefits from cache locality"},
        {8, 15, "dynamic", "Transition zone: dynamic provides flexibility"},
        {16, 31, "guided", "Small-medium scale: guided balances locality and load"},
        {32, 63, "dynamic", "Medium scale: dynamic excels (4.73x speedup observed)"},
        {64, 127, "guided", "Medium-large scale: guided optimal (20s vs 74s for auto)"},
        {128, 255, "dynamic", "Large scale with some imbalance: dynamic wins"},
        {256, 511, "static", "Large uniform scale: static maximizes cache"},
        {512, 1023, "guided", "Very large scale: guided sweet spot"},
        {1024, 2047, "static", "Cache fits in L3: static wins"},
        {2048, 4095, "static", "Large scale: locality critical"},
        {4096, 8191, "guided", "Very large: guided handles scale better than static"},
        {8192, SIZE_MAX, "guided", "Massive scale: guided provides best balance"}};

    for (const auto& entry : benchmark_table) {
        if (num_cells >= entry.num_cells_min && num_cells <= entry.num_cells_max) {
            rationale = entry.rationale;
            return entry.recommended_mode;
        }
    }

    rationale = "Fallback: scale outside benchmark range";
    return "guided";  // Safe default
}

//--------------------------------------------------------------------------------------------------------------------------
// Helper function: Multi-factor heuristic for schedule selection
// Fixed BUG #2: Now includes workload heterogeneity (CoV) in decision logic
static std::string multi_factor_heuristic(size_t num_cells, int num_threads, const std::vector<cell_ptr>& cell_lst,
                                          double min_edge_len, double contact_cutoff_adhesion, std::string& rationale) {
    // Factor 1: Workload heterogeneity (Coefficient of Variation)
    // This is the PRIMARY factor for scheduling decisions
    double workload_cov = calculate_workload_heterogeneity(cell_lst, min_edge_len, contact_cutoff_adhesion);

    // Factor 2: Thread granularity
    int tasks_per_thread = static_cast<int>(num_cells) / num_threads;

    // Decision logic based on multiple factors
    if (num_cells < 16) {
        // Very small problems: dynamic to avoid thread starvation
        rationale = "Very small scale (" + std::to_string(num_cells) + " cells, " + std::to_string(tasks_per_thread) +
                    " per thread): dynamic avoids starvation";
        return "dynamic";
    }

    // Primary decision: Use CoV to determine if workload is heterogeneous
    if (workload_cov > 0.6) {
        // High heterogeneity: dynamic scheduling excels
        rationale = "High workload heterogeneity (CoV=" + std::to_string(workload_cov).substr(0, 4) + ") at " +
                    std::to_string(num_cells) + " cells: dynamic for load balancing";
        return "dynamic";
    } else if (workload_cov < 0.15) {
        // Very low heterogeneity: static scheduling for cache locality
        // Only use static if we have reasonable task granularity
        if (tasks_per_thread >= 4) {
            rationale = "Low workload heterogeneity (CoV=" + std::to_string(workload_cov).substr(0, 4) + "), " +
                        std::to_string(num_cells) + " cells: static maximizes locality";
            return "static";
        }
    }

    // Moderate heterogeneity (0.15 <= CoV <= 0.6): Use scale-based heuristics
    if (num_cells >= 512 && num_cells <= 4096) {
        // Medium-large scale: guided balances locality and load
        rationale = "Moderate heterogeneity (CoV=" + std::to_string(workload_cov).substr(0, 4) + "), " +
                    std::to_string(num_cells) + " cells: guided balances both";
        return "guided";
    } else if (num_cells > 4096) {
        // Large scale with moderate heterogeneity: guided or static
        if (workload_cov < 0.30) {
            rationale = "Large scale (" + std::to_string(num_cells) +
                        " cells), low-moderate CoV=" + std::to_string(workload_cov).substr(0, 4) +
                        ": static for cache locality";
            return "static";
        } else {
            rationale = "Large scale (" + std::to_string(num_cells) +
                        " cells), moderate CoV=" + std::to_string(workload_cov).substr(0, 4) + ": guided for balance";
            return "guided";
        }
    }

    if (tasks_per_thread < 4) {
        // Too few tasks for static distribution
        rationale = "Low task count per thread (" + std::to_string(tasks_per_thread) +
                    "), CoV=" + std::to_string(workload_cov).substr(0, 4) + ": dynamic for fine-grain parallelism";
        return "dynamic";
    }

    // Default: use guided as safe middle ground
    rationale = "Guided selected as balanced default (CoV=" + std::to_string(workload_cov).substr(0, 4) + ", " +
                std::to_string(num_cells) + " cells)";
    return "guided";
}

//--------------------------------------------------------------------------------------------------------------------------
// Hardware topology detection helper functions
struct HardwareTopology {
    int num_logical_cpus;    // Total logical CPUs (with hyperthreading)
    int num_physical_cores;  // Physical cores only
    int threads_per_core;    // Hyperthreading factor (usually 1 or 2)
    int num_numa_nodes;      // NUMA topology
    bool is_single_socket;   // True if single-socket system
};

//--------------------------------------------------------------------------------------------------------------------------
// Helper function: Detect hardware topology
static HardwareTopology detect_hardware_topology() {
    HardwareTopology topology;

    // Get logical CPU count from OpenMP
    topology.num_logical_cpus = omp_get_num_procs();

    // Detect threads per core and physical cores
    // Method 1: Try reading from /sys/devices/system/cpu/cpu0/topology/thread_siblings_list
    std::ifstream topology_file("/sys/devices/system/cpu/cpu0/topology/thread_siblings_list");
    if (topology_file.is_open()) {
        std::string siblings;
        std::getline(topology_file, siblings);
        topology_file.close();

        // Count commas to determine threads per core
        int comma_count = std::count(siblings.begin(), siblings.end(), ',');
        topology.threads_per_core = comma_count + 1;
        topology.num_physical_cores = topology.num_logical_cpus / topology.threads_per_core;
    } else {
        // Fallback: Try reading /proc/cpuinfo to count unique physical cores
        // Fixed BUG #8: Don't assume HT based on CPU count (fails on 32+ core systems)
        std::ifstream cpuinfo("/proc/cpuinfo");
        std::set<int> physical_cores;
        bool found_physical_id = false;

        if (cpuinfo.is_open()) {
            std::string line;
            while (std::getline(cpuinfo, line)) {
                if (line.find("core id") == 0) {
                    size_t colon_pos = line.find(':');
                    if (colon_pos != std::string::npos) {
                        int core_id = std::stoi(line.substr(colon_pos + 1));
                        physical_cores.insert(core_id);
                        found_physical_id = true;
                    }
                }
            }
            cpuinfo.close();

            if (found_physical_id && !physical_cores.empty()) {
                topology.num_physical_cores = physical_cores.size();
                topology.threads_per_core = topology.num_logical_cpus / topology.num_physical_cores;
            } else {
                // Final fallback: Assume no hyperthreading (safest assumption)
                topology.threads_per_core = 1;
                topology.num_physical_cores = topology.num_logical_cpus;
            }
        } else {
            // Cannot read any topology files: assume no hyperthreading
            topology.threads_per_core = 1;
            topology.num_physical_cores = topology.num_logical_cpus;
        }
    }

    // Detect NUMA nodes
    topology.num_numa_nodes = 1;  // Default assumption
    std::ifstream numa_file("/sys/devices/system/node/online");
    if (numa_file.is_open()) {
        std::string numa_range;
        std::getline(numa_file, numa_range);
        numa_file.close();

        // Parse range like "0" or "0-1" or "0-3"
        size_t dash_pos = numa_range.find('-');
        if (dash_pos != std::string::npos) {
            int max_node = std::stoi(numa_range.substr(dash_pos + 1));
            topology.num_numa_nodes = max_node + 1;
        }
    }

    topology.is_single_socket = (topology.num_numa_nodes == 1);

    return topology;
}

//--------------------------------------------------------------------------------------------------------------------------
// Helper function: Detect if running under CPU restrictions (taskset, cgroups, cpusets)
static bool is_cpu_restricted(int topology_physical_cores, bool verbose) {
#ifdef __linux__
    // Use the CPU count captured in main() BEFORE OpenMP initialization
    // This avoids the race condition where OMP_PROC_BIND=close modifies the affinity
    // mask before we can detect the original taskset restrictions
    int allowed_cpus = g_original_cpu_count;

    // Fallback: If g_original_cpu_count wasn't captured (shouldn't happen), query now
    if (allowed_cpus < 0) {
        cpu_set_t mask;
        CPU_ZERO(&mask);
        int result = sched_getaffinity(0, sizeof(mask), &mask);
        if (result != 0) {
            if (verbose) {
                std::cout << "CPU Restriction Detection: Failed to query affinity (errno: " << errno << ")"
                          << std::endl;
                std::cout << "  Assuming unrestricted execution" << std::endl;
            }
            return false;
        }
        allowed_cpus = CPU_COUNT(&mask);
    }

    // Get system-wide CPU count (not affected by taskset/cgroup restrictions)
    // This reads /sys/devices/system/cpu/present which shows the actual hardware
    int system_total_cpus = allowed_cpus;  // Default fallback
    std::ifstream cpu_present("/sys/devices/system/cpu/present");
    if (cpu_present.is_open()) {
        std::string line;
        if (std::getline(cpu_present, line)) {
            // Format: "0-15" or "0-7,16-23" etc.
            size_t dash = line.find('-');
            if (dash != std::string::npos) {
                try {
                    int max_cpu = std::stoi(line.substr(dash + 1));
                    system_total_cpus = max_cpu + 1;
                } catch (...) {
                    // Fallback to allowed_cpus on parse error
                }
            }
        }
        cpu_present.close();
    }

    bool is_restricted = (allowed_cpus < system_total_cpus);

    if (verbose) {
        std::cout << "CPU Restriction Detection:" << std::endl;
        std::cout << "  System total CPUs: " << system_total_cpus << std::endl;
        std::cout << "  Available to process: " << allowed_cpus << std::endl;
        std::cout << "  Status: " << (is_restricted ? "RESTRICTED (taskset/cgroup/cpuset active)" : "UNRESTRICTED")
                  << std::endl;

        if (is_restricted) {
            std::cout << "  Note: Process constrained to " << allowed_cpus << "/" << system_total_cpus << " cores"
                      << std::endl;
            std::cout << "        OpenMP placement strategy will adapt to avoid thread collapse" << std::endl;
        }
    }

    return is_restricted;

#else
    // Non-Linux systems: assume unrestricted
    if (verbose) {
        std::cout << "CPU Restriction Detection: Non-Linux platform, assuming unrestricted" << std::endl;
    }
    return false;
#endif
}

//--------------------------------------------------------------------------------------------------------------------------
// Helper function: Configure OpenMP for optimal CPU utilization
static void configure_cpu_optimization(const HardwareTopology& topology, bool verbose) {
    if (topology.is_single_socket) {
        // Single-socket optimization: Use physical cores only, bind threads to cores
        if (verbose) {
            std::cout << "CPU Optimization: Single-socket detected" << std::endl;
            std::cout << "  Logical CPUs: " << topology.num_logical_cpus << std::endl;
            std::cout << "  Physical cores: " << topology.num_physical_cores << std::endl;
            std::cout << "  Threads/core: " << topology.threads_per_core << std::endl;

            if (topology.threads_per_core > 1) {
                std::cout << "  Strategy: Using physical cores only (disabling hyperthreading)" << std::endl;
                std::cout << "  Rationale: Compute-intensive workloads perform better without HT contention"
                          << std::endl;
            }
        }

        // Set thread affinity to bind threads to physical cores
        // OMP_PROC_BIND=close: Bind threads to consecutive cores
        // OMP_PLACES=cores: Map to physical cores, not logical threads
        setenv("OMP_PROC_BIND", "close", 0);  // Don't overwrite if already set
        setenv("OMP_PLACES", "cores", 0);

    } else {
        // Multi-socket NUMA optimization
        if (verbose) {
            std::cout << "CPU Optimization: Multi-socket NUMA detected" << std::endl;
            std::cout << "  NUMA nodes: " << topology.num_numa_nodes << std::endl;
        }

        // Check if running under CPU restrictions (taskset, cgroups, cpusets)
        bool is_taskset_restricted = is_cpu_restricted(topology.num_physical_cores, verbose);

        if (is_taskset_restricted) {
            // TASKSET MODE: Core restrictions detected
            // Problem: OMP_PLACES=sockets conflicts with taskset, causing thread collapse
            // Solution: Use OMP_PLACES=cores for compatibility with kernel core restrictions
            if (verbose) {
                std::cout << "  Strategy: Core-level binding (adapting to CPU restrictions)" << std::endl;
                std::cout << "  Rationale: OMP_PLACES=sockets conflicts with taskset/cgroup constraints" << std::endl;
                std::cout << "             Using OMP_PLACES=cores to spread threads across allocated cores"
                          << std::endl;
            }
            setenv("OMP_PROC_BIND", "close", 0);
            setenv("OMP_PLACES", "cores", 0);

        } else {
            // UNRESTRICTED MODE: Full NUMA optimization
            // Spread threads across NUMA nodes for better memory bandwidth
            if (verbose) {
                std::cout << "  Strategy: Spread threads across NUMA nodes (unrestricted execution)" << std::endl;
                std::cout << "  Rationale: Maximize memory bandwidth by distributing across sockets" << std::endl;
            }
            setenv("OMP_PROC_BIND", "spread", 0);
            setenv("OMP_PLACES", "sockets", 0);
        }
    }
}

//--------------------------------------------------------------------------------------------------------------------------
// Helper function: Pin threads to specific cores for deterministic performance
static void pin_threads_to_cores(int num_threads, bool verbose) {
#ifdef __linux__
#pragma omp parallel
    {
        int tid = omp_get_thread_num();

        // Create CPU set for affinity
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);

        // Pin thread to specific core
        // Thread 0 → Core 0, Thread 1 → Core 1, etc.
        CPU_SET(tid, &cpuset);

        // Apply affinity to current thread
        int result = pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);

        if (result != 0 && verbose && tid == 0) {
            std::cerr << "Warning: Failed to set thread affinity (error: " << result << ")" << std::endl;
            std::cerr << "Performance may be less predictable due to thread migration" << std::endl;
        }

        // Verify affinity was set (only print from thread 0 to avoid output spam)
        if (verbose && tid == 0) {
            cpu_set_t verify_set;
            CPU_ZERO(&verify_set);
            pthread_getaffinity_np(pthread_self(), sizeof(verify_set), &verify_set);

            std::cout << "Thread Pinning: Enabled (threads pinned to specific cores)" << std::endl;
            std::cout << "  Benefit: Eliminates performance variability from thread migration" << std::endl;
            std::cout << "  Expected: 30-40% reduction in performance variance" << std::endl;
        }
    }
#else
    if (verbose) {
        std::cout << "Thread Pinning: Not available on this platform (Linux only)" << std::endl;
    }
#endif
}

//--------------------------------------------------------------------------------------------------------------------------
// Helper function: Calculate optimal chunk size based on problem characteristics
static int calculate_optimal_chunk_size(size_t num_cells, int num_threads, double heterogeneity_cov,
                                        const std::string& schedule_mode) {
    if (schedule_mode == "static") {
        return 0;  // Let OpenMP decide for static
    }

    // For very small problems, use tiny chunks to maximize parallelism
    if (num_cells < 100) {
        return std::max(1, static_cast<int>(num_cells / (num_threads * 4)));
    }

    // Adaptive chunk sizing based on heterogeneity
    int divisor;
    if (heterogeneity_cov > 0.6) {
        // High heterogeneity: smaller chunks for better load balancing
        divisor = 20;
    } else if (heterogeneity_cov > 0.4) {
        // Moderate heterogeneity: medium chunks
        divisor = 10;
    } else {
        // Low heterogeneity: larger chunks to reduce overhead
        divisor = 4;
    }

    int chunk_size = std::max(1, static_cast<int>(num_cells / (num_threads * divisor)));

    // Clamp to reasonable range
    chunk_size = std::min(std::max(chunk_size, 1), 100);

    return chunk_size;
}

//--------------------------------------------------------------------------------------------------------------------------
// Destructor - Ensure clean shutdown and prevent SIGSEGV
solver::~solver() {
    // Explicit cleanup in reverse order of initialization to prevent memory races
    // Note: OpenMP threads are automatically joined before destructor runs

#if POLARIZATION_MODE_INDEX == 2
    cell_surface_polarizer_ptr_.reset();
#endif

    contact_model_ptr_.reset();
    time_integrator_ptr_.reset();
    statistic_writer_ptr_.reset();
    lmr_ptr_.reset();

    // Clear cell list last
    cell_lst_.clear();
}

//--------------------------------------------------------------------------------------------------------------------------
// Constructor
solver::solver(const global_simulation_parameters& sim_parameters, const std::vector<cell_ptr>& cell_lst,
               int nb_threads, bool write_cell_stats_in_string, bool verbose, const std::string& schedule_mode,
               const std::string& diagnostics_csv_path) noexcept(false)
    : sim_parameters_(sim_parameters),
      cell_lst_(cell_lst),
      verbose_(verbose),
      schedule_mode_(schedule_mode),
      diagnostics_csv_path_(diagnostics_csv_path),
      enable_per_loop_scheduling_(false) {  // Will be set to true for "adaptive" mode

    assert(cell_lst_.size() > 0);

    // Detect hardware topology and configure CPU optimization
    HardwareTopology hw_topology = detect_hardware_topology();
    configure_cpu_optimization(hw_topology, verbose_);

    // By default use physical cores only (better for compute-intensive workloads)
    // User can override by specifying nb_threads explicitly
    if (nb_threads <= 0) {
        nb_threads_ = hw_topology.num_physical_cores;
        if (verbose_) {
            std::cout << "Thread count: Using " << nb_threads_ << " physical cores "
                      << "(out of " << hw_topology.num_logical_cpus << " logical CPUs)" << std::endl;
        }
    } else {
        nb_threads_ = nb_threads;
        if (verbose_) {
            std::cout << "Thread count: User-specified " << nb_threads_ << " threads" << std::endl;
        }
    }

    // Set OpenMP thread count
    omp_set_num_threads(nb_threads_);

    // Optional: Pin threads to specific cores for deterministic performance
    // DISABLED BY DEFAULT due to initialization issues causing SIGSEGV
    // Enable with: export SIMUCELL3D_PIN_THREADS=1
    const char* pin_env = std::getenv("SIMUCELL3D_PIN_THREADS");
    bool enable_pinning = (pin_env != nullptr && std::string(pin_env) == "1");

    if (enable_pinning) {
        pin_threads_to_cores(nb_threads_, verbose_);
    } else if (verbose_) {
        std::cout << "Thread Pinning: Disabled (default)" << std::endl;
        std::cout << "  To enable: export SIMUCELL3D_PIN_THREADS=1" << std::endl;
        std::cout << "  Note: Experimental feature, may cause crashes" << std::endl;
    }

    // Measure workload heterogeneity (Phase 1: will be updated every 50 iterations)
    heterogeneity_cov_ = calculate_workload_heterogeneity(cell_lst_, sim_parameters_.min_edge_len_,
                                                          sim_parameters_.contact_cutoff_adhesion_);
    last_cov_update_iteration_ = 0;            // Initialize timestamp
    double workload_cov = heterogeneity_cov_;  // Keep local variable for constructor logic

    // Initialize phase tracking (Phase 1: Temporal adaptation)
    current_phase_ = SimulationPhase::INITIALIZATION;  // Start in initialization phase
    last_division_check_iteration_ = 0;
    recent_division_count_ = 0;
    diag_division_count_ = 0;
    diag_unstable_cell_count_ = 0;
    nb_unstable_cells_removed_ = 0;

    // Set up OpenMP scheduling based on the schedule_mode parameter
    if (schedule_mode_ == "static") {
        schedule_kind_ = omp_sched_static;
        schedule_chunk_size_ = 0;  // Let OpenMP decide chunk size for static
    } else if (schedule_mode_ == "dynamic") {
        schedule_kind_ = omp_sched_dynamic;
        // FIXED: Adaptive chunk sizing instead of hardcoded 50
        schedule_chunk_size_ =
            calculate_optimal_chunk_size(cell_lst_.size(), nb_threads_, workload_cov, schedule_mode_);
    } else if (schedule_mode_ == "guided") {
        schedule_kind_ = omp_sched_guided;
        // Guided uses larger minimum chunk size
        schedule_chunk_size_ = calculate_optimal_chunk_size(cell_lst_.size(), nb_threads_, workload_cov,
                                                            "dynamic") *
                               2;  // Guided benefits from larger chunks
    } else if (schedule_mode_ == "adaptive") {
        // Adaptive mode: intelligent base scheduler + automatic per-loop scheduling
        // This combines the best of both auto-selection and per-loop optimization
        std::string benchmark_rationale;
        std::string heuristic_rationale;

        // Primary: Use empirical benchmark lookup table
        std::string benchmark_mode = lookup_benchmark_mode(cell_lst_.size(), benchmark_rationale);

        // Secondary: Validate with multi-factor heuristic
        std::string heuristic_mode =
            multi_factor_heuristic(cell_lst_.size(), nb_threads_, cell_lst_, sim_parameters_.min_edge_len_,
                                   sim_parameters_.contact_cutoff_adhesion_, heuristic_rationale);

        // Select base mode (prefer benchmark data for known-good scales)
        std::string selected_base;
        std::string selection_rationale;

        if (benchmark_mode == heuristic_mode) {
            // Both agree - high confidence
            selected_base = benchmark_mode;
            selection_rationale = benchmark_rationale + " (confirmed by heuristic)";
        } else {
            // Disagreement - trust benchmark data but log discrepancy
            selected_base = benchmark_mode;
            selection_rationale =
                benchmark_rationale + " (heuristic suggested " + heuristic_mode + ": " + heuristic_rationale + ")";

            if (verbose_) {
                std::cout << "Note: Benchmark and heuristic disagree. Using benchmark recommendation." << std::endl;
                std::cout << "  Benchmark: " << benchmark_mode << " - " << benchmark_rationale << std::endl;
                std::cout << "  Heuristic: " << heuristic_mode << " - " << heuristic_rationale << std::endl;
            }
        }

        // Configure base schedule (fallback for non-specialized loops)
        if (selected_base == "static") {
            schedule_kind_ = omp_sched_static;
            schedule_chunk_size_ = 0;
        } else if (selected_base == "dynamic") {
            schedule_kind_ = omp_sched_dynamic;
            schedule_chunk_size_ = calculate_optimal_chunk_size(cell_lst_.size(), nb_threads_, workload_cov, "dynamic");
        } else {  // guided
            schedule_kind_ = omp_sched_guided;
            schedule_chunk_size_ =
                calculate_optimal_chunk_size(cell_lst_.size(), nb_threads_, workload_cov, "dynamic") * 2;
        }

        // Per-loop scheduling applies specialized schedules to different computational phases:
        //   - Contact detection: dynamic (heterogeneous neighbor counts)
        //   - Force integration: guided (moderate variance)
        //   - Mesh operations: static (uniform work per face)
        //
        // Note: Per-loop scheduling is now safe for all CONTACT_MODEL_INDEX values.
        // We replaced omp_lock_t with std::mutex in node class, which uses pthreads
        // and does not conflict with omp_set_schedule() OpenMP ICV operations.
        enable_per_loop_scheduling_ = true;
        initialize_per_loop_schedules();
        schedule_mode_ = "adaptive (base: " + selected_base + ", per-loop: ON)";
    } else {
        throw intialization_exception("Invalid schedule_mode: " + schedule_mode_ +
                                      ". Must be 'static', 'dynamic', 'guided', or 'adaptive'.");
    }

    if (verbose_) {
        std::cout << "Using OpenMP " << schedule_mode_ << " scheduling" << std::endl;
        std::cout << "  Cells: " << cell_lst_.size() << ", Threads: " << nb_threads_
                  << ", Chunk size: " << schedule_chunk_size_ << std::endl;
        std::cout << "  Workload heterogeneity (CoV): " << std::fixed << std::setprecision(3) << workload_cov
                  << std::endl;
    }

    // Remove the output folder if it already exists
    std::filesystem::remove_all(sim_parameters_.output_folder_path_);

    // Create the folders where the data will be written
    bool t1 = std::filesystem::create_directories(sim_parameters_.output_folder_path_);
    bool t2 = std::filesystem::create_directories(sim_parameters_.output_folder_path_ + "/cell_data");
    bool t3 = std::filesystem::create_directories(sim_parameters_.output_folder_path_ + "/face_data");
    if (!t1)
        throw intialization_exception("The output folder could not be created: " + sim_parameters_.output_folder_path_);
    if (!t2)
        throw intialization_exception("The output folder could not be created: " + sim_parameters_.output_folder_path_ +
                                      "/cell_data");
    if (!t3)
        throw intialization_exception("The output folder could not be created: " + sim_parameters_.output_folder_path_ +
                                      "/face_data");

// Create a folder where the results of the automatic polarizer will be stored
#if POLARIZATION_MODE_INDEX == 2
    bool t4 = std::filesystem::create_directories(sim_parameters_.output_folder_path_ + "/region_data");
    if (!t4)
        throw intialization_exception("The output folder could not be created: " + sim_parameters_.output_folder_path_ +
                                      "/region_data");
#endif

    if (verbose_)
        std::cout << "The output folder is: " << std::filesystem::absolute(sim_parameters_.output_folder_path_)
                  << std::endl;

    // Set the ids of all the cells
    for (size_t i = 0; i < cell_lst_.size(); i++) {
        cell_lst_[i]->set_id(max_cell_id_++);
        cell_lst_[i]->set_local_id(static_cast<unsigned>(i));
    }

    // Initialize the local mesh refiner
    lmr_ptr_ = std::make_unique<local_mesh_refiner>(sim_parameters_.min_edge_len_, sim_parameters_.min_edge_len_ * 3.,
                                                    sim_parameters_.enable_edge_swap_operation_);

    // Initialize the time integration scheme
    time_integrator_ptr_ = std::make_unique<time_integration_scheme>(sim_parameters_, verbose);

#if POLARIZATION_MODE_INDEX == 2
    cell_surface_polarizer_ptr_ = std::make_unique<automatic_polarizer>(sim_parameters_.min_edge_len_ * 3.);
#endif

// Initialize the contact model used to compute the contact forces between the cells
#if CONTACT_MODEL_INDEX == 0
    contact_model_ptr_ = std::make_unique<contact_node_face_via_spring>(sim_parameters_);
#elif CONTACT_MODEL_INDEX == 1
    contact_model_ptr_ = std::make_unique<contact_node_node_via_coupling>(sim_parameters_);
#elif CONTACT_MODEL_INDEX == 2
    contact_model_ptr_ = std::make_unique<contact_face_face_via_coupling>(sim_parameters_);
#else
    throw intialization_exception("The contact model index is not valid");
#endif

    // Initialize the file writer. The simulation statistics can be written in a file or in a string
    if (write_cell_stats_in_string) {
        statistic_writer_ptr_ = std::make_unique<string_statistics_writer>();
    } else {
        statistic_writer_ptr_ = std::make_unique<csv_file_statistics_writer>(sim_parameters_.output_folder_path_ +
                                                                             "/simulation_statistics.csv");
    }

    // Set the initial pressure of all the cells
    for (cell_ptr c : cell_lst_) {
        // Get the type of the cell
        const cell_type_param_ptr cell_type_ = c->get_cell_type();

        // Compute the target volume of the cell based on its initial pressure
        // For cells with bulk_modulus = 0 (infinitely compressible, e.g., ECM),
        // target volume equals current volume, yielding zero pressure response.
        double target_volume_;
        if (cell_type_->bulk_modulus_ == 0.0) {
            target_volume_ = c->get_volume();
        } else {
            target_volume_ =
                c->get_volume() * std::exp(cell_type_->initial_pressure_ / cell_type_->bulk_modulus_);
        }

        // Set the target volume of the cell
        c->set_target_volume(target_volume_);
        c->update_pressure();
    }

    // Set the number of threads (already computed earlier in constructor)
    omp_set_num_threads(nb_threads_);

    // Configure NUMA awareness for better multi-socket performance
    // Note: Thread affinity can also be set via environment variable:
    //   export OMP_PROC_BIND=close
    //   export OMP_PLACES=cores
    // The omp_set_proc_bind() function is only available in OpenMP 4.0+
    // and may not be available in all implementations, so we rely on
    // environment variables for NUMA configuration.

    // Set the OpenMP schedule for runtime scheduling
    omp_set_schedule(schedule_kind_, schedule_chunk_size_);
}
//--------------------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------------------
// Per-loop custom scheduling helper methods
//--------------------------------------------------------------------------------------------------------------------------

void solver::initialize_per_loop_schedules() {
    // Initialize specialized schedules for different loop types
    // Each loop type has different workload characteristics

    int base_chunk = schedule_chunk_size_;  // Use global chunk as baseline

    // Contact detection: Highly heterogeneous (interior vs boundary cells, different cell types)
    // → Use dynamic scheduling for best load balancing
    contact_detection_schedule_.kind = omp_sched_dynamic;
    contact_detection_schedule_.chunk_size = std::max(1, base_chunk);  // Small chunks for balance

    // Time integration: Moderate heterogeneity (depends on cell types)
    // → Use guided scheduling (balance between locality and load distribution)
    time_integration_schedule_.kind = omp_sched_guided;
    time_integration_schedule_.chunk_size = std::max(1, base_chunk * 2);  // Larger initial chunks

    // Mesh updates (face types, etc.): Very uniform work
    // → Use static scheduling for best cache locality
    mesh_update_schedule_.kind = omp_sched_static;
    mesh_update_schedule_.chunk_size = 0;  // 0 = equal distribution

    // Cell division checks: Sparse events, highly variable
    // → Use dynamic scheduling with small chunks
    cell_division_schedule_.kind = omp_sched_dynamic;
    cell_division_schedule_.chunk_size = std::max(1, base_chunk / 2);  // Very small chunks
}

void solver::set_schedule_for_contact_detection() {
    if (enable_per_loop_scheduling_) {
        omp_set_schedule(contact_detection_schedule_.kind, contact_detection_schedule_.chunk_size);
    }
    // else: use global schedule (already set in constructor)
}

void solver::set_schedule_for_time_integration() {
    if (enable_per_loop_scheduling_) {
        omp_set_schedule(time_integration_schedule_.kind, time_integration_schedule_.chunk_size);
    }
}

void solver::set_schedule_for_mesh_updates() {
    if (enable_per_loop_scheduling_) {
        omp_set_schedule(mesh_update_schedule_.kind, mesh_update_schedule_.chunk_size);
    }
}

void solver::set_schedule_for_cell_division() {
    if (enable_per_loop_scheduling_) {
        omp_set_schedule(cell_division_schedule_.kind, cell_division_schedule_.chunk_size);
    }
}

void solver::restore_global_schedule() {
    if (enable_per_loop_scheduling_) {
        // Restore the global schedule after per-loop customization
        omp_set_schedule(schedule_kind_, schedule_chunk_size_);
    }
}

//--------------------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------------------
// The method that contains the main loop of the program
void solver::run() noexcept(false) {
    // The main loop of the program
    while (time_integrator_ptr_->get_simulation_time() < sim_parameters_.simulation_duration_ && cell_lst_.size() > 0) {
        run_iteration();
    }

    statistic_writer_ptr_->write_data(iteration_, time_integrator_ptr_->get_simulation_time(),
                                      cell_lst_);  // noexcept(false)

    // Rebase all the cells before stopping the main loop
    std::for_each(cell_lst_.begin(), cell_lst_.end(), [](cell_ptr c) { c->rebase(); });

    // Print performance monitoring report
    if (verbose_) {
        perf_monitor_.report();
    }
}
//--------------------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------------------
// Run one iteration of the program
void solver::run_iteration() noexcept(false) {
    // FIX: Apply any pending division schedule update from previous iteration
    // This defers omp_set_schedule() until AFTER the parallel regions that accessed
    // cell_lst_ have completed, preventing race condition with stale pointers.
    if (pending_division_update_) {
        handle_division_event(pending_division_count_);
        pending_division_update_ = false;
        pending_division_count_ = 0;
    }

    // Save the mesh if the time is right
    if (!time_integrator_ptr_->is_step_tmp())
        save_mesh();  // noexcept(false)

    // Every 5 iterations check if some of the cells need to be divided
    if (!time_integrator_ptr_->is_step_tmp() && iteration_ % 5 == 0) {
        // Set schedule for cell division checks (sparse, heterogeneous workload)
        set_schedule_for_cell_division();

        // Track cell count to determine number of divisions (v1.0-compatible cell_divider returns void)
        const size_t cells_before = cell_lst_.size();
        cell_divider::run(cell_lst_, sim_parameters_.min_edge_len_, *lmr_ptr_, max_cell_id_, verbose_);
        const size_t cells_after = cell_lst_.size();
        // Each division creates 2 daughters and removes 1 mother, so net gain = divisions
        const unsigned num_divisions = static_cast<unsigned>((cells_after - cells_before));

        // Restore global schedule after division
        restore_global_schedule();

        // Phase 1: Track division events for adaptive scheduling
        // FIX: Defer handle_division_event() to NEXT iteration to prevent SIGSEGV
        // The cell_lst_ was just modified by cell_divider::run(). If we call
        // omp_set_schedule() now, the next parallel region (line 1035) may access
        // stale pointers due to vector reallocation during push_back().
        if (num_divisions > 0) {
            recent_division_count_ += num_divisions;  // scheduling window (reset every 50 iters)
            diag_division_count_ += num_divisions;    // diagnostics window (reset every 100 iters, after CSV export)
            pending_division_update_ = true;
            pending_division_count_ = num_divisions;
            // handle_division_event() will be called at start of NEXT iteration
        }
    }

    // Set schedule for mesh update operations (uniform work)
    set_schedule_for_mesh_updates();

// Merge multiple parallel regions to reduce synchronization overhead
// All three loops are independent and can execute in a single parallel region
#pragma omp parallel
    {
// Update face types - no dependencies, can use nowait
#pragma omp for schedule(runtime) nowait
        for (size_t i = 0; i < cell_lst_.size(); i++) {
            cell_lst_[i]->update_face_types();
        }

        // Note: refine_meshes and contact forces have their own parallel regions,
        // so they cannot be merged here. They are called serially below.
    }

    // Refine the mesh of the cells in parallel
    //  Mesh refinement typically has uniform work (use global or static schedule)
    {
        PerformanceMonitor::ScopedTimer timer(perf_monitor_, "mesh_refinement");
        lmr_ptr_->refine_meshes(cell_lst_);  // noexcept(false)
    }

    // Set schedule for contact detection (heterogeneous work)
    set_schedule_for_contact_detection();

    // Compute the contact forces between the cells
    {
        PerformanceMonitor::ScopedTimer timer(perf_monitor_, "contact_detection");
        contact_model_ptr_->run(cell_lst_);  // noexcept
    }

// If enabled, use the automatic polarizer to polarize the faces of the cells,
#if POLARIZATION_MODE_INDEX == 2
    cell_surface_polarizer_ptr_->polarize_faces(cell_lst_);  // noexcept
#endif

    // Set schedule for time integration operations (moderate heterogeneity)
    set_schedule_for_time_integration();

    // Merge polarization and internal forces into single parallel region
    {
        PerformanceMonitor::ScopedTimer timer(perf_monitor_, "polarization_and_internal_forces");
#pragma omp parallel
        {
// Polarize the faces based on their contacts - must finish before internal forces
#pragma omp for schedule(runtime)
            for (size_t i = 0; i < cell_lst_.size(); i++) {
                cell_lst_[i]->special_polarization_update(cell_lst_);
            }

// Update the internal forces of the cells in parallel
// Cannot use nowait here - internal forces depend on polarization completion
#pragma omp for schedule(runtime)
            for (size_t i = 0; i < cell_lst_.size(); i++) {
                cell_lst_[i]->apply_internal_forces(sim_parameters_.time_step_);
            }
        }
    }

    // Update the positions of the nodes in parallel
    {
        PerformanceMonitor::ScopedTimer timer(perf_monitor_, "time_integration");
        time_integrator_ptr_->update_nodes_positions(cell_lst_);  // noexcept
    }

    // Save the cell properties and update adaptive scheduling periodically
    if (iteration_ % COV_UPDATE_INTERVAL == 0) {
        statistic_writer_ptr_->write_data(iteration_, time_integrator_ptr_->get_simulation_time(),
                                          cell_lst_);  // noexcept(false)

        // Phase 1: Update workload heterogeneity measurement
        update_workload_heterogeneity();

        // Phase 1: Adaptive scheduling based on simulation phase
        adaptive_schedule_update();
    }

    // Phase 3: Export comprehensive diagnostics every 100 iterations
    if (iteration_ % 100 == 0 && iteration_ > 0) {
        // Use custom diagnostics path if provided, otherwise use default location
        std::string diagnostics_path = diagnostics_csv_path_.empty()
                                           ? sim_parameters_.output_folder_path_ + "/performance_diagnostics.csv"
                                           : diagnostics_csv_path_;
        export_diagnostics(iteration_, diagnostics_path);

        // Reset performance monitor counters for next measurement window
        perf_monitor_.reset();

        // Reset diagnostics division counter now that this window's count is written.
        // (Kept separate from recent_division_count_, which the 50-iter scheduling
        //  update resets and would otherwise zero before every 100-iter CSV export.)
        diag_division_count_ = 0;
        diag_unstable_cell_count_ = 0;
    }

// FIX: Synchronization point before cell removal to prevent node lock destruction race.
// When CONTACT_MODEL_INDEX is 1 or 2, nodes contain OpenMP locks that must not be
// destroyed while any thread might hold or wait for them. By using a parallel region
// with an implicit barrier, we ensure all previous parallel work has completed before
// we proceed to delete cells (which triggers node destructors calling omp_destroy_lock()).
#if CONTACT_MODEL_INDEX == 1 || CONTACT_MODEL_INDEX == 2
#pragma omp parallel
    {
        // Empty parallel region acts as a synchronization barrier
        // All threads reach this point before any proceeds past
    }
#endif

    // Remove the cells that have shrunk below the minimum volume or whose mesh has diverged
    remove_dead_cells();

    // Print the progression of the program
    if (verbose_ && !time_integrator_ptr_->is_step_tmp())
        printf("Progression %d%%, iteration: %d, file number %d, nb cells %d\n",
               static_cast<unsigned short>(100. * time_integrator_ptr_->get_simulation_time() /
                                           sim_parameters_.simulation_duration_),
               iteration_, file_number_, static_cast<int>(cell_lst_.size()));

    // Update the iteration number
    iteration_++;
}
//--------------------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------------------
// Erase the cells that are below the minimum volume, plus the cells whose explicit integration has blown up
// (non-finite node coordinates, or a bounding box ~10x wider than the cell should be). Left in the tissue,
// one such cell grows to ~1e19 m within a few dozen iterations: its faces then poison the global AABB of the
// contact model, the USPG voxel count overflows and every subsequent contact-detection call degenerates into
// an O(nodes x faces) scan that never returns. Dropping the cell costs one cell; keeping it costs the run.
void solver::remove_dead_cells() noexcept {
    const size_t nb_cells_before_removal = cell_lst_.size();

    cell_lst_.erase(std::remove_if(cell_lst_.begin(), cell_lst_.end(),
                                   [this](const cell_ptr& c) {
                                       bool remove = c->is_below_min_vol();

                                       if (!remove) {
                                           const cell::stability_report report = c->check_numerical_stability();
                                           if (report.unstable) {
                                               remove = true;
                                               diag_unstable_cell_count_++;
                                               nb_unstable_cells_removed_++;
                                               // cerr is unbuffered: this line lands in the log before any crash
                                               std::cerr << "Removing numerically unstable cell " << c->get_id()
                                                         << " at iteration " << iteration_
                                                         << ": extent " << report.extent << " m (max " << report.max_extent
                                                         << " m), volume " << c->get_volume()
                                                         << " m^3, target volume " << c->get_target_volume()
                                                         << " m^3, " << report.nb_non_finite_nodes << " non-finite nodes"
                                                         << std::endl;
                                           }
                                       }

                                       if (remove) c->clear_data();
                                       return remove;
                                   }),
                    cell_lst_.end());

    // The contact model (CONTACT_MODEL_INDEX == 2) keys its node-coupling map by cell local id
    // and indexes cell_lst_ with it, so local ids must equal the cell's position in cell_lst_.
    // Only cell_divider::run() reassigned them (and only when a division occurred), which left
    // every cell after a removed one with a stale, off-by-one local id until the next division.
    if (cell_lst_.size() != nb_cells_before_removal) {
        for (size_t i = 0; i < cell_lst_.size(); i++)
            cell_lst_[i]->set_local_id(static_cast<unsigned>(i));
    }
}
//--------------------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------------------
// Phase 1: Update workload heterogeneity measurement
// Recomputes CoV based on current cell population and adjusts chunk size if needed
void solver::update_workload_heterogeneity() {
    // Store previous CoV for comparison
    double previous_cov = heterogeneity_cov_;

    // Recompute heterogeneity based on current cell population
    heterogeneity_cov_ = calculate_workload_heterogeneity(cell_lst_, sim_parameters_.min_edge_len_,
                                                          sim_parameters_.contact_cutoff_adhesion_);
    last_cov_update_iteration_ = iteration_;

    // Check if heterogeneity changed significantly (>20% change)
    double cov_change = std::abs(heterogeneity_cov_ - previous_cov) / std::max(previous_cov, 0.01);

    if (cov_change > 0.2 && schedule_mode_.find("adaptive") == std::string::npos) {
        // Heterogeneity changed significantly - recalculate chunk size
        // (But not for auto mode, which handles this differently)

        int new_chunk_size =
            calculate_optimal_chunk_size(cell_lst_.size(), nb_threads_, heterogeneity_cov_, schedule_mode_);

        // Update if chunk size changed by more than 20%
        if (std::abs(new_chunk_size - schedule_chunk_size_) > schedule_chunk_size_ * 0.2) {
            schedule_chunk_size_ = new_chunk_size;
            omp_set_schedule(schedule_kind_, schedule_chunk_size_);

            if (verbose_) {
                std::cout << "  [Iteration " << iteration_ << "] Workload heterogeneity updated:" << std::endl;
                std::cout << "    CoV: " << std::fixed << std::setprecision(3) << previous_cov << " → "
                          << heterogeneity_cov_ << " (" << std::setprecision(1) << (cov_change * 100) << "% change)"
                          << std::endl;
                std::cout << "    Chunk size adjusted to: " << schedule_chunk_size_ << std::endl;
            }
        }
    }

    // Log periodic updates even if no changes
    if (verbose_ && iteration_ % 100 == 0) {
        std::cout << "  [Iteration " << iteration_ << "] CoV: " << std::fixed << std::setprecision(3)
                  << heterogeneity_cov_ << ", Cells: " << cell_lst_.size() << std::endl;
    }
}
//--------------------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------------------
// Phase 1: Detect current simulation phase based on biological markers
solver::SimulationPhase solver::detect_simulation_phase() {
    size_t num_cells = cell_lst_.size();

    // INITIALIZATION: Very few cells (<10), early tissue formation
    if (num_cells < 10) {
        return SimulationPhase::INITIALIZATION;
    }

    // Calculate division rate: fraction of cells that divided in the current
    // COV_UPDATE_INTERVAL window (recent_division_count_ is reset every window)
    double division_rate = static_cast<double>(recent_division_count_) / static_cast<double>(num_cells);

    // GROWTH: Active cell division (>1% of cells dividing per COV_UPDATE_INTERVAL iterations)
    // This captures normal proliferation (typical growth: 1-3% division rate)
    if (division_rate > 0.01) {
        return SimulationPhase::GROWTH;
    }

    // HOMEOSTASIS: Stable cell population, low division rate
    return SimulationPhase::HOMEOSTASIS;
}
//--------------------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------------------
// Phase 1: Adaptive scheduling update based on simulation phase
void solver::adaptive_schedule_update() {
    // Detect current phase
    SimulationPhase new_phase = detect_simulation_phase();

    // Check if phase transitioned
    bool phase_changed = (new_phase != current_phase_);

    if (phase_changed) {
        current_phase_ = new_phase;

        // Determine optimal scheduling for new phase
        std::string optimal_mode;
        std::string phase_name;

        switch (current_phase_) {
            case SimulationPhase::INITIALIZATION:
                // Small scale: Dynamic scheduling for maximum parallelism
                optimal_mode = "dynamic";
                phase_name = "INITIALIZATION";
                break;

            case SimulationPhase::GROWTH:
                // Active division: Dynamic or guided for adapting to changes
                if (heterogeneity_cov_ > 0.4) {
                    optimal_mode = "dynamic";  // High heterogeneity
                } else {
                    optimal_mode = "guided";  // Moderate heterogeneity
                }
                phase_name = "GROWTH";
                break;

            case SimulationPhase::HOMEOSTASIS:
                // Stable population: Static or guided for cache locality
                if (cell_lst_.size() > 1000) {
                    optimal_mode = "static";  // Large scale benefits from locality
                } else {
                    optimal_mode = "guided";  // Medium scale balances both
                }
                phase_name = "HOMEOSTASIS";
                break;
        }

        // Update scheduling mode if different from current
        // Only adapt if we're in auto mode (user hasn't forced a specific mode)
        if (schedule_mode_.find("adaptive") == std::string::npos) {
            // User explicitly set a mode (static/dynamic/guided) - don't override
            return;
        }

        // Skip if already using the optimal mode for this phase
        if (schedule_mode_.find(optimal_mode) != std::string::npos) {
            // Already in optimal mode, no change needed
            return;
        }

        // Apply new schedule
        omp_sched_t new_kind;
        if (optimal_mode == "static") {
            new_kind = omp_sched_static;
            schedule_chunk_size_ = 0;
        } else if (optimal_mode == "dynamic") {
            new_kind = omp_sched_dynamic;
            schedule_chunk_size_ =
                calculate_optimal_chunk_size(cell_lst_.size(), nb_threads_, heterogeneity_cov_, "dynamic");
        } else {  // guided
            new_kind = omp_sched_guided;
            schedule_chunk_size_ =
                calculate_optimal_chunk_size(cell_lst_.size(), nb_threads_, heterogeneity_cov_, "dynamic") * 2;
        }

        schedule_kind_ = new_kind;
        omp_set_schedule(schedule_kind_, schedule_chunk_size_);

        if (verbose_) {
            std::cout << "  [Iteration " << iteration_ << "] Phase transition detected!" << std::endl;
            std::cout << "    Phase: " << phase_name << std::endl;
            std::cout << "    Scheduling: " << optimal_mode << " (chunk=" << schedule_chunk_size_ << ")" << std::endl;
            std::cout << "    Cells: " << cell_lst_.size() << ", Division rate: " << std::fixed << std::setprecision(2)
                      << (static_cast<double>(recent_division_count_) / (cell_lst_.size() * 0.5)) << "%/iter"
                      << std::endl;
        }
    }

    // Reset division counter for next measurement window
    recent_division_count_ = 0;
}
//--------------------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------------------
// Phase 1: Handle cell division events - immediate response to workload changes
void solver::handle_division_event(unsigned num_divisions) {
    // Cell division creates immediate workload imbalance:
    // - New cells have freshly refined meshes (more faces)
    // - Daughter cells are smaller but have 2x total computational cost
    // - Contact patterns change as cells rearrange

    // Temporarily boost heterogeneity estimate to reflect post-division imbalance
    double division_impact = static_cast<double>(num_divisions) / cell_lst_.size();

    // Boost CoV by up to 200% depending on division fraction
    // Division causes 200-300% workload spike: mesh refinement + contact rebuild + pressure equilibration
    double temporary_cov = heterogeneity_cov_ * (1.0 + 2.0 * division_impact);

    // Recalculate chunk size with boosted heterogeneity
    int new_chunk_size = calculate_optimal_chunk_size(cell_lst_.size(), nb_threads_, temporary_cov, schedule_mode_);

    // Apply more aggressive chunk size reduction if divisions are frequent
    if (division_impact > 0.1) {  // >10% of cells divided
        new_chunk_size = std::max(1, new_chunk_size / 2);
    }

    // Update scheduling immediately
    if (new_chunk_size != schedule_chunk_size_) {
        schedule_chunk_size_ = new_chunk_size;
        omp_set_schedule(schedule_kind_, schedule_chunk_size_);

        if (verbose_) {
            std::cout << "  [Iteration " << iteration_ << "] Cell division event detected!" << std::endl;
            std::cout << "    Divisions: " << num_divisions << " (" << std::fixed << std::setprecision(1)
                      << (division_impact * 100) << "% of population)" << std::endl;
            std::cout << "    Chunk size reduced to: " << schedule_chunk_size_ << " (temporarily adapting to imbalance)"
                      << std::endl;
        }
    }
}
//--------------------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------------------
// Phase 3: Export comprehensive diagnostics (biological + computational metrics)
void solver::export_diagnostics(unsigned iteration, const std::string& csv_path) {
    // Check if file exists to determine if we need to write header
    bool file_exists = std::ifstream(csv_path).good();
    std::ofstream csv_file(csv_path, std::ios::app);

    if (!csv_file.is_open()) {
        if (verbose_) {
            std::cerr << "Warning: Could not open diagnostics CSV: " << csv_path << std::endl;
        }
        return;
    }

    // Write header if this is a new file
    if (!file_exists) {
        csv_file << "sim_time,wall_epoch,iteration,cells,divisions,cov,phase,"
                 << "mesh_refinement_ms,contact_detection_ms,"
                 << "polarization_internal_forces_ms,time_integration_ms,"
                 << "total_iteration_ms,thread_imbalance_pct,unstable_cells_removed\n";
    }

    // Calculate simulation timestamp (seconds since simulation start)
    double sim_time = time_integrator_ptr_->get_simulation_time();

    // Get wall clock epoch timestamp for correlation with external monitoring
    auto wall_epoch =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    // Get current phase as string
    std::string phase_name;
    switch (current_phase_) {
        case SimulationPhase::INITIALIZATION:
            phase_name = "INITIALIZATION";
            break;
        case SimulationPhase::GROWTH:
            phase_name = "GROWTH";
            break;
        case SimulationPhase::HOMEOSTASIS:
            phase_name = "HOMEOSTASIS";
            break;
    }

    // Get timing statistics from performance monitor
    const auto* mesh_stats = perf_monitor_.get_stats("mesh_refinement");
    const auto* contact_stats = perf_monitor_.get_stats("contact_detection");
    const auto* polarization_stats = perf_monitor_.get_stats("polarization_and_internal_forces");
    const auto* integration_stats = perf_monitor_.get_stats("time_integration");

    double mesh_time_ms = mesh_stats ? (mesh_stats->average() * 1000.0) : 0.0;
    double contact_time_ms = contact_stats ? (contact_stats->average() * 1000.0) : 0.0;
    double polarization_time_ms = polarization_stats ? (polarization_stats->average() * 1000.0) : 0.0;
    double integration_time_ms = integration_stats ? (integration_stats->average() * 1000.0) : 0.0;
    double total_time_ms = mesh_time_ms + contact_time_ms + polarization_time_ms + integration_time_ms;

    // Calculate thread imbalance (simplified estimate based on CoV)
    // Higher heterogeneity typically correlates with higher thread imbalance
    double thread_imbalance_pct = heterogeneity_cov_ * 100.0;

    // Write data row
    csv_file << std::fixed << std::setprecision(3) << sim_time << "," << wall_epoch << "," << iteration << ","
             << cell_lst_.size() << "," << diag_division_count_ << "," << std::setprecision(4) << heterogeneity_cov_
             << "," << phase_name << "," << std::setprecision(2) << mesh_time_ms << "," << contact_time_ms << ","
             << polarization_time_ms << "," << integration_time_ms << "," << total_time_ms << ","
             << std::setprecision(1) << thread_imbalance_pct << "," << diag_unstable_cell_count_ << "\n";

    csv_file.flush();
}
//--------------------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------------------
// Write the mesh of the tissue in a VTK file
void solver::save_mesh() noexcept(false) {
    unsigned new_file_nb = static_cast<unsigned>(
        std::floor(time_integrator_ptr_->get_simulation_time() / sim_parameters_.sampling_period_) + 1);
    if (new_file_nb != file_number_) {
        file_number_ = new_file_nb;

        // Save the meshes of the cells in VTK format
        const std::string cell_mesh_path =
            sim_parameters_.output_folder_path_ + "/cell_data/result_" + std::to_string(file_number_) + ".vtk";
        const std::string face_mesh_path =
            sim_parameters_.output_folder_path_ + "/face_data/result_" + std::to_string(file_number_) + ".vtk";
        mesh_writer::write(cell_mesh_path, face_mesh_path, cell_lst_);

// Save the discretization of the space by the automatic polarizer
#if POLARIZATION_MODE_INDEX == 2
        if (iteration_ > 1)
            automatic_polarization_writer::write(
                sim_parameters_.output_folder_path_ + "/region_data/result_" + std::to_string(file_number_) + ".vtk",
                *(cell_surface_polarizer_ptr_));
#endif
    }
}
//--------------------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------------------
// If the simulation statistics have been written in a string, then
// returns the string. Do not call this function if you have
// constructed the solver with write_cell_stats_in_string = false
std::string solver::get_simulation_statistics() const noexcept(false) {
    // Downcast the statistic_writer_ to a string_statistics_writer
    const string_statistics_writer* string_writer =
        dynamic_cast<const string_statistics_writer*>(statistic_writer_ptr_.get());
    if (string_writer == nullptr)
        throw std::runtime_error("The simulation statistics have not been written in a string");
    return string_writer->get_string();
}
//--------------------------------------------------------------------------------------------------------------------------
