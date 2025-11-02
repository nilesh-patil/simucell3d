#The version of cmake required
cmake_minimum_required(VERSION 3.0)

# Explicit source file list
set(SRC_FILES
    uspg_3d.cpp
    uspg_4d.cpp
    uspg_abstract.cpp
)

#Create the source library
add_library(uspg ${SRC_FILES})


target_include_directories(uspg PUBLIC  "${PROJECT_SOURCE_DIR}/include/"
                                        "${PROJECT_SOURCE_DIR}/include/uspg"
                                        "${PROJECT_SOURCE_DIR}/include/math_modules"

)



