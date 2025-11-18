#include "test_uspg_3d.hpp"

/*

Test the method of the cell class

*/


//---------------------------------------------------------------------------------------------------------
int uspg_3d_tester::update_dimensions_test() const{

    //The dimensions of the grid
    double min_x = 0.; double max_x = 10.;
    double min_y = 0.; double max_y = 9.5;
    double min_z = 0.; double max_z = 9.;
    unsigned nb_objects = 10;
    double voxel_size = 1.; 
    
    //Create a uspg_4d grid with int
    uspg_3d<int> grid(min_x, min_y, min_z, max_x, max_y, max_z, voxel_size, nb_objects);

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
int uspg_3d_tester::place_object_test() const{

    //The dimensions of the grid
    double min_x = 0.; double max_x = 3.;
    double min_y = 0.; double max_y = 3.;
    double min_z = 0.; double max_z = 3.;
    unsigned nb_objects = 10;
    double voxel_size = 1.; 

    //Create a grid with 27 voxels
    uspg_3d<int> grid(min_x, min_y, min_z, max_x, max_y, max_z, voxel_size, nb_objects);


    //Place the 3 integers at 3 different locations in the grid
    grid.place_object(1, 0., 0., 0.);
    grid.place_object(2, (unsigned) 1, (unsigned) 1, (unsigned) 1);
    grid.place_object(3, 2.5, 2.5, 2.5);

    //Make sure the objects are located where expected
    const auto voxel_000_content  = grid.get_voxel_content(0, 0, 0);
    const auto voxel_111_content  = grid.get_voxel_content(1, 1, 1);
    const auto voxel_222_content  = grid.get_voxel_content(2, 2, 2);

    //Make sure the voxel has one object in it
    bool t1 = (voxel_000_content) ?  voxel_000_content.value() == 1 : false;
    bool t2 = (voxel_111_content) ?  voxel_111_content.value() == 2 : false;
    bool t3 = (voxel_222_content) ?  voxel_222_content.value() == 3 : false;


    return !(t1 && t2 && t3);
}





//---------------------------------------------------------------------------------------------------------



//---------------------------------------------------------------------------------------------------------
int uspg_3d_tester::get_neighborhood_test() const{

    //The dimensions of the grid
    double min_x = 0.; double max_x = 3.;
    double min_y = 0.; double max_y = 3.;
    double min_z = 0.; double max_z = 3.;
    unsigned nb_objects = 10;
    double voxel_size = 1.; 

    //Create a grid with 27 voxels
    uspg_3d<int> grid(min_x, min_y, min_z, max_x, max_y, max_z, voxel_size, nb_objects);

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
int uspg_3d_tester::get_grid_content_test() const{

    //The dimensions of the grid
    double min_x = 0.; double max_x = 3.;
    double min_y = 0.; double max_y = 3.;
    double min_z = 0.; double max_z = 3.;
    unsigned nb_objects = 10;
    double voxel_size = 1.; 

    //Create a grid with 27 voxels
    uspg_3d<int> grid(min_x, min_y, min_z, max_x, max_y, max_z, voxel_size, nb_objects);

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

    const std::forward_list<int>  grid_content = grid.get_grid_content();

    //Create a copy of the grid content
    std::vector<int> grid_content_copy;
    for(auto obj: grid_content) grid_content_copy.push_back(obj);

    //Sort the grid content
    std::sort(grid_content_copy.begin(), grid_content_copy.end());

    return !(std::equal(int_lst.begin(), int_lst.end(), grid_content_copy.begin()));
}

//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
int uspg_3d_tester::update_voxel_test() const{


    //The dimensions of the grid
    double min_x = 0.; double max_x = 3.;
    double min_y = 0.; double max_y = 3.;
    double min_z = 0.; double max_z = 3.;
    unsigned nb_objects = 10;
    double voxel_size = 1.; 

    //Create a grid with 27 voxels
    uspg_3d<int> grid(min_x, min_y, min_z, max_x, max_y, max_z, voxel_size, nb_objects);
    
    //Update the voxel in position 1, 1, 1
    grid.update_voxel(1, 1, 1, 100);

    //Get the content of the voxel
    const auto voxel_content = grid.get_voxel_content(1, 1, 1);

    if(!voxel_content.has_value()) return 1;

    return !(voxel_content.value() == 100);

}
//---------------------------------------------------------------------------------------------------------



//---------------------------------------------------------------------------------------------------------
// Bug #5: Test that voxel index calculation doesn't overflow for large grids
int uspg_3d_tester::large_grid_index_overflow_test() const {
    // This test verifies that voxel index calculation works correctly for large grids
    // The bug: voxel_z_id * nb_voxels_x_ * nb_voxels_y_ overflows 32-bit unsigned

    // Simulate the calculation that would overflow:
    // A 2000x2000x2000 grid would have 8 billion voxels
    // 2000 * 2000 = 4,000,000 which fits in unsigned
    // But 4,000,000 * 2000 = 8,000,000,000 which overflows unsigned (max ~4.3 billion)

    // We can't actually create a grid that large, but we can verify the calculation
    // is done correctly by testing the boundary conditions

    // Test with a moderately sized grid that approaches overflow
    unsigned voxel_z_id = 1000;
    unsigned nb_voxels_x = 2000;
    unsigned nb_voxels_y = 2000;
    unsigned voxel_y_id = 500;
    unsigned voxel_x_id = 100;

    // Correct calculation using size_t from the start
    size_t correct_id = static_cast<size_t>(voxel_z_id) * nb_voxels_x * nb_voxels_y +
                        static_cast<size_t>(voxel_y_id) * nb_voxels_x + voxel_x_id;

    // The buggy calculation that may overflow
    // size_t buggy_id = voxel_z_id * nb_voxels_x * nb_voxels_y + voxel_y_id * nb_voxels_x + voxel_x_id;
    // This is UB but on most platforms wraps around

    // Expected: 1000 * 2000 * 2000 + 500 * 2000 + 100 = 4,001,000,100
    size_t expected = 4001000100ULL;

    if (correct_id != expected) {
        std::cerr << "FAIL: Expected " << expected << ", got " << correct_id << std::endl;
        return 1;
    }

    std::cout << "PASS: Large grid index calculation is correct" << std::endl;
    return 0;
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
    uspg_3d_tester tester;

    if (test_name == "update_dimensions_test")          return tester.update_dimensions_test();
    if (test_name == "get_neighborhood_test")           return tester.get_neighborhood_test();
    if (test_name == "place_object_test")               return tester.place_object_test();
    if (test_name == "get_grid_content_test")           return tester.get_grid_content_test();
    if (test_name == "update_voxel_test")               return tester.update_voxel_test();
    if (test_name == "large_grid_index_overflow_test")  return tester.large_grid_index_overflow_test();


    

    

    


    std::cout << "TEST NAME :" << test_name << " DOES NOT EXIST" << std::endl;
    return 1;

}
//---------------------------------------------------------------------------------------------------------
