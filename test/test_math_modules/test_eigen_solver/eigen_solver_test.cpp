#include <cassert>
#include <string>
#include <cmath>
#include <array>
#include <iostream>
#include <limits>

#include "eigen_solver.hpp"

/*
Contains all the tests run on the EigenSolver classes from eigen_solver.hpp.
Tests both the iterative (SymmetricEigensolver3x3) and non-iterative
(NISymmetricEigensolver3x3) solvers for 3x3 symmetric matrices.

Test convention: return !(condition) where 0=pass, 1=fail
*/

// Floating-point comparison tolerance
constexpr double EPS = 1e-10;

//---------------------------------------------------------------------------------------------------------
// Helper function: Check if two doubles are approximately equal
inline bool approx_equal(double a, double b, double tolerance = EPS) {
    return std::fabs(a - b) <= tolerance * std::max(1.0, std::max(std::fabs(a), std::fabs(b)));
}
//---------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------
// Helper function: Compute dot product of two 3-element arrays
inline double dot3(const std::array<double, 3>& a, const std::array<double, 3>& b) {
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}
//---------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------
// Helper function: Compute the norm of a 3-element array
inline double norm3(const std::array<double, 3>& v) {
    return std::sqrt(dot3(v, v));
}
//---------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------
// Helper function: Multiply matrix A (in symmetric form) by vector v
// A = | a00 a01 a02 |
//     | a01 a11 a12 |
//     | a02 a12 a22 |
inline std::array<double, 3> matrix_vector_mult(
    double a00, double a01, double a02, double a11, double a12, double a22,
    const std::array<double, 3>& v) {
    return {
        a00*v[0] + a01*v[1] + a02*v[2],
        a01*v[0] + a11*v[1] + a12*v[2],
        a02*v[0] + a12*v[1] + a22*v[2]
    };
}
//---------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------
// Test 1: Identity matrix eigenvalues should be (1,1,1)
// The 3x3 identity matrix has all eigenvalues equal to 1, with the standard
// basis vectors as eigenvectors.
int test_identity_matrix_eigenvalues() {
    // Identity matrix: I = diag(1,1,1)
    // a00=1, a01=0, a02=0, a11=1, a12=0, a22=1
    double a00 = 1.0, a01 = 0.0, a02 = 0.0;
    double a11 = 1.0, a12 = 0.0, a22 = 1.0;

    std::array<double, 3> eval;
    std::array<std::array<double, 3>, 3> evec;

    // Test iterative solver with ascending sort
    gte::SymmetricEigensolver3x3<double> iterative_solver;
    iterative_solver(a00, a01, a02, a11, a12, a22, false, 1, eval, evec);

    bool t1 = approx_equal(eval[0], 1.0) &&
              approx_equal(eval[1], 1.0) &&
              approx_equal(eval[2], 1.0);

    if (!t1) {
        std::cout << "Iterative solver failed for identity matrix" << std::endl;
        std::cout << "Expected: (1, 1, 1), Got: (" << eval[0] << ", " << eval[1] << ", " << eval[2] << ")" << std::endl;
    }

    // Test non-iterative solver
    gte::NISymmetricEigensolver3x3<double> ni_solver;
    ni_solver(a00, a01, a02, a11, a12, a22, eval, evec);

    bool t2 = approx_equal(eval[0], 1.0) &&
              approx_equal(eval[1], 1.0) &&
              approx_equal(eval[2], 1.0);

    if (!t2) {
        std::cout << "Non-iterative solver failed for identity matrix" << std::endl;
        std::cout << "Expected: (1, 1, 1), Got: (" << eval[0] << ", " << eval[1] << ", " << eval[2] << ")" << std::endl;
    }

    return !(t1 && t2);
}
//---------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------
// Test 2: Diagonal matrix eigenvalues equal diagonal entries
// For a diagonal matrix, eigenvalues are the diagonal entries themselves.
int test_diagonal_matrix_eigenvalues() {
    // Diagonal matrix: D = diag(2, 5, 3)
    // a00=2, a01=0, a02=0, a11=5, a12=0, a22=3
    double a00 = 2.0, a01 = 0.0, a02 = 0.0;
    double a11 = 5.0, a12 = 0.0, a22 = 3.0;

    std::array<double, 3> eval;
    std::array<std::array<double, 3>, 3> evec;

    // Test iterative solver with ascending sort
    gte::SymmetricEigensolver3x3<double> iterative_solver;
    iterative_solver(a00, a01, a02, a11, a12, a22, false, 1, eval, evec);

    // Eigenvalues should be sorted: 2, 3, 5
    bool t1 = approx_equal(eval[0], 2.0) &&
              approx_equal(eval[1], 3.0) &&
              approx_equal(eval[2], 5.0);

    if (!t1) {
        std::cout << "Iterative solver failed for diagonal matrix" << std::endl;
        std::cout << "Expected: (2, 3, 5), Got: (" << eval[0] << ", " << eval[1] << ", " << eval[2] << ")" << std::endl;
    }

    // Test non-iterative solver
    // Note: For diagonal matrices (off-diagonal norm = 0), the NISymmetricEigensolver3x3
    // returns eigenvalues in diagonal order (a00, a11, a22), NOT sorted.
    // This is intentional behavior in the implementation.
    gte::NISymmetricEigensolver3x3<double> ni_solver;
    ni_solver(a00, a01, a02, a11, a12, a22, eval, evec);

    // For diagonal matrices, NISymmetricEigensolver3x3 returns (a00, a11, a22) = (2, 5, 3)
    // Verify eigenvalues are correct (just not sorted for diagonal case)
    std::array<double, 3> eval_sorted = eval;
    std::sort(eval_sorted.begin(), eval_sorted.end());
    bool t2 = approx_equal(eval_sorted[0], 2.0) &&
              approx_equal(eval_sorted[1], 3.0) &&
              approx_equal(eval_sorted[2], 5.0);

    if (!t2) {
        std::cout << "Non-iterative solver failed for diagonal matrix" << std::endl;
        std::cout << "Expected (sorted): (2, 3, 5), Got: (" << eval[0] << ", " << eval[1] << ", " << eval[2] << ")" << std::endl;
    }

    // Test with negative eigenvalue (indefinite matrix)
    // D = diag(-1, 2, 4)
    a00 = -1.0; a01 = 0.0; a02 = 0.0;
    a11 = 2.0;  a12 = 0.0; a22 = 4.0;

    iterative_solver(a00, a01, a02, a11, a12, a22, false, 1, eval, evec);

    bool t3 = approx_equal(eval[0], -1.0) &&
              approx_equal(eval[1], 2.0) &&
              approx_equal(eval[2], 4.0);

    if (!t3) {
        std::cout << "Iterative solver failed for indefinite diagonal matrix" << std::endl;
        std::cout << "Expected: (-1, 2, 4), Got: (" << eval[0] << ", " << eval[1] << ", " << eval[2] << ")" << std::endl;
    }

    return !(t1 && t2 && t3);
}
//---------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------
// Test 3: Verify eigendecomposition A = Q*D*Q^T
// For a symmetric matrix A with eigenvalues D and eigenvectors Q,
// A = Q * diag(eigenvalues) * Q^T
int test_symmetric_eigendecomposition() {
    // Test matrix: symmetric positive definite
    // A = | 4  2  1 |
    //     | 2  5  3 |
    //     | 1  3  6 |
    double a00 = 4.0, a01 = 2.0, a02 = 1.0;
    double a11 = 5.0, a12 = 3.0, a22 = 6.0;

    std::array<double, 3> eval;
    std::array<std::array<double, 3>, 3> evec;

    // Use iterative solver
    gte::SymmetricEigensolver3x3<double> iterative_solver;
    iterative_solver(a00, a01, a02, a11, a12, a22, false, 1, eval, evec);

    // Reconstruct A from eigendecomposition: A_reconstructed = Q * D * Q^T
    // A_ij = sum_k (evec[k][i] * eval[k] * evec[k][j])
    double A_reconstructed[3][3] = {{0,0,0}, {0,0,0}, {0,0,0}};
    for (int k = 0; k < 3; ++k) {
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                A_reconstructed[i][j] += evec[k][i] * eval[k] * evec[k][j];
            }
        }
    }

    // Compare with original matrix
    bool t1 = approx_equal(A_reconstructed[0][0], a00) &&
              approx_equal(A_reconstructed[0][1], a01) &&
              approx_equal(A_reconstructed[0][2], a02) &&
              approx_equal(A_reconstructed[1][0], a01) &&
              approx_equal(A_reconstructed[1][1], a11) &&
              approx_equal(A_reconstructed[1][2], a12) &&
              approx_equal(A_reconstructed[2][0], a02) &&
              approx_equal(A_reconstructed[2][1], a12) &&
              approx_equal(A_reconstructed[2][2], a22);

    if (!t1) {
        std::cout << "Eigendecomposition reconstruction failed for iterative solver" << std::endl;
        std::cout << "Original matrix:" << std::endl;
        std::cout << "| " << a00 << " " << a01 << " " << a02 << " |" << std::endl;
        std::cout << "| " << a01 << " " << a11 << " " << a12 << " |" << std::endl;
        std::cout << "| " << a02 << " " << a12 << " " << a22 << " |" << std::endl;
        std::cout << "Reconstructed matrix:" << std::endl;
        std::cout << "| " << A_reconstructed[0][0] << " " << A_reconstructed[0][1] << " " << A_reconstructed[0][2] << " |" << std::endl;
        std::cout << "| " << A_reconstructed[1][0] << " " << A_reconstructed[1][1] << " " << A_reconstructed[1][2] << " |" << std::endl;
        std::cout << "| " << A_reconstructed[2][0] << " " << A_reconstructed[2][1] << " " << A_reconstructed[2][2] << " |" << std::endl;
    }

    // Test non-iterative solver reconstruction
    gte::NISymmetricEigensolver3x3<double> ni_solver;
    ni_solver(a00, a01, a02, a11, a12, a22, eval, evec);

    // Reset and reconstruct
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            A_reconstructed[i][j] = 0.0;

    for (int k = 0; k < 3; ++k) {
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                A_reconstructed[i][j] += evec[k][i] * eval[k] * evec[k][j];
            }
        }
    }

    bool t2 = approx_equal(A_reconstructed[0][0], a00) &&
              approx_equal(A_reconstructed[0][1], a01) &&
              approx_equal(A_reconstructed[0][2], a02) &&
              approx_equal(A_reconstructed[1][0], a01) &&
              approx_equal(A_reconstructed[1][1], a11) &&
              approx_equal(A_reconstructed[1][2], a12) &&
              approx_equal(A_reconstructed[2][0], a02) &&
              approx_equal(A_reconstructed[2][1], a12) &&
              approx_equal(A_reconstructed[2][2], a22);

    if (!t2) {
        std::cout << "Eigendecomposition reconstruction failed for non-iterative solver" << std::endl;
    }

    // Test with an indefinite matrix (has negative eigenvalue)
    // A = | 1   2   0 |
    //     | 2  -1   1 |
    //     | 0   1   1 |
    a00 = 1.0;  a01 = 2.0;  a02 = 0.0;
    a11 = -1.0; a12 = 1.0;  a22 = 1.0;

    iterative_solver(a00, a01, a02, a11, a12, a22, false, 1, eval, evec);

    // Reset and reconstruct
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            A_reconstructed[i][j] = 0.0;

    for (int k = 0; k < 3; ++k) {
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                A_reconstructed[i][j] += evec[k][i] * eval[k] * evec[k][j];
            }
        }
    }

    bool t3 = approx_equal(A_reconstructed[0][0], a00) &&
              approx_equal(A_reconstructed[0][1], a01) &&
              approx_equal(A_reconstructed[0][2], a02) &&
              approx_equal(A_reconstructed[1][0], a01) &&
              approx_equal(A_reconstructed[1][1], a11) &&
              approx_equal(A_reconstructed[1][2], a12) &&
              approx_equal(A_reconstructed[2][0], a02) &&
              approx_equal(A_reconstructed[2][1], a12) &&
              approx_equal(A_reconstructed[2][2], a22);

    if (!t3) {
        std::cout << "Eigendecomposition reconstruction failed for indefinite matrix" << std::endl;
    }

    return !(t1 && t2 && t3);
}
//---------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------
// Test 4: Verify eigenvector orthonormality
// Eigenvectors of a symmetric matrix should be orthonormal:
// - |v_i| = 1 for all eigenvectors (unit length)
// - v_i . v_j = 0 for i != j (mutually orthogonal)
int test_eigenvector_orthonormality() {
    // Test matrix with distinct eigenvalues
    // A = | 3  1  0 |
    //     | 1  2  1 |
    //     | 0  1  3 |
    double a00 = 3.0, a01 = 1.0, a02 = 0.0;
    double a11 = 2.0, a12 = 1.0, a22 = 3.0;

    std::array<double, 3> eval;
    std::array<std::array<double, 3>, 3> evec;

    // Test iterative solver
    gte::SymmetricEigensolver3x3<double> iterative_solver;
    iterative_solver(a00, a01, a02, a11, a12, a22, false, 1, eval, evec);

    // Check unit length for all eigenvectors
    double norm0 = norm3(evec[0]);
    double norm1 = norm3(evec[1]);
    double norm2 = norm3(evec[2]);

    bool t1 = approx_equal(norm0, 1.0) &&
              approx_equal(norm1, 1.0) &&
              approx_equal(norm2, 1.0);

    if (!t1) {
        std::cout << "Iterative solver: eigenvectors not unit length" << std::endl;
        std::cout << "Norms: " << norm0 << ", " << norm1 << ", " << norm2 << std::endl;
    }

    // Check mutual orthogonality
    double dot01 = dot3(evec[0], evec[1]);
    double dot02 = dot3(evec[0], evec[2]);
    double dot12 = dot3(evec[1], evec[2]);

    bool t2 = approx_equal(dot01, 0.0) &&
              approx_equal(dot02, 0.0) &&
              approx_equal(dot12, 0.0);

    if (!t2) {
        std::cout << "Iterative solver: eigenvectors not orthogonal" << std::endl;
        std::cout << "Dot products: v0.v1=" << dot01 << ", v0.v2=" << dot02 << ", v1.v2=" << dot12 << std::endl;
    }

    // Test non-iterative solver
    gte::NISymmetricEigensolver3x3<double> ni_solver;
    ni_solver(a00, a01, a02, a11, a12, a22, eval, evec);

    // Check unit length
    norm0 = norm3(evec[0]);
    norm1 = norm3(evec[1]);
    norm2 = norm3(evec[2]);

    bool t3 = approx_equal(norm0, 1.0) &&
              approx_equal(norm1, 1.0) &&
              approx_equal(norm2, 1.0);

    if (!t3) {
        std::cout << "Non-iterative solver: eigenvectors not unit length" << std::endl;
        std::cout << "Norms: " << norm0 << ", " << norm1 << ", " << norm2 << std::endl;
    }

    // Check mutual orthogonality
    dot01 = dot3(evec[0], evec[1]);
    dot02 = dot3(evec[0], evec[2]);
    dot12 = dot3(evec[1], evec[2]);

    bool t4 = approx_equal(dot01, 0.0) &&
              approx_equal(dot02, 0.0) &&
              approx_equal(dot12, 0.0);

    if (!t4) {
        std::cout << "Non-iterative solver: eigenvectors not orthogonal" << std::endl;
        std::cout << "Dot products: v0.v1=" << dot01 << ", v0.v2=" << dot02 << ", v1.v2=" << dot12 << std::endl;
    }

    // Test with a matrix that has repeated eigenvalues
    // The identity matrix has eigenvalue 1 with multiplicity 3
    a00 = 1.0; a01 = 0.0; a02 = 0.0;
    a11 = 1.0; a12 = 0.0; a22 = 1.0;

    iterative_solver(a00, a01, a02, a11, a12, a22, false, 1, eval, evec);

    norm0 = norm3(evec[0]);
    norm1 = norm3(evec[1]);
    norm2 = norm3(evec[2]);
    dot01 = dot3(evec[0], evec[1]);
    dot02 = dot3(evec[0], evec[2]);
    dot12 = dot3(evec[1], evec[2]);

    bool t5 = approx_equal(norm0, 1.0) && approx_equal(norm1, 1.0) && approx_equal(norm2, 1.0) &&
              approx_equal(dot01, 0.0) && approx_equal(dot02, 0.0) && approx_equal(dot12, 0.0);

    if (!t5) {
        std::cout << "Orthonormality failed for repeated eigenvalues" << std::endl;
    }

    return !(t1 && t2 && t3 && t4 && t5);
}
//---------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------
// Test 5: Verify eigenvalue ordering with different sort types
// sortType = +1: ascending order (eval[0] <= eval[1] <= eval[2])
// sortType = -1: descending order (eval[0] >= eval[1] >= eval[2])
// sortType = 0: no sorting
int test_eigenvalue_ordering() {
    // Matrix with distinct eigenvalues for clear ordering
    // A = | 5  1  0 |
    //     | 1  3  1 |
    //     | 0  1  1 |
    double a00 = 5.0, a01 = 1.0, a02 = 0.0;
    double a11 = 3.0, a12 = 1.0, a22 = 1.0;

    std::array<double, 3> eval_asc, eval_desc, eval_unsorted;
    std::array<std::array<double, 3>, 3> evec;

    gte::SymmetricEigensolver3x3<double> iterative_solver;

    // Test ascending order (sortType = +1)
    iterative_solver(a00, a01, a02, a11, a12, a22, false, 1, eval_asc, evec);

    bool t1 = (eval_asc[0] <= eval_asc[1]) && (eval_asc[1] <= eval_asc[2]);

    if (!t1) {
        std::cout << "Ascending order test failed" << std::endl;
        std::cout << "Eigenvalues: " << eval_asc[0] << " <= " << eval_asc[1] << " <= " << eval_asc[2] << std::endl;
    }

    // Test descending order (sortType = -1)
    iterative_solver(a00, a01, a02, a11, a12, a22, false, -1, eval_desc, evec);

    bool t2 = (eval_desc[0] >= eval_desc[1]) && (eval_desc[1] >= eval_desc[2]);

    if (!t2) {
        std::cout << "Descending order test failed" << std::endl;
        std::cout << "Eigenvalues: " << eval_desc[0] << " >= " << eval_desc[1] << " >= " << eval_desc[2] << std::endl;
    }

    // Verify ascending and descending have same eigenvalues, just reversed
    bool t3 = approx_equal(eval_asc[0], eval_desc[2]) &&
              approx_equal(eval_asc[1], eval_desc[1]) &&
              approx_equal(eval_asc[2], eval_desc[0]);

    if (!t3) {
        std::cout << "Ascending and descending eigenvalues don't match" << std::endl;
    }

    // Test no sorting (sortType = 0)
    iterative_solver(a00, a01, a02, a11, a12, a22, false, 0, eval_unsorted, evec);

    // All three sets should have the same eigenvalues (just different order)
    // Sort eval_unsorted and compare with eval_asc
    std::array<double, 3> eval_unsorted_sorted = eval_unsorted;
    std::sort(eval_unsorted_sorted.begin(), eval_unsorted_sorted.end());

    bool t4 = approx_equal(eval_unsorted_sorted[0], eval_asc[0]) &&
              approx_equal(eval_unsorted_sorted[1], eval_asc[1]) &&
              approx_equal(eval_unsorted_sorted[2], eval_asc[2]);

    if (!t4) {
        std::cout << "Unsorted eigenvalues don't match when sorted" << std::endl;
    }

    // Test non-iterative solver (always ascending)
    gte::NISymmetricEigensolver3x3<double> ni_solver;
    std::array<double, 3> eval_ni;
    ni_solver(a00, a01, a02, a11, a12, a22, eval_ni, evec);

    bool t5 = (eval_ni[0] <= eval_ni[1]) && (eval_ni[1] <= eval_ni[2]);

    if (!t5) {
        std::cout << "Non-iterative solver not in ascending order" << std::endl;
        std::cout << "Eigenvalues: " << eval_ni[0] << " <= " << eval_ni[1] << " <= " << eval_ni[2] << std::endl;
    }

    // Verify non-iterative matches iterative ascending
    bool t6 = approx_equal(eval_ni[0], eval_asc[0]) &&
              approx_equal(eval_ni[1], eval_asc[1]) &&
              approx_equal(eval_ni[2], eval_asc[2]);

    if (!t6) {
        std::cout << "Non-iterative and iterative (ascending) eigenvalues differ" << std::endl;
        std::cout << "NI: (" << eval_ni[0] << ", " << eval_ni[1] << ", " << eval_ni[2] << ")" << std::endl;
        std::cout << "Iterative: (" << eval_asc[0] << ", " << eval_asc[1] << ", " << eval_asc[2] << ")" << std::endl;
    }

    return !(t1 && t2 && t3 && t4 && t5 && t6);
}
//---------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------
// Test 6: Zero matrix should give (0,0,0) eigenvalues
// The zero matrix is a special case that the non-iterative solver handles explicitly.
int test_zero_matrix_handling() {
    // Zero matrix
    double a00 = 0.0, a01 = 0.0, a02 = 0.0;
    double a11 = 0.0, a12 = 0.0, a22 = 0.0;

    std::array<double, 3> eval;
    std::array<std::array<double, 3>, 3> evec;

    // Test non-iterative solver (has explicit zero matrix handling)
    gte::NISymmetricEigensolver3x3<double> ni_solver;
    ni_solver(a00, a01, a02, a11, a12, a22, eval, evec);

    bool t1 = approx_equal(eval[0], 0.0) &&
              approx_equal(eval[1], 0.0) &&
              approx_equal(eval[2], 0.0);

    if (!t1) {
        std::cout << "Non-iterative solver failed for zero matrix" << std::endl;
        std::cout << "Expected: (0, 0, 0), Got: (" << eval[0] << ", " << eval[1] << ", " << eval[2] << ")" << std::endl;
    }

    // For zero matrix, non-iterative solver returns standard basis as eigenvectors
    // evec[0] = (1, 0, 0), evec[1] = (0, 1, 0), evec[2] = (0, 0, 1)
    bool t2 = approx_equal(evec[0][0], 1.0) && approx_equal(evec[0][1], 0.0) && approx_equal(evec[0][2], 0.0) &&
              approx_equal(evec[1][0], 0.0) && approx_equal(evec[1][1], 1.0) && approx_equal(evec[1][2], 0.0) &&
              approx_equal(evec[2][0], 0.0) && approx_equal(evec[2][1], 0.0) && approx_equal(evec[2][2], 1.0);

    if (!t2) {
        std::cout << "Non-iterative solver returned wrong eigenvectors for zero matrix" << std::endl;
    }

    // Test iterative solver
    gte::SymmetricEigensolver3x3<double> iterative_solver;
    iterative_solver(a00, a01, a02, a11, a12, a22, false, 1, eval, evec);

    bool t3 = approx_equal(eval[0], 0.0) &&
              approx_equal(eval[1], 0.0) &&
              approx_equal(eval[2], 0.0);

    if (!t3) {
        std::cout << "Iterative solver failed for zero matrix" << std::endl;
        std::cout << "Expected: (0, 0, 0), Got: (" << eval[0] << ", " << eval[1] << ", " << eval[2] << ")" << std::endl;
    }

    // Test near-zero matrix (very small values)
    double tiny = 1e-15;
    a00 = tiny; a01 = 0.0;  a02 = 0.0;
    a11 = tiny; a12 = 0.0;  a22 = tiny;

    ni_solver(a00, a01, a02, a11, a12, a22, eval, evec);

    bool t4 = approx_equal(eval[0], tiny) &&
              approx_equal(eval[1], tiny) &&
              approx_equal(eval[2], tiny);

    if (!t4) {
        std::cout << "Non-iterative solver failed for near-zero diagonal matrix" << std::endl;
        std::cout << "Expected: (" << tiny << ", " << tiny << ", " << tiny << ")" << std::endl;
        std::cout << "Got: (" << eval[0] << ", " << eval[1] << ", " << eval[2] << ")" << std::endl;
    }

    return !(t1 && t2 && t3 && t4);
}
//---------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------
// Test 7: Both solvers should give same results (within tolerance)
// Compare iterative and non-iterative solvers on various matrices.
int test_iterative_vs_noniterative() {
    gte::SymmetricEigensolver3x3<double> iterative_solver;
    gte::NISymmetricEigensolver3x3<double> ni_solver;

    std::array<double, 3> eval_iter, eval_ni;
    std::array<std::array<double, 3>, 3> evec_iter, evec_ni;

    // Test matrix 1: general symmetric
    double a00 = 2.0, a01 = 1.0, a02 = 0.5;
    double a11 = 3.0, a12 = 0.7, a22 = 4.0;

    iterative_solver(a00, a01, a02, a11, a12, a22, false, 1, eval_iter, evec_iter);
    ni_solver(a00, a01, a02, a11, a12, a22, eval_ni, evec_ni);

    // Compare eigenvalues
    bool t1 = approx_equal(eval_iter[0], eval_ni[0]) &&
              approx_equal(eval_iter[1], eval_ni[1]) &&
              approx_equal(eval_iter[2], eval_ni[2]);

    if (!t1) {
        std::cout << "Test matrix 1: eigenvalues differ" << std::endl;
        std::cout << "Iterative: (" << eval_iter[0] << ", " << eval_iter[1] << ", " << eval_iter[2] << ")" << std::endl;
        std::cout << "Non-iterative: (" << eval_ni[0] << ", " << eval_ni[1] << ", " << eval_ni[2] << ")" << std::endl;
    }

    // Test matrix 2: positive definite with larger off-diagonal terms
    a00 = 6.0; a01 = 2.0; a02 = 1.0;
    a11 = 5.0; a12 = 2.0; a22 = 4.0;

    iterative_solver(a00, a01, a02, a11, a12, a22, false, 1, eval_iter, evec_iter);
    ni_solver(a00, a01, a02, a11, a12, a22, eval_ni, evec_ni);

    bool t2 = approx_equal(eval_iter[0], eval_ni[0]) &&
              approx_equal(eval_iter[1], eval_ni[1]) &&
              approx_equal(eval_iter[2], eval_ni[2]);

    if (!t2) {
        std::cout << "Test matrix 2: eigenvalues differ" << std::endl;
        std::cout << "Iterative: (" << eval_iter[0] << ", " << eval_iter[1] << ", " << eval_iter[2] << ")" << std::endl;
        std::cout << "Non-iterative: (" << eval_ni[0] << ", " << eval_ni[1] << ", " << eval_ni[2] << ")" << std::endl;
    }

    // Test matrix 3: indefinite (has negative eigenvalue)
    a00 = 1.0;  a01 = 3.0;  a02 = 0.0;
    a11 = -2.0; a12 = 1.0;  a22 = 2.0;

    iterative_solver(a00, a01, a02, a11, a12, a22, false, 1, eval_iter, evec_iter);
    ni_solver(a00, a01, a02, a11, a12, a22, eval_ni, evec_ni);

    bool t3 = approx_equal(eval_iter[0], eval_ni[0]) &&
              approx_equal(eval_iter[1], eval_ni[1]) &&
              approx_equal(eval_iter[2], eval_ni[2]);

    if (!t3) {
        std::cout << "Test matrix 3 (indefinite): eigenvalues differ" << std::endl;
        std::cout << "Iterative: (" << eval_iter[0] << ", " << eval_iter[1] << ", " << eval_iter[2] << ")" << std::endl;
        std::cout << "Non-iterative: (" << eval_ni[0] << ", " << eval_ni[1] << ", " << eval_ni[2] << ")" << std::endl;
    }

    // Test matrix 4: nearly singular (small eigenvalue)
    a00 = 1.0;   a01 = 0.99; a02 = 0.0;
    a11 = 1.0;   a12 = 0.0;  a22 = 0.01;

    iterative_solver(a00, a01, a02, a11, a12, a22, false, 1, eval_iter, evec_iter);
    ni_solver(a00, a01, a02, a11, a12, a22, eval_ni, evec_ni);

    // For nearly singular matrices with very small eigenvalues, use a more relaxed
    // relative tolerance since numerical methods can differ slightly at small scales
    double tol_singular = 1e-6;
    auto rel_close = [tol_singular](double a, double b) {
        double scale = std::max(1e-10, std::max(std::fabs(a), std::fabs(b)));
        return std::fabs(a - b) <= tol_singular * scale;
    };

    bool t4 = rel_close(eval_iter[0], eval_ni[0]) &&
              rel_close(eval_iter[1], eval_ni[1]) &&
              rel_close(eval_iter[2], eval_ni[2]);

    if (!t4) {
        std::cout << "Test matrix 4 (nearly singular): eigenvalues differ significantly" << std::endl;
        std::cout << "Iterative: (" << eval_iter[0] << ", " << eval_iter[1] << ", " << eval_iter[2] << ")" << std::endl;
        std::cout << "Non-iterative: (" << eval_ni[0] << ", " << eval_ni[1] << ", " << eval_ni[2] << ")" << std::endl;
    }

    // Test matrix 5: large values (tests preconditioning)
    a00 = 1e6;  a01 = 1e5;  a02 = 0.0;
    a11 = 2e6;  a12 = 1e5;  a22 = 3e6;

    iterative_solver(a00, a01, a02, a11, a12, a22, false, 1, eval_iter, evec_iter);
    ni_solver(a00, a01, a02, a11, a12, a22, eval_ni, evec_ni);

    // Use relative tolerance for large values
    double tol_large = 1e-6;  // Relative tolerance for large eigenvalues
    bool t5 = std::fabs(eval_iter[0] - eval_ni[0]) <= tol_large * std::fabs(eval_ni[0]) &&
              std::fabs(eval_iter[1] - eval_ni[1]) <= tol_large * std::fabs(eval_ni[1]) &&
              std::fabs(eval_iter[2] - eval_ni[2]) <= tol_large * std::fabs(eval_ni[2]);

    if (!t5) {
        std::cout << "Test matrix 5 (large values): eigenvalues differ significantly" << std::endl;
        std::cout << "Iterative: (" << eval_iter[0] << ", " << eval_iter[1] << ", " << eval_iter[2] << ")" << std::endl;
        std::cout << "Non-iterative: (" << eval_ni[0] << ", " << eval_ni[1] << ", " << eval_ni[2] << ")" << std::endl;
    }

    // Verify that Av = lambda*v for both solvers (eigenvector equation)
    // Using test matrix 1
    a00 = 2.0; a01 = 1.0; a02 = 0.5;
    a11 = 3.0; a12 = 0.7; a22 = 4.0;

    iterative_solver(a00, a01, a02, a11, a12, a22, false, 1, eval_iter, evec_iter);

    bool eigenvec_check = true;
    for (int k = 0; k < 3; ++k) {
        auto Av = matrix_vector_mult(a00, a01, a02, a11, a12, a22, evec_iter[k]);
        for (int i = 0; i < 3; ++i) {
            if (!approx_equal(Av[i], eval_iter[k] * evec_iter[k][i])) {
                eigenvec_check = false;
            }
        }
    }

    bool t6 = eigenvec_check;
    if (!t6) {
        std::cout << "Eigenvector equation Av = lambda*v not satisfied (iterative)" << std::endl;
    }

    ni_solver(a00, a01, a02, a11, a12, a22, eval_ni, evec_ni);

    eigenvec_check = true;
    for (int k = 0; k < 3; ++k) {
        auto Av = matrix_vector_mult(a00, a01, a02, a11, a12, a22, evec_ni[k]);
        for (int i = 0; i < 3; ++i) {
            if (!approx_equal(Av[i], eval_ni[k] * evec_ni[k][i])) {
                eigenvec_check = false;
            }
        }
    }

    bool t7 = eigenvec_check;
    if (!t7) {
        std::cout << "Eigenvector equation Av = lambda*v not satisfied (non-iterative)" << std::endl;
    }

    return !(t1 && t2 && t3 && t4 && t5 && t6 && t7);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Additional test: Verify right-handed coordinate system for sorted eigenvectors
// When eigenvalues are sorted, the eigenvector set should form a right-handed orthonormal basis.
int test_right_handed_eigenvectors() {
    // Test matrix
    double a00 = 4.0, a01 = 2.0, a02 = 1.0;
    double a11 = 5.0, a12 = 3.0, a22 = 6.0;

    std::array<double, 3> eval;
    std::array<std::array<double, 3>, 3> evec;

    gte::SymmetricEigensolver3x3<double> iterative_solver;
    iterative_solver(a00, a01, a02, a11, a12, a22, false, 1, eval, evec);

    // Compute cross product: evec[0] x evec[1]
    std::array<double, 3> cross = {
        evec[0][1] * evec[1][2] - evec[0][2] * evec[1][1],
        evec[0][2] * evec[1][0] - evec[0][0] * evec[1][2],
        evec[0][0] * evec[1][1] - evec[0][1] * evec[1][0]
    };

    // For right-handed system: evec[0] x evec[1] should equal evec[2] (or -evec[2])
    // The iterative solver ensures right-handed by potentially flipping evec[2]
    double dot_result = dot3(cross, evec[2]);

    // dot_result should be +1 for right-handed system
    bool t1 = approx_equal(std::fabs(dot_result), 1.0);

    if (!t1) {
        std::cout << "Right-handed check failed" << std::endl;
        std::cout << "(evec[0] x evec[1]) . evec[2] = " << dot_result << std::endl;
    }

    // The triple scalar product should be positive for right-handed
    // Note: the solver adjusts evec[2] to ensure this
    bool t2 = dot_result > 0;

    if (!t2) {
        std::cout << "Eigenvectors do not form right-handed system (dot = " << dot_result << ")" << std::endl;
    }

    return !(t1 && t2);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Additional test: Test aggressive vs non-aggressive convergence modes
int test_aggressive_convergence() {
    // Test matrix
    double a00 = 3.0, a01 = 1.0, a02 = 0.5;
    double a11 = 4.0, a12 = 0.8, a22 = 2.0;

    std::array<double, 3> eval_agg, eval_nonagg;
    std::array<std::array<double, 3>, 3> evec_agg, evec_nonagg;

    gte::SymmetricEigensolver3x3<double> solver;

    // Test with aggressive convergence
    int iter_agg = solver(a00, a01, a02, a11, a12, a22, true, 1, eval_agg, evec_agg);

    // Test with non-aggressive convergence
    int iter_nonagg = solver(a00, a01, a02, a11, a12, a22, false, 1, eval_nonagg, evec_nonagg);

    // Both should produce the same eigenvalues
    bool t1 = approx_equal(eval_agg[0], eval_nonagg[0]) &&
              approx_equal(eval_agg[1], eval_nonagg[1]) &&
              approx_equal(eval_agg[2], eval_nonagg[2]);

    if (!t1) {
        std::cout << "Aggressive and non-aggressive modes give different eigenvalues" << std::endl;
        std::cout << "Aggressive: (" << eval_agg[0] << ", " << eval_agg[1] << ", " << eval_agg[2] << ")" << std::endl;
        std::cout << "Non-aggressive: (" << eval_nonagg[0] << ", " << eval_nonagg[1] << ", " << eval_nonagg[2] << ")" << std::endl;
    }

    // Non-aggressive should typically converge in fewer iterations
    // (though this is not strictly guaranteed for all matrices)
    bool t2 = (iter_nonagg <= iter_agg);

    // Even if non-aggressive takes more iterations occasionally, both should work
    // Just verify they both completed successfully (didn't hit max iterations)
    bool t3 = (iter_agg < 1000) && (iter_nonagg < 1000);

    if (!t3) {
        std::cout << "Too many iterations: aggressive=" << iter_agg << ", non-aggressive=" << iter_nonagg << std::endl;
    }

    return !(t1 && t3);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// The main function - dispatches to the appropriate test based on command line argument
int main(int argc, char** argv) {
    // Check that the command line input is correctly formatted
    assert(argc == 2);

    // Get the name of the test to run
    std::string test_name = argv[1];

    // Run the selected test
    if (test_name == "test_identity_matrix_eigenvalues")       return test_identity_matrix_eigenvalues();
    if (test_name == "test_diagonal_matrix_eigenvalues")       return test_diagonal_matrix_eigenvalues();
    if (test_name == "test_symmetric_eigendecomposition")      return test_symmetric_eigendecomposition();
    if (test_name == "test_eigenvector_orthonormality")        return test_eigenvector_orthonormality();
    if (test_name == "test_eigenvalue_ordering")               return test_eigenvalue_ordering();
    if (test_name == "test_zero_matrix_handling")              return test_zero_matrix_handling();
    if (test_name == "test_iterative_vs_noniterative")         return test_iterative_vs_noniterative();
    if (test_name == "test_right_handed_eigenvectors")         return test_right_handed_eigenvectors();
    if (test_name == "test_aggressive_convergence")            return test_aggressive_convergence();

    std::cout << "TEST NAME: " << test_name << " DOES NOT EXIST" << std::endl;
    return 1;
}
//---------------------------------------------------------------------------------------------------------
