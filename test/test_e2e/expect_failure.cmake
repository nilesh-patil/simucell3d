# ctest helper: run simucell3d on an invalid input and check that it fails cleanly, that
# is, exits with status 1 (not 0, and not killed by a signal) and prints MESSAGE_REGEX.
#
# Usage: cmake -DSIMUCELL3D_BIN=<path> -DARGS=<arg|arg|...> -DMESSAGE_REGEX=<regex>
#              [-DWORK_DIR=<dir>] [-DWRITE_FILE=<name>] [-DWRITE_CONTENT=<text>]
#              -P expect_failure.cmake
#
# WRITE_FILE (relative to WORK_DIR) is created with WRITE_CONTENT before the run, so a
# malformed input file can be generated without checking it into the repository.

cmake_minimum_required(VERSION 3.10)   # sets script-mode policies

foreach(var SIMUCELL3D_BIN ARGS MESSAGE_REGEX)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "expect_failure.cmake: -D${var}=... is required")
    endif()
endforeach()

if(DEFINED WORK_DIR)
    file(REMOVE_RECURSE "${WORK_DIR}")
    file(MAKE_DIRECTORY "${WORK_DIR}")
    if(DEFINED WRITE_FILE)
        file(WRITE "${WORK_DIR}/${WRITE_FILE}" "${WRITE_CONTENT}")
    endif()
else()
    set(WORK_DIR "${CMAKE_CURRENT_BINARY_DIR}")
endif()

# Arguments are separated by '|' because add_test() would split a ';'-separated list.
string(REPLACE "|" ";" ARGS "${ARGS}")

execute_process(
    COMMAND "${SIMUCELL3D_BIN}" ${ARGS}
    WORKING_DIRECTORY "${WORK_DIR}"
    RESULT_VARIABLE exit_code
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
)

if(NOT exit_code STREQUAL "1")
    message(FATAL_ERROR "expected exit status 1, got '${exit_code}'\nstdout: ${out}\nstderr: ${err}")
endif()
if(NOT "${out}${err}" MATCHES "${MESSAGE_REGEX}")
    message(FATAL_ERROR "output does not match '${MESSAGE_REGEX}'\nstdout: ${out}\nstderr: ${err}")
endif()
message(STATUS "OK: exit status 1 with the expected message")
