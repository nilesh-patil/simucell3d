cmake_minimum_required(VERSION 3.0)

#Use CPP 17
set(CMAKE_CXX_STANDARD 17)

#Indicate to main the path to project source directory
add_definitions(-DPROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")


#Compile the files containing the tests to run
add_executable(test_parameter_reader test_parameter_reader.cpp)

target_link_libraries(test_parameter_reader PUBLIC   
    src
    mesh
    io
)

#Add all the header files required to run the root file of the project
target_include_directories(test_parameter_reader PUBLIC "${PROJECT_SOURCE_DIR}/include/")
target_include_directories(test_parameter_reader PUBLIC "${PROJECT_SOURCE_DIR}/include/mesh")
target_include_directories(test_parameter_reader PUBLIC "${PROJECT_SOURCE_DIR}/include/io")

#Run the test - original tests
add_test(NAME io_parameter_reader_read_numerical_parameters_test  COMMAND test_parameter_reader read_numerical_parameters_test)
add_test(NAME io_parameter_reader_read_cell_type_parameters_test  COMMAND test_parameter_reader read_cell_type_parameters_test)
add_test(NAME io_parameter_reader_read_face_type_parameters_test  COMMAND test_parameter_reader read_face_type_parameters_test)
add_test(NAME io_parameter_reader_read_biomechanical_parameters_test  COMMAND test_parameter_reader read_biomechanical_parameters_test)

#Run the test - contact detection algorithm parameter tests
add_test(NAME io_parameter_reader_contact_detection_default_enum_value  COMMAND test_parameter_reader test_contact_detection_default_enum_value)
add_test(NAME io_parameter_reader_contact_detection_parse_uspg  COMMAND test_parameter_reader test_contact_detection_parse_uspg)
add_test(NAME io_parameter_reader_contact_detection_parse_sweep_and_prune  COMMAND test_parameter_reader test_contact_detection_parse_sweep_and_prune)
add_test(NAME io_parameter_reader_contact_detection_parse_adaptive  COMMAND test_parameter_reader test_contact_detection_parse_adaptive)
add_test(NAME io_parameter_reader_contact_detection_optional_parameter_defaults_to_uspg  COMMAND test_parameter_reader test_contact_detection_optional_parameter_defaults_to_uspg)
add_test(NAME io_parameter_reader_contact_detection_invalid_algorithm_throws  COMMAND test_parameter_reader test_contact_detection_invalid_algorithm_throws)
add_test(NAME io_parameter_reader_contact_detection_case_insensitivity  COMMAND test_parameter_reader test_contact_detection_case_insensitivity)








