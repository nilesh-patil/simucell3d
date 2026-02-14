#include "test_uspg_4d.hpp"

/*

Test the method of the cell class

*/


//---------------------------------------------------------------------------------------------------------
int uspg_4d_tester::update_dimensions_test() const{

    //The dimensions of the grid
    double min_x = 0.; double max_x = 10.;
    double min_y = 0.; double max_y = 9.5;
    double min_z = 0.; double max_z = 9.;
    unsigned nb_objects = 10;
    double voxel_size = 1.; 
    
    //Create a uspg_4d grid with int
    uspg_4d<int> grid(min_x, min_y, min_z, max_x, max_y, max_z, voxel_size, nb_objects);

    //Check the dimensions of the grid
    bool t1 = grid.nb_voxels_x_ == 10;
    bool t2 = grid.nb_voxels_y_ == 10;
    bool t3 = grid.nb_voxels_z_ == 9;
    bool t4 = grid.voxel_size_ == voxel_size;

    //Check the size of the grid voxel lst
    const size_t total_nb_voxels = 10 * 10 * 9;
    bool t5 = grid.voxel_lst_.size() == total_nb_voxels;

    return !(t1 && t2 && t3 && t4 && t5);
}
//---------------------------------------------------------------------------------------------------------



//---------------------------------------------------------------------------------------------------------
int uspg_4d_tester::place_object_test() const{

    //The dimensions of the grid
    double min_x = 0.; double max_x = 3.;
    double min_y = 0.; double max_y = 3.;
    double min_z = 0.; double max_z = 3.;
    unsigned nb_objects = 10;
    double voxel_size = 1.; 

    //Create a grid with 27 voxels
    uspg_4d<int> grid(min_x, min_y, min_z, max_x, max_y, max_z, voxel_size, nb_objects);

    //Create and store some integers. Those have to be stored otherwise their pointers
    //because the grid store pointers and not copies of the objects inserted
    std::vector<int> int_lst(3);
    std::iota(int_lst.begin(), int_lst.end(), 1);

    //Place the 3 integers at 3 different locations in the grid
    grid.place_object(int_lst[0], 0., 0., 0.);
    grid.place_object(int_lst[1], (unsigned) 1, (unsigned) 1, (unsigned) 1);
    grid.place_object(int_lst[2], 2.5, 2.5, 2.5);

    //Make sure the objects are located where expected
    const auto voxel_000_content  = grid.get_voxel_content(0, 0, 0);
    const auto voxel_111_content  = grid.get_voxel_content(1, 1, 1);
    const auto voxel_222_content  = grid.get_voxel_content(2, 2, 2);


    //Make sure the voxel has one object in it (using O(1) .size() with vector)
    bool t1 = voxel_000_content.size() == 1;
    bool t2 = voxel_111_content.size() == 1;
    bool t3 = voxel_222_content.size() == 1;

    //Make sure the content is correct
    bool t4 = false, t5 = false, t6 = false;
    if(t1) t4 = (voxel_000_content.front()) == 1;
    if(t2) t5 = (voxel_111_content.front()) == 2;
    if(t3) t6 = (voxel_222_content.front()) == 3;

    return !(t1 && t2 && t3 && t4 && t5 && t6);
}





//---------------------------------------------------------------------------------------------------------



//---------------------------------------------------------------------------------------------------------
int uspg_4d_tester::get_neighborhood_test() const{

    //The dimensions of the grid
    double min_x = 0.; double max_x = 3.;
    double min_y = 0.; double max_y = 3.;
    double min_z = 0.; double max_z = 3.;
    unsigned nb_objects = 10;
    double voxel_size = 1.; 

    //Create a grid with 27 voxels
    uspg_4d<int> grid(min_x, min_y, min_z, max_x, max_y, max_z, voxel_size, nb_objects);

    //Create and store some integers. Those have to be stored otherwise their pointers
    //because the grid store pointers and not copies of the objects inserted
    std::vector<int> int_lst(27);
    std::iota(int_lst.begin(), int_lst.end(), 1);

    int counter = 0;
    //In each voxel insert 2 integers
    for(int z = 0; z < 3 ; z++){
    for(int y = 0; y < 3 ; y++){
    for(int x = 0; x < 3 ; x++){
        
        grid.place_object(int_lst[counter], x, y, z);

        counter++;
    }}}

    //Get all the objects around the voxel 000 and convert the content into a vector
    auto neighborhood_000_lst  = grid.get_neighborhood((unsigned) 0, (unsigned) 0, (unsigned) 0);
    std::vector<int> neighborhood_000_vec;
    for(auto elt: neighborhood_000_lst) neighborhood_000_vec.push_back(elt);
    std::sort(neighborhood_000_vec.begin(), neighborhood_000_vec.end());

    //Make sure the correct objects are in the neighborhood of the vector 000
    std::vector<int> correct_neighborhood_000{1, 2, 4, 5, 10, 11, 13, 14};
    bool t1 = std::equal(correct_neighborhood_000.begin(), correct_neighborhood_000.end(), neighborhood_000_vec.begin());


    //Repeat the same operation with the voxel 111 which is at the center of the grid
    auto neighborhood_111_lst  = grid.get_neighborhood((unsigned) 1, (unsigned) 1, (unsigned) 1);
    std::vector<int> neighborhood_111_vec;
    for(auto elt: neighborhood_111_lst) neighborhood_111_vec.push_back(elt);
    std::sort(neighborhood_111_vec.begin(), neighborhood_111_vec.end());

    //This voxel should be in contact with all the other voxels
    bool t2 = std::equal(neighborhood_111_vec.begin(), neighborhood_111_vec.end(), int_lst.begin());


    return !(t1 && t2);
}
//---------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------
int uspg_4d_tester::get_grid_content_test() const{
    //The dimensions of the grid
    double min_x = 0.; double max_x = 3.;
    double min_y = 0.; double max_y = 3.;
    double min_z = 0.; double max_z = 3.;
    unsigned nb_objects = 10;
    double voxel_size = 1.; 

    //Create a grid with 27 voxels
    uspg_4d<int> grid(min_x, min_y, min_z, max_x, max_y, max_z, voxel_size, nb_objects);

    //Create and store some integers. Those have to be stored otherwise their pointers
    //because the grid store pointers and not copies of the objects inserted
    std::vector<int> int_lst(27);
    std::iota(int_lst.begin(), int_lst.end(), 1);

    int counter = 0;
    //In each voxel insert 2 integers
    for(int z = 0; z < 3 ; z++){
    for(int y = 0; y < 3 ; y++){
    for(int x = 0; x < 3 ; x++){
        
        grid.place_object(int_lst[counter], x, y, z);

        counter++;
    }}}

    const std::vector<int> grid_content = grid.get_grid_content();

    //Create a sorted copy of the grid content for comparison
    std::vector<int> grid_content_sorted = grid_content;
    std::sort(grid_content_sorted.begin(), grid_content_sorted.end());

    return !(std::equal(int_lst.begin(), int_lst.end(), grid_content_sorted.begin()));
}
//---------------------------------------------------------------------------------------------------------




//---------------------------------------------------------------------------------------------------------
// New tests for vector storage optimization (Phase 1)
//---------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------
// Test that .size() returns the correct count (validates vector API)
int uspg_4d_tester::vector_storage_size_test() const {
    // Setup: 3x3x3 grid
    double min_x = 0., max_x = 3.;
    double min_y = 0., max_y = 3.;
    double min_z = 0., max_z = 3.;
    unsigned nb_objects = 10;
    double voxel_size = 1.;

    uspg_4d<int> grid(min_x, min_y, min_z, max_x, max_y, max_z, voxel_size, nb_objects);

    // Insert 3 objects into voxel (0,0,0)
    grid.place_object(1, 0.1, 0.1, 0.1);
    grid.place_object(2, 0.2, 0.2, 0.2);
    grid.place_object(3, 0.3, 0.3, 0.3);

    // Verify .size() returns 3
    const auto& voxel_content = grid.get_voxel_content(0, 0, 0);
    bool t1 = voxel_content.size() == 3;

    // Insert 1 object into voxel (1,1,1)
    grid.place_object(4, 1.5, 1.5, 1.5);
    const auto& voxel_111 = grid.get_voxel_content(1, 1, 1);
    bool t2 = voxel_111.size() == 1;

    return !(t1 && t2);
}
//---------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------
// Test that push_back maintains insertion order (vector-specific behavior)
int uspg_4d_tester::vector_push_back_ordering_test() const {
    double min_x = 0., max_x = 3.;
    double min_y = 0., max_y = 3.;
    double min_z = 0., max_z = 3.;
    unsigned nb_objects = 10;
    double voxel_size = 1.;

    uspg_4d<int> grid(min_x, min_y, min_z, max_x, max_y, max_z, voxel_size, nb_objects);

    // Insert objects in order: 10, 20, 30
    grid.place_object(10, 0.1, 0.1, 0.1);
    grid.place_object(20, 0.2, 0.2, 0.2);
    grid.place_object(30, 0.3, 0.3, 0.3);

    const auto& voxel_content = grid.get_voxel_content(0, 0, 0);

    // With vector and push_back, order should be: 10, 20, 30
    // Convert to vector for easy checking
    std::vector<int> content_vec(voxel_content.begin(), voxel_content.end());

    // Check size and values
    bool t1 = content_vec.size() == 3;
    bool t2 = content_vec[0] == 10;  // First inserted
    bool t3 = content_vec[1] == 20;  // Second inserted
    bool t4 = content_vec[2] == 30;  // Third inserted

    return !(t1 && t2 && t3 && t4);
}
//---------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------
// Test that empty voxels return an empty container (not null or undefined)
int uspg_4d_tester::empty_voxel_returns_empty_vector_test() const {
    double min_x = 0., max_x = 3.;
    double min_y = 0., max_y = 3.;
    double min_z = 0., max_z = 3.;
    unsigned nb_objects = 10;
    double voxel_size = 1.;

    uspg_4d<int> grid(min_x, min_y, min_z, max_x, max_y, max_z, voxel_size, nb_objects);

    // Don't insert anything - all voxels should be empty
    const auto& voxel_content = grid.get_voxel_content(0, 0, 0);

    // Verify empty() returns true and size() returns 0
    bool t1 = voxel_content.empty();
    bool t2 = voxel_content.size() == 0;

    // Check another voxel
    const auto& voxel_222 = grid.get_voxel_content(2, 2, 2);
    bool t3 = voxel_222.empty();

    return !(t1 && t2 && t3);
}
//---------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------
// Test multiple insertions into the same voxel
int uspg_4d_tester::multiple_insertions_same_voxel_test() const {
    double min_x = 0., max_x = 3.;
    double min_y = 0., max_y = 3.;
    double min_z = 0., max_z = 3.;
    unsigned nb_objects = 100;
    double voxel_size = 1.;

    uspg_4d<int> grid(min_x, min_y, min_z, max_x, max_y, max_z, voxel_size, nb_objects);

    // Insert 50 objects into the same voxel
    for (int i = 0; i < 50; ++i) {
        grid.place_object(i, 0.5, 0.5, 0.5);
    }

    const auto& voxel_content = grid.get_voxel_content(0, 0, 0);

    // Verify all 50 objects are present
    bool t1 = voxel_content.size() == 50;

    // Verify sum of all elements (0+1+2+...+49 = 1225)
    int sum = 0;
    for (int val : voxel_content) {
        sum += val;
    }
    bool t2 = sum == 1225;

    return !(t1 && t2);
}
//---------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------
// Test that get_neighborhood returns a vector with all expected elements
int uspg_4d_tester::neighborhood_returns_vector_test() const {
    double min_x = 0., max_x = 3.;
    double min_y = 0., max_y = 3.;
    double min_z = 0., max_z = 3.;
    unsigned nb_objects = 27;
    double voxel_size = 1.;

    uspg_4d<int> grid(min_x, min_y, min_z, max_x, max_y, max_z, voxel_size, nb_objects);

    // Place one object in each of the 27 voxels
    std::vector<int> int_lst(27);
    std::iota(int_lst.begin(), int_lst.end(), 1);

    int counter = 0;
    for (int z = 0; z < 3; z++) {
        for (int y = 0; y < 3; y++) {
            for (int x = 0; x < 3; x++) {
                grid.place_object(int_lst[counter], x + 0.5, y + 0.5, z + 0.5);
                counter++;
            }
        }
    }

    // Get neighborhood of center voxel (1,1,1) - should contain all 27 objects
    auto neighborhood = grid.get_neighborhood((unsigned)1, (unsigned)1, (unsigned)1);

    // Verify neighborhood is the correct type and has correct elements
    // Sort for comparison
    std::vector<int> neighborhood_sorted(neighborhood.begin(), neighborhood.end());
    std::sort(neighborhood_sorted.begin(), neighborhood_sorted.end());

    bool t1 = neighborhood_sorted.size() == 27;
    bool t2 = std::equal(int_lst.begin(), int_lst.end(), neighborhood_sorted.begin());

    return !(t1 && t2);
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
    uspg_4d_tester tester;

    // Existing tests
    if (test_name == "update_dimensions_test")     return tester.update_dimensions_test();
    if (test_name == "get_neighborhood_test")           return tester.get_neighborhood_test();
    if (test_name == "place_object_test")               return tester.place_object_test();
    if (test_name == "get_grid_content_test")           return tester.get_grid_content_test();

    // New tests for vector storage optimization (Phase 1)
    if (test_name == "vector_storage_size_test")        return tester.vector_storage_size_test();
    if (test_name == "vector_push_back_ordering_test")  return tester.vector_push_back_ordering_test();
    if (test_name == "empty_voxel_returns_empty_vector_test") return tester.empty_voxel_returns_empty_vector_test();
    if (test_name == "multiple_insertions_same_voxel_test")   return tester.multiple_insertions_same_voxel_test();
    if (test_name == "neighborhood_returns_vector_test")      return tester.neighborhood_returns_vector_test();

    std::cout << "TEST NAME :" << test_name << " DOES NOT EXIST" << std::endl;
    return 1;

}
//---------------------------------------------------------------------------------------------------------
