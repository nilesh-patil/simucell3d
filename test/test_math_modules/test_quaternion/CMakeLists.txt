cmake_minimum_required(VERSION 3.0)

# Use C++17 standard
set(CMAKE_CXX_STANDARD 17)

# Compile the test executable
add_executable(quaternion_test
    test_quaternion.cpp
)

# Link against the project source library
target_link_libraries(quaternion_test PUBLIC src)

# Add header file include directories
target_include_directories(quaternion_test PUBLIC "${PROJECT_SOURCE_DIR}/include/")
target_include_directories(quaternion_test PUBLIC "${PROJECT_SOURCE_DIR}/include/math_modules")

# Register all quaternion tests with CTest
# Test naming convention: math_modules_quaternion_<test_name>

# Core functionality tests
add_test(NAME math_modules_quaternion_test_construction          COMMAND quaternion_test test_quaternion_construction)
add_test(NAME math_modules_quaternion_test_normalize              COMMAND quaternion_test test_quaternion_normalize)
add_test(NAME math_modules_quaternion_test_from_matrix            COMMAND quaternion_test test_quaternion_from_matrix)
add_test(NAME math_modules_quaternion_test_to_matrix              COMMAND quaternion_test test_quaternion_to_matrix)
add_test(NAME math_modules_quaternion_test_rotation_roundtrip     COMMAND quaternion_test test_rotation_matrix_roundtrip)
add_test(NAME math_modules_quaternion_test_inverse                COMMAND quaternion_test test_quaternion_inverse)

# Operator tests
add_test(NAME math_modules_quaternion_test_scalar_multiply        COMMAND quaternion_test test_quaternion_scalar_multiply)
add_test(NAME math_modules_quaternion_test_scalar_divide          COMMAND quaternion_test test_quaternion_scalar_divide)

# Practical validation tests
add_test(NAME math_modules_quaternion_test_preserves_length       COMMAND quaternion_test test_rotation_preserves_length)
