#The version of cmake required
cmake_minimum_required(VERSION 3.0)

# Explicit source file list
set(SRC_FILES
    mat33.cpp
    vec3.cpp
)

#Create the source library
add_library(math_modules ${SRC_FILES})


target_include_directories(math_modules PUBLIC  "${PROJECT_SOURCE_DIR}/include/"
                                                "${PROJECT_SOURCE_DIR}/include/math_modules"                                             
)
