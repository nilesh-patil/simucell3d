cmake_minimum_required(VERSION 3.0)

# Use C++17 standard
set(CMAKE_CXX_STANDARD 17)

# Indicate to the test source the path to project source directory
add_definitions(-DPROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

# --- Determinism test ---
add_executable(determinism_test determinism_test.cpp)
target_link_libraries(determinism_test PUBLIC
    src mesh io contact_models time_integration triangulation_modules
)
target_include_directories(determinism_test PUBLIC
    "${PROJECT_SOURCE_DIR}/include/"
    "${PROJECT_SOURCE_DIR}/include/mesh"
    "${PROJECT_SOURCE_DIR}/include/io"
    "${PROJECT_SOURCE_DIR}/include/contact_models"
    "${PROJECT_SOURCE_DIR}/include/time_integration"
    "${PROJECT_SOURCE_DIR}/include/triangulation_modules"
    "${PROJECT_SOURCE_DIR}/include/math_modules"
)
add_test(NAME stability_determinism_single_thread            COMMAND determinism_test test_determinism_single_thread)
add_test(NAME stability_determinism_energy_reproducibility   COMMAND determinism_test test_determinism_energy_reproducibility)

# --- Timestep convergence test ---
add_executable(timestep_convergence_test timestep_convergence_test.cpp)
target_link_libraries(timestep_convergence_test PUBLIC
    src mesh io contact_models time_integration triangulation_modules
)
target_include_directories(timestep_convergence_test PUBLIC
    "${PROJECT_SOURCE_DIR}/include/"
    "${PROJECT_SOURCE_DIR}/include/mesh"
    "${PROJECT_SOURCE_DIR}/include/io"
    "${PROJECT_SOURCE_DIR}/include/contact_models"
    "${PROJECT_SOURCE_DIR}/include/time_integration"
    "${PROJECT_SOURCE_DIR}/include/triangulation_modules"
    "${PROJECT_SOURCE_DIR}/include/math_modules"
)
add_test(NAME stability_timestep_convergence_order           COMMAND timestep_convergence_test test_convergence_order)
add_test(NAME stability_timestep_energy_convergence          COMMAND timestep_convergence_test test_energy_convergence)

# --- Extreme parameters test ---
add_executable(extreme_parameters_test extreme_parameters_test.cpp)
target_link_libraries(extreme_parameters_test PUBLIC
    src mesh io contact_models time_integration triangulation_modules
)
target_include_directories(extreme_parameters_test PUBLIC
    "${PROJECT_SOURCE_DIR}/include/"
    "${PROJECT_SOURCE_DIR}/include/mesh"
    "${PROJECT_SOURCE_DIR}/include/io"
    "${PROJECT_SOURCE_DIR}/include/contact_models"
    "${PROJECT_SOURCE_DIR}/include/time_integration"
    "${PROJECT_SOURCE_DIR}/include/triangulation_modules"
    "${PROJECT_SOURCE_DIR}/include/math_modules"
)
add_test(NAME stability_extreme_zero_bulk_modulus            COMMAND extreme_parameters_test test_zero_bulk_modulus)
add_test(NAME stability_extreme_high_pressure                COMMAND extreme_parameters_test test_high_pressure)
add_test(NAME stability_extreme_degenerate_mesh              COMMAND extreme_parameters_test test_degenerate_mesh)
add_test(NAME stability_extreme_contact_cutoff_boundary      COMMAND extreme_parameters_test test_contact_cutoff_boundary)
