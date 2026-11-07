cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 17)


#Compile the files containing the tests to run
add_executable(test_local_mesh_refiner
    test_local_mesh_refiner.cpp
)

#Indicate to the tests the path to project source directory (fixtures)
add_definitions(-DPROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

target_link_libraries(test_local_mesh_refiner PUBLIC  
    src 
    mesh
    triangulation_modules
    io
)

target_include_directories(test_local_mesh_refiner PUBLIC     
    "${PROJECT_SOURCE_DIR}/include/"
    "${PROJECT_SOURCE_DIR}/include/triangulation_modules"
    "${PROJECT_SOURCE_DIR}/include/math_modules"
    "${PROJECT_SOURCE_DIR}/include/mesh"
    "${PROJECT_SOURCE_DIR}/test/test_triangulation_modules/test_local_mesh_refiner"                                                                           
    "${PROJECT_SOURCE_DIR}/include/io"
)



#Run the test
add_test(NAME triangulation_modules_local_mesh_refiner_can_be_merged      COMMAND test_local_mesh_refiner can_be_merged_test)
#add_test(NAME triangulation_modules_local_mesh_refiner_prepare_merger_test_1    COMMAND test_local_mesh_refiner prepare_merger_test_1)
#add_test(NAME triangulation_modules_local_mesh_refiner_prepare_merger_test_2    COMMAND test_local_mesh_refiner prepare_merger_test_2)
add_test(NAME triangulation_modules_local_mesh_refiner_edge_swap_test           COMMAND test_local_mesh_refiner edge_swap_test)
add_test(NAME triangulation_modules_local_mesh_refiner_split_edge_test          COMMAND test_local_mesh_refiner split_edge_test)
add_test(NAME triangulation_modules_local_mesh_refiner_merge_edge_test          COMMAND test_local_mesh_refiner merge_edge_test)
add_test(NAME triangulation_modules_local_mesh_refiner_get_triangle_score_test  COMMAND test_local_mesh_refiner get_triangle_score_test)
add_test(NAME triangulation_modules_local_mesh_refiner_refine_mesh_keeps_winding_consistent COMMAND test_local_mesh_refiner refine_mesh_keeps_winding_consistent_test)
add_test(NAME triangulation_modules_local_mesh_refiner_winding_self_check COMMAND test_local_mesh_refiner winding_self_check_test)
add_test(NAME triangulation_modules_local_mesh_refiner_refine_meshes_contains_failure         COMMAND test_local_mesh_refiner refine_meshes_contains_failure_test)
add_test(NAME triangulation_modules_local_mesh_refiner_refine_meshes_contains_failure_threads COMMAND test_local_mesh_refiner refine_meshes_contains_failure_threads_test)
add_test(NAME triangulation_modules_local_mesh_refiner_refine_meshes_rethrows_fatal           COMMAND test_local_mesh_refiner refine_meshes_rethrows_fatal_test)
add_test(NAME triangulation_modules_local_mesh_refiner_refine_meshes_public_wrapper           COMMAND test_local_mesh_refiner refine_meshes_public_wrapper_test)
