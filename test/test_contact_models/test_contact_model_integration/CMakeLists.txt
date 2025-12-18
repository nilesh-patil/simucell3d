cmake_minimum_required(VERSION 3.0)
set(CMAKE_CXX_STANDARD 17)

# Indicate to main the path to project source directory
add_definitions(-DPROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

# Add test executable
add_executable(test_contact_model_integration
    test_contact_model_integration.cpp
    integration_test_fixtures.cpp
)

# Link against required libraries (use same pattern as other contact model tests)
target_link_libraries(test_contact_model_integration PUBLIC
    src
    mesh
    contact_models
)

# Include directories
target_include_directories(test_contact_model_integration PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/mesh
    ${CMAKE_SOURCE_DIR}/include/mesh/cell_types
    ${CMAKE_SOURCE_DIR}/include/contact_models
    ${CMAKE_SOURCE_DIR}/include/math_modules
    ${CMAKE_SOURCE_DIR}/include/uspg
)

# Add to CTest
add_test(NAME test_contact_model_integration
         COMMAND test_contact_model_integration)

# Set test properties
set_tests_properties(test_contact_model_integration PROPERTIES
    TIMEOUT 120
    LABELS "integration;contact_models;phase5"
)
