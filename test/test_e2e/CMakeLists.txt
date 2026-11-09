cmake_minimum_required(VERSION 3.10)

# End-to-end tests: run the simucell3d executable on shortened copies of shipped parameter
# files and on invalid inputs. They need no extra build target; the helper scripts are
# CMake scripts (run with cmake -P) so they work wherever ctest works.
#
# Labels (see test/test_labels.cmake for the tier definitions):
#   integration  short runs, part of every CI run (Release, Debug, sanitizers)
#   long         longer growth runs, part of the long tier (.github/workflows/long.yml)

set(E2E_WORK_ROOT "${CMAKE_CURRENT_BINARY_DIR}")
set(E2E_RUN "${CMAKE_CURRENT_SOURCE_DIR}/run_simulation.cmake")
set(E2E_FAIL "${CMAKE_CURRENT_SOURCE_DIR}/expect_failure.cmake")

# simucell3d_add_e2e_run(<name> <parameter file> <duration s> <sampling period s>
#                        <threads> <min cells> <timeout s> <labels>)
function(simucell3d_add_e2e_run name parameter_file duration sampling threads min_cells timeout labels)
    add_test(NAME ${name}
        COMMAND ${CMAKE_COMMAND}
            -DSIMUCELL3D_BIN=$<TARGET_FILE:simucell3d>
            -DPARAMETER_FILE=${PROJECT_SOURCE_DIR}/${parameter_file}
            -DWORK_DIR=${E2E_WORK_ROOT}/${name}
            -DSIMULATION_DURATION=${duration}
            -DSAMPLING_PERIOD=${sampling}
            -DTHREADS=${threads}
            -DMIN_CELLS=${min_cells}
            -DSOURCE_DIR=${PROJECT_SOURCE_DIR}
            -P ${E2E_RUN}
    )
    set_tests_properties(${name} PROPERTIES
        TIMEOUT ${timeout}
        LABELS "${labels}"
        PROCESSORS ${threads}
    )
endfunction()

# Growth and division: one cell from parameters/longrun_fast_growth.xml (the fast-growth
# configuration of the 2.1 long runs) grows and divides once in the first 1000 iterations
# (1e-4 s at a time step of 1e-7 s). About 3 s in Release with 2 threads.
simucell3d_add_e2e_run(e2e_growth_division
    parameters/longrun_fast_growth.xml 1e-4 2e-5 2 2 900 "integration;e2e")

# The 434-cell vesicle of parameters/core/parameters_vesicle_quick.xml (default compile
# switches), cut to 2e-6 s = 20 iterations at a time step of 1e-7 s. Requiring all 434
# cells at the end also checks that no cell was removed as numerically unstable.
# Reads a 29 MB mesh and needs about 0.5 GB of memory.
simucell3d_add_e2e_run(e2e_vesicle_quick
    parameters/core/parameters_vesicle_quick.xml 2e-6 1e-6 2 434 900 "integration;e2e")

# Long tier: the same growth configuration for 7e-4 s of simulated time (7000
# iterations), from one cell to a few hundred cells through repeated divisions. The run
# must reach the end without an abort and with at least 100 cells (two local runs
# reached 391 and 443; growth is stochastic). 3 to 5 min in Release with 4
# threads, longer with sanitizers. Override with
# -DE2E_LONG_DURATION=<s> -DE2E_LONG_MIN_CELLS=<n>.
if(NOT DEFINED E2E_LONG_DURATION)
    set(E2E_LONG_DURATION 7e-4)
endif()
if(NOT DEFINED E2E_LONG_MIN_CELLS)
    set(E2E_LONG_MIN_CELLS 100)
endif()
simucell3d_add_e2e_run(e2e_growth_long
    parameters/longrun_fast_growth.xml ${E2E_LONG_DURATION} 2e-4 4 ${E2E_LONG_MIN_CELLS} 3600 "long;e2e")

# Error paths: invalid input must give exit status 1 and a message, never a crash.
add_test(NAME e2e_missing_parameter_file
    COMMAND ${CMAKE_COMMAND}
        -DSIMUCELL3D_BIN=$<TARGET_FILE:simucell3d>
        -DARGS=${E2E_WORK_ROOT}/does_not_exist.xml
        "-DMESSAGE_REGEX=could not be found"
        -P ${E2E_FAIL}
)
add_test(NAME e2e_malformed_parameter_file
    COMMAND ${CMAKE_COMMAND}
        -DSIMUCELL3D_BIN=$<TARGET_FILE:simucell3d>
        -DWORK_DIR=${E2E_WORK_ROOT}/e2e_malformed_parameter_file
        -DWRITE_FILE=malformed.xml
        "-DWRITE_CONTENT=<simulation_parameters><io_parameters>"
        -DARGS=${E2E_WORK_ROOT}/e2e_malformed_parameter_file/malformed.xml
        "-DMESSAGE_REGEX=parameter file"
        -P ${E2E_FAIL}
)
add_test(NAME e2e_incomplete_parameter_file
    COMMAND ${CMAKE_COMMAND}
        -DSIMUCELL3D_BIN=$<TARGET_FILE:simucell3d>
        -DWORK_DIR=${E2E_WORK_ROOT}/e2e_incomplete_parameter_file
        -DWRITE_FILE=incomplete.xml
        "-DWRITE_CONTENT=<simulation_parameters></simulation_parameters>"
        -DARGS=${E2E_WORK_ROOT}/e2e_incomplete_parameter_file/incomplete.xml
        "-DMESSAGE_REGEX=was not found in the parameter file"
        -P ${E2E_FAIL}
)
add_test(NAME e2e_invalid_threads_flag
    COMMAND ${CMAKE_COMMAND}
        -DSIMUCELL3D_BIN=$<TARGET_FILE:simucell3d>
        "-DARGS=${PROJECT_SOURCE_DIR}/parameters/longrun_fast_growth.xml|--threads=0"
        "-DMESSAGE_REGEX=--threads requires a positive integer"
        -P ${E2E_FAIL}
)
set_tests_properties(e2e_missing_parameter_file e2e_malformed_parameter_file
    e2e_incomplete_parameter_file e2e_invalid_threads_flag
    PROPERTIES TIMEOUT 60 LABELS "integration;e2e")
