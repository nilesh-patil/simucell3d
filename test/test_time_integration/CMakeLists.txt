cmake_minimum_required(VERSION 3.0)

# Use CPP 17
set(CMAKE_CXX_STANDARD 17)

# Compile the test executable
add_executable(time_integration_test
    test_time_integration.cpp
)

# Link with main source library
target_link_libraries(time_integration_test PUBLIC src)

# Add all required header file directories
target_include_directories(time_integration_test PUBLIC "${PROJECT_SOURCE_DIR}/include/")
target_include_directories(time_integration_test PUBLIC "${PROJECT_SOURCE_DIR}/include/io")
target_include_directories(time_integration_test PUBLIC "${PROJECT_SOURCE_DIR}/include/math_modules")
target_include_directories(time_integration_test PUBLIC "${PROJECT_SOURCE_DIR}/include/mesh")
target_include_directories(time_integration_test PUBLIC "${PROJECT_SOURCE_DIR}/include/mesh/cell_types")
target_include_directories(time_integration_test PUBLIC "${PROJECT_SOURCE_DIR}/include/time_integration")

# Add tests for time integration module
add_test(NAME time_integration_test_time_step_accumulation        COMMAND time_integration_test test_time_step_accumulation)
add_test(NAME time_integration_test_semi_implicit_euler_single_node COMMAND time_integration_test test_semi_implicit_euler_single_node)
add_test(NAME time_integration_test_velocity_damping              COMMAND time_integration_test test_velocity_damping)
add_test(NAME time_integration_test_energy_behavior               COMMAND time_integration_test test_energy_behavior)
add_test(NAME time_integration_test_force_response                COMMAND time_integration_test test_force_response)
add_test(NAME time_integration_test_zero_velocity_zero_force      COMMAND time_integration_test test_zero_velocity_zero_force)
add_test(NAME time_integration_test_very_small_time_step          COMMAND time_integration_test test_very_small_time_step)
add_test(NAME time_integration_test_kinetic_energy_calculation    COMMAND time_integration_test test_kinetic_energy_calculation)
add_test(NAME time_integration_test_multiple_cells_independence   COMMAND time_integration_test test_multiple_cells_independence)
add_test(NAME time_integration_test_overdamped_dynamics           COMMAND time_integration_test test_overdamped_dynamics)
