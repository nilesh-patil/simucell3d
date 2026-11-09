# ctest helper: run the simucell3d binary end to end on a shortened copy of a shipped
# parameter file and check that the run completes and writes readable output.
#
# Usage (see test/test_e2e/CMakeLists.txt):
#   cmake -DSIMUCELL3D_BIN=<path> -DPARAMETER_FILE=<xml> -DWORK_DIR=<dir>
#         -DSIMULATION_DURATION=<s> -DSAMPLING_PERIOD=<s> -DTHREADS=<n>
#         [-DMIN_CELLS=<n>] [-DSOURCE_DIR=<repo root>] -P run_simulation.cmake
#
# Checks:
#   1. the binary exits with status 0;
#   2. <out>/cell_data and <out>/face_data each hold at least 2 legacy-VTK files
#      (the initial state and at least one later frame) that start with the VTK header;
#   3. <out>/simulation_statistics.csv has the expected header, no nan/inf, and a last
#      row at iteration round(SIMULATION_DURATION / time_step), give or take one (the run
#      went to the end);
#   4. the last statistics iteration lists at least MIN_CELLS cells (default 1).
#
# The binary deletes the content of --output-dir at startup, so WORK_DIR must be a
# directory that belongs to this test only.

cmake_minimum_required(VERSION 3.12)   # sets script-mode policies; list(SUBLIST) needs 3.12

foreach(var SIMUCELL3D_BIN PARAMETER_FILE WORK_DIR SIMULATION_DURATION SAMPLING_PERIOD THREADS)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "run_simulation.cmake: -D${var}=... is required")
    endif()
endforeach()
if(NOT DEFINED MIN_CELLS)
    set(MIN_CELLS 1)
endif()
if(NOT DEFINED SOURCE_DIR)
    get_filename_component(SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
endif()

# --- 1. Shortened copy of the parameter file ------------------------------------------
file(READ "${PARAMETER_FILE}" xml)
foreach(pair "simulation_duration=${SIMULATION_DURATION}" "sampling_period=${SAMPLING_PERIOD}")
    string(REGEX MATCH "^([a-z_]+)=(.*)$" _ "${pair}")
    set(tag "${CMAKE_MATCH_1}")
    set(value "${CMAKE_MATCH_2}")
    if(NOT xml MATCHES "<${tag}>[^<]*</${tag}>")
        message(FATAL_ERROR "${PARAMETER_FILE}: no <${tag}> element to override")
    endif()
    string(REGEX REPLACE "<${tag}>[^<]*</${tag}>" "<${tag}>${value}</${tag}>" xml "${xml}")
endforeach()

# The time step is read from the (first, uncommented) <time_step> element to compute the
# expected number of iterations.
# CMake regular expressions are greedy, so XML comments are removed one by one.
set(xml_no_comments "${xml}")
while(TRUE)
    string(FIND "${xml_no_comments}" "<!--" comment_start)
    if(comment_start EQUAL -1)
        break()
    endif()
    string(SUBSTRING "${xml_no_comments}" ${comment_start} -1 rest)
    string(FIND "${rest}" "-->" comment_length)
    if(comment_length EQUAL -1)
        message(FATAL_ERROR "${PARAMETER_FILE}: unterminated XML comment")
    endif()
    math(EXPR comment_end "${comment_start} + ${comment_length} + 3")
    string(SUBSTRING "${xml_no_comments}" 0 ${comment_start} before)
    string(SUBSTRING "${xml_no_comments}" ${comment_end} -1 after)
    set(xml_no_comments "${before}${after}")
endwhile()
string(REGEX MATCH "<time_step>[ \t]*([^< \t]+)[ \t]*</time_step>" _ "${xml_no_comments}")
set(time_step "${CMAKE_MATCH_1}")
if(time_step STREQUAL "")
    message(FATAL_ERROR "${PARAMETER_FILE}: no <time_step> element")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
set(params "${WORK_DIR}/parameters.xml")
set(out "${WORK_DIR}/output")
set(log "${WORK_DIR}/run.log")
file(WRITE "${params}" "${xml}")

# --- 2. Run ------------------------------------------------------------------------------
message(STATUS "Running: ${SIMUCELL3D_BIN} ${params} --output-dir=${out} --threads=${THREADS}")
execute_process(
    COMMAND "${SIMUCELL3D_BIN}" "${params}" "--output-dir=${out}" "--threads=${THREADS}"
    WORKING_DIRECTORY "${SOURCE_DIR}"
    RESULT_VARIABLE exit_code
    OUTPUT_FILE "${log}"
    ERROR_FILE "${log}.stderr"
)

function(fail msg)
    set(tail_text "")
    foreach(f "${log}" "${log}.stderr")
        if(EXISTS "${f}")
            file(STRINGS "${f}" lines)
            list(LENGTH lines n)
            if(n GREATER 40)
                math(EXPR first "${n} - 40")
                list(SUBLIST lines ${first} 40 lines)
            endif()
            string(REPLACE ";" "\n" lines "${lines}")
            string(APPEND tail_text "\n--- last lines of ${f} ---\n${lines}")
        endif()
    endforeach()
    message(FATAL_ERROR "${msg}${tail_text}")
endfunction()

if(NOT exit_code STREQUAL "0")
    fail("simucell3d exited with '${exit_code}'")
endif()

# --- 3. Output files ---------------------------------------------------------------------
foreach(sub cell_data face_data)
    file(GLOB frames "${out}/${sub}/*.vtk")
    list(LENGTH frames nb_frames)
    if(nb_frames LESS 2)
        fail("${out}/${sub}: expected at least 2 .vtk files, found ${nb_frames}")
    endif()
    foreach(frame ${frames})
        file(READ "${frame}" head LIMIT 64)
        if(NOT head MATCHES "^# vtk DataFile Version")
            fail("${frame}: not a legacy VTK file")
        endif()
    endforeach()
endforeach()

set(stats "${out}/simulation_statistics.csv")
if(NOT EXISTS "${stats}")
    fail("${stats} not found")
endif()
file(STRINGS "${stats}" rows)
list(LENGTH rows nb_rows)
if(nb_rows LESS 2)
    fail("${stats}: no data rows")
endif()
list(GET rows 0 header)
if(NOT header MATCHES "^iteration,.*,cell_id,")
    fail("${stats}: unexpected header '${header}'")
endif()
file(READ "${stats}" stats_text)
string(TOLOWER "${stats_text}" stats_lower)
if(stats_lower MATCHES "(^|[,\n])[-+]?(nan|inf)")
    fail("${stats}: contains nan or inf")
endif()

# Expected last iteration = round(duration / time step). math(EXPR) is integer-only, so
# both decimal numbers are split into an integer mantissa and a power of ten.
function(to_scaled number out_mantissa out_exponent)
    string(TOLOWER "${number}" number)
    if(NOT number MATCHES "^([0-9]+)(\\.([0-9]+))?(e([-+]?[0-9]+))?$")
        message(FATAL_ERROR "cannot parse number '${number}'")
    endif()
    set(int "${CMAKE_MATCH_1}")
    set(frac "${CMAKE_MATCH_3}")
    set(exp "${CMAKE_MATCH_5}")
    if(exp STREQUAL "")
        set(exp 0)
    endif()
    string(LENGTH "${frac}" nfrac)
    math(EXPR exp "${exp} - ${nfrac}")
    string(REGEX REPLACE "^0+([0-9])" "\\1" mant "${int}${frac}")
    set(${out_mantissa} "${mant}" PARENT_SCOPE)
    set(${out_exponent} "${exp}" PARENT_SCOPE)
endfunction()
to_scaled("${SIMULATION_DURATION}" dm de)
to_scaled("${time_step}" tm te)
math(EXPR shift "${de} - ${te}")
set(num "${dm}")
set(den "${tm}")
while(shift GREATER 0)
    math(EXPR num "${num} * 10")
    math(EXPR shift "${shift} - 1")
endwhile()
while(shift LESS 0)
    math(EXPR den "${den} * 10")
    math(EXPR shift "${shift} + 1")
endwhile()
math(EXPR expected_iterations "(${num} + ${den} / 2) / ${den}")

list(GET rows -1 last_row)
string(REGEX MATCH "^[0-9]+" last_iteration "${last_row}")
# The simulated time is accumulated in floating point, so the solver can take one step
# more than round(duration / time step) (7001 for 7e-4 s at 1e-7 s, for example).
math(EXPR min_iteration "${expected_iterations} - 1")
math(EXPR max_iteration "${expected_iterations} + 1")
if(last_iteration LESS min_iteration OR last_iteration GREATER max_iteration)
    fail("${stats}: last iteration is ${last_iteration}, expected ${expected_iterations} "
         "(duration ${SIMULATION_DURATION} s / time step ${time_step} s): the run stopped early")
endif()

set(nb_cells 0)
foreach(row IN LISTS rows)
    if(row MATCHES "^${last_iteration},")
        math(EXPR nb_cells "${nb_cells} + 1")
    endif()
endforeach()
if(nb_cells LESS MIN_CELLS)
    fail("${stats}: ${nb_cells} cells at iteration ${last_iteration}, expected at least ${MIN_CELLS}")
endif()

message(STATUS "OK: ${last_iteration} iterations, ${nb_cells} cells at the end, output in ${out}")
