#ifndef DEF_TEST_USPG_4D
#define DEF_TEST_USPG_4D

#include <numeric>

#include "uspg_4d.hpp"



class uspg_4d_tester
{

    public:
        uspg_4d_tester() = default;

        // Existing tests
        int update_dimensions_test() const;
        int place_object_test() const;
        int get_neighborhood_test() const;
        int get_grid_content_test() const;

        // New tests for vector storage optimization (Phase 1)
        int vector_storage_size_test() const;           // Verify .size() works correctly
        int vector_push_back_ordering_test() const;     // Verify push_back maintains insertion order
        int empty_voxel_returns_empty_vector_test() const;  // Verify empty voxels return empty vector
        int multiple_insertions_same_voxel_test() const;    // Verify multiple insertions work correctly
        int neighborhood_returns_vector_test() const;       // Verify get_neighborhood returns vector with correct elements

};

#endif