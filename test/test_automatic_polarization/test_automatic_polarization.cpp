#include <cassert>
#include <iostream>
#include <optional>
#include <string>

#include "custom_structures.hpp"
#include "epithelial_cell.hpp"
#include "mesh_reader.hpp"
#include "mesh_writer.hpp"

#include "automatic_polarizer.hpp"
#include "automatic_polarization_writer.hpp"


//---------------------------------------------------------------------------------------------------------
//Test if the discretization grid is properly built
int test_grid_construction(){

    //Read a mesh file with 2 cubic cells that have been triangulated with a mun edge len of 5e-7m
    mesh_reader mesh_reader(std::string(PROJECT_SOURCE_DIR) + "/test/test_automatic_polarization/lumen_geometry.vtk");
    std::vector<mesh> cell_mesh_lst = mesh_reader.read();

    constexpr double max_edge_length = 3. * 5e-7;

    //Create a dummy epithelial cell type
    auto cell_type_ptr = std::make_shared<cell_type_parameters>();
    cell_type_ptr->name_ = "epithelial";
    cell_type_ptr->global_type_id_ = 0;

    //Create 3 dummy face types
    face_type_parameters apical_type;
    apical_type.name_ = "apical";
    apical_type.face_type_global_id_ = 0;

    face_type_parameters lateral_type;
    lateral_type.name_ = "lateral";
    lateral_type.face_type_global_id_ = 1;

    face_type_parameters basal_type;
    basal_type.name_ = "basal";
    basal_type.face_type_global_id_ = 2;

    cell_type_ptr->add_face_type(apical_type);
    cell_type_ptr->add_face_type(lateral_type);
    cell_type_ptr->add_face_type(basal_type);

    //Convert the cell meshes into cells
    std::vector<cell_ptr> cell_lst;
    unsigned cell_id = 0;
    for(const mesh& m: cell_mesh_lst){

        cell_ptr c = std::make_shared<epithelial_cell>(m, cell_id++, cell_type_ptr);
        c->initialize_cell_properties();
        c->update_centroid();
        c->update_all_face_normals_and_areas();
        cell_lst.push_back(c);
        
    }


    //Create the automatic polarizer
    automatic_polarizer polarizer(max_edge_length);

    //Update the grid dimensions
    polarizer.polarize_faces(cell_lst);

    //Write the grid in a file
    automatic_polarization_writer::write("./test_grid_construction.vtk", polarizer);


    std::ofstream face_data_file("./test_grid_construction_face_data.vtk", std::ofstream::out);

    //Throw an error if the file was not opened correctly
    if(!face_data_file){throw mesh_writer_exception("Unable to write file");}

    mesh_writer::write_face_data_file(face_data_file, cell_lst);

    mesh_writer::add_face_data_arrays_to_mesh(face_data_file, cell_lst);

    face_data_file.close();

    // Test passes if we get here without exceptions
    return 0;
}
//---------------------------------------------------------------------------------------------------------



//---------------------------------------------------------------------------------------------------------
//A face that sees a region label other than cytoplasm, exterior space or lumen must raise an
//automatic_polarization_exception instead of keeping its previous face type silently
int test_unknown_region_throws(){

    mesh_reader mesh_reader(std::string(PROJECT_SOURCE_DIR) + "/test/test_automatic_polarization/lumen_geometry.vtk");
    std::vector<mesh> cell_mesh_lst = mesh_reader.read();

    constexpr double max_edge_length = 3. * 5e-7;

    auto cell_type_ptr = std::make_shared<cell_type_parameters>();
    cell_type_ptr->name_ = "epithelial";
    cell_type_ptr->global_type_id_ = 0;

    face_type_parameters apical_type;
    apical_type.name_ = "apical";
    apical_type.face_type_global_id_ = 0;

    face_type_parameters lateral_type;
    lateral_type.name_ = "lateral";
    lateral_type.face_type_global_id_ = 1;

    cell_type_ptr->add_face_type(apical_type);
    cell_type_ptr->add_face_type(lateral_type);

    std::vector<cell_ptr> cell_lst;
    unsigned cell_id = 0;
    for(const mesh& m: cell_mesh_lst){
        cell_ptr c = std::make_shared<epithelial_cell>(m, cell_id++, cell_type_ptr);
        c->initialize_cell_properties();
        c->update_centroid();
        c->update_all_face_normals_and_areas();
        cell_lst.push_back(c);
    }
    assert(!cell_lst.empty());

    //Label the grid normally, then relabel every non-boundary voxel with a label that is not a region
    automatic_polarizer polarizer(max_edge_length);
    polarizer.polarize_faces(cell_lst);

    constexpr unsigned short unknown_region_id = 7;
    auto& grid = const_cast<uspg_3d<unsigned short>&>(polarizer.get_grid());
    const auto [nb_x, nb_y, nb_z] = grid.get_nb_voxels();
    for(unsigned i = 0; i < nb_x; i++){
    for(unsigned j = 0; j < nb_y; j++){
    for(unsigned k = 0; k < nb_z; k++){
        const std::optional<unsigned short> label = grid.get_voxel_content(i, j, k);
        if(!label.has_value() || label.value() != boundary_region_id){
            grid.update_voxel(i, j, k, unknown_region_id);
        }
    }}}

    try{
        polarizer.polarize_faces_based_on_contacts_with_discretized_space(cell_lst[0]);
    }
    catch(const automatic_polarization_exception& e){
        std::cout << "Exception correctly thrown: " << e.what() << std::endl;
        return 0;
    }

    std::cout << "ERROR: no automatic_polarization_exception for a face in contact with an unknown region" << std::endl;
    return 1;
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
    if (test_name == "test_grid_construction")   return test_grid_construction();
    if (test_name == "test_unknown_region_throws") return test_unknown_region_throws();


    std::cout << "TEST NAME :" << test_name << " DOES NOT EXIST" << std::endl;
    return 1;
}
//---------------------------------------------------------------------------------------------------------



