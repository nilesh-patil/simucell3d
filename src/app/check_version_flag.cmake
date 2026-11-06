# ctest helper: run `simucell3d --version` and check its exit code and output.
# Usage: cmake -DSIMUCELL3D_BIN=<path> -DEXPECTED_VERSION=<x.y.z> -P check_version_flag.cmake

execute_process(
    COMMAND ${SIMUCELL3D_BIN} --version
    RESULT_VARIABLE exit_code
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(NOT exit_code EQUAL 0)
    message(FATAL_ERROR "simucell3d --version exited with ${exit_code}\nstdout: ${output}\nstderr: ${error_output}")
endif()

string(REPLACE "." "\\." version_regex "${EXPECTED_VERSION}")
if(NOT output MATCHES "^SimuCell3D ${version_regex} \\([^)]+\\)$")
    message(FATAL_ERROR "Unexpected --version output: '${output}' (expected 'SimuCell3D ${EXPECTED_VERSION} (<sha>)')")
endif()

message(STATUS "OK: ${output}")
