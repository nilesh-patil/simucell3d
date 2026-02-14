cmake_minimum_required(VERSION 3.0)

# Use CPP 17
set(CMAKE_CXX_STANDARD 17)

# Indicate to main the path to project source directory
add_definitions(-DPROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

# Compile the files containing the tests to run
add_executable(test_statistics_writer test_statistics_writer.cpp)

target_link_libraries(test_statistics_writer PUBLIC   src
                                                      mesh
                                                      io
)

# Add all the header files required to run the root file of the project
target_include_directories(test_statistics_writer PUBLIC
    "${PROJECT_SOURCE_DIR}/include/"
    "${PROJECT_SOURCE_DIR}/include/mesh"
    "${PROJECT_SOURCE_DIR}/include/io"
)

# Run the tests
add_test(NAME io_statistics_writer_csv_file_writer_creation   COMMAND test_statistics_writer test_csv_file_writer_creation)
add_test(NAME io_statistics_writer_csv_header_format          COMMAND test_statistics_writer test_csv_header_format)
add_test(NAME io_statistics_writer_string_writer_accumulation COMMAND test_statistics_writer test_string_writer_accumulation)
add_test(NAME io_statistics_writer_write_data_formatting      COMMAND test_statistics_writer test_write_data_formatting)
add_test(NAME io_statistics_writer_multiple_iterations        COMMAND test_statistics_writer test_multiple_iterations)
add_test(NAME io_statistics_writer_invalid_path               COMMAND test_statistics_writer test_csv_writer_invalid_path)
