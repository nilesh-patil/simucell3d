#ifndef DEF_CPU_TOPOLOGY
#define DEF_CPU_TOPOLOGY

/**
 * @file
 * @brief CPU topology detection and OpenMP thread pinning.
 *
 * Answers three questions: which logical CPUs the process may use, how they group into
 * physical cores, and which CPU OpenMP thread i should run on. The solver uses these
 * functions to run one thread per physical core and to pin each thread to its CPU.
 *
 * The solver pins threads itself instead of relying on OMP_PROC_BIND/OMP_PLACES because
 * libgomp reads those variables when the library is loaded, before main(), so a setenv()
 * from the solver constructor has no effect. In addition, with OMP_PROC_BIND=close but no
 * OMP_PLACES, libgomp enumerates its default place list core by core (0,4,1,5,... on a
 * 4-core/8-thread machine), so N threads end up on N/2 physical cores.
 *
 * @ingroup runtime
 */

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <omp.h>

#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#endif

/**
 * @brief Detection of logical CPUs and physical cores, and thread pinning.
 *
 * Header-only. Topology information is read from Linux sysfs; on other systems every
 * logical CPU is treated as its own core and pinning is not available.
 *
 * @ingroup runtime
 */
namespace cpu_topology {

/**
 * @brief One logical CPU and the physical core it belongs to.
 * @ingroup runtime
 */
struct logical_cpu {
    int id;       ///< Logical CPU number as seen by the kernel.
    int package;  ///< physical_package_id (socket).
    int core;     ///< core_id, unique within a package only.
};

/**
 * @brief The logical CPUs available to the process, grouped by package and core.
 * @ingroup runtime
 */
struct topology {
    std::vector<logical_cpu> cpus;  ///< The CPUs this process may run on, in ascending id order.

    /** @brief Number of distinct (package, core) pairs, i.e. physical cores. */
    int nb_physical_cores() const {
        std::set<std::pair<int, int>> cores;
        for (const logical_cpu& c : cpus) cores.insert({c.package, c.core});
        return static_cast<int>(cores.size());
    }

    /** @brief Number of distinct packages (sockets). */
    int nb_packages() const {
        std::set<int> packages;
        for (const logical_cpu& c : cpus) packages.insert(c.package);
        return static_cast<int>(packages.size());
    }

    /** @brief Largest number of allowed hyperthread siblings sharing one physical core. */
    int threads_per_core() const {
        std::map<std::pair<int, int>, int> count;
        int max_count = 0;
        for (const logical_cpu& c : cpus) max_count = std::max(max_count, ++count[{c.package, c.core}]);
        return max_count;
    }
};

/**
 * @brief Parse a kernel CPU list such as "0,4", "0-3" or "0-1,4-5".
 * @param text CPU list in the kernel's list format.
 * @return Sorted, unique CPU ids. Malformed tokens are ignored.
 */
inline std::vector<int> parse_cpu_list(const std::string& text) {
    std::set<int> ids;
    std::string token;
    auto flush = [&]() {
        if (token.empty()) return;
        const size_t dash = token.find('-');
        try {
            if (dash == std::string::npos) {
                ids.insert(std::stoi(token));
            } else {
                const int lo = std::stoi(token.substr(0, dash));
                const int hi = std::stoi(token.substr(dash + 1));
                for (int i = lo; i <= hi; i++) ids.insert(i);
            }
        } catch (...) {
            // Ignore malformed tokens: an unparsable sysfs entry must not abort a simulation
        }
        token.clear();
    };
    for (char ch : text) {
        if (ch == ',') {
            flush();
        } else if (!isspace(static_cast<unsigned char>(ch))) {
            token.push_back(ch);
        }
    }
    flush();
    return std::vector<int>(ids.begin(), ids.end());
}

/**
 * @brief Order in which OpenMP threads are assigned to logical CPUs.
 *
 * The lowest-numbered CPU of every physical core comes first, so that up to
 * topology::nb_physical_cores() threads never share a core; the remaining hyperthread
 * siblings follow. Cores are taken round-robin across packages, so consecutive threads on a
 * multi-socket host are spread over the sockets' memory controllers.
 *
 * @param topo Topology returned by detect().
 * @return Logical CPU ids; thread i is pinned to element i.
 */
inline std::vector<int> placement_order(const topology& topo) {
    std::vector<logical_cpu> cpus = topo.cpus;
    std::sort(cpus.begin(), cpus.end(), [](const logical_cpu& a, const logical_cpu& b) { return a.id < b.id; });

    // package -> list of cores (in ascending id order) -> list of sibling ids
    std::map<int, std::vector<std::vector<int>>> cores_by_package;
    std::map<std::pair<int, int>, size_t> core_slot;
    for (const logical_cpu& c : cpus) {
        auto key = std::make_pair(c.package, c.core);
        auto it = core_slot.find(key);
        if (it == core_slot.end()) {
            core_slot[key] = cores_by_package[c.package].size();
            cores_by_package[c.package].push_back({c.id});
        } else {
            cores_by_package[c.package][it->second].push_back(c.id);
        }
    }

    // Interleave packages: core 0 of package 0, core 0 of package 1, core 1 of package 0, ...
    std::vector<std::vector<int>> cores;
    for (size_t slot = 0;; slot++) {
        bool any = false;
        for (auto& [package, package_cores] : cores_by_package) {
            if (slot < package_cores.size()) {
                cores.push_back(package_cores[slot]);
                any = true;
            }
        }
        if (!any) break;
    }

    // Representatives first, then second siblings, then third, ...
    std::vector<int> order;
    for (size_t rank = 0;; rank++) {
        bool any = false;
        for (const std::vector<int>& siblings : cores) {
            if (rank < siblings.size()) {
                order.push_back(siblings[rank]);
                any = true;
            }
        }
        if (!any) break;
    }
    return order;
}

/**
 * @brief Read one integer sysfs attribute.
 * @param path File to read.
 * @param fallback Value returned when the file cannot be read.
 * @return The integer in the file, or @p fallback.
 */
inline int read_sysfs_int(const std::string& path, int fallback) {
    std::ifstream file(path);
    int value;
    if (file.is_open() && (file >> value)) return value;
    return fallback;
}

/**
 * @brief Group logical CPUs into packages and cores using sysfs.
 *
 * Without sysfs (non-Linux systems, containers with a masked /sys) every CPU is treated as
 * its own core in package 0.
 *
 * @param allowed_cpus Logical CPU ids, usually allowed_cpus().
 * @return The topology of those CPUs.
 */
inline topology detect(const std::vector<int>& allowed_cpus) {
    topology topo;
    for (int id : allowed_cpus) {
        const std::string dir = "/sys/devices/system/cpu/cpu" + std::to_string(id) + "/topology/";
        topo.cpus.push_back({id, read_sysfs_int(dir + "physical_package_id", 0), read_sysfs_int(dir + "core_id", id)});
    }
    return topo;
}

/**
 * @brief Query the logical CPUs this process may run on.
 *
 * If libgomp built a place list (OMP_PROC_BIND or OMP_PLACES set in the environment), the
 * main thread is already pinned to a single CPU when main() runs and sched_getaffinity()
 * would under-report; the place list is then the only record of the original taskset or
 * cgroup mask, and it is used first. Otherwise the affinity mask is used, and as a last
 * resort all omp_get_num_procs() CPUs.
 *
 * @return Sorted logical CPU ids.
 * @see allowed_cpus(), which caches the result.
 */
inline std::vector<int> query_allowed_cpus() {
    std::set<int> ids;
    const int nb_places = omp_get_num_places();
    for (int p = 0; p < nb_places; p++) {
        std::vector<int> procs(std::max(0, omp_get_place_num_procs(p)));
        if (!procs.empty()) {
            omp_get_place_proc_ids(p, procs.data());
            ids.insert(procs.begin(), procs.end());
        }
    }
#ifdef __linux__
    if (ids.empty()) {
        cpu_set_t mask;
        CPU_ZERO(&mask);
        if (sched_getaffinity(0, sizeof(mask), &mask) == 0) {
            for (int i = 0; i < CPU_SETSIZE; i++) {
                if (CPU_ISSET(i, &mask)) ids.insert(i);
            }
        }
    }
#endif
    if (ids.empty()) {
        for (int i = 0; i < omp_get_num_procs(); i++) ids.insert(i);
    }
    return std::vector<int>(ids.begin(), ids.end());
}

/**
 * @brief The logical CPUs this process may run on, captured once on first use.
 *
 * The value is cached because, after the first solver has pinned the calling thread,
 * sched_getaffinity() reports a single CPU, and every later solver built in the same
 * process (tests, Python bindings, benchmark drivers) would otherwise use one thread.
 *
 * @return Reference to the cached result of query_allowed_cpus().
 */
inline const std::vector<int>& allowed_cpus() {
    static const std::vector<int> cached = query_allowed_cpus();
    return cached;
}

/**
 * @brief Whether the solver should pin its OpenMP threads.
 *
 * Pinning is on by default. It is disabled by SIMUCELL3D_PIN_THREADS=0, and by any
 * OMP_PLACES setting, which is taken as a placement chosen by the user.
 *
 * @return true if threads should be pinned.
 */
inline bool pinning_requested() {
    const char* pin_env = std::getenv("SIMUCELL3D_PIN_THREADS");
    if (pin_env != nullptr && std::string(pin_env) == "0") return false;
    return std::getenv("OMP_PLACES") == nullptr;
}

/**
 * @brief Restrict the calling thread to one logical CPU.
 * @param cpu_id Logical CPU id.
 * @return false if the operating system refused, or on non-Linux systems.
 */
inline bool pin_current_thread(int cpu_id) {
#ifdef __linux__
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpu_id, &mask);
    return pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) == 0;
#else
    (void)cpu_id;
    return false;
#endif
}

/**
 * @brief The single CPU the calling thread is restricted to.
 * @return The logical CPU id, or -1 if the thread may run on several CPUs or the query is
 *         not supported.
 */
inline int current_thread_cpu() {
#ifdef __linux__
    cpu_set_t mask;
    CPU_ZERO(&mask);
    if (pthread_getaffinity_np(pthread_self(), sizeof(mask), &mask) != 0) return -1;
    if (CPU_COUNT(&mask) != 1) return -1;
    for (int i = 0; i < CPU_SETSIZE; i++) {
        if (CPU_ISSET(i, &mask)) return i;
    }
#endif
    return -1;
}

}  // namespace cpu_topology

#endif
