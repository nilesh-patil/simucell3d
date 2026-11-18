#include <cassert>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "cpu_topology.hpp"

using cpu_topology::logical_cpu;
using cpu_topology::topology;

static bool same(const std::vector<int>& a, const std::vector<int>& b) { return a == b; }

//---------------------------------------------------------------------------------------------------------
// Kernel cpu-list syntax: single ids, ranges, mixed, whitespace, garbage
int parse_cpu_list_test() {
    bool t1 = same(cpu_topology::parse_cpu_list("0,4"), {0, 4});
    bool t2 = same(cpu_topology::parse_cpu_list("0-3"), {0, 1, 2, 3});
    bool t3 = same(cpu_topology::parse_cpu_list("0-1,4-5"), {0, 1, 4, 5});
    bool t4 = same(cpu_topology::parse_cpu_list(" 7 , 2 \n"), {2, 7});
    bool t5 = same(cpu_topology::parse_cpu_list(""), {});
    bool t6 = same(cpu_topology::parse_cpu_list("x,3"), {3});
    std::cout << "  t1..t6: " << t1 << t2 << t3 << t4 << t5 << t6 << std::endl;
    return !(t1 && t2 && t3 && t4 && t5 && t6);
}

//---------------------------------------------------------------------------------------------------------
// The layout of the machine that exposed the bug: 4 cores x 2 hyperthreads, siblings (0,4),(1,5),(2,6),(3,7).
// Every thread up to 4 must land on a different core; threads 5..8 fill in the siblings.
int placement_4c8t_test() {
    topology topo;
    for (int id = 0; id < 8; id++) topo.cpus.push_back({id, 0, id % 4});

    bool t1 = topo.nb_physical_cores() == 4;
    bool t2 = topo.threads_per_core() == 2;
    bool t3 = topo.nb_packages() == 1;
    std::vector<int> order = cpu_topology::placement_order(topo);
    bool t4 = same(order, {0, 1, 2, 3, 4, 5, 6, 7});
    std::cout << "  t1..t4: " << t1 << t2 << t3 << t4 << std::endl;
    return !(t1 && t2 && t3 && t4);
}

//---------------------------------------------------------------------------------------------------------
// Sibling ids need not be lowest-first or contiguous (e.g. "0-1" style enumeration: core k = cpus 2k, 2k+1)
int placement_adjacent_siblings_test() {
    topology topo;
    for (int id = 0; id < 8; id++) topo.cpus.push_back({id, 0, id / 2});

    bool t1 = topo.nb_physical_cores() == 4;
    std::vector<int> order = cpu_topology::placement_order(topo);
    bool t2 = same(order, {0, 2, 4, 6, 1, 3, 5, 7});
    std::cout << "  t1..t2: " << t1 << t2 << std::endl;
    return !(t1 && t2);
}

//---------------------------------------------------------------------------------------------------------
// taskset -c 0-3 on the 4c8t machine leaves 4 distinct cores with one sibling each: all 4 usable.
// taskset -c 0,4 leaves one core with both siblings: 1 physical core, order = [0, 4].
int placement_restricted_test() {
    topology quarter;
    for (int id : {0, 1, 2, 3}) quarter.cpus.push_back({id, 0, id % 4});
    bool t1 = quarter.nb_physical_cores() == 4 && quarter.threads_per_core() == 1;
    bool t2 = same(cpu_topology::placement_order(quarter), {0, 1, 2, 3});

    topology one_core;
    for (int id : {0, 4}) one_core.cpus.push_back({id, 0, 0});
    bool t3 = one_core.nb_physical_cores() == 1 && one_core.threads_per_core() == 2;
    bool t4 = same(cpu_topology::placement_order(one_core), {0, 4});
    std::cout << "  t1..t4: " << t1 << t2 << t3 << t4 << std::endl;
    return !(t1 && t2 && t3 && t4);
}

//---------------------------------------------------------------------------------------------------------
// Two sockets, 2 cores each, HT: consecutive threads alternate sockets, cores before siblings.
// core_id restarts at 0 on each package, so (package, core) must be the key.
int placement_two_sockets_test() {
    topology topo;
    // package 0: cpus 0,1 (cores 0,1) + siblings 4,5 ; package 1: cpus 2,3 (cores 0,1) + siblings 6,7
    topo.cpus = {{0, 0, 0}, {1, 0, 1}, {2, 1, 0}, {3, 1, 1}, {4, 0, 0}, {5, 0, 1}, {6, 1, 0}, {7, 1, 1}};

    bool t1 = topo.nb_physical_cores() == 4;
    bool t2 = topo.nb_packages() == 2;
    std::vector<int> order = cpu_topology::placement_order(topo);
    bool t3 = same(order, {0, 2, 1, 3, 4, 6, 5, 7});
    std::cout << "  t1..t3: " << t1 << t2 << t3 << std::endl;
    return !(t1 && t2 && t3);
}

//---------------------------------------------------------------------------------------------------------
// Live check on this machine: allowed_cpus() must be non-empty and consistent with sysfs detection,
// and the placement order must be a permutation of the allowed CPUs.
int live_detection_test() {
    std::vector<int> allowed = cpu_topology::allowed_cpus();
    topology topo = cpu_topology::detect(allowed);
    std::vector<int> order = cpu_topology::placement_order(topo);

    bool t1 = !allowed.empty();
    bool t2 = topo.cpus.size() == allowed.size();
    bool t3 = topo.nb_physical_cores() >= 1 && topo.nb_physical_cores() <= static_cast<int>(allowed.size());
    std::set<int> order_set(order.begin(), order.end());
    bool t4 = order.size() == allowed.size() && order_set == std::set<int>(allowed.begin(), allowed.end());
    std::cout << "  allowed=" << allowed.size() << " cores=" << topo.nb_physical_cores()
              << " threads/core=" << topo.threads_per_core() << std::endl;
    std::cout << "  t1..t4: " << t1 << t2 << t3 << t4 << std::endl;
    return !(t1 && t2 && t3 && t4);
}

//---------------------------------------------------------------------------------------------------------
int main(int argc, char** argv) {
    assert(argc == 2);
    std::string test_name = argv[1];

    if (test_name == "parse_cpu_list_test") return parse_cpu_list_test();
    if (test_name == "placement_4c8t_test") return placement_4c8t_test();
    if (test_name == "placement_adjacent_siblings_test") return placement_adjacent_siblings_test();
    if (test_name == "placement_restricted_test") return placement_restricted_test();
    if (test_name == "placement_two_sockets_test") return placement_two_sockets_test();
    if (test_name == "live_detection_test") return live_detection_test();

    std::cout << "TEST NAME :" << test_name << " DOES NOT EXIST" << std::endl;
    return 1;
}
//---------------------------------------------------------------------------------------------------------
