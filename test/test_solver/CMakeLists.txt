cmake_minimum_required(VERSION 3.10)

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
add_test(NAME solver_test_local_ids_resynced_after_cell_removal COMMAND test_solver test_local_ids_resynced_after_cell_removal)
add_test(NAME solver_test_unstable_cell_removed              COMMAND test_solver test_unstable_cell_removed)
add_test(NAME solver_test_thread_placement                    COMMAND test_solver test_thread_placement)
add_test(NAME solver_test_contact_model_throws_on_diverged_domain COMMAND test_solver test_contact_model_throws_on_diverged_domain)
add_test(NAME solver_test_inverted_cell_removed               COMMAND test_solver test_inverted_cell_removed)

# Release-style builds only (NDEBUG): in Debug the refiner's asserts fire first, so the test returns 77, which
# CTest then reports as skipped instead of passed
add_test(NAME solver_test_refinement_failure_end_to_end       COMMAND test_solver test_refinement_failure_end_to_end)
set_tests_properties(solver_test_refinement_failure_end_to_end PROPERTIES SKIP_RETURN_CODE 77)

# Handling of the failures reported by local_mesh_refiner::refine_meshes()
add_test(NAME solver_test_refinement_failure_removed          COMMAND test_solver test_refinement_failure_removed)
add_test(NAME solver_test_refinement_mass_failure_stops       COMMAND test_solver test_refinement_mass_failure_stops)
add_test(NAME solver_test_refinement_failure_all_cells_stop   COMMAND test_solver test_refinement_failure_all_cells_stop)
add_test(NAME solver_test_refinement_failure_limit            COMMAND test_solver test_refinement_failure_limit)

# Default ctest labels (tiers) for the tests registered above
include(${PROJECT_SOURCE_DIR}/test/test_labels.cmake)
