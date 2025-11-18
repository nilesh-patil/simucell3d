cmake_minimum_required(VERSION 3.0)

#Use CPP 17
set(CMAKE_CXX_STANDARD 17)

#Compile the files containing the tests to run
add_executable(eigen_solver_test
    eigen_solver_test.cpp
)

target_link_libraries(eigen_solver_test PUBLIC src)

#Add all the header files required to run the root file of the project
target_include_directories(eigen_solver_test PUBLIC "${PROJECT_SOURCE_DIR}/include/")
target_include_directories(eigen_solver_test PUBLIC "${PROJECT_SOURCE_DIR}/include/math_modules")

#Run the tests
add_test(NAME math_modules_eigen_solver_identity_matrix          COMMAND eigen_solver_test test_identity_matrix_eigenvalues)
add_test(NAME math_modules_eigen_solver_diagonal_matrix          COMMAND eigen_solver_test test_diagonal_matrix_eigenvalues)
add_test(NAME math_modules_eigen_solver_eigendecomposition       COMMAND eigen_solver_test test_symmetric_eigendecomposition)
add_test(NAME math_modules_eigen_solver_orthonormality           COMMAND eigen_solver_test test_eigenvector_orthonormality)
add_test(NAME math_modules_eigen_solver_ordering                 COMMAND eigen_solver_test test_eigenvalue_ordering)
add_test(NAME math_modules_eigen_solver_zero_matrix              COMMAND eigen_solver_test test_zero_matrix_handling)
add_test(NAME math_modules_eigen_solver_iterative_vs_noniterative COMMAND eigen_solver_test test_iterative_vs_noniterative)
add_test(NAME math_modules_eigen_solver_right_handed             COMMAND eigen_solver_test test_right_handed_eigenvectors)
add_test(NAME math_modules_eigen_solver_aggressive_convergence   COMMAND eigen_solver_test test_aggressive_convergence)
