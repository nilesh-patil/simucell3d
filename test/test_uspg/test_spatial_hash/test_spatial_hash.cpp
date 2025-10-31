#include "test_spatial_hash.hpp"

/*
 * Test suite for spatial_hash implementation
 *
 * This test suite verifies:
 * 1. API compatibility with uspg_4d (drop-in replacement)
 * 2. Correctness of Morton encoding and decoding
 * 3. Sparse storage efficiency
 * 4. Boundary condition handling
 */

//---------------------------------------------------------------------------------------------------------
int spatial_hash_tester::update_dimensions_test() const {
    // The dimensions of the grid
    double min_x = 0., max_x = 10.;
    double min_y = 0., max_y = 9.5;
    double min_z = 0., max_z = 9.;
    unsigned nb_objects = 10;
    double voxel_size = 1.;

    // Create a spatial_hash grid with int
    spatial_hash<int> grid(min_x, min_y, min_z, max_x, max_y, max_z, voxel_size, nb_objects);

    // Check the dimensions of the grid (accessing protected members via friend class)
    bool t1 = grid.get_nb_voxels()[0] == 10;
    bool t2 = grid.get_nb_voxels()[1] == 10;
    bool t3 = grid.get_nb_voxels()[2] == 9;
    bool t4 = grid.get_voxel_size() == voxel_size;

    // Check that hash table is initially empty (sparse storage)
    bool t5 = grid.get_num_occupied_voxels() == 0;

    return !(t1 && t2 && t3 && t4 && t5);
}
//---------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------
int spatial_hash_tester::place_object_test() const {
    // The dimensions of the grid
    double min_x = 0., max_x = 3.;
    double min_y = 0., max_y = 3.;
    double min_z = 0., max_z = 3.;
    unsigned nb_objects = 10;
    double voxel_size = 1.;

    // Create a grid with 27 voxels
    spatial_hash<int> grid(min_x, min_y, min_z, max_x, max_y, max_z, voxel_size, nb_objects);

    // Create and store some integers
    std::vector<int> int_lst(3);
    std::iota(int_lst.begin(), int_lst.end(), 1);

    // Place the 3 integers at 3 different locations in the grid
    grid.place_object(int_lst[0], 0., 0., 0.);
    grid.place_object(int_lst[1], 1.5, 1.5, 1.5);  // Center of voxel (1,1,1)
    grid.place_object(int_lst[2], 2.5, 2.5, 2.5);

    // Make sure the objects are located where expected
    const auto voxel_000_content = grid.get_voxel_content(0, 0, 0);
    const auto voxel_111_content = grid.get_voxel_content(1, 1, 1);
    const auto voxel_222_content = grid.get_voxel_content(2, 2, 2);

    // Make sure each voxel has one object in it
    bool t1 = std::distance(voxel_000_content.begin(), voxel_000_content.end()) == 1;
    bool t2 = std::distance(voxel_111_content.begin(), voxel_111_content.end()) == 1;
    bool t3 = std::distance(voxel_222_content.begin(), voxel_222_content.end()) == 1;

    // Make sure the content is correct
    bool t4 = false, t5 = false, t6 = false;
    if (t1) t4 = (voxel_000_content.front()) == 1;
    if (t2) t5 = (voxel_111_content.front()) == 2;
    if (t3) t6 = (voxel_222_content.front()) == 3;

    // Verify sparse storage: only 3 voxels should be occupied
    bool t7 = grid.get_num_occupied_voxels() == 3;

    return !(t1 && t2 && t3 && t4 && t5 && t6 && t7);
}
//---------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------
int spatial_hash_tester::get_neighborhood_test() const {
    // The dimensions of the grid
    double min_x = 0., max_x = 3.;
    double min_y = 0., max_y = 3.;
    double min_z = 0., max_z = 3.;
    unsigned nb_objects = 10;
    double voxel_size = 1.;

    // Create a grid with 27 voxels
    spatial_hash<int> grid(min_x, min_y, min_z, max_x, max_y, max_z, voxel_size, nb_objects);

    // Create and store integers
    std::vector<int> int_lst(27);
    std::iota(int_lst.begin(), int_lst.end(), 1);

    int counter = 0;
    // In each voxel insert 1 integer
    for (int z = 0; z < 3; z++) {
        for (int y = 0; y < 3; y++) {
            for (int x = 0; x < 3; x++) {
                grid.place_object(int_lst[counter], x, y, z);
                counter++;
            }
        }
    }

    // Get all the objects around the voxel 000 and convert to vector
    auto neighborhood_000_lst = grid.get_neighborhood((unsigned)0, (unsigned)0, (unsigned)0);
    std::vector<int> neighborhood_000_vec;
    for (auto elt : neighborhood_000_lst) neighborhood_000_vec.push_back(elt);
    std::sort(neighborhood_000_vec.begin(), neighborhood_000_vec.end());

    // Make sure the correct objects are in the neighborhood of voxel 000
    // Corner voxel (0,0,0) should see 8 neighbors: (0,0,0), (1,0,0), (0,1,0), (1,1,0),
    //                                               (0,0,1), (1,0,1), (0,1,1), (1,1,1)
    std::vector<int> correct_neighborhood_000{1, 2, 4, 5, 10, 11, 13, 14};
    bool t1 = std::equal(correct_neighborhood_000.begin(), correct_neighborhood_000.end(),
                        neighborhood_000_vec.begin());

    // Repeat with voxel 111 which is at the center of the grid
    auto neighborhood_111_lst = grid.get_neighborhood((unsigned)1, (unsigned)1, (unsigned)1);
    std::vector<int> neighborhood_111_vec;
    for (auto elt : neighborhood_111_lst) neighborhood_111_vec.push_back(elt);
    std::sort(neighborhood_111_vec.begin(), neighborhood_111_vec.end());

    // Center voxel should be in contact with all 27 voxels
    bool t2 = std::equal(neighborhood_111_vec.begin(), neighborhood_111_vec.end(), int_lst.begin());

    return !(t1 && t2);
}
//---------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------
int spatial_hash_tester::get_grid_content_test() const {
    // The dimensions of the grid
    double min_x = 0., max_x = 3.;
    double min_y = 0., max_y = 3.;
    double min_z = 0., max_z = 3.;
    unsigned nb_objects = 10;
    double voxel_size = 1.;

    // Create a grid with 27 voxels
    spatial_hash<int> grid(min_x, min_y, min_z, max_x, max_y, max_z, voxel_size, nb_objects);

    // Create and store integers
    std::vector<int> int_lst(27);
    std::iota(int_lst.begin(), int_lst.end(), 1);

    int counter = 0;
    // In each voxel insert 1 integer
    for (int z = 0; z < 3; z++) {
        for (int y = 0; y < 3; y++) {
            for (int x = 0; x < 3; x++) {
                grid.place_object(int_lst[counter], x, y, z);
                counter++;
            }
        }
    }

    const std::forward_list<int> grid_content = grid.get_grid_content();

    // Create a copy of the grid content
    std::vector<int> grid_content_copy;
    for (auto obj : grid_content) grid_content_copy.push_back(obj);

    // Sort the grid content
    std::sort(grid_content_copy.begin(), grid_content_copy.end());

    bool t1 = std::equal(int_lst.begin(), int_lst.end(), grid_content_copy.begin());

    // Verify total object count
    bool t2 = grid.get_total_objects() == 27;

    return !(t1 && t2);
}
//---------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------
int spatial_hash_tester::morton_encoding_test() const {
    // Test Morton encoding round-trip (encode -> decode)

    // Test case 1: Origin
    uint32_t x1 = 0, y1 = 0, z1 = 0;
    uint64_t morton1 = morton::encode_3d(x1, y1, z1);
    uint32_t x1_out, y1_out, z1_out;
    morton::decode_3d(morton1, x1_out, y1_out, z1_out);
    bool t1 = (x1 == x1_out && y1 == y1_out && z1 == z1_out);

    // Test case 2: Large coordinates
    uint32_t x2 = 1000, y2 = 2000, z2 = 3000;
    uint64_t morton2 = morton::encode_3d(x2, y2, z2);
    uint32_t x2_out, y2_out, z2_out;
    morton::decode_3d(morton2, x2_out, y2_out, z2_out);
    bool t2 = (x2 == x2_out && y2 == y2_out && z2 == z2_out);

    // Test case 3: Maximum valid coordinates (21 bits each)
    uint32_t x3 = (1 << 21) - 1, y3 = (1 << 21) - 1, z3 = (1 << 21) - 1;
    uint64_t morton3 = morton::encode_3d(x3, y3, z3);
    uint32_t x3_out, y3_out, z3_out;
    morton::decode_3d(morton3, x3_out, y3_out, z3_out);
    bool t3 = (x3 == x3_out && y3 == y3_out && z3 == z3_out);

    // Test case 4: Spatial locality (adjacent voxels have similar Morton codes)
    uint64_t morton_000 = morton::encode_3d(0, 0, 0);
    uint64_t morton_001 = morton::encode_3d(0, 0, 1);
    uint64_t morton_010 = morton::encode_3d(0, 1, 0);
    uint64_t morton_100 = morton::encode_3d(1, 0, 0);

    // Adjacent voxels should have Morton codes that differ by small amounts
    bool t4 = (morton_001 < 10 * morton_000 + 100) &&
              (morton_010 < 10 * morton_000 + 100) &&
              (morton_100 < 10 * morton_000 + 100);

    // Test case 5: Manhattan distance
    uint64_t morton_a = morton::encode_3d(5, 5, 5);
    uint64_t morton_b = morton::encode_3d(8, 5, 5);
    uint32_t manhattan_dist = morton::manhattan_distance(morton_a, morton_b);
    bool t5 = (manhattan_dist == 3);  // |8-5| + |5-5| + |5-5| = 3

    return !(t1 && t2 && t3 && t4 && t5);
}
//---------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------
int spatial_hash_tester::boundary_conditions_test() const {
    // Test behavior at grid boundaries
    double min_x = 0., max_x = 3.;
    double min_y = 0., max_y = 3.;
    double min_z = 0., max_z = 3.;
    unsigned nb_objects = 10;
    double voxel_size = 1.;

    spatial_hash<int> grid(min_x, min_y, min_z, max_x, max_y, max_z, voxel_size, nb_objects);

    // Test case 1: Place object at exact boundary (should be in grid)
    int obj1 = 100;
    grid.place_object(obj1, min_x, min_y, min_z);
    auto voxel_content = grid.get_voxel_content(0, 0, 0);
    bool t1 = !voxel_content.empty() && voxel_content.front() == obj1;

    // Test case 2: Corner voxel neighborhood (should handle boundaries correctly)
    int obj2 = 200;
    grid.place_object(obj2, max_x - 0.5, max_y - 0.5, max_z - 0.5);  // Last voxel
    auto neighborhood = grid.get_neighborhood((unsigned)2, (unsigned)2, (unsigned)2);

    // Convert to vector to count
    std::vector<int> neighbor_vec;
    for (auto obj : neighborhood) neighbor_vec.push_back(obj);

    // Corner voxel should have some neighbors (at least itself)
    bool t2 = !neighbor_vec.empty();

    // Test case 3: Query neighborhood at (0,0,0) - should not go out of bounds
    auto corner_neighborhood = grid.get_neighborhood((unsigned)0, (unsigned)0, (unsigned)0);
    std::vector<int> corner_vec;
    for (auto obj : corner_neighborhood) corner_vec.push_back(obj);

    // Should contain objects from this test without crashing
    bool t3 = (corner_vec.size() >= 1);

    return !(t1 && t2 && t3);
}
//---------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------
int spatial_hash_tester::sparse_storage_test() const {
    // Test that sparse storage is working efficiently
    double min_x = 0., max_x = 100.;
    double min_y = 0., max_y = 100.;
    double min_z = 0., max_z = 100.;
    unsigned nb_objects = 10;
    double voxel_size = 1.;

    // This creates a 100x100x100 = 1,000,000 voxel grid
    spatial_hash<int> grid(min_x, min_y, min_z, max_x, max_y, max_z, voxel_size, nb_objects);

    // Place only 10 objects (should occupy ≤10 voxels out of 1M)
    std::vector<int> objects(10);
    std::iota(objects.begin(), objects.end(), 1);

    for (size_t i = 0; i < objects.size(); ++i) {
        // Place objects sparsely
        grid.place_object(objects[i],
                         i * 10.0 + 0.5,
                         i * 10.0 + 0.5,
                         i * 10.0 + 0.5);
    }

    // Verify sparse storage: only 10 voxels should be occupied
    bool t1 = grid.get_num_occupied_voxels() == 10;

    // Verify all objects are present
    bool t2 = grid.get_total_objects() == 10;

    // Verify load factor (objects per occupied voxel)
    bool t3 = std::abs(grid.get_load_factor() - 1.0) < 0.001;  // Should be exactly 1.0

    // Verify memory efficiency: hash table size should be much smaller than total voxels
    size_t total_voxels = 100 * 100 * 100;
    bool t4 = grid.get_num_occupied_voxels() < total_voxels / 1000;  // < 0.1% occupancy

    return !(t1 && t2 && t3 && t4);
}
//---------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------
// The main function
int main(int argc, char** argv) {
    // Check that the command line input is correctly formatted
    assert(argc == 2);

    // Get the name of the test to run
    std::string test_name = argv[1];

    // Run the selected test
    spatial_hash_tester tester;

    // Core USPG compatibility tests
    if (test_name == "update_dimensions_test")    return tester.update_dimensions_test();
    if (test_name == "get_neighborhood_test")     return tester.get_neighborhood_test();
    if (test_name == "place_object_test")         return tester.place_object_test();
    if (test_name == "get_grid_content_test")     return tester.get_grid_content_test();

    // Spatial hash specific tests
    if (test_name == "morton_encoding_test")      return tester.morton_encoding_test();
    if (test_name == "boundary_conditions_test")  return tester.boundary_conditions_test();
    if (test_name == "sparse_storage_test")       return tester.sparse_storage_test();

    std::cout << "TEST NAME: " << test_name << " DOES NOT EXIST" << std::endl;
    return 1;
}
//---------------------------------------------------------------------------------------------------------
