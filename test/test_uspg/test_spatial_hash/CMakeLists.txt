
cmake_minimum_required(VERSION 3.0)
set(CMAKE_CXX_STANDARD 17)


# Compile the files containing the tests to run
add_executable(test_spatial_hash test_spatial_hash.cpp)

target_link_libraries(test_spatial_hash PUBLIC uspg)


target_include_directories(test_spatial_hash PUBLIC
                                        "${PROJECT_SOURCE_DIR}/test/test_uspg/test_spatial_hash/"
                                        "${PROJECT_SOURCE_DIR}/include"
                                        "${PROJECT_SOURCE_DIR}/include/uspg"
                                        "${PROJECT_SOURCE_DIR}/include/math_modules"

)


# Run the core USPG compatibility tests
add_test(NAME spatial_hash_update_dimensions_test   COMMAND test_spatial_hash update_dimensions_test)
add_test(NAME spatial_hash_get_neighborhood_test    COMMAND test_spatial_hash get_neighborhood_test)
add_test(NAME spatial_hash_place_object_test        COMMAND test_spatial_hash place_object_test)
add_test(NAME spatial_hash_get_grid_content_test    COMMAND test_spatial_hash get_grid_content_test)

# Run spatial hash specific tests
add_test(NAME spatial_hash_morton_encoding_test     COMMAND test_spatial_hash morton_encoding_test)
add_test(NAME spatial_hash_boundary_conditions_test COMMAND test_spatial_hash boundary_conditions_test)
add_test(NAME spatial_hash_sparse_storage_test      COMMAND test_spatial_hash sparse_storage_test)
