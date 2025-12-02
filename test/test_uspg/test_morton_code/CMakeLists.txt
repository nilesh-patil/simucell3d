
cmake_minimum_required(VERSION 3.0)
set(CMAKE_CXX_STANDARD 17)


#Compile the files containing the tests to run
add_executable(test_morton_code test_morton_code.cpp)

target_link_libraries(test_morton_code PUBLIC uspg math_modules)


target_include_directories(test_morton_code PUBLIC
                                        "${PROJECT_SOURCE_DIR}/test/test_uspg/test_morton_code/"
                                        "${PROJECT_SOURCE_DIR}/include/**"
                                        "${PROJECT_SOURCE_DIR}/include/uspg"
                                        "${PROJECT_SOURCE_DIR}/include/math_modules"

)


# Morton code tests
add_test(NAME morton_encode_test                          COMMAND test_morton_code morton_encode_test)
add_test(NAME morton_spatial_locality_test                COMMAND test_morton_code morton_spatial_locality_test)
add_test(NAME morton_boundary_test                        COMMAND test_morton_code morton_boundary_test)
add_test(NAME morton_sorting_preserves_correctness_test   COMMAND test_morton_code morton_sorting_preserves_correctness_test)
