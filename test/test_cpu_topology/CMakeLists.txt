cmake_minimum_required(VERSION 3.0)
set(CMAKE_CXX_STANDARD 17)

# cpu_topology.hpp is header-only; it needs OpenMP for the place-list queries
add_executable(test_cpu_topology test_cpu_topology.cpp)

find_package(OpenMP REQUIRED)
target_link_libraries(test_cpu_topology PUBLIC OpenMP::OpenMP_CXX)

target_include_directories(test_cpu_topology PUBLIC "${PROJECT_SOURCE_DIR}/include")

add_test(NAME test_cpu_topology_parse_cpu_list                COMMAND test_cpu_topology parse_cpu_list_test)
add_test(NAME test_cpu_topology_placement_4c8t                COMMAND test_cpu_topology placement_4c8t_test)
add_test(NAME test_cpu_topology_placement_adjacent_siblings   COMMAND test_cpu_topology placement_adjacent_siblings_test)
add_test(NAME test_cpu_topology_placement_restricted          COMMAND test_cpu_topology placement_restricted_test)
add_test(NAME test_cpu_topology_placement_two_sockets         COMMAND test_cpu_topology placement_two_sockets_test)
add_test(NAME test_cpu_topology_live_detection                COMMAND test_cpu_topology live_detection_test)
