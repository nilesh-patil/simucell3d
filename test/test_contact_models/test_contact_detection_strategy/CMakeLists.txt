cmake_minimum_required(VERSION 3.0)

# Use C++17
set(CMAKE_CXX_STANDARD 17)

# Indicate to main the path to project source directory
add_definitions(-DPROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

# Compile the test executable
add_executable(test_contact_detection_strategy
    test_contact_detection_strategy.cpp
)

# Link required libraries
target_link_libraries(test_contact_detection_strategy PUBLIC
    src
    mesh
    contact_models
)

# Add all the header files required to run the tests
target_include_directories(test_contact_detection_strategy PUBLIC
    "${PROJECT_SOURCE_DIR}/include/"
    "${PROJECT_SOURCE_DIR}/include/mesh"
    "${PROJECT_SOURCE_DIR}/include/contact_models"
    "${PROJECT_SOURCE_DIR}/include/math_modules"
)

# Register all tests with CTest
# Factory Tests
add_test(NAME contact_detection_strategy_factory_creates_uspg_by_default
    COMMAND test_contact_detection_strategy test_factory_creates_uspg_by_default)

add_test(NAME contact_detection_strategy_factory_creates_sap_for_sweep_and_prune
    COMMAND test_contact_detection_strategy test_factory_creates_sap_for_sweep_and_prune)

add_test(NAME contact_detection_strategy_factory_returns_non_null_pointer
    COMMAND test_contact_detection_strategy test_factory_returns_non_null_pointer)

add_test(NAME contact_detection_strategy_strategies_are_polymorphic
    COMMAND test_contact_detection_strategy test_strategies_are_polymorphic)

# Interface Contract Tests
add_test(NAME contact_detection_strategy_prepare_accepts_empty_cell_list
    COMMAND test_contact_detection_strategy test_prepare_accepts_empty_cell_list)

add_test(NAME contact_detection_strategy_get_candidate_faces_returns_vector
    COMMAND test_contact_detection_strategy test_get_candidate_faces_returns_vector)

# Type Identity Tests
add_test(NAME contact_detection_strategy_type_identity_is_preserved
    COMMAND test_contact_detection_strategy test_strategy_type_identity_is_preserved)

add_test(NAME contact_detection_strategy_factory_handles_multiple_algorithm_types
    COMMAND test_contact_detection_strategy test_factory_handles_multiple_algorithm_types)


# =============================================================================================
# SAP Algorithm Tests
# =============================================================================================

# Compile the SAP algorithm test executable
add_executable(test_sap_algorithm
    test_sap_algorithm.cpp
)

# Link required libraries
target_link_libraries(test_sap_algorithm PUBLIC
    src
    mesh
    contact_models
)

# Add all the header files required to run the tests
target_include_directories(test_sap_algorithm PUBLIC
    "${PROJECT_SOURCE_DIR}/include/"
    "${PROJECT_SOURCE_DIR}/include/mesh"
    "${PROJECT_SOURCE_DIR}/include/contact_models"
    "${PROJECT_SOURCE_DIR}/include/math_modules"
)

# Register SAP algorithm tests with CTest

# Basic Tests
add_test(NAME sap_algorithm_empty_input_returns_empty_results
    COMMAND test_sap_algorithm test_sap_empty_input_returns_empty_results)

add_test(NAME sap_algorithm_single_aabb_no_self_overlap
    COMMAND test_sap_algorithm test_sap_single_aabb_no_self_overlap)

# AABB Structure Tests
add_test(NAME sap_algorithm_aabb_structure_correctness
    COMMAND test_sap_algorithm test_aabb_structure_correctness)

add_test(NAME sap_algorithm_aabb_3axis_overlap_predicate
    COMMAND test_sap_algorithm test_aabb_3axis_overlap_predicate)

add_test(NAME sap_algorithm_aabb_expansion_by_cutoff
    COMMAND test_sap_algorithm test_aabb_expansion_by_cutoff)

# Conservative Property Tests
add_test(NAME sap_algorithm_conservative_property_no_false_negatives
    COMMAND test_sap_algorithm test_conservative_property_no_false_negatives)

add_test(NAME sap_algorithm_point_aabb_containment_predicate
    COMMAND test_sap_algorithm test_point_aabb_containment_predicate)

# Multi-Box Overlap Tests
add_test(NAME sap_algorithm_multiple_aabbs_clustering
    COMMAND test_sap_algorithm test_multiple_aabbs_clustering)

add_test(NAME sap_algorithm_transitivity_three_box_chain
    COMMAND test_sap_algorithm test_transitivity_three_box_chain)

# Edge Case Tests
add_test(NAME sap_algorithm_extreme_aabb_sizes
    COMMAND test_sap_algorithm test_extreme_aabb_sizes)

add_test(NAME sap_algorithm_boundary_simulation_domain
    COMMAND test_sap_algorithm test_boundary_simulation_domain)

add_test(NAME sap_algorithm_self_exclusion_contract
    COMMAND test_sap_algorithm test_self_exclusion_contract)

# Lifecycle Tests
add_test(NAME sap_algorithm_prepare_query_lifecycle
    COMMAND test_sap_algorithm test_prepare_query_lifecycle)

add_test(NAME sap_algorithm_contact_cutoff_parameter_usage
    COMMAND test_sap_algorithm test_contact_cutoff_parameter_usage)

add_test(NAME sap_algorithm_algorithm_type_identification
    COMMAND test_sap_algorithm test_algorithm_type_identification)


# =============================================================================================
# SAP OpenMP Parallelization Tests
# =============================================================================================

# Compile the SAP OpenMP test executable
add_executable(test_sap_openmp
    test_sap_openmp.cpp
)

# Link required libraries (OpenMP is linked via global settings)
target_link_libraries(test_sap_openmp PUBLIC
    src
    mesh
    contact_models
)

# Add all the header files required to run the tests
target_include_directories(test_sap_openmp PUBLIC
    "${PROJECT_SOURCE_DIR}/include/"
    "${PROJECT_SOURCE_DIR}/include/mesh"
    "${PROJECT_SOURCE_DIR}/include/contact_models"
    "${PROJECT_SOURCE_DIR}/include/math_modules"
)

# Register SAP OpenMP tests with CTest
add_test(NAME sap_openmp_serial_parallel_equivalence_small
    COMMAND test_sap_openmp test_serial_parallel_equivalence_small)

add_test(NAME sap_openmp_determinism_multiple_runs
    COMMAND test_sap_openmp test_determinism_multiple_runs)

add_test(NAME sap_openmp_thread_count_independence
    COMMAND test_sap_openmp test_thread_count_independence)

add_test(NAME sap_openmp_performance_speedup
    COMMAND test_sap_openmp test_performance_speedup)

add_test(NAME sap_openmp_empty_input_thread_safety
    COMMAND test_sap_openmp test_empty_input_thread_safety)

add_test(NAME sap_openmp_large_scale_stress
    COMMAND test_sap_openmp test_large_scale_stress)
