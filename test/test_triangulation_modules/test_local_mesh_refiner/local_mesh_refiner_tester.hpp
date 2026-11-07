#ifndef DEF_LOCAL_MESH_REFINER_TESTER
#define DEF_LOCAL_MESH_REFINER_TESTER

#include <cassert>
#include <string>
#include <type_traits>
#include <vector>
#include <algorithm>
#include <functional>

#include "local_mesh_refiner.hpp"
#include "mesh_writer.hpp"

#include "mesh_reader.hpp"



class local_mesh_refiner_tester
{   
    public:

        int can_be_merged_test();
        int edge_swap_test();
        int split_edge_test();
        int merge_edge_test();
        int get_triangle_score_test();
        int refine_mesh_keeps_winding_consistent_test();
        int winding_self_check_test();

        //refine_meshes(): one cell's failure is contained and reported, the others are refined as if alone
        int refine_meshes_contains_failure_test();
        int refine_meshes_contains_failure_threads_test();
        int refine_meshes_rethrows_fatal_test();
        int refine_meshes_public_wrapper_test();

};

#endif