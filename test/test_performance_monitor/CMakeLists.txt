cmake_minimum_required(VERSION 3.0)

# Use C++ 17
set(CMAKE_CXX_STANDARD 17)

# Compile the test executable
add_executable(performance_monitor_test
    test_performance_monitor.cpp
)

# Link against project source library
target_link_libraries(performance_monitor_test PUBLIC src)

# Add header file includes
target_include_directories(performance_monitor_test PUBLIC "${PROJECT_SOURCE_DIR}/include/")

# Find threads library (required for std::thread in timing tests)
find_package(Threads REQUIRED)
target_link_libraries(performance_monitor_test PRIVATE Threads::Threads)

# Register all tests with CTest
add_test(NAME performance_monitor_timing_stats_update        COMMAND performance_monitor_test test_timing_stats_update)
add_test(NAME performance_monitor_scoped_timer_records_time  COMMAND performance_monitor_test test_scoped_timer_records_time)
add_test(NAME performance_monitor_record_and_get_stats       COMMAND performance_monitor_test test_record_and_get_stats)
add_test(NAME performance_monitor_reset_clears_data          COMMAND performance_monitor_test test_reset_clears_data)
add_test(NAME performance_monitor_get_total_time             COMMAND performance_monitor_test test_get_total_time)
add_test(NAME performance_monitor_csv_export_format          COMMAND performance_monitor_test test_csv_export_format)
add_test(NAME performance_monitor_timing_stats_edge_cases    COMMAND performance_monitor_test test_timing_stats_edge_cases)
add_test(NAME performance_monitor_multiple_named_timers      COMMAND performance_monitor_test test_multiple_named_timers)
add_test(NAME performance_monitor_csv_export_default_params  COMMAND performance_monitor_test test_csv_export_default_params)
add_test(NAME performance_monitor_report_no_crash            COMMAND performance_monitor_test test_report_no_crash)
add_test(NAME performance_monitor_small_time_precision       COMMAND performance_monitor_test test_small_time_precision)
add_test(NAME performance_monitor_large_time_values          COMMAND performance_monitor_test test_large_time_values)
