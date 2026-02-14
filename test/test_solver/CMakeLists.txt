cmake_minimum_required(VERSION 3.0)

# Use C++17 standard
set(CMAKE_CXX_STANDARD 17)

# Indicate to the test source the path to project source directory
# This allows tests to locate parameter files and test data
add_definitions(-DPROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

# Compile the test executable
add_executable(test_solver test_solver.cpp)

# Link required libraries
# - src: Main solver and core functionality
# - mesh: Cell and mesh data structures
# - io: simulation_initializer, parameter_reader
# - contact_models: Required by solver
# - time_integration: Required by solver
# - triangulation_modules: Required by solver
target_link_libraries(test_solver PUBLIC
    src
    mesh
    io
    contact_models
    time_integration
    triangulation_modules
)

# Include directories for header files
target_include_directories(test_solver PUBLIC
    "${PROJECT_SOURCE_DIR}/include/"
    "${PROJECT_SOURCE_DIR}/include/mesh"
    "${PROJECT_SOURCE_DIR}/include/io"
    "${PROJECT_SOURCE_DIR}/include/contact_models"
    "${PROJECT_SOURCE_DIR}/include/time_integration"
    "${PROJECT_SOURCE_DIR}/include/triangulation_modules"
    "${PROJECT_SOURCE_DIR}/include/math_modules"
)

# Register tests with CTest
# Test naming convention: <module>_<submodule>_<test_function_name>
add_test(NAME solver_test_solver_construction                 COMMAND test_solver test_solver_construction)
add_test(NAME solver_test_detect_simulation_phase             COMMAND test_solver test_detect_simulation_phase)
add_test(NAME solver_test_workload_heterogeneity_calculation  COMMAND test_solver test_workload_heterogeneity_calculation)
