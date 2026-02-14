#The version of cmake required
cmake_minimum_required(VERSION 3.0)

# Explicit source file list
set(SRC_FILES
    cell.cpp
    edge.cpp
    face.cpp
    node.cpp
)

#Create the source library
add_library(mesh ${SRC_FILES})


target_include_directories(mesh PUBLIC  "${PROJECT_SOURCE_DIR}/include/"
                                        "${PROJECT_SOURCE_DIR}/include/mesh"
                                        "${PROJECT_SOURCE_DIR}/include/time_integration"
                                        "${PROJECT_SOURCE_DIR}/include/mesh/cell_types"
                                        "${PROJECT_SOURCE_DIR}/include/math_modules"
)

target_link_libraries(mesh PUBLIC 
    math_modules    
)

