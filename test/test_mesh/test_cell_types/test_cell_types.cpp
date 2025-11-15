#include <cassert>
#include <string>
#include <iostream>
#include <cmath>
#include <vector>
#include <memory>
#include <type_traits>

#include "cell.hpp"
#include "epithelial_cell.hpp"
#include "lumen_cell.hpp"
#include "static_cell.hpp"
#include "ecm_cell.hpp"
#include "nucleus_cell.hpp"
#include "node.hpp"
#include "face.hpp"
#include "vec3.hpp"
#include "custom_structures.hpp"

/*
 * Comprehensive unit tests for Cell Type classes.
 * Tests verify biological behavior specific to each cell type:
 * - epithelial_cell: Can divide when volume threshold reached
 * - lumen_cell: Non-dividing cavity cell
 * - static_cell: Immobile boundary cell
 * - ecm_cell: Extracellular matrix with specific mechanical properties
 * - nucleus_cell: Cell nucleus
 */


//---------------------------------------------------------------------------------------------------------
// Helper function: Create cell type parameters for testing
std::shared_ptr<cell_type_parameters> create_test_cell_type_params(
    const std::string& name,
    unsigned short global_type_id,
    double avg_division_vol = 0.0,
    double avg_growth_rate = 0.0
) {
    auto cell_type = std::make_shared<cell_type_parameters>();
    cell_type->name_ = name;
    cell_type->global_type_id_ = global_type_id;
    cell_type->mass_density_ = 1.0e3;  // kg/m^3
    cell_type->bulk_modulus_ = 1.0e3;
    cell_type->max_pressure_ = 1.0e4;
    cell_type->initial_pressure_ = 0.0;
    cell_type->area_elasticity_modulus_ = 100.0;
    cell_type->avg_division_vol_ = avg_division_vol;
    cell_type->std_division_vol_ = 0.0;
    cell_type->avg_growth_rate_ = avg_growth_rate;
    cell_type->std_growth_rate_ = 0.0;
    cell_type->min_vol_ = 1.0e-18;
    cell_type->angle_regularization_factor_ = 0.0;
    cell_type->target_isoperimetric_ratio_ = 0.0;
    cell_type->surface_coupling_max_curvature_ = 0.0;

    // Add default face type
    face_type_parameters face_type;
    face_type.name_ = "default";
    face_type.face_type_global_id_ = 0;
    face_type.surface_tension_ = 0.01;
    face_type.adherence_strength_ = 0.0;
    face_type.repulsion_strength_ = 1.0;
    face_type.bending_modulus_ = 0.0;
    cell_type->add_face_type(face_type);

    return cell_type;
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Helper function: Create a minimal tetrahedron cell for testing
template<typename CellType>
std::shared_ptr<CellType> create_test_cell_of_type(
    unsigned cell_id,
    std::shared_ptr<cell_type_parameters> cell_type_params,
    double scale = 1.0e-6
) {
    // Create tetrahedron vertices (scaled to realistic size)
    std::vector<double> node_coords = {
        0.0, 0.0, 0.0,
        scale, 0.0, 0.0,
        0.0, scale, 0.0,
        0.0, 0.0, scale
    };

    // Define tetrahedron faces
    std::vector<unsigned> face_node_ids = {
        0, 1, 2,  // Face 0
        0, 1, 3,  // Face 1
        0, 2, 3,  // Face 2
        1, 2, 3   // Face 3
    };

    auto cell = std::make_shared<CellType>(node_coords, face_node_ids, cell_id, cell_type_params);
    cell->initialize_cell_properties();
    cell->initialize_random_properties();

    return cell;
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 1: Verify epithelial cells can divide when volume exceeds threshold
int test_epithelial_cell_division() {

    // Create epithelial cell type with division volume
    double division_volume = 5.0e-19;  // m^3
    auto cell_type = create_test_cell_type_params("epithelial", 0, division_volume, 1.0e-16);

    // Create epithelial cell with small initial size
    auto epi_cell = create_test_cell_of_type<epithelial_cell>(0, cell_type, 0.5e-6);

    // Get initial volume and division volume
    double initial_volume = epi_cell->get_volume();
    double cell_division_vol = epi_cell->get_division_volume();

    // Verify cell is not ready to divide initially (volume too small)
    bool t1 = !epi_cell->is_ready_to_divide();

    // Verify initial volume is less than cell's division threshold
    bool t2 = initial_volume < cell_division_vol;

    // Create a larger epithelial cell that should exceed division threshold
    auto large_epi_cell = create_test_cell_of_type<epithelial_cell>(1, cell_type, 1.5e-6);
    double large_volume = large_epi_cell->get_volume();
    double large_division_vol = large_epi_cell->get_division_volume();

    // Verify large cell is ready to divide (volume >= division_volume)
    bool t3 = large_epi_cell->is_ready_to_divide();

    // Verify large volume exceeds its division threshold
    bool t4 = large_volume >= large_division_vol;

    // Verify growth rate is correctly set
    double growth_rate = large_epi_cell->get_growth_rate();
    bool t5 = growth_rate > 0.0;  // Should be initialized to avg_growth_rate

    // Verify cell type ID is correct (0 = epithelial)
    bool t6 = epi_cell->get_cell_type_id() == 0;

    return !(t1 && t2 && t3 && t4 && t5 && t6);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 2: Verify lumen cells do not divide regardless of volume
int test_lumen_cell_no_division() {

    // Create lumen cell type (type ID 2 = lumen)
    double division_volume = 5.0e-19;
    auto cell_type = create_test_cell_type_params("lumen", 2, division_volume, 0.0);

    // Create small lumen cell
    auto small_lumen = create_test_cell_of_type<lumen_cell>(0, cell_type, 0.5e-6);

    // Verify small lumen cell is not ready to divide
    bool t1 = !small_lumen->is_ready_to_divide();

    // Create very large lumen cell
    auto large_lumen = create_test_cell_of_type<lumen_cell>(1, cell_type, 2.0e-6);
    double large_volume = large_lumen->get_volume();

    // Even with large volume exceeding threshold, lumen cells should NOT divide
    // This is the key behavioral difference from epithelial cells
    bool t2 = !large_lumen->is_ready_to_divide();

    // Verify volume is actually large (to ensure test validity)
    bool t3 = large_volume > division_volume;

    // Verify cell type ID is correct (2 = lumen)
    bool t4 = large_lumen->get_cell_type_id() == 2;

    // Verify lumen cells inherit from base cell class
    bool t5 = std::is_base_of<cell, lumen_cell>::value;

    // Verify lumen cell has correct type parameters
    bool t6 = large_lumen->get_cell_type()->name_ == "lumen";

    return !(t1 && t2 && t3 && t4 && t5 && t6);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 3: Verify static cells are immobile
int test_static_cell_immobility() {

    // Create static cell type (type ID 3 = static)
    auto cell_type = create_test_cell_type_params("static", 3, 0.0, 0.0);

    // Create static cell
    auto static_c = create_test_cell_of_type<static_cell>(0, cell_type, 1.0e-6);

    // Verify is_static flag is set to true
    bool t1 = static_c->is_static();

    // Static cells should never be ready to divide
    bool t2 = !static_c->is_ready_to_divide();

    // Verify cell type ID is correct (3 = static for this test)
    bool t3 = static_c->get_cell_type_id() == 3;

    // Verify static cells inherit from base cell class
    bool t4 = std::is_base_of<cell, static_cell>::value;

    // Access nodes to verify they exist
    const auto& node_lst = static_c->get_node_lst();
    bool t5 = node_lst.size() > 0;

    // Verify we can get initial positions (baseline for immobility verification)
    std::vector<vec3> initial_positions;
    for (const auto& n : node_lst) {
        if (n.is_used()) {
            initial_positions.push_back(n.pos());
        }
    }
    bool t6 = initial_positions.size() == static_c->get_nb_of_nodes();

    return !(t1 && t2 && t3 && t4 && t5 && t6);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 4: Verify ECM cells have correct mechanical properties and immobility
int test_ecm_cell_properties() {

    // Create ECM cell type with specific mechanical properties (type ID 1 = ECM)
    auto cell_type = create_test_cell_type_params("ecm", 1, 0.0, 0.0);

    // ECM typically has higher stiffness - modify bulk modulus
    cell_type->bulk_modulus_ = 5.0e3;  // Stiffer than epithelial cells
    cell_type->area_elasticity_modulus_ = 500.0;  // Higher elasticity

    // Create ECM cell
    auto ecm = create_test_cell_of_type<ecm_cell>(0, cell_type, 1.0e-6);

    // Verify ECM cells are static (immobile)
    bool t1 = ecm->is_static();

    // ECM cells should never divide
    bool t2 = !ecm->is_ready_to_divide();

    // Verify cell type ID is correct (1 = ECM)
    bool t3 = ecm->get_cell_type_id() == 1;

    // Verify bulk modulus is correctly set
    bool t4 = ecm->get_cell_type()->bulk_modulus_ == 5.0e3;

    // Verify area elasticity modulus is correctly set
    bool t5 = ecm->get_cell_type()->area_elasticity_modulus_ == 500.0;

    // Verify ECM cells inherit from base cell class
    bool t6 = std::is_base_of<cell, ecm_cell>::value;

    // Verify the cell has faces with face types
    bool t7 = ecm->get_face_lst().size() > 0;

    return !(t1 && t2 && t3 && t4 && t5 && t6 && t7);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 5: Verify nucleus cells have correct properties
int test_nucleus_cell_properties() {

    // Create nucleus cell type (type ID 5 = nucleus)
    auto cell_type = create_test_cell_type_params("nucleus", 5, 0.0, 0.0);

    // Create nucleus cell
    auto nucleus = create_test_cell_of_type<nucleus_cell>(0, cell_type, 0.8e-6);

    // Nucleus cells should not divide on their own
    bool t1 = !nucleus->is_ready_to_divide();

    // Verify cell type ID is correct (5 = nucleus)
    bool t2 = nucleus->get_cell_type_id() == 5;

    // Verify nucleus cells inherit from base cell class
    bool t3 = std::is_base_of<cell, nucleus_cell>::value;

    // Verify nucleus is not static (it can move with parent cell)
    bool t4 = !nucleus->is_static();

    // Verify cell has volume
    double volume = nucleus->get_volume();
    bool t5 = volume > 0.0;

    // Verify cell has area
    double area = nucleus->get_area();
    bool t6 = area > 0.0;

    // Verify cell type name is correct
    bool t7 = nucleus->get_cell_type()->name_ == "nucleus";

    return !(t1 && t2 && t3 && t4 && t5 && t6 && t7);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 6: Verify cell type identification for all types
int test_cell_type_identification() {

    // Create one cell of each type with unique type IDs
    auto epithelial_type = create_test_cell_type_params("epithelial", 0, 1.0e-18, 1.0e-16);
    auto ecm_type = create_test_cell_type_params("ecm", 1, 0.0, 0.0);
    auto lumen_type = create_test_cell_type_params("lumen", 2, 0.0, 0.0);
    auto static_type = create_test_cell_type_params("static", 3, 0.0, 0.0);
    auto nucleus_type = create_test_cell_type_params("nucleus", 5, 0.0, 0.0);

    auto epi = create_test_cell_of_type<epithelial_cell>(0, epithelial_type);
    auto ecm = create_test_cell_of_type<ecm_cell>(1, ecm_type);
    auto lumen = create_test_cell_of_type<lumen_cell>(2, lumen_type);
    auto static_c = create_test_cell_of_type<static_cell>(3, static_type);
    auto nucleus = create_test_cell_of_type<nucleus_cell>(4, nucleus_type);

    // Verify each cell reports correct type ID
    bool t1 = epi->get_cell_type_id() == 0;
    bool t2 = ecm->get_cell_type_id() == 1;
    bool t3 = lumen->get_cell_type_id() == 2;
    bool t4 = static_c->get_cell_type_id() == 3;
    bool t5 = nucleus->get_cell_type_id() == 5;

    // Verify each cell has correct name
    bool t6 = epi->get_cell_type()->name_ == "epithelial";
    bool t7 = ecm->get_cell_type()->name_ == "ecm";
    bool t8 = lumen->get_cell_type()->name_ == "lumen";
    bool t9 = static_c->get_cell_type()->name_ == "static";
    bool t10 = nucleus->get_cell_type()->name_ == "nucleus";

    return !(t1 && t2 && t3 && t4 && t5 && t6 && t7 && t8 && t9 && t10);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 7: Verify epithelial cell division volume threshold
int test_epithelial_division_threshold() {

    // Create epithelial cell with specific division volume
    double avg_division_volume = 1.0e-18;  // 1 cubic micrometer
    auto cell_type = create_test_cell_type_params("epithelial", 0, avg_division_volume, 0.0);

    // Create cell just below threshold
    auto cell_below = create_test_cell_of_type<epithelial_cell>(0, cell_type, 0.8e-6);
    double volume_below = cell_below->get_volume();
    double div_vol_below = cell_below->get_division_volume();

    // Create cell at or above threshold
    auto cell_above = create_test_cell_of_type<epithelial_cell>(1, cell_type, 2.0e-6);
    double volume_above = cell_above->get_volume();
    double div_vol_above = cell_above->get_division_volume();

    // Verify volumes are as expected relative to cell's own threshold
    bool t1 = volume_below < div_vol_below;
    bool t2 = volume_above >= div_vol_above;

    // Verify division readiness matches volume vs division_volume
    bool t3 = !cell_below->is_ready_to_divide();
    bool t4 = cell_above->is_ready_to_divide();

    // Verify division volume is correctly stored (should be around avg_division_volume)
    bool t5 = cell_below->get_division_volume() > 0.0;
    bool t6 = cell_above->get_division_volume() > 0.0;

    return !(t1 && t2 && t3 && t4 && t5 && t6);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 8: Verify static and ECM cells both have immobility flag
int test_immobile_cell_types() {

    auto ecm_type = create_test_cell_type_params("ecm", 1, 0.0, 0.0);
    auto static_type = create_test_cell_type_params("static", 3, 0.0, 0.0);
    auto epithelial_type = create_test_cell_type_params("epithelial", 0, 1.0e-18, 1.0e-16);

    auto ecm = create_test_cell_of_type<ecm_cell>(0, ecm_type);
    auto static_c = create_test_cell_of_type<static_cell>(1, static_type);
    auto epi = create_test_cell_of_type<epithelial_cell>(2, epithelial_type);

    // Verify ECM is static
    bool t1 = ecm->is_static();

    // Verify static cell is static
    bool t2 = static_c->is_static();

    // Verify epithelial is NOT static
    bool t3 = !epi->is_static();

    // Verify immobile cells don't divide
    bool t4 = !ecm->is_ready_to_divide();
    bool t5 = !static_c->is_ready_to_divide();

    return !(t1 && t2 && t3 && t4 && t5);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 9: Verify cell type polymorphism (get_cell_same_type)
int test_cell_type_polymorphism() {

    // Create cells of different types
    auto epi_type = create_test_cell_type_params("epithelial", 0, 1.0e-18, 1.0e-16);
    auto lumen_type = create_test_cell_type_params("lumen", 2, 0.0, 0.0);

    auto epi = create_test_cell_of_type<epithelial_cell>(0, epi_type);
    auto lumen = create_test_cell_of_type<lumen_cell>(1, lumen_type);

    // Get mesh representation
    mesh epi_mesh = epi->get_mesh();
    mesh lumen_mesh = lumen->get_mesh();

    // Verify meshes have nodes and faces
    bool t1 = epi_mesh.get_nb_nodes() > 0;
    bool t2 = epi_mesh.get_nb_faces() > 0;
    bool t3 = lumen_mesh.get_nb_nodes() > 0;
    bool t4 = lumen_mesh.get_nb_faces() > 0;

    // Create new cells of same type from mesh
    auto epi_copy = epi->get_cell_same_type(epi_mesh);
    auto lumen_copy = lumen->get_cell_same_type(lumen_mesh);

    // Verify copies are not null
    bool t5 = epi_copy != nullptr;
    bool t6 = lumen_copy != nullptr;

    // Verify copies have correct type IDs
    bool t7 = epi_copy->get_cell_type_id() == 0;
    bool t8 = lumen_copy->get_cell_type_id() == 2;

    return !(t1 && t2 && t3 && t4 && t5 && t6 && t7 && t8);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 10: Verify growth rate initialization for epithelial cells
int test_epithelial_growth_rate() {

    // Create epithelial cell with specific growth rate
    double avg_growth_rate = 1.5e-16;  // m^3/s
    auto cell_type = create_test_cell_type_params("epithelial", 0, 1.0e-18, avg_growth_rate);

    // Create multiple cells to test growth rate initialization
    auto cell1 = create_test_cell_of_type<epithelial_cell>(0, cell_type);
    auto cell2 = create_test_cell_of_type<epithelial_cell>(1, cell_type);
    auto cell3 = create_test_cell_of_type<epithelial_cell>(2, cell_type);

    // Verify growth rates are initialized
    double gr1 = cell1->get_growth_rate();
    double gr2 = cell2->get_growth_rate();
    double gr3 = cell3->get_growth_rate();

    // Growth rates should be positive (assuming avg > 0)
    bool t1 = gr1 > 0.0;
    bool t2 = gr2 > 0.0;
    bool t3 = gr3 > 0.0;

    // Verify we can modify growth rate
    cell1->set_growth_rate(2.0e-16);
    bool t4 = cell1->get_growth_rate() == 2.0e-16;

    // Verify other cells unchanged
    bool t5 = cell2->get_growth_rate() == gr2;

    return !(t1 && t2 && t3 && t4 && t5);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 11: Verify all cell types have correct inheritance
int test_cell_type_inheritance() {

    // Verify all cell types inherit from base cell class
    bool t1 = std::is_base_of<cell, epithelial_cell>::value;
    bool t2 = std::is_base_of<cell, lumen_cell>::value;
    bool t3 = std::is_base_of<cell, static_cell>::value;
    bool t4 = std::is_base_of<cell, ecm_cell>::value;
    bool t5 = std::is_base_of<cell, nucleus_cell>::value;

    // Verify all cell types are not default constructible (following base class)
    bool t6 = !std::is_default_constructible<epithelial_cell>::value;
    bool t7 = !std::is_default_constructible<lumen_cell>::value;
    bool t8 = !std::is_default_constructible<static_cell>::value;
    bool t9 = !std::is_default_constructible<ecm_cell>::value;
    bool t10 = !std::is_default_constructible<nucleus_cell>::value;

    return !(t1 && t2 && t3 && t4 && t5 && t6 && t7 && t8 && t9 && t10);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 12: Verify epithelial cell face contact behavior exists
int test_epithelial_face_contact() {

    // Create two epithelial cells
    auto epi_type = create_test_cell_type_params("epithelial", 0, 1.0e-18, 1.0e-16);
    auto cell1 = create_test_cell_of_type<epithelial_cell>(0, epi_type);
    auto cell2 = create_test_cell_of_type<epithelial_cell>(1, epi_type);

    // Verify cells have faces
    bool t1 = cell1->get_nb_of_faces() > 0;
    bool t2 = cell2->get_nb_of_faces() > 0;

    // Verify cells have face type parameters
    const auto& face_lst1 = cell1->get_face_lst();
    bool t3 = face_lst1.size() > 0;

    if (t3 && face_lst1[0].is_used()) {
        const auto& face_type = cell1->get_face_type(0);
        bool t4 = face_type.surface_tension_ >= 0.0;
        bool t5 = face_type.repulsion_strength_ >= 0.0;

        return !(t1 && t2 && t3 && t4 && t5);
    }

    return !(t1 && t2 && t3);
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
    if (test_name == "test_epithelial_cell_division")      return test_epithelial_cell_division();
    if (test_name == "test_lumen_cell_no_division")        return test_lumen_cell_no_division();
    if (test_name == "test_static_cell_immobility")        return test_static_cell_immobility();
    if (test_name == "test_ecm_cell_properties")           return test_ecm_cell_properties();
    if (test_name == "test_nucleus_cell_properties")       return test_nucleus_cell_properties();
    if (test_name == "test_cell_type_identification")      return test_cell_type_identification();
    if (test_name == "test_epithelial_division_threshold") return test_epithelial_division_threshold();
    if (test_name == "test_immobile_cell_types")           return test_immobile_cell_types();
    if (test_name == "test_cell_type_polymorphism")        return test_cell_type_polymorphism();
    if (test_name == "test_epithelial_growth_rate")        return test_epithelial_growth_rate();
    if (test_name == "test_cell_type_inheritance")         return test_cell_type_inheritance();
    if (test_name == "test_epithelial_face_contact")       return test_epithelial_face_contact();

    std::cout << "TEST NAME: " << test_name << " DOES NOT EXIST" << std::endl;
    return 1;
}
//---------------------------------------------------------------------------------------------------------
