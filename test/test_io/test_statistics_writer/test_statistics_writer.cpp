#include <cassert>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdio>
#include <unistd.h>

#include "statistics_writer.hpp"
#include "cell.hpp"


//---------------------------------------------------------------------------------------------------------
// Helper function to generate a list of test cells with known properties
std::vector<cell_ptr> generate_test_cells() {
    // Create simple cube-like cells with 8 nodes and 12 triangular faces
    std::vector<double> cell_node_pos_1{
        0,0,0,  1,0,0,  1,0,1,
        0,0,1,  0,1,0,  1,1,0,
        0,1,1,  1,1,1
    };

    std::vector<double> cell_node_pos_2{
        2,0,0,  3,0,0,  3,0,1,
        2,0,1,  2,1,0,  3,1,0,
        2,1,1,  3,1,1
    };

    std::vector<unsigned> cell_face_connectivity{
        0, 1, 3,
        2, 3, 1,
        0, 4, 1,
        5, 1, 4,
        0, 3, 4,
        6, 4, 3,
        1, 5, 2,
        7, 2, 5,
        5, 4, 7,
        6, 7, 4,
        3, 2, 6,
        7, 6, 2
    };

    cell_ptr c0 = std::make_shared<cell>(cell_node_pos_1, cell_face_connectivity, 0);
    cell_ptr c1 = std::make_shared<cell>(cell_node_pos_2, cell_face_connectivity, 1);

    return {c0, c1};
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Helper function to generate a unique temporary file path
std::string get_temp_file_path(const std::string& suffix = "") {
    char temp_path[] = "/tmp/simucell3d_test_XXXXXX";
    int fd = mkstemp(temp_path);
    if (fd == -1) {
        throw std::runtime_error("Failed to create temporary file");
    }
    close(fd);

    // Add suffix if provided
    std::string path = std::string(temp_path) + suffix;

    // Remove the file created by mkstemp (we just need the unique name)
    std::remove(temp_path);

    return path;
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Helper function to read entire file contents
std::string read_file_contents(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + file_path);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Helper function to count lines in a string
size_t count_lines(const std::string& str) {
    if (str.empty()) return 0;
    return std::count(str.begin(), str.end(), '\n');
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Helper function to check if a string contains a substring
bool contains(const std::string& str, const std::string& substr) {
    return str.find(substr) != std::string::npos;
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 1: Verify that csv_file_statistics_writer creates a file at the specified path
int test_csv_file_writer_creation() {
    std::cout << "Running test_csv_file_writer_creation..." << std::endl;

    std::string temp_path = get_temp_file_path(".csv");

    // Ensure file does not exist before test
    std::remove(temp_path.c_str());

    // Verify file does not exist
    std::ifstream check_before(temp_path);
    bool file_existed_before = check_before.good();
    check_before.close();

    if (file_existed_before) {
        std::cout << "FAIL: File existed before creating writer" << std::endl;
        return 1;
    }

    // Create the writer - this should create the file
    {
        csv_file_statistics_writer writer(temp_path);
    }

    // Verify file now exists
    std::ifstream check_after(temp_path);
    bool file_exists_after = check_after.good();
    check_after.close();

    // Cleanup
    std::remove(temp_path.c_str());

    if (!file_exists_after) {
        std::cout << "FAIL: File was not created by csv_file_statistics_writer" << std::endl;
        return 1;
    }

    std::cout << "PASS: CSV file was created successfully" << std::endl;
    return 0;
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 2: Verify that the CSV header contains expected columns
int test_csv_header_format() {
    std::cout << "Running test_csv_header_format..." << std::endl;

    std::string temp_path = get_temp_file_path(".csv");

    // Create the writer
    {
        csv_file_statistics_writer writer(temp_path);
    }

    // Read the file contents
    std::string contents = read_file_contents(temp_path);

    // Cleanup
    std::remove(temp_path.c_str());

    // Verify required header columns are present
    // Based on mesh_data.hpp, the header should contain:
    // iteration, computation_time_(hh::mm:ss), simulation_time, cell_id, type_id, area, target_area,
    // volume, target_volume, pressure, kinetic_energy, surface_tension_energy,
    // membrane_elasticity_energy, bending_energy, pressure_energy, total_potential_energy

    std::vector<std::string> required_columns = {
        "iteration",
        "computation_time_(hh::mm:ss)",
        "simulation_time",
        "cell_id",
        "type_id",
        "area",
        "target_area",
        "volume",
        "target_volume",
        "pressure",
        "kinetic_energy",
        "surface_tension_energy",
        "membrane_elasticity_energy",
        "bending_energy",
        "pressure_energy",
        "total_potential_energy"
    };

    bool all_columns_present = true;
    for (const auto& col : required_columns) {
        if (!contains(contents, col)) {
            std::cout << "FAIL: Missing required column: " << col << std::endl;
            all_columns_present = false;
        }
    }

    if (!all_columns_present) {
        std::cout << "Header contents: " << contents << std::endl;
        return 1;
    }

    // Verify header ends with newline (exactly one line for header only)
    size_t num_lines = count_lines(contents);
    if (num_lines != 1) {
        std::cout << "FAIL: Expected 1 header line, got " << num_lines << std::endl;
        return 1;
    }

    std::cout << "PASS: CSV header format is correct" << std::endl;
    return 0;
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 3: Verify that string_statistics_writer accumulates data correctly
int test_string_writer_accumulation() {
    std::cout << "Running test_string_writer_accumulation..." << std::endl;

    // Create the string writer
    string_statistics_writer writer;

    // Get initial string (should contain header only)
    std::string initial_str = writer.get_string();
    size_t initial_lines = count_lines(initial_str);

    if (initial_lines != 1) {
        std::cout << "FAIL: Initial string should have 1 line (header), got " << initial_lines << std::endl;
        return 1;
    }

    // Create test cells
    std::vector<cell_ptr> cells = generate_test_cells();

    // Write data for first iteration
    writer.write_data(0, 0.0, cells);
    std::string after_first = writer.get_string();
    size_t lines_after_first = count_lines(after_first);

    // Should have header + 2 data lines (one per cell)
    if (lines_after_first != 3) {
        std::cout << "FAIL: After first write should have 3 lines (header + 2 cells), got " << lines_after_first << std::endl;
        return 1;
    }

    // Write data for second iteration
    writer.write_data(1, 0.001, cells);
    std::string after_second = writer.get_string();
    size_t lines_after_second = count_lines(after_second);

    // Should have header + 4 data lines (2 cells x 2 iterations)
    if (lines_after_second != 5) {
        std::cout << "FAIL: After second write should have 5 lines (header + 4 cells), got " << lines_after_second << std::endl;
        return 1;
    }

    // Verify the string contains expected data markers
    if (!contains(after_second, "iteration")) {
        std::cout << "FAIL: Output missing 'iteration' column" << std::endl;
        return 1;
    }

    std::cout << "PASS: String writer accumulates data correctly" << std::endl;
    return 0;
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 4: Verify that cell data (volume, area, etc.) is formatted correctly
int test_write_data_formatting() {
    std::cout << "Running test_write_data_formatting..." << std::endl;

    // Create the string writer
    string_statistics_writer writer;

    // Create test cells
    std::vector<cell_ptr> cells = generate_test_cells();

    // Write data
    writer.write_data(42, 1.5e-3, cells);

    std::string output = writer.get_string();

    // Verify iteration number is present
    if (!contains(output, "42")) {
        std::cout << "FAIL: Output missing iteration number '42'" << std::endl;
        return 1;
    }

    // Verify simulation time is formatted (should be in scientific notation like "1.50e-03")
    if (!contains(output, "1.50e-03")) {
        std::cout << "FAIL: Output missing properly formatted simulation time '1.50e-03'" << std::endl;
        std::cout << "Output: " << output << std::endl;
        return 1;
    }

    // Verify cell IDs are present (cells have IDs 0 and 1)
    // Cell ID should appear in the data columns
    // The format uses comma separator, so look for the cell_id values
    bool has_cell_data = contains(output, ",0,") || contains(output, ",1,");
    if (!has_cell_data) {
        std::cout << "FAIL: Output missing cell ID data" << std::endl;
        return 1;
    }

    // Verify data uses scientific notation for floating point values
    // Volume and area should be in format like "X.XXXe+XX" or "X.XXXe-XX"
    bool has_scientific = output.find("e+") != std::string::npos ||
                          output.find("e-") != std::string::npos;
    if (!has_scientific) {
        std::cout << "FAIL: Output should contain scientific notation for cell data" << std::endl;
        return 1;
    }

    // Verify computation time format (hh:mm:ss)
    if (!contains(output, "00:00:00")) {
        std::cout << "FAIL: Output missing computation time in hh:mm:ss format" << std::endl;
        return 1;
    }

    std::cout << "PASS: Data formatting is correct" << std::endl;
    return 0;
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 5: Write data for multiple iterations and verify all are recorded
int test_multiple_iterations() {
    std::cout << "Running test_multiple_iterations..." << std::endl;

    std::string temp_path = get_temp_file_path(".csv");

    // Create test cells
    std::vector<cell_ptr> cells = generate_test_cells();
    size_t num_cells = cells.size();

    // Create writer and write multiple iterations
    {
        csv_file_statistics_writer writer(temp_path);

        // Write 5 iterations of data
        for (unsigned iter = 0; iter < 5; ++iter) {
            double sim_time = iter * 0.001;
            writer.write_data(iter, sim_time, cells);
        }
    }

    // Read and verify
    std::string contents = read_file_contents(temp_path);

    // Cleanup
    std::remove(temp_path.c_str());

    // Count lines: should be 1 header + (5 iterations * 2 cells) = 11 lines
    size_t expected_lines = 1 + (5 * num_cells);
    size_t actual_lines = count_lines(contents);

    if (actual_lines != expected_lines) {
        std::cout << "FAIL: Expected " << expected_lines << " lines, got " << actual_lines << std::endl;
        return 1;
    }

    // Verify each iteration number appears in the output
    for (unsigned iter = 0; iter < 5; ++iter) {
        std::string iter_str = std::to_string(iter) + ",";
        if (!contains(contents, iter_str)) {
            std::cout << "FAIL: Missing iteration " << iter << " in output" << std::endl;
            return 1;
        }
    }

    // Verify different simulation times are recorded
    std::vector<std::string> expected_times = {
        "0.00e+00",
        "1.00e-03",
        "2.00e-03",
        "3.00e-03",
        "4.00e-03"
    };

    for (const auto& time_str : expected_times) {
        if (!contains(contents, time_str)) {
            std::cout << "FAIL: Missing simulation time " << time_str << " in output" << std::endl;
            return 1;
        }
    }

    std::cout << "PASS: Multiple iterations recorded correctly" << std::endl;
    return 0;
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test for exception handling when writing to invalid path
int test_csv_writer_invalid_path() {
    std::cout << "Running test_csv_writer_invalid_path..." << std::endl;

    // Try to create a writer with an invalid path (non-existent directory)
    std::string invalid_path = "/nonexistent_directory_12345/test.csv";

    bool exception_thrown = false;
    try {
        csv_file_statistics_writer writer(invalid_path);
    } catch (const intialization_exception& e) {
        exception_thrown = true;
        std::cout << "Caught expected exception: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        // Accept any exception for invalid path
        exception_thrown = true;
        std::cout << "Caught exception: " << e.what() << std::endl;
    }

    if (!exception_thrown) {
        std::cout << "FAIL: Expected exception for invalid path" << std::endl;
        return 1;
    }

    std::cout << "PASS: Invalid path throws exception" << std::endl;
    return 0;
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// The main function
int main(int argc, char** argv) {

    // Check that the command line input is correctly formatted
    assert(argc == 2);

    // Get the name of the test to run
    std::string test_name = argv[1];

    // Run the selected test
    if (test_name == "test_csv_file_writer_creation")   return test_csv_file_writer_creation();
    if (test_name == "test_csv_header_format")          return test_csv_header_format();
    if (test_name == "test_string_writer_accumulation") return test_string_writer_accumulation();
    if (test_name == "test_write_data_formatting")      return test_write_data_formatting();
    if (test_name == "test_multiple_iterations")        return test_multiple_iterations();
    if (test_name == "test_csv_writer_invalid_path")    return test_csv_writer_invalid_path();

    std::cout << "TEST NAME: " << test_name << " DOES NOT EXIST" << std::endl;
    return 1;
}
//---------------------------------------------------------------------------------------------------------
