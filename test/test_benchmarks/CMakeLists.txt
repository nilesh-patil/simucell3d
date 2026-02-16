cmake_minimum_required(VERSION 3.0)

# Use C++17 standard
set(CMAKE_CXX_STANDARD 17)

# Indicate to the test source the path to project source directory
add_definitions(-DPROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

# Common include directories for all benchmark tests
set(BENCHMARK_INCLUDES
    "${PROJECT_SOURCE_DIR}/include/"
    "${PROJECT_SOURCE_DIR}/include/mesh"
    "${PROJECT_SOURCE_DIR}/include/contact_models"
    "${PROJECT_SOURCE_DIR}/include/math_modules"
)

# Common libraries
set(BENCHMARK_LIBS src mesh contact_models)


# =============================================================================================
# Contact Detection Latency Micro-benchmark
# =============================================================================================

add_executable(test_contact_latency
    test_contact_latency.cpp
)
target_link_libraries(test_contact_latency PUBLIC ${BENCHMARK_LIBS})
target_include_directories(test_contact_latency PUBLIC ${BENCHMARK_INCLUDES})

# Register contact latency tests
add_test(NAME benchmark_uspg_latency_small   COMMAND test_contact_latency test_uspg_latency_small)
add_test(NAME benchmark_sap_latency_small    COMMAND test_contact_latency test_sap_latency_small)
add_test(NAME benchmark_uspg_latency_medium  COMMAND test_contact_latency test_uspg_latency_medium)
add_test(NAME benchmark_sap_latency_medium   COMMAND test_contact_latency test_sap_latency_medium)
add_test(NAME benchmark_uspg_latency_large   COMMAND test_contact_latency test_uspg_latency_large)
add_test(NAME benchmark_sap_latency_large    COMMAND test_contact_latency test_sap_latency_large)
add_test(NAME benchmark_scaling_behavior     COMMAND test_contact_latency test_scaling_behavior)


# =============================================================================================
# Atomic Contention Micro-benchmark
# =============================================================================================

add_executable(test_atomic_contention
    test_atomic_contention.cpp
)
target_link_libraries(test_atomic_contention PUBLIC ${BENCHMARK_LIBS})
target_include_directories(test_atomic_contention PUBLIC ${BENCHMARK_INCLUDES})

# Find threads library (required for std::mutex benchmarks)
find_package(Threads REQUIRED)
target_link_libraries(test_atomic_contention PRIVATE Threads::Threads)

# Register atomic contention tests
add_test(NAME benchmark_uncontended_mutex_cost       COMMAND test_atomic_contention test_uncontended_mutex_cost)
add_test(NAME benchmark_contended_mutex_scaling      COMMAND test_atomic_contention test_contended_mutex_scaling)
add_test(NAME benchmark_atomic_vs_mutex_accumulation COMMAND test_atomic_contention test_atomic_vs_mutex_accumulation)
add_test(NAME benchmark_false_sharing_detection      COMMAND test_atomic_contention test_false_sharing_detection)
add_test(NAME benchmark_per_node_mutex_pattern       COMMAND test_atomic_contention test_per_node_mutex_pattern)


# =============================================================================================
# Memory Bandwidth Micro-benchmark
# =============================================================================================

add_executable(test_memory_bandwidth
    test_memory_bandwidth.cpp
)
target_link_libraries(test_memory_bandwidth PUBLIC ${BENCHMARK_LIBS})
target_include_directories(test_memory_bandwidth PUBLIC ${BENCHMARK_INCLUDES})
target_link_libraries(test_memory_bandwidth PRIVATE Threads::Threads)

# Register memory bandwidth tests
add_test(NAME benchmark_sequential_read_bandwidth     COMMAND test_memory_bandwidth test_sequential_read_bandwidth)
add_test(NAME benchmark_vec3_aos_bandwidth             COMMAND test_memory_bandwidth test_vec3_aos_bandwidth)
add_test(NAME benchmark_random_access_latency          COMMAND test_memory_bandwidth test_random_access_latency)
add_test(NAME benchmark_soa_vs_aos_comparison          COMMAND test_memory_bandwidth test_soa_vs_aos_comparison)
add_test(NAME benchmark_parallel_bandwidth_scaling     COMMAND test_memory_bandwidth test_parallel_bandwidth_scaling)
