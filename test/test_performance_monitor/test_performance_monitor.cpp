#include <cassert>
#include <string>
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <cmath>
#include <filesystem>

#include "performance_monitor.hpp"
#include "utils.hpp"

/**
 * Test suite for PerformanceMonitor class
 *
 * Test framework convention: return 0 = pass, return 1 = fail
 * Uses: return !(condition) where condition = true means test passed
 */

//---------------------------------------------------------------------------------------------------------
/**
 * Test 1: Verify TimingStats correctly tracks min/max/avg/count
 *
 * Strategy: Create TimingStats and add known samples, verify all fields
 */
int test_timing_stats_update() {
    PerformanceMonitor::TimingStats stats;

    // Initially count should be 0
    bool t1 = (stats.count == 0);

    // Add first sample: 0.1 seconds
    stats.update(0.1);
    bool t2 = (stats.count == 1);
    bool t3 = almost_equal(stats.total_time, 0.1);
    bool t4 = almost_equal(stats.min_time, 0.1);
    bool t5 = almost_equal(stats.max_time, 0.1);
    bool t6 = almost_equal(stats.average(), 0.1);

    // Add second sample: 0.2 seconds
    stats.update(0.2);
    bool t7 = (stats.count == 2);
    bool t8 = almost_equal(stats.total_time, 0.3);
    bool t9 = almost_equal(stats.min_time, 0.1);
    bool t10 = almost_equal(stats.max_time, 0.2);
    bool t11 = almost_equal(stats.average(), 0.15);

    // Add third sample: 0.05 seconds (new minimum)
    stats.update(0.05);
    bool t12 = (stats.count == 3);
    bool t13 = almost_equal(stats.total_time, 0.35);
    bool t14 = almost_equal(stats.min_time, 0.05);
    bool t15 = almost_equal(stats.max_time, 0.2);
    bool t16 = almost_equal(stats.average(), 0.35 / 3.0);

    // Add fourth sample: 0.3 seconds (new maximum)
    stats.update(0.3);
    bool t17 = (stats.count == 4);
    bool t18 = almost_equal(stats.total_time, 0.65);
    bool t19 = almost_equal(stats.min_time, 0.05);
    bool t20 = almost_equal(stats.max_time, 0.3);
    bool t21 = almost_equal(stats.average(), 0.65 / 4.0);

    // Verify std_dev calculation (max - min) / 4
    double expected_std_dev = (0.3 - 0.05) / 4.0;
    bool t22 = almost_equal(stats.std_dev(), expected_std_dev);

    return !(t1 && t2 && t3 && t4 && t5 && t6 && t7 && t8 && t9 && t10 &&
             t11 && t12 && t13 && t14 && t15 && t16 && t17 && t18 && t19 && t20 &&
             t21 && t22);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
/**
 * Test 2: Verify ScopedTimer correctly measures elapsed time
 *
 * Strategy: Create timer, sleep for known duration, verify recorded time is within tolerance
 *
 * Numerical tolerance: Sleep precision varies by OS, use 20ms tolerance
 */
int test_scoped_timer_records_time() {
    PerformanceMonitor monitor;

    // Create scoped timer that sleeps for ~50ms
    {
        PerformanceMonitor::ScopedTimer timer(monitor, "test_sleep");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    // Timer destructor should have recorded the time

    const PerformanceMonitor::TimingStats* stats = monitor.get_stats("test_sleep");

    // Verify stats were recorded
    bool t1 = (stats != nullptr);
    bool t2 = (stats->count == 1);

    // Verify time is approximately 50ms (0.05 seconds)
    // Allow ±20ms tolerance for OS scheduling variance
    double recorded_time = stats->total_time;
    bool t3 = (recorded_time >= 0.030);  // At least 30ms
    bool t4 = (recorded_time <= 0.100);  // At most 100ms

    // Test multiple timings with same name
    {
        PerformanceMonitor::ScopedTimer timer(monitor, "test_sleep");
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    stats = monitor.get_stats("test_sleep");
    bool t5 = (stats->count == 2);
    bool t6 = (stats->total_time >= 0.070);  // At least 80ms total

    return !(t1 && t2 && t3 && t4 && t5 && t6);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
/**
 * Test 3: Verify record() and get_stats() work correctly
 *
 * Strategy: Manually record several named measurements, retrieve and verify
 */
int test_record_and_get_stats() {
    PerformanceMonitor monitor;

    // Record measurements for different phases
    monitor.record("phase_A", 0.123);
    monitor.record("phase_B", 0.456);
    monitor.record("phase_A", 0.234);
    monitor.record("phase_C", 0.789);
    monitor.record("phase_B", 0.567);

    // Verify phase_A stats
    const PerformanceMonitor::TimingStats* stats_A = monitor.get_stats("phase_A");
    bool t1 = (stats_A != nullptr);
    bool t2 = (stats_A->count == 2);
    bool t3 = almost_equal(stats_A->total_time, 0.123 + 0.234);
    bool t4 = almost_equal(stats_A->min_time, 0.123);
    bool t5 = almost_equal(stats_A->max_time, 0.234);
    bool t6 = almost_equal(stats_A->average(), (0.123 + 0.234) / 2.0);

    // Verify phase_B stats
    const PerformanceMonitor::TimingStats* stats_B = monitor.get_stats("phase_B");
    bool t7 = (stats_B != nullptr);
    bool t8 = (stats_B->count == 2);
    bool t9 = almost_equal(stats_B->total_time, 0.456 + 0.567);
    bool t10 = almost_equal(stats_B->min_time, 0.456);
    bool t11 = almost_equal(stats_B->max_time, 0.567);

    // Verify phase_C stats
    const PerformanceMonitor::TimingStats* stats_C = monitor.get_stats("phase_C");
    bool t12 = (stats_C != nullptr);
    bool t13 = (stats_C->count == 1);
    bool t14 = almost_equal(stats_C->total_time, 0.789);

    // Verify non-existent phase returns nullptr
    const PerformanceMonitor::TimingStats* stats_D = monitor.get_stats("nonexistent");
    bool t15 = (stats_D == nullptr);

    return !(t1 && t2 && t3 && t4 && t5 && t6 && t7 && t8 && t9 && t10 &&
             t11 && t12 && t13 && t14 && t15);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
/**
 * Test 4: Verify reset() clears all accumulated statistics
 *
 * Strategy: Record data, verify it exists, reset, verify it's gone
 */
int test_reset_clears_data() {
    PerformanceMonitor monitor;

    // Record some measurements
    monitor.record("phase_X", 0.111);
    monitor.record("phase_Y", 0.222);
    monitor.record("phase_X", 0.333);

    // Verify data exists before reset
    const PerformanceMonitor::TimingStats* stats_before = monitor.get_stats("phase_X");
    bool t1 = (stats_before != nullptr);
    bool t2 = (stats_before->count == 2);

    double total_before = monitor.get_total_time();
    bool t3 = (total_before > 0.0);
    bool t4 = almost_equal(total_before, 0.111 + 0.333 + 0.222);

    // Reset monitor
    monitor.reset();

    // Verify data is cleared
    const PerformanceMonitor::TimingStats* stats_after_X = monitor.get_stats("phase_X");
    const PerformanceMonitor::TimingStats* stats_after_Y = monitor.get_stats("phase_Y");
    bool t5 = (stats_after_X == nullptr);
    bool t6 = (stats_after_Y == nullptr);

    double total_after = monitor.get_total_time();
    bool t7 = almost_equal(total_after, 0.0);

    // Verify can record new data after reset
    monitor.record("phase_Z", 0.555);
    const PerformanceMonitor::TimingStats* stats_new = monitor.get_stats("phase_Z");
    bool t8 = (stats_new != nullptr);
    bool t9 = (stats_new->count == 1);
    bool t10 = almost_equal(stats_new->total_time, 0.555);

    return !(t1 && t2 && t3 && t4 && t5 && t6 && t7 && t8 && t9 && t10);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
/**
 * Test 5: Verify get_total_time() sums all phases correctly
 *
 * Strategy: Record multiple phases, verify sum matches get_total_time()
 */
int test_get_total_time() {
    PerformanceMonitor monitor;

    // Initially total should be zero
    bool t1 = almost_equal(monitor.get_total_time(), 0.0);

    // Record single phase
    monitor.record("phase_1", 0.100);
    bool t2 = almost_equal(monitor.get_total_time(), 0.100);

    // Add another phase
    monitor.record("phase_2", 0.250);
    bool t3 = almost_equal(monitor.get_total_time(), 0.350);

    // Add to existing phase (accumulates)
    monitor.record("phase_1", 0.150);
    bool t4 = almost_equal(monitor.get_total_time(), 0.500);

    // Add third phase
    monitor.record("phase_3", 0.075);
    bool t5 = almost_equal(monitor.get_total_time(), 0.575);

    // Add multiple samples
    monitor.record("phase_2", 0.125);
    monitor.record("phase_3", 0.200);
    double expected_total = 0.100 + 0.150 + 0.250 + 0.125 + 0.075 + 0.200;
    bool t6 = almost_equal(monitor.get_total_time(), expected_total);

    return !(t1 && t2 && t3 && t4 && t5 && t6);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
/**
 * Test 6: Verify export_to_csv() creates valid CSV with correct format
 *
 * Strategy: Export to temp file, read back, verify format and content
 *
 * CSV Format: iteration,num_cells,phase,avg_ms,min_ms,max_ms,count,total_ms
 */
int test_csv_export_format() {
    PerformanceMonitor monitor;

    // Use temporary file in /tmp directory
    std::string csv_filename = "/tmp/test_performance_monitor_export.csv";

    // Remove file if it exists from previous test run
    std::filesystem::remove(csv_filename);

    // Record some test data
    monitor.record("contact_forces", 0.123);
    monitor.record("time_integration", 0.045);
    monitor.record("contact_forces", 0.134);
    monitor.record("mesh_refinement", 0.078);

    // Export to CSV with iteration=10, num_cells=512
    monitor.export_to_csv(csv_filename, 10, 512);

    // Verify file was created
    bool t1 = std::filesystem::exists(csv_filename);

    // Read file contents
    std::ifstream csv_file(csv_filename);
    bool t2 = csv_file.is_open();

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(csv_file, line)) {
        lines.push_back(line);
    }
    csv_file.close();

    // Verify header line exists
    bool t3 = (lines.size() >= 1);
    bool t4 = (lines[0] == "iteration,num_cells,phase,avg_ms,min_ms,max_ms,count,total_ms");

    // Verify data lines (should be 3 phases + 1 header = 4 lines total)
    bool t5 = (lines.size() == 4);

    // Parse and verify contact_forces line (appears twice, so avg != min/max)
    bool found_contact_forces = false;
    bool found_time_integration = false;
    bool found_mesh_refinement = false;

    for (size_t i = 1; i < lines.size(); i++) {
        if (lines[i].find("contact_forces") != std::string::npos) {
            found_contact_forces = true;
            // Should start with "10,512,contact_forces,"
            bool starts_correct = (lines[i].find("10,512,contact_forces,") == 0);
            if (!starts_correct) {
                std::cout << "contact_forces line format error: " << lines[i] << std::endl;
                return 1;
            }
        }
        if (lines[i].find("time_integration") != std::string::npos) {
            found_time_integration = true;
            bool starts_correct = (lines[i].find("10,512,time_integration,") == 0);
            if (!starts_correct) {
                std::cout << "time_integration line format error: " << lines[i] << std::endl;
                return 1;
            }
        }
        if (lines[i].find("mesh_refinement") != std::string::npos) {
            found_mesh_refinement = true;
            bool starts_correct = (lines[i].find("10,512,mesh_refinement,") == 0);
            if (!starts_correct) {
                std::cout << "mesh_refinement line format error: " << lines[i] << std::endl;
                return 1;
            }
        }
    }

    bool t6 = found_contact_forces;
    bool t7 = found_time_integration;
    bool t8 = found_mesh_refinement;

    // Test appending to existing file
    monitor.reset();
    monitor.record("new_phase", 0.999);
    monitor.export_to_csv(csv_filename, 20, 1024);

    // Read file again
    csv_file.open(csv_filename);
    lines.clear();
    while (std::getline(csv_file, line)) {
        lines.push_back(line);
    }
    csv_file.close();

    // Should have header + 3 phases from first export + 1 phase from second export = 5 lines
    bool t9 = (lines.size() == 5);

    // Verify new line has correct iteration number
    bool found_new_phase = false;
    for (size_t i = 1; i < lines.size(); i++) {
        if (lines[i].find("20,1024,new_phase,") == 0) {
            found_new_phase = true;
            break;
        }
    }
    bool t10 = found_new_phase;

    // Cleanup
    std::filesystem::remove(csv_filename);

    return !(t1 && t2 && t3 && t4 && t5 && t6 && t7 && t8 && t9 && t10);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
/**
 * Test 7: Verify TimingStats handles edge cases
 *
 * Strategy: Test zero samples, single sample, identical samples
 */
int test_timing_stats_edge_cases() {
    // Test 1: Zero samples - average should return 0
    PerformanceMonitor::TimingStats stats_empty;
    bool t1 = (stats_empty.count == 0);
    bool t2 = almost_equal(stats_empty.average(), 0.0);
    bool t3 = almost_equal(stats_empty.total_time, 0.0);

    // Test 2: Single sample - min/max/avg should all equal sample value
    PerformanceMonitor::TimingStats stats_single;
    stats_single.update(0.777);
    bool t4 = (stats_single.count == 1);
    bool t5 = almost_equal(stats_single.min_time, 0.777);
    bool t6 = almost_equal(stats_single.max_time, 0.777);
    bool t7 = almost_equal(stats_single.average(), 0.777);
    bool t8 = almost_equal(stats_single.std_dev(), 0.0);  // (max - min) / 4 = 0

    // Test 3: Identical samples - min/max/avg should all be equal
    PerformanceMonitor::TimingStats stats_identical;
    stats_identical.update(0.5);
    stats_identical.update(0.5);
    stats_identical.update(0.5);
    bool t9 = (stats_identical.count == 3);
    bool t10 = almost_equal(stats_identical.min_time, 0.5);
    bool t11 = almost_equal(stats_identical.max_time, 0.5);
    bool t12 = almost_equal(stats_identical.average(), 0.5);
    bool t13 = almost_equal(stats_identical.total_time, 1.5);
    bool t14 = almost_equal(stats_identical.std_dev(), 0.0);

    return !(t1 && t2 && t3 && t4 && t5 && t6 && t7 && t8 && t9 && t10 &&
             t11 && t12 && t13 && t14);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
/**
 * Test 8: Verify multiple named timers can coexist
 *
 * Strategy: Create multiple independent timers, verify isolation
 */
int test_multiple_named_timers() {
    PerformanceMonitor monitor;

    // Create scoped timers for different phases
    {
        PerformanceMonitor::ScopedTimer timer1(monitor, "phase_alpha");
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    {
        PerformanceMonitor::ScopedTimer timer2(monitor, "phase_beta");
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    {
        PerformanceMonitor::ScopedTimer timer3(monitor, "phase_alpha");
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    // Verify phase_alpha has 2 samples
    const PerformanceMonitor::TimingStats* stats_alpha = monitor.get_stats("phase_alpha");
    bool t1 = (stats_alpha != nullptr);
    bool t2 = (stats_alpha->count == 2);

    // Verify phase_beta has 1 sample
    const PerformanceMonitor::TimingStats* stats_beta = monitor.get_stats("phase_beta");
    bool t3 = (stats_beta != nullptr);
    bool t4 = (stats_beta->count == 1);

    // Verify timings are approximately correct (with tolerance)
    // phase_alpha: ~20ms + ~25ms = ~45ms total
    bool t5 = (stats_alpha->total_time >= 0.030);  // At least 30ms
    bool t6 = (stats_alpha->total_time <= 0.100);  // At most 100ms

    // phase_beta: ~30ms
    bool t7 = (stats_beta->total_time >= 0.015);  // At least 15ms
    bool t8 = (stats_beta->total_time <= 0.070);  // At most 70ms

    return !(t1 && t2 && t3 && t4 && t5 && t6 && t7 && t8);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
/**
 * Test 9: Verify CSV export handles missing optional parameters
 *
 * Strategy: Export with default iteration=-1 and num_cells=0
 */
int test_csv_export_default_params() {
    PerformanceMonitor monitor;

    std::string csv_filename = "/tmp/test_performance_monitor_defaults.csv";
    std::filesystem::remove(csv_filename);

    monitor.record("test_phase", 0.123);

    // Export with default parameters (iteration=-1, num_cells=0)
    monitor.export_to_csv(csv_filename);

    // Read file
    std::ifstream csv_file(csv_filename);
    std::string header, data_line;
    std::getline(csv_file, header);
    std::getline(csv_file, data_line);
    csv_file.close();

    // Verify line starts with "-1,0,test_phase,"
    bool t1 = (data_line.find("-1,0,test_phase,") == 0);

    // Cleanup
    std::filesystem::remove(csv_filename);

    return !(t1);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
/**
 * Test 10: Verify report() doesn't crash and handles empty monitor
 *
 * Strategy: Call report() on empty and populated monitor, verify no crash
 * Note: This test only verifies no crash/exception, not output format
 */
int test_report_no_crash() {
    PerformanceMonitor monitor_empty;
    PerformanceMonitor monitor_full;

    // Test report on empty monitor (should print "No timing data collected")
    std::cout << "\n[TEST] Testing report() on empty monitor:" << std::endl;
    monitor_empty.report();

    // Test report on populated monitor
    monitor_full.record("contact_forces", 0.123);
    monitor_full.record("time_integration", 0.045);
    monitor_full.record("contact_forces", 0.134);

    std::cout << "\n[TEST] Testing report() on populated monitor:" << std::endl;
    monitor_full.report();

    std::cout << "\n[TEST] Testing detailed report():" << std::endl;
    monitor_full.report(true);

    // If we reach here without crash, test passes
    return 0;
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
/**
 * Test 11: Verify very small time measurements are handled correctly
 *
 * Strategy: Record sub-microsecond times, verify precision
 */
int test_small_time_precision() {
    PerformanceMonitor monitor;

    // Record very small times (nanosecond scale)
    monitor.record("fast_op", 0.000000123);  // 123 nanoseconds
    monitor.record("fast_op", 0.000000456);  // 456 nanoseconds

    const PerformanceMonitor::TimingStats* stats = monitor.get_stats("fast_op");
    bool t1 = (stats != nullptr);
    bool t2 = (stats->count == 2);
    bool t3 = almost_equal(stats->min_time, 0.000000123);
    bool t4 = almost_equal(stats->max_time, 0.000000456);
    bool t5 = almost_equal(stats->total_time, 0.000000579);

    return !(t1 && t2 && t3 && t4 && t5);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
/**
 * Test 12: Verify large time measurements are handled correctly
 *
 * Strategy: Record large times (hours), verify no overflow
 */
int test_large_time_values() {
    PerformanceMonitor monitor;

    // Record large times (simulating long-running phases)
    monitor.record("long_phase", 3600.0);      // 1 hour
    monitor.record("long_phase", 7200.0);      // 2 hours
    monitor.record("very_long", 86400.0);      // 24 hours

    const PerformanceMonitor::TimingStats* stats = monitor.get_stats("long_phase");
    bool t1 = (stats != nullptr);
    bool t2 = (stats->count == 2);
    bool t3 = almost_equal(stats->total_time, 10800.0);
    bool t4 = almost_equal(stats->average(), 5400.0);

    double total = monitor.get_total_time();
    bool t5 = almost_equal(total, 97200.0);  // 3600 + 7200 + 86400

    return !(t1 && t2 && t3 && t4 && t5);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Main test runner
int main(int argc, char** argv) {
    // Require test name argument
    assert(argc == 2);

    std::string test_name = argv[1];

    // Run selected test
    if (test_name == "test_timing_stats_update")           return test_timing_stats_update();
    if (test_name == "test_scoped_timer_records_time")     return test_scoped_timer_records_time();
    if (test_name == "test_record_and_get_stats")          return test_record_and_get_stats();
    if (test_name == "test_reset_clears_data")             return test_reset_clears_data();
    if (test_name == "test_get_total_time")                return test_get_total_time();
    if (test_name == "test_csv_export_format")             return test_csv_export_format();
    if (test_name == "test_timing_stats_edge_cases")       return test_timing_stats_edge_cases();
    if (test_name == "test_multiple_named_timers")         return test_multiple_named_timers();
    if (test_name == "test_csv_export_default_params")     return test_csv_export_default_params();
    if (test_name == "test_report_no_crash")               return test_report_no_crash();
    if (test_name == "test_small_time_precision")          return test_small_time_precision();
    if (test_name == "test_large_time_values")             return test_large_time_values();

    std::cout << "TEST NAME: " << test_name << " DOES NOT EXIST" << std::endl;
    return 1;
}
//---------------------------------------------------------------------------------------------------------
