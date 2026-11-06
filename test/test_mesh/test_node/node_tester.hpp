#ifndef DEF_NODE_TESTER
#define DEF_NODE_TESTER

#include <cassert>
#include <string>
#include <type_traits>
#include <vector>
#include <algorithm>
#include <functional>
#include <random>
#include <numeric>
#include <omp.h>

#include "node.hpp"
#include "vec3.hpp"
#include "cell.hpp"



class node_tester
{   
    public:
        int default_constructors_test();
        int trivial_instantiation_test();
        int node_reset_test();
        int reset_local_id_test();
        int set_is_used_test();
        int operator_minus_test();
        int coupling_compare_and_set_test();
        int couple_if_closest_test();
        int concurrent_coupling_test();
        int atomic_force_test();

};

#endif