# The version of cmake required
cmake_minimum_required(VERSION 3.10)

# Explicit source file list
set(SRC_FILES
    main.cpp
)

# Locate the openMP library to parallelize the code
find_package(OpenMP REQUIRED)

# Create the main executable
add_executable(simucell3d ${SRC_FILES})

# Indicate to main the path to project source directory
add_definitions(-DPROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

# Link the libraries with the main executable
target_link_libraries(simucell3d PUBLIC
    src
    OpenMP::OpenMP_CXX
)

target_include_directories(simucell3d PRIVATE
    "${PROJECT_SOURCE_DIR}/include/"
    "${PROJECT_SOURCE_DIR}/include/io"
    "${PROJECT_SOURCE_DIR}/include/mesh"
    "${PROJECT_BINARY_DIR}/generated"   # version.hpp (configured in the top-level CMakeLists.txt)
)

# `simucell3d --version` must exit 0 and print "SimuCell3D <PROJECT_VERSION> (<sha>)"
add_test(NAME cli_version_flag
    COMMAND ${CMAKE_COMMAND}
        -DSIMUCELL3D_BIN=$<TARGET_FILE:simucell3d>
        -DEXPECTED_VERSION=${PROJECT_VERSION}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_version_flag.cmake
)
set_tests_properties(cli_version_flag PROPERTIES LABELS "integration" TIMEOUT 60)

# Create symlink in build root for backward compatibility with scripts
add_custom_command(TARGET simucell3d POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        $<TARGET_FILE:simucell3d>
        ${CMAKE_BINARY_DIR}/simucell3d
    COMMENT "Creating symlink: ${CMAKE_BINARY_DIR}/simucell3d -> $<TARGET_FILE:simucell3d>"
)
