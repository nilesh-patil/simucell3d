cmake_minimum_required(VERSION 3.0)

# Use C++17 standard
set(CMAKE_CXX_STANDARD 17)

# Indicate to the test source the path to project source directory
add_definitions(-DPROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

# Common include directories for all scientific benchmark tests
set(SCIENTIFIC_BENCH_INCLUDES
    "${PROJECT_SOURCE_DIR}/include/"
    "${PROJECT_SOURCE_DIR}/include/mesh"
    "${PROJECT_SOURCE_DIR}/include/io"
    "${PROJECT_SOURCE_DIR}/include/contact_models"
    "${PROJECT_SOURCE_DIR}/include/time_integration"
    "${PROJECT_SOURCE_DIR}/include/triangulation_modules"
    "${PROJECT_SOURCE_DIR}/include/math_modules"
)

# Common libraries for all scientific benchmark tests
set(SCIENTIFIC_BENCH_LIBS src mesh io contact_models time_integration triangulation_modules)

# Energy conservation benchmark
add_executable(test_energy_conservation energy_conservation_benchmark.cpp)
target_link_libraries(test_energy_conservation PUBLIC ${SCIENTIFIC_BENCH_LIBS})
target_include_directories(test_energy_conservation PUBLIC ${SCIENTIFIC_BENCH_INCLUDES})

add_test(NAME scientific_energy_conservation_single_cell  COMMAND test_energy_conservation test_energy_conservation_single_cell)
add_test(NAME scientific_energy_conservation_two_cells    COMMAND test_energy_conservation test_energy_conservation_two_cells)
add_test(NAME scientific_energy_monotonic_dissipation     COMMAND test_energy_conservation test_energy_monotonic_dissipation)

# Pressure validation benchmark
add_executable(test_pressure_validation pressure_validation_benchmark.cpp)
target_link_libraries(test_pressure_validation PUBLIC ${SCIENTIFIC_BENCH_LIBS})
target_include_directories(test_pressure_validation PUBLIC ${SCIENTIFIC_BENCH_INCLUDES})

add_test(NAME scientific_pressure_physiological_range     COMMAND test_pressure_validation test_pressure_physiological_range)
add_test(NAME scientific_pressure_bulk_modulus_response    COMMAND test_pressure_validation test_pressure_bulk_modulus_response)
add_test(NAME scientific_laplace_pressure                 COMMAND test_pressure_validation test_laplace_pressure)

# Geometry validation benchmark
add_executable(test_geometry_validation geometry_validation_benchmark.cpp)
target_link_libraries(test_geometry_validation PUBLIC ${SCIENTIFIC_BENCH_LIBS})
target_include_directories(test_geometry_validation PUBLIC ${SCIENTIFIC_BENCH_INCLUDES})

add_test(NAME scientific_volume_preservation              COMMAND test_geometry_validation test_volume_preservation)
add_test(NAME scientific_mesh_quality                     COMMAND test_geometry_validation test_mesh_quality)
add_test(NAME scientific_face_normal_consistency           COMMAND test_geometry_validation test_face_normal_consistency)
add_test(NAME scientific_manifoldness                     COMMAND test_geometry_validation test_manifoldness)
add_test(NAME scientific_isoperimetric_ratio              COMMAND test_geometry_validation test_isoperimetric_ratio)

# Contact mechanics validation benchmark
add_executable(test_contact_validation contact_validation_benchmark.cpp)
target_link_libraries(test_contact_validation PUBLIC ${SCIENTIFIC_BENCH_LIBS})
target_include_directories(test_contact_validation PUBLIC ${SCIENTIFIC_BENCH_INCLUDES})

add_test(NAME scientific_contact_reciprocity              COMMAND test_contact_validation test_contact_reciprocity)
add_test(NAME scientific_force_balance_equilibrium        COMMAND test_contact_validation test_force_balance_equilibrium)
add_test(NAME scientific_momentum_conservation            COMMAND test_contact_validation test_momentum_conservation)
add_test(NAME scientific_contact_area_fraction            COMMAND test_contact_validation test_contact_area_fraction)
