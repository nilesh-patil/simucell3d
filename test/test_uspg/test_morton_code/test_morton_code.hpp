#ifndef DEF_TEST_MORTON_CODE
#define DEF_TEST_MORTON_CODE

#include "morton_code.hpp"


class morton_code_tester
{

    public:
        morton_code_tester() = default;

        // Test Morton code encoding produces expected values
        int morton_encode_test() const;

        // Test spatial locality - nearby points have similar codes
        int morton_spatial_locality_test() const;

        // Test boundary conditions (0,0,0), (1,1,1)
        int morton_boundary_test() const;

        // Test that Morton sorting doesn't change neighborhood results
        int morton_sorting_preserves_correctness_test() const;

};

#endif
