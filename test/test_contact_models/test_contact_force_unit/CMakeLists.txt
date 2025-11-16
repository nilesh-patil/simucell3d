cmake_minimum_required(VERSION 3.0)

# Use C++17 standard
set(CMAKE_CXX_STANDARD 17)

# Indicate to main the path to project source directory
add_definitions(-DPROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

# Compile the test executable
add_executable(test_contact_force_unit test_contact_force_unit.cpp)

# Link required libraries
target_link_libraries(test_contact_force_unit PUBLIC
    src
    mesh
    contact_models
)

# Include required header directories
target_include_directories(test_contact_force_unit PUBLIC
    "${PROJECT_SOURCE_DIR}/include/"
    "${PROJECT_SOURCE_DIR}/include/mesh"
    "${PROJECT_SOURCE_DIR}/include/contact_models"
    "${PROJECT_SOURCE_DIR}/include/math_modules"
)

# Register individual tests with CTest
# These tests validate isolated contact force calculations

add_test(
    NAME contact_force_spring_calculation
    COMMAND test_contact_force_unit test_contact_spring_force_calculation
)

add_test(
    NAME contact_force_symmetry
    COMMAND test_contact_force_unit test_contact_force_symmetry
)

add_test(
    NAME contact_radius_boundary
    COMMAND test_contact_force_unit test_contact_radius_boundary
)

add_test(
    NAME contact_repulsion_at_overlap
    COMMAND test_contact_force_unit test_repulsion_at_overlap
)

add_test(
    NAME contact_edge_cases
    COMMAND test_contact_force_unit test_edge_cases
)

add_test(
    NAME contact_force_direction
    COMMAND test_contact_force_unit test_force_direction_correctness
)
