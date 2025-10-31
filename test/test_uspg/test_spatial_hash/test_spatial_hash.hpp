#ifndef DEF_TEST_SPATIAL_HASH
#define DEF_TEST_SPATIAL_HASH

#include <numeric>
#include <algorithm>
#include <vector>

#include "spatial_hash.hpp"
#include "morton_encoding.hpp"

/**
 * @brief Test class for spatial_hash implementation
 *
 * Tests verify:
 * 1. Core functionality (insertion, retrieval, neighborhood queries)
 * 2. API compatibility with uspg_4d
 * 3. Morton encoding correctness
 * 4. Sparse storage efficiency
 * 5. Boundary conditions
 * 6. Thread safety (if applicable)
 */
class spatial_hash_tester
{
public:
    spatial_hash_tester() = default;

    // Core USPG compatibility tests (matching uspg_4d test suite)
    int update_dimensions_test() const;
    int place_object_test() const;
    int get_neighborhood_test() const;
    int get_grid_content_test() const;

    // Spatial hash specific tests
    int morton_encoding_test() const;
    int boundary_conditions_test() const;
    int sparse_storage_test() const;
};

#endif
