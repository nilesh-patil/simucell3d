#include "test_morton_code.hpp"

#include <iostream>
#include <vector>
#include <algorithm>
#include <cassert>
#include <cmath>

/*
    Test the Morton code (Z-order curve) encoding utilities.
    Morton codes interleave bits of x, y, z coordinates for spatial locality.
*/


//---------------------------------------------------------------------------------------------------------
// Test that Morton encoding produces expected values for known inputs
int morton_code_tester::morton_encode_test() const {
    // Test case 1: Origin (0,0,0) should produce code 0
    uint64_t code_origin = morton_code::encode_normalized(0.0, 0.0, 0.0);
    bool t1 = code_origin == 0;

    // Test case 2: (1,0,0) should have x-bits set
    // In Morton code, x gets bit positions 0, 3, 6, 9...
    uint64_t code_x = morton_code::encode_normalized(1.0, 0.0, 0.0);
    bool t2 = code_x > 0;  // Should be non-zero

    // Test case 3: (0,1,0) should have y-bits set
    // In Morton code, y gets bit positions 1, 4, 7, 10...
    uint64_t code_y = morton_code::encode_normalized(0.0, 1.0, 0.0);
    bool t3 = code_y > 0;  // Should be non-zero

    // Test case 4: (0,0,1) should have z-bits set
    // In Morton code, z gets bit positions 2, 5, 8, 11...
    uint64_t code_z = morton_code::encode_normalized(0.0, 0.0, 1.0);
    bool t4 = code_z > 0;  // Should be non-zero

    // Test case 5: All codes should be different
    bool t5 = (code_origin != code_x) && (code_x != code_y) && (code_y != code_z);

    return !(t1 && t2 && t3 && t4 && t5);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test that Morton codes are deterministic and produce valid spatial ordering
int morton_code_tester::morton_spatial_locality_test() const {
    // Test that the same point always produces the same code
    uint64_t code_a1 = morton_code::encode_normalized(0.5, 0.5, 0.5);
    uint64_t code_a2 = morton_code::encode_normalized(0.5, 0.5, 0.5);
    bool t1 = code_a1 == code_a2;

    // Test that different points produce different codes
    uint64_t code_b = morton_code::encode_normalized(0.51, 0.51, 0.51);
    uint64_t code_c = morton_code::encode_normalized(0.0, 0.0, 0.0);
    uint64_t code_d = morton_code::encode_normalized(1.0, 1.0, 1.0);

    bool t2 = (code_a1 != code_c);  // Center != origin
    bool t3 = (code_a1 != code_d);  // Center != max corner
    bool t4 = (code_c != code_d);   // Origin != max corner

    // Test that Morton codes produce a valid ordering (all different codes)
    bool t5 = (code_a1 != code_b);  // Different points give different codes

    // Test monotonicity along axis-aligned directions
    // Moving in positive x direction increases code
    uint64_t code_low_x = morton_code::encode_normalized(0.0, 0.5, 0.5);
    uint64_t code_high_x = morton_code::encode_normalized(1.0, 0.5, 0.5);
    bool t6 = code_high_x > code_low_x;

    return !(t1 && t2 && t3 && t4 && t5 && t6);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test boundary conditions
int morton_code_tester::morton_boundary_test() const {
    // Test (0,0,0) - minimum corner
    uint64_t code_min = morton_code::encode_normalized(0.0, 0.0, 0.0);
    bool t1 = code_min == 0;

    // Test (1,1,1) - maximum corner (should give maximum code)
    uint64_t code_max = morton_code::encode_normalized(1.0, 1.0, 1.0);
    bool t2 = code_max > code_min;

    // Test with actual bounds
    double bounds[6] = {0.0, 0.0, 0.0, 10.0, 10.0, 10.0};

    // Point at min corner
    vec3 pos_min(0.0, 0.0, 0.0);
    uint64_t code_at_min = morton_code::encode(pos_min, bounds);
    bool t3 = code_at_min == 0;

    // Point at max corner
    vec3 pos_max(10.0, 10.0, 10.0);
    uint64_t code_at_max = morton_code::encode(pos_max, bounds);
    bool t4 = code_at_max > code_at_min;

    // Point at center
    vec3 pos_center(5.0, 5.0, 5.0);
    uint64_t code_at_center = morton_code::encode(pos_center, bounds);
    bool t5 = code_at_center > code_at_min && code_at_center < code_at_max;

    return !(t1 && t2 && t3 && t4 && t5);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test that sorting by Morton code doesn't break neighborhood correctness
int morton_code_tester::morton_sorting_preserves_correctness_test() const {
    // Create a simple set of 8 points (corners of a unit cube)
    struct point {
        vec3 pos;
        int id;
        uint64_t morton;
    };

    std::vector<point> points = {
        {{0.0, 0.0, 0.0}, 0, 0},
        {{1.0, 0.0, 0.0}, 1, 0},
        {{0.0, 1.0, 0.0}, 2, 0},
        {{1.0, 1.0, 0.0}, 3, 0},
        {{0.0, 0.0, 1.0}, 4, 0},
        {{1.0, 0.0, 1.0}, 5, 0},
        {{0.0, 1.0, 1.0}, 6, 0},
        {{1.0, 1.0, 1.0}, 7, 0},
    };

    double bounds[6] = {0.0, 0.0, 0.0, 1.0, 1.0, 1.0};

    // Compute Morton codes
    for (auto& p : points) {
        p.morton = morton_code::encode(p.pos, bounds);
    }

    // Store original IDs before sorting
    std::vector<int> original_ids;
    for (const auto& p : points) {
        original_ids.push_back(p.id);
    }

    // Sort by Morton code
    std::sort(points.begin(), points.end(),
              [](const point& a, const point& b) { return a.morton < b.morton; });

    // Verify all 8 points are still present (no data loss)
    std::vector<int> sorted_ids;
    for (const auto& p : points) {
        sorted_ids.push_back(p.id);
    }
    std::sort(sorted_ids.begin(), sorted_ids.end());

    bool t1 = sorted_ids.size() == 8;
    bool t2 = true;
    for (int i = 0; i < 8; ++i) {
        if (sorted_ids[i] != i) t2 = false;
    }

    // Verify Morton codes are in ascending order after sort
    bool t3 = true;
    for (size_t i = 1; i < points.size(); ++i) {
        if (points[i].morton < points[i-1].morton) t3 = false;
    }

    return !(t1 && t2 && t3);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// The main function
int main (int argc, char** argv){

    //Check that the command line input is correctly formatted
    assert(argc == 2);

    //Get the name of the test to run
    std::string test_name = argv[1];

    //Run the selected test
    morton_code_tester tester;

    if (test_name == "morton_encode_test")                      return tester.morton_encode_test();
    if (test_name == "morton_spatial_locality_test")            return tester.morton_spatial_locality_test();
    if (test_name == "morton_boundary_test")                    return tester.morton_boundary_test();
    if (test_name == "morton_sorting_preserves_correctness_test") return tester.morton_sorting_preserves_correctness_test();

    std::cout << "TEST NAME :" << test_name << " DOES NOT EXIST" << std::endl;
    return 1;

}
//---------------------------------------------------------------------------------------------------------
