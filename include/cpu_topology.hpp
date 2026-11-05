#ifndef DEF_CPU_TOPOLOGY
#define DEF_CPU_TOPOLOGY

// Which logical CPUs may this process use, how do they group into physical cores,
// and which CPU should OpenMP thread i run on.
//
// Why the solver pins threads itself instead of relying on OMP_PROC_BIND/OMP_PLACES:
// libgomp reads those variables when the library is loaded, i.e. before main(), so a
// setenv() from the solver constructor is a no-op. And with OMP_PROC_BIND=close but no
// OMP_PLACES, libgomp enumerates its default place list core-by-core (0,4,1,5,... on a
// 4-core/8-thread machine), so N threads end up on N/2 physical cores.

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

namespace cpu_topology {

struct logical_cpu {
    int id;       // logical CPU number as seen by the kernel
    int package;  // physical_package_id (socket)
    int core;     // core_id, unique within a package only
};

struct topology {
    std::vector<logical_cpu> cpus;  // the CPUs this process may run on, ascending id

    int nb_physical_cores() const {
        std::set<std::pair<int, int>> cores;
        for (const logical_cpu& c : cpus) cores.insert({c.package, c.core});
        return static_cast<int>(cores.size());
    }

    int nb_packages() const {
        std::set<int> packages;
        for (const logical_cpu& c : cpus) packages.insert(c.package);
        return static_cast<int>(packages.size());
    }

    // Largest number of allowed hyperthread siblings sharing one core
    int threads_per_core() const {
        std::map<std::pair<int, int>, int> count;
        int max_count = 0;
        for (const logical_cpu& c : cpus) max_count = std::max(max_count, ++count[{c.package, c.core}]);
        return max_count;
    }
};

// Parse a kernel cpu list such as "0,4", "0-3" or "0-1,4-5" into sorted unique ids.
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

// Thread placement order: the lowest-numbered CPU of every physical core first (so up to
// nb_physical_cores() threads never share a core), then the remaining hyperthread
// siblings. Cores are taken round-robin across packages so that consecutive threads on a
// multi-socket host spread over the sockets' memory controllers.
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

// Read one integer sysfs attribute; returns fallback when unavailable.
inline int read_sysfs_int(const std::string& path, int fallback) {
    std::ifstream file(path);
    int value;
    if (file.is_open() && (file >> value)) return value;
    return fallback;
}

// Group the given logical CPUs into packages/cores using sysfs. Without sysfs (non-Linux,
// containers with a masked /sys) every CPU is treated as its own core.
inline topology detect(const std::vector<int>& allowed_cpus) {
    topology topo;
    for (int id : allowed_cpus) {
        const std::string dir = "/sys/devices/system/cpu/cpu" + std::to_string(id) + "/topology/";
        topo.cpus.push_back({id, read_sysfs_int(dir + "physical_package_id", 0), read_sysfs_int(dir + "core_id", id)});
    }
    return topo;
}

// The logical CPUs this process may run on. If libgomp built a place list (any
// OMP_PROC_BIND/OMP_PLACES in the environment) the main thread is already pinned to a
// single CPU by the time main() runs, so sched_getaffinity() would under-report; the place
// list is then the only record of the original taskset/cgroup mask.
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

// Captured once, on first use: after the first solver has pinned the calling thread,
// sched_getaffinity() reports a single CPU, and every later solver built in the same
// process (tests, Python bindings, benchmark drivers) would collapse to one thread.
inline const std::vector<int>& allowed_cpus() {
    static const std::vector<int> cached = query_allowed_cpus();
    return cached;
}

// Pinning is on by default; SIMUCELL3D_PIN_THREADS=0 disables it, and an explicit
// OMP_PLACES means the user has chosen a placement that must be respected.
inline bool pinning_requested() {
    const char* pin_env = std::getenv("SIMUCELL3D_PIN_THREADS");
    if (pin_env != nullptr && std::string(pin_env) == "0") return false;
    return std::getenv("OMP_PLACES") == nullptr;
}

// Restrict the calling thread to one logical CPU. Returns false if the OS refused.
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

// The single CPU the calling thread is currently restricted to, or -1 if it may run on
// several (or the query is unsupported).
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
