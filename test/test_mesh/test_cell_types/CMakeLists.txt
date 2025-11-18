cmake_minimum_required(VERSION 3.0)

# Use CPP 17
set(CMAKE_CXX_STANDARD 17)

# Compile the test executable
add_executable(cell_types_test
    test_cell_types.cpp
)

# Link with main source library
target_link_libraries(cell_types_test PUBLIC src)

# Add all required header file directories
target_include_directories(cell_types_test PUBLIC "${PROJECT_SOURCE_DIR}/include/")
target_include_directories(cell_types_test PUBLIC "${PROJECT_SOURCE_DIR}/include/io")
target_include_directories(cell_types_test PUBLIC "${PROJECT_SOURCE_DIR}/include/math_modules")
target_include_directories(cell_types_test PUBLIC "${PROJECT_SOURCE_DIR}/include/mesh")
target_include_directories(cell_types_test PUBLIC "${PROJECT_SOURCE_DIR}/include/mesh/cell_types")
target_include_directories(cell_types_test PUBLIC "${PROJECT_SOURCE_DIR}/include/triangulation_modules")

# Add tests for cell type module
add_test(NAME cell_types_test_epithelial_cell_division      COMMAND cell_types_test test_epithelial_cell_division)
add_test(NAME cell_types_test_lumen_cell_no_division        COMMAND cell_types_test test_lumen_cell_no_division)
add_test(NAME cell_types_test_static_cell_immobility        COMMAND cell_types_test test_static_cell_immobility)
add_test(NAME cell_types_test_ecm_cell_properties           COMMAND cell_types_test test_ecm_cell_properties)
add_test(NAME cell_types_test_nucleus_cell_properties       COMMAND cell_types_test test_nucleus_cell_properties)
add_test(NAME cell_types_test_cell_type_identification      COMMAND cell_types_test test_cell_type_identification)
add_test(NAME cell_types_test_epithelial_division_threshold COMMAND cell_types_test test_epithelial_division_threshold)
add_test(NAME cell_types_test_immobile_cell_types           COMMAND cell_types_test test_immobile_cell_types)
add_test(NAME cell_types_test_cell_type_polymorphism        COMMAND cell_types_test test_cell_type_polymorphism)
add_test(NAME cell_types_test_epithelial_growth_rate        COMMAND cell_types_test test_epithelial_growth_rate)
add_test(NAME cell_types_test_cell_type_inheritance         COMMAND cell_types_test test_cell_type_inheritance)
add_test(NAME cell_types_test_epithelial_face_contact       COMMAND cell_types_test test_epithelial_face_contact)
