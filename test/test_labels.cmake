# Test tiers as ctest labels. Every test/**/CMakeLists.txt that registers tests ends with
#   include(${PROJECT_SOURCE_DIR}/test/test_labels.cmake)
# which labels the tests of that directory (CMake < 3.28 can only change the properties of
# tests in the current directory). Select a tier with `ctest -L <regex>`:
#
#   unit         fast tests of one module (cassert-based executables under test/)
#   integration  tests across modules and end-to-end runs of the simucell3d executable
#                (test/test_e2e/, cli_version_flag, test_contact_model_integration)
#   benchmark    micro-benchmarks (test/test_benchmarks/); report timings, never fail on
#                speed, and are only meaningful in a Release build
#   long         the long tier: stability_* and scientific_* tests (which are also `unit`,
#                because they are fast enough for every run) plus e2e_growth_long
#
#   ctest -L "unit|integration"   what every CI run executes (Release, Debug, sanitizers)
#   ctest -L long                 the long tier (.github/workflows/long.yml)
#   ctest -L benchmark            micro-benchmarks (long tier, Release only)
#
# A test that sets LABELS itself keeps them. Every other test gets its label from its name:
#   benchmark_*                  -> benchmark
#   stability_*, scientific_*    -> unit;long plus stability or scientific
#   anything else                -> unit
#
# Measured on an 8-vCPU Xeon (3.1 GHz), Release, GCC 10, 2026-11-12: `ctest -L long`
# takes 3 to 5 min, almost all of it e2e_growth_long (7000 iterations of
# parameters/longrun_fast_growth.xml on 4 threads, from 1 to 391-443 cells).

if(CMAKE_VERSION VERSION_LESS 3.12)
    # The TESTS directory property needs CMake 3.12; older versions get no default labels.
    return()
endif()

get_property(_simucell3d_tests DIRECTORY PROPERTY TESTS)
foreach(_simucell3d_test IN LISTS _simucell3d_tests)
    get_test_property(${_simucell3d_test} LABELS _simucell3d_labels)
    if(NOT _simucell3d_labels)
        if(_simucell3d_test MATCHES "^benchmark_")
            set(_simucell3d_labels "benchmark")
        elseif(_simucell3d_test MATCHES "^stability_")
            set(_simucell3d_labels "unit;long;stability")
        elseif(_simucell3d_test MATCHES "^scientific_")
            set(_simucell3d_labels "unit;long;scientific")
        else()
            set(_simucell3d_labels "unit")
        endif()
        set_tests_properties(${_simucell3d_test} PROPERTIES LABELS "${_simucell3d_labels}")
    endif()
endforeach()
