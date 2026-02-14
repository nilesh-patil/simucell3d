

cmake_minimum_required(VERSION 3.0)
set(CMAKE_CXX_STANDARD 17)


#Compile the files containing the tests to run
add_executable(test_uspg_4d test_uspg_4d.cpp)

target_link_libraries(test_uspg_4d PUBLIC uspg)


target_include_directories(test_uspg_4d PUBLIC  
                                        "${PROJECT_SOURCE_DIR}/test/test_uspg/test_uspg_4d/"  
                                        "${PROJECT_SOURCE_DIR}/include/**"
                                        "${PROJECT_SOURCE_DIR}/include/uspg"
                                        "${PROJECT_SOURCE_DIR}/include/math_modules"
                                      
)



#Run the test - existing tests
add_test(NAME uspg_4d_update_dimensions_test       COMMAND test_uspg_4d update_dimensions_test)
add_test(NAME uspg_4d_get_neighborhood_test             COMMAND test_uspg_4d get_neighborhood_test)
add_test(NAME uspg_4d_place_object_test                 COMMAND test_uspg_4d place_object_test)
add_test(NAME uspg_4d_get_grid_content_test             COMMAND test_uspg_4d get_grid_content_test)

# New tests for vector storage optimization (Phase 1)
add_test(NAME uspg_4d_vector_storage_size_test          COMMAND test_uspg_4d vector_storage_size_test)
add_test(NAME uspg_4d_vector_push_back_ordering_test    COMMAND test_uspg_4d vector_push_back_ordering_test)
add_test(NAME uspg_4d_empty_voxel_returns_empty_vector_test COMMAND test_uspg_4d empty_voxel_returns_empty_vector_test)
add_test(NAME uspg_4d_multiple_insertions_same_voxel_test   COMMAND test_uspg_4d multiple_insertions_same_voxel_test)
add_test(NAME uspg_4d_neighborhood_returns_vector_test      COMMAND test_uspg_4d neighborhood_returns_vector_test)





