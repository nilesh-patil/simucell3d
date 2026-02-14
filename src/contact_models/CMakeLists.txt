#The verscontact_modelsn of cmake required
cmake_minimum_required(VERSION 3.0)

# Explicit source file list
set(SRC_FILES
    contact_face_face_via_coupling.cpp
    contact_model_abstract.cpp
    contact_node_face_via_spring.cpp
    contact_node_node_via_coupling.cpp
    contact_detection_strategy.cpp
    contact_detection_uspg_strategy.cpp
    contact_detection_sap_strategy.cpp
)

#Create the source library
add_library(contact_models ${SRC_FILES})

#Locate the openMP library to parallelize the code
find_package(OpenMP REQUIRED)

#Indicate to the modules the path to project source directory
#add_definitcontact_modelsns(-DPROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

target_include_directories(contact_models PUBLIC    "${PROJECT_SOURCE_DIR}/include/"
                                        "${PROJECT_SOURCE_DIR}/include/mesh" 
                                        "${PROJECT_SOURCE_DIR}/include/uspg"  
                                        "${PROJECT_SOURCE_DIR}/include/contact_models"  
                                        "${PROJECT_SOURCE_DIR}/include/math_modules"  
                                                                                                  
)
                                   
target_link_libraries(contact_models PUBLIC 
    OpenMP::OpenMP_CXX     
)
