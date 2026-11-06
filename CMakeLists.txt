cmake_minimum_required(VERSION 3.10)

#Use CPP 17
set(CMAKE_CXX_STANDARD 17)


#The name and version of the project
project(SimuCell3D VERSION 2.1.0)

# Generate compile_commands.json for IDE/tooling support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Expose the version (and the git short sha at configure time) to the code
# through a generated header: ${PROJECT_BINARY_DIR}/generated/version.hpp
set(SIMUCELL3D_GIT_SHA "unknown")
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE _simucell3d_git_sha
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _simucell3d_git_result
    )
    if(_simucell3d_git_result EQUAL 0 AND NOT _simucell3d_git_sha STREQUAL "")
        set(SIMUCELL3D_GIT_SHA "${_simucell3d_git_sha}")
    endif()
endif()
configure_file(
    ${PROJECT_SOURCE_DIR}/include/version.hpp.in
    ${PROJECT_BINARY_DIR}/generated/version.hpp
    @ONLY
)
message(STATUS "SimuCell3D version ${PROJECT_VERSION} (${SIMUCELL3D_GIT_SHA})")

#Check if the python bindings should be compiled
if(NOT DEFINED ENABLE_PYTHON_BINDINGS)
    set(ENABLE_PYTHON_BINDINGS FALSE)
endif()

if(ENABLE_PYTHON_BINDINGS)
    message(STATUS "Python bindings enabled")
else()
    message(STATUS "Python bindings disabled")
endif()


#Activate unit testing
enable_testing()

#Contains the external library such as tinyxml (the xml reader)
add_subdirectory(lib)

#Contains the source files (.cpp)
add_subdirectory(src bin) 

#Library containing all the tests to run
add_subdirectory(test)

# Application executable (in src/app/)
add_subdirectory(src/app)

#In debug mode, we do not use openMP
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    message("-- Building in debug mode")
else()
    message("-- Building in release mode")
endif()





