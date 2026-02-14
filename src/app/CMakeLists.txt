# The version of cmake required
cmake_minimum_required(VERSION 3.0)

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
)

# Create symlink in build root for backward compatibility with scripts
add_custom_command(TARGET simucell3d POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        $<TARGET_FILE:simucell3d>
        ${CMAKE_BINARY_DIR}/simucell3d
    COMMENT "Creating symlink: ${CMAKE_BINARY_DIR}/simucell3d -> $<TARGET_FILE:simucell3d>"
)
