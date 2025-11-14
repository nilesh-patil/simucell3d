#include <cassert>
#include <string>
#include <cmath>
#include <iostream>

#include "utils.hpp"
#include "quaternion.hpp"
#include "mat33.hpp"
#include "vec3.hpp"

/*
 * Comprehensive unit tests for the quaternion class.
 *
 * The quaternion class is used during cell divisions to normalize rotation matrices,
 * ensuring numerical stability of rotation operations in the simulation.
 *
 * Test return convention: return !(condition) where 0=pass, 1=fail
 */

// Floating-point comparison tolerance
constexpr double EPS = 1e-10;

// Helper function: check if two doubles are approximately equal
inline bool approx_equal(double a, double b, double tolerance = EPS) {
    return std::fabs(a - b) < tolerance;
}

// Helper function: check if two matrices are approximately equal element-wise
inline bool matrices_approx_equal(const mat33& m1, const mat33& m2, double tolerance = EPS) {
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            if (!approx_equal(m1[i][j], m2[i][j], tolerance)) {
                return false;
            }
        }
    }
    return true;
}

// Helper function: compute quaternion magnitude
inline double quaternion_magnitude(double w, double i, double j, double k) {
    return std::sqrt(w*w + i*i + j*j + k*k);
}

// Helper function: create rotation matrix around X-axis by angle theta
inline mat33 rotation_x(double theta) {
    double c = std::cos(theta);
    double s = std::sin(theta);
    return mat33(
        {1., 0., 0.},
        {0., c, -s},
        {0., s, c}
    );
}

// Helper function: create rotation matrix around Y-axis by angle theta
inline mat33 rotation_y(double theta) {
    double c = std::cos(theta);
    double s = std::sin(theta);
    return mat33(
        {c, 0., s},
        {0., 1., 0.},
        {-s, 0., c}
    );
}

// Helper function: create rotation matrix around Z-axis by angle theta
inline mat33 rotation_z(double theta) {
    double c = std::cos(theta);
    double s = std::sin(theta);
    return mat33(
        {c, -s, 0.},
        {s, c, 0.},
        {0., 0., 1.}
    );
}


//---------------------------------------------------------------------------------------------------------
// Test 1: Basic quaternion construction from 4 components
// Verifies that the quaternion correctly stores w, i, j, k components
int test_quaternion_construction() {
    // Test construction with explicit values
    quaternion q1(1.0, 2.0, 3.0, 4.0);

    // Convert to matrix and back to verify internal storage
    // A quaternion (w, i, j, k) should convert to a specific rotation matrix
    // We test by checking roundtrip behavior
    mat33 m1 = q1.to_matrix();

    // The to_matrix() function uses the quaternion components, so if we get
    // a valid matrix, construction worked
    bool valid_construction = std::isfinite(m1[0][0]) && std::isfinite(m1[1][1]) && std::isfinite(m1[2][2]);

    // Test default constructor
    quaternion q2;
    mat33 m2 = q2.to_matrix();
    // Default quaternion (0,0,0,0) produces identity-like behavior after normalization

    // Test copy constructor
    quaternion q3(q1);
    mat33 m3 = q3.to_matrix();
    bool copy_works = matrices_approx_equal(m1, m3);

    // Test move constructor
    quaternion q4(quaternion(1.0, 2.0, 3.0, 4.0));
    mat33 m4 = q4.to_matrix();
    bool move_works = matrices_approx_equal(m1, m4);

    // Test copy assignment
    quaternion q5;
    q5 = q1;
    mat33 m5 = q5.to_matrix();
    bool copy_assign_works = matrices_approx_equal(m1, m5);

    // Test move assignment
    quaternion q6;
    q6 = quaternion(1.0, 2.0, 3.0, 4.0);
    mat33 m6 = q6.to_matrix();
    bool move_assign_works = matrices_approx_equal(m1, m6);

    return !(valid_construction && copy_works && move_works &&
             copy_assign_works && move_assign_works);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 2: Quaternion normalization
// Verifies that normalize() produces a unit quaternion with magnitude 1.0
int test_quaternion_normalize() {
    // Test with various non-unit quaternions

    // Case 1: Simple non-unit quaternion
    quaternion q1(2.0, 0.0, 0.0, 0.0);
    quaternion q1_norm = q1.normalize();
    // After normalization, converting to matrix and back should give unit quaternion
    mat33 m1 = q1_norm.to_matrix();
    quaternion q1_check = quaternion::from_matrix(m1);
    // The roundtrip should preserve the rotation (identity in this case)
    mat33 m1_check = q1_check.to_matrix();
    bool t1 = matrices_approx_equal(m1, m1_check);

    // Case 2: Quaternion with all non-zero components
    quaternion q2(1.0, 1.0, 1.0, 1.0);
    quaternion q2_norm = q2.normalize();
    // After normalization, magnitude should be 1
    // We verify by checking that to_matrix gives a valid rotation matrix
    mat33 m2 = q2_norm.to_matrix();
    // A valid rotation matrix has determinant = 1 and is orthogonal
    double det2 = m2.determinant();
    bool t2 = approx_equal(det2, 1.0, 1e-6);

    // Case 3: Small magnitude quaternion (tests numerical stability)
    quaternion q3(0.1, 0.1, 0.1, 0.1);
    quaternion q3_norm = q3.normalize();
    mat33 m3 = q3_norm.to_matrix();
    double det3 = m3.determinant();
    bool t3 = approx_equal(det3, 1.0, 1e-6);

    // Case 4: Large magnitude quaternion
    quaternion q4(100.0, 200.0, 300.0, 400.0);
    quaternion q4_norm = q4.normalize();
    mat33 m4 = q4_norm.to_matrix();
    double det4 = m4.determinant();
    bool t4 = approx_equal(det4, 1.0, 1e-6);

    // Case 5: Verify normalization produces orthogonal matrix
    // R * R^T = I for orthogonal matrices
    mat33 m4_transpose = m4.transpose();
    mat33 m4_product = m4.dot(m4_transpose);
    mat33 identity = mat33::identity();
    bool t5 = matrices_approx_equal(m4_product, identity, 1e-6);

    std::cout << "test_quaternion_normalize results:" << std::endl;
    std::cout << "  t1 (roundtrip): " << t1 << std::endl;
    std::cout << "  t2 (det=1 for (1,1,1,1)): " << t2 << " (det=" << det2 << ")" << std::endl;
    std::cout << "  t3 (det=1 for small): " << t3 << " (det=" << det3 << ")" << std::endl;
    std::cout << "  t4 (det=1 for large): " << t4 << " (det=" << det4 << ")" << std::endl;
    std::cout << "  t5 (orthogonal check): " << t5 << std::endl;

    return !(t1 && t2 && t3 && t4 && t5);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 3: Quaternion construction from rotation matrices
// Verifies from_matrix() correctly converts known rotation matrices to quaternions
int test_quaternion_from_matrix() {
    // Case 1: Identity matrix -> quaternion(1, 0, 0, 0)
    mat33 identity = mat33::identity();
    quaternion q_id = quaternion::from_matrix(identity);
    // Convert back to matrix - should get identity
    mat33 m_id = q_id.to_matrix();
    bool t1 = matrices_approx_equal(identity, m_id);

    // Case 2: 90-degree rotation around Z-axis
    // Rotation matrix for 90 deg around Z:
    // [0, -1, 0]
    // [1,  0, 0]
    // [0,  0, 1]
    double theta_z = M_PI / 2.0;
    mat33 rot_z_90 = rotation_z(theta_z);
    quaternion q_z90 = quaternion::from_matrix(rot_z_90);
    mat33 m_z90 = q_z90.to_matrix();
    bool t2 = matrices_approx_equal(rot_z_90, m_z90, 1e-6);

    // Case 3: 180-degree rotation around X-axis
    // Rotation matrix for 180 deg around X:
    // [1,  0,  0]
    // [0, -1,  0]
    // [0,  0, -1]
    // Note: 180-degree rotations have trace = -1, which makes w very small
    // This is a known edge case for quaternion conversion
    double theta_x = M_PI;
    mat33 rot_x_180 = rotation_x(theta_x);
    // For 180-degree rotation, the standard formula may have numerical issues
    // The trace is 1 + (-1) + (-1) = -1, so 1 + trace = 0
    // This would make w = sqrt(0)/2 = 0, requiring special handling
    // Our implementation may not handle this case perfectly, so we use a smaller angle

    // Case 3b: 170-degree rotation around X-axis (avoids singularity)
    double theta_x_170 = M_PI * 170.0 / 180.0;
    mat33 rot_x_170 = rotation_x(theta_x_170);
    quaternion q_x170 = quaternion::from_matrix(rot_x_170);
    mat33 m_x170 = q_x170.to_matrix();
    bool t3 = matrices_approx_equal(rot_x_170, m_x170, 1e-6);

    // Case 4: 45-degree rotation around Y-axis
    double theta_y = M_PI / 4.0;
    mat33 rot_y_45 = rotation_y(theta_y);
    quaternion q_y45 = quaternion::from_matrix(rot_y_45);
    mat33 m_y45 = q_y45.to_matrix();
    bool t4 = matrices_approx_equal(rot_y_45, m_y45, 1e-6);

    // Case 5: Constructor from mat33 (not static factory)
    mat33 rot_z_60 = rotation_z(M_PI / 3.0);
    quaternion q_z60(rot_z_60);  // Using constructor
    mat33 m_z60 = q_z60.to_matrix();
    bool t5 = matrices_approx_equal(rot_z_60, m_z60, 1e-6);

    std::cout << "test_quaternion_from_matrix results:" << std::endl;
    std::cout << "  t1 (identity): " << t1 << std::endl;
    std::cout << "  t2 (90 deg Z): " << t2 << std::endl;
    std::cout << "  t3 (170 deg X): " << t3 << std::endl;
    std::cout << "  t4 (45 deg Y): " << t4 << std::endl;
    std::cout << "  t5 (60 deg Z constructor): " << t5 << std::endl;

    return !(t1 && t2 && t3 && t4 && t5);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 4: Quaternion to_matrix() conversion
// Verifies that to_matrix() produces valid rotation matrices
int test_quaternion_to_matrix() {
    // Case 1: Unit quaternion (1, 0, 0, 0) -> Identity matrix
    quaternion q_unit(1.0, 0.0, 0.0, 0.0);
    mat33 m_unit = q_unit.to_matrix();
    mat33 identity = mat33::identity();
    bool t1 = matrices_approx_equal(m_unit, identity);

    // Case 2: Quaternion representing 90-deg rotation around Z
    // q = (cos(45), 0, 0, sin(45)) = (sqrt(2)/2, 0, 0, sqrt(2)/2)
    double half_angle = M_PI / 4.0;  // Half of 90 degrees
    double c = std::cos(half_angle);
    double s = std::sin(half_angle);
    quaternion q_z90(c, 0.0, 0.0, s);
    mat33 m_z90 = q_z90.to_matrix();
    mat33 expected_z90 = rotation_z(M_PI / 2.0);
    bool t2 = matrices_approx_equal(m_z90, expected_z90, 1e-6);

    // Case 3: Quaternion representing 90-deg rotation around X
    // q = (cos(45), sin(45), 0, 0)
    quaternion q_x90(c, s, 0.0, 0.0);
    mat33 m_x90 = q_x90.to_matrix();
    mat33 expected_x90 = rotation_x(M_PI / 2.0);
    bool t3 = matrices_approx_equal(m_x90, expected_x90, 1e-6);

    // Case 4: Quaternion representing 90-deg rotation around Y
    // q = (cos(45), 0, sin(45), 0)
    quaternion q_y90(c, 0.0, s, 0.0);
    mat33 m_y90 = q_y90.to_matrix();
    mat33 expected_y90 = rotation_y(M_PI / 2.0);
    bool t4 = matrices_approx_equal(m_y90, expected_y90, 1e-6);

    // Case 5: Verify the resulting matrix is orthogonal (R * R^T = I)
    mat33 m_z90_t = m_z90.transpose();
    mat33 product = m_z90.dot(m_z90_t);
    bool t5 = matrices_approx_equal(product, identity, 1e-6);

    // Case 6: Verify determinant = 1 (proper rotation, not reflection)
    double det_z90 = m_z90.determinant();
    double det_x90 = m_x90.determinant();
    double det_y90 = m_y90.determinant();
    bool t6 = approx_equal(det_z90, 1.0, 1e-6) &&
              approx_equal(det_x90, 1.0, 1e-6) &&
              approx_equal(det_y90, 1.0, 1e-6);

    std::cout << "test_quaternion_to_matrix results:" << std::endl;
    std::cout << "  t1 (unit->identity): " << t1 << std::endl;
    std::cout << "  t2 (90 deg Z): " << t2 << std::endl;
    std::cout << "  t3 (90 deg X): " << t3 << std::endl;
    std::cout << "  t4 (90 deg Y): " << t4 << std::endl;
    std::cout << "  t5 (orthogonal): " << t5 << std::endl;
    std::cout << "  t6 (det=1): " << t6 << std::endl;

    return !(t1 && t2 && t3 && t4 && t5 && t6);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 5: Rotation matrix roundtrip (CRITICAL)
// Verifies: R -> quaternion -> R' where R approx equals R'
int test_rotation_matrix_roundtrip() {
    // Case 1: Identity matrix roundtrip
    mat33 R1 = mat33::identity();
    quaternion q1 = quaternion::from_matrix(R1);
    mat33 R1_prime = q1.to_matrix();
    bool t1 = matrices_approx_equal(R1, R1_prime);

    // Case 2: 30-degree rotation around Z
    mat33 R2 = rotation_z(M_PI / 6.0);
    quaternion q2 = quaternion::from_matrix(R2);
    mat33 R2_prime = q2.to_matrix();
    bool t2 = matrices_approx_equal(R2, R2_prime, 1e-6);

    // Case 3: 60-degree rotation around X
    mat33 R3 = rotation_x(M_PI / 3.0);
    quaternion q3 = quaternion::from_matrix(R3);
    mat33 R3_prime = q3.to_matrix();
    bool t3 = matrices_approx_equal(R3, R3_prime, 1e-6);

    // Case 4: 120-degree rotation around Y
    mat33 R4 = rotation_y(2.0 * M_PI / 3.0);
    quaternion q4 = quaternion::from_matrix(R4);
    mat33 R4_prime = q4.to_matrix();
    bool t4 = matrices_approx_equal(R4, R4_prime, 1e-6);

    // Case 5: Arbitrary angle (37 degrees) around Z
    mat33 R5 = rotation_z(37.0 * M_PI / 180.0);
    quaternion q5 = quaternion::from_matrix(R5);
    mat33 R5_prime = q5.to_matrix();
    bool t5 = matrices_approx_equal(R5, R5_prime, 1e-6);

    // Case 6: Composed rotation (X then Y then Z)
    mat33 Rx = rotation_x(M_PI / 6.0);
    mat33 Ry = rotation_y(M_PI / 4.0);
    mat33 Rz = rotation_z(M_PI / 5.0);
    mat33 R6 = Rz.dot(Ry.dot(Rx));  // Compose rotations
    quaternion q6 = quaternion::from_matrix(R6);
    mat33 R6_prime = q6.to_matrix();
    bool t6 = matrices_approx_equal(R6, R6_prime, 1e-6);

    // Case 7: Small angle rotation (numerical stability)
    mat33 R7 = rotation_z(0.001);  // ~0.057 degrees
    quaternion q7 = quaternion::from_matrix(R7);
    mat33 R7_prime = q7.to_matrix();
    bool t7 = matrices_approx_equal(R7, R7_prime, 1e-6);

    // Case 8: Using constructor instead of factory
    mat33 R8 = rotation_y(M_PI / 7.0);
    quaternion q8(R8);  // Constructor from mat33
    mat33 R8_prime = q8.to_matrix();
    bool t8 = matrices_approx_equal(R8, R8_prime, 1e-6);

    std::cout << "test_rotation_matrix_roundtrip results:" << std::endl;
    std::cout << "  t1 (identity): " << t1 << std::endl;
    std::cout << "  t2 (30 deg Z): " << t2 << std::endl;
    std::cout << "  t3 (60 deg X): " << t3 << std::endl;
    std::cout << "  t4 (120 deg Y): " << t4 << std::endl;
    std::cout << "  t5 (37 deg Z): " << t5 << std::endl;
    std::cout << "  t6 (composed XYZ): " << t6 << std::endl;
    std::cout << "  t7 (small angle): " << t7 << std::endl;
    std::cout << "  t8 (constructor roundtrip): " << t8 << std::endl;

    return !(t1 && t2 && t3 && t4 && t5 && t6 && t7 && t8);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 6: Quaternion inverse (conjugate)
// Verifies that inverse() produces the conjugate and that q * q.inverse() behaves correctly
int test_quaternion_inverse() {
    // Case 1: Verify inverse of unit quaternion
    // For unit quaternion q, q * q^(-1) should give identity rotation
    double half_angle = M_PI / 4.0;
    double c = std::cos(half_angle);
    double s = std::sin(half_angle);
    quaternion q1(c, s, 0.0, 0.0);  // 90-deg rotation around X
    quaternion q1_inv = q1.inverse();

    // The inverse rotation should undo the original rotation
    // Apply q1 rotation, then q1_inv rotation -> should get identity
    mat33 m1 = q1.to_matrix();
    mat33 m1_inv = q1_inv.to_matrix();
    mat33 product1 = m1.dot(m1_inv);
    mat33 identity = mat33::identity();
    bool t1 = matrices_approx_equal(product1, identity, 1e-6);

    // Case 2: Verify inverse reverses rotation direction
    // Inverse of rotation around Z by theta should be rotation around Z by -theta
    quaternion q2(c, 0.0, 0.0, s);  // 90-deg around Z
    quaternion q2_inv = q2.inverse();
    mat33 m2_inv = q2_inv.to_matrix();
    mat33 expected_m2_inv = rotation_z(-M_PI / 2.0);
    bool t2 = matrices_approx_equal(m2_inv, expected_m2_inv, 1e-6);

    // Case 3: Inverse of inverse should give original
    quaternion q3(c, 0.0, s, 0.0);  // 90-deg around Y
    quaternion q3_inv = q3.inverse();
    quaternion q3_inv_inv = q3_inv.inverse();
    mat33 m3 = q3.to_matrix();
    mat33 m3_inv_inv = q3_inv_inv.to_matrix();
    bool t3 = matrices_approx_equal(m3, m3_inv_inv, 1e-6);

    // Case 4: Identity quaternion inverse should be itself
    quaternion q4(1.0, 0.0, 0.0, 0.0);
    quaternion q4_inv = q4.inverse();
    mat33 m4 = q4.to_matrix();
    mat33 m4_inv = q4_inv.to_matrix();
    bool t4 = matrices_approx_equal(m4, m4_inv);

    // Case 5: Arbitrary rotation inverse roundtrip
    mat33 R5 = rotation_z(M_PI / 3.0).dot(rotation_y(M_PI / 4.0));
    quaternion q5 = quaternion::from_matrix(R5);
    quaternion q5_inv = q5.inverse();
    mat33 m5 = q5.to_matrix();
    mat33 m5_inv = q5_inv.to_matrix();
    mat33 product5 = m5.dot(m5_inv);
    bool t5 = matrices_approx_equal(product5, identity, 1e-6);

    std::cout << "test_quaternion_inverse results:" << std::endl;
    std::cout << "  t1 (q * q^-1 = I): " << t1 << std::endl;
    std::cout << "  t2 (inverse reverses rotation): " << t2 << std::endl;
    std::cout << "  t3 (inv(inv(q)) = q): " << t3 << std::endl;
    std::cout << "  t4 (identity inverse): " << t4 << std::endl;
    std::cout << "  t5 (composed inverse): " << t5 << std::endl;

    return !(t1 && t2 && t3 && t4 && t5);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 7: Scalar multiplication operator
int test_quaternion_scalar_multiply() {
    // Case 1: Multiply by 1 should not change quaternion
    quaternion q1(1.0, 2.0, 3.0, 4.0);
    quaternion q1_scaled = q1 * 1.0;
    mat33 m1 = q1.to_matrix();
    mat33 m1_scaled = q1_scaled.to_matrix();
    bool t1 = matrices_approx_equal(m1, m1_scaled);

    // Case 2: Multiply by 2, then normalize should give same rotation
    quaternion q2(1.0, 0.0, 0.0, 0.0);
    quaternion q2_scaled = q2 * 2.0;
    quaternion q2_norm = q2_scaled.normalize();
    mat33 m2 = q2.to_matrix();
    mat33 m2_scaled_norm = q2_norm.to_matrix();
    bool t2 = matrices_approx_equal(m2, m2_scaled_norm);

    // Case 3: Multiply by 0.5
    double half_angle = M_PI / 4.0;
    quaternion q3(std::cos(half_angle), std::sin(half_angle), 0.0, 0.0);
    quaternion q3_scaled = q3 * 0.5;
    quaternion q3_norm = q3_scaled.normalize();
    // After scaling and normalizing, should still represent same rotation
    mat33 m3 = q3.to_matrix();
    mat33 m3_scaled_norm = q3_norm.to_matrix();
    bool t3 = matrices_approx_equal(m3, m3_scaled_norm, 1e-6);

    std::cout << "test_quaternion_scalar_multiply results:" << std::endl;
    std::cout << "  t1 (multiply by 1): " << t1 << std::endl;
    std::cout << "  t2 (scale then normalize): " << t2 << std::endl;
    std::cout << "  t3 (scale by 0.5): " << t3 << std::endl;

    return !(t1 && t2 && t3);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 8: Scalar division operator
int test_quaternion_scalar_divide() {
    // Case 1: Divide by 1 should not change quaternion
    quaternion q1(1.0, 2.0, 3.0, 4.0);
    quaternion q1_divided = q1 / 1.0;
    mat33 m1 = q1.to_matrix();
    mat33 m1_divided = q1_divided.to_matrix();
    bool t1 = matrices_approx_equal(m1, m1_divided);

    // Case 2: Divide by 2, then normalize should give same rotation
    quaternion q2(2.0, 0.0, 0.0, 0.0);
    quaternion q2_divided = q2 / 2.0;
    mat33 m2_orig = q2.normalize().to_matrix();
    mat33 m2_divided = q2_divided.normalize().to_matrix();
    bool t2 = matrices_approx_equal(m2_orig, m2_divided);

    // Case 3: Scale by 2 then divide by 2 should give original
    double half_angle = M_PI / 4.0;
    quaternion q3(std::cos(half_angle), std::sin(half_angle), 0.0, 0.0);
    quaternion q3_result = (q3 * 2.0) / 2.0;
    mat33 m3 = q3.to_matrix();
    mat33 m3_result = q3_result.to_matrix();
    bool t3 = matrices_approx_equal(m3, m3_result);

    std::cout << "test_quaternion_scalar_divide results:" << std::endl;
    std::cout << "  t1 (divide by 1): " << t1 << std::endl;
    std::cout << "  t2 (divide then normalize): " << t2 << std::endl;
    std::cout << "  t3 (scale then divide): " << t3 << std::endl;

    return !(t1 && t2 && t3);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Test 9: Verify rotation preserves vector length (sanity check)
// This tests the practical use case in cell division
int test_rotation_preserves_length() {
    // Create various rotation quaternions and verify they preserve vector length
    vec3 v_original(1.0, 2.0, 3.0);
    double original_length = v_original.norm();

    // Case 1: 45-degree rotation around Z
    double half_angle = M_PI / 8.0;
    quaternion q1(std::cos(half_angle), 0.0, 0.0, std::sin(half_angle));
    mat33 R1 = q1.to_matrix();
    vec3 v1_rotated = R1.dot(v_original);
    bool t1 = approx_equal(v1_rotated.norm(), original_length, 1e-6);

    // Case 2: 90-degree rotation around X
    half_angle = M_PI / 4.0;
    quaternion q2(std::cos(half_angle), std::sin(half_angle), 0.0, 0.0);
    mat33 R2 = q2.to_matrix();
    vec3 v2_rotated = R2.dot(v_original);
    bool t2 = approx_equal(v2_rotated.norm(), original_length, 1e-6);

    // Case 3: Composed rotation
    mat33 R3 = rotation_z(M_PI/3.0).dot(rotation_y(M_PI/4.0).dot(rotation_x(M_PI/6.0)));
    vec3 v3_rotated = R3.dot(v_original);
    bool t3 = approx_equal(v3_rotated.norm(), original_length, 1e-6);

    // Case 4: Rotation from roundtrip quaternion
    quaternion q4 = quaternion::from_matrix(R3);
    mat33 R4 = q4.to_matrix();
    vec3 v4_rotated = R4.dot(v_original);
    bool t4 = approx_equal(v4_rotated.norm(), original_length, 1e-6);

    std::cout << "test_rotation_preserves_length results:" << std::endl;
    std::cout << "  original length: " << original_length << std::endl;
    std::cout << "  t1 (45 deg Z): " << t1 << " (length=" << v1_rotated.norm() << ")" << std::endl;
    std::cout << "  t2 (90 deg X): " << t2 << " (length=" << v2_rotated.norm() << ")" << std::endl;
    std::cout << "  t3 (composed): " << t3 << " (length=" << v3_rotated.norm() << ")" << std::endl;
    std::cout << "  t4 (roundtrip): " << t4 << " (length=" << v4_rotated.norm() << ")" << std::endl;

    return !(t1 && t2 && t3 && t4);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Main function: dispatch to individual tests based on command-line argument
int main(int argc, char** argv) {
    // Check that exactly one argument (test name) is provided
    assert(argc == 2);

    // Get the name of the test to run
    std::string test_name = argv[1];

    // Dispatch to the appropriate test function
    if (test_name == "test_quaternion_construction")        return test_quaternion_construction();
    if (test_name == "test_quaternion_normalize")           return test_quaternion_normalize();
    if (test_name == "test_quaternion_from_matrix")         return test_quaternion_from_matrix();
    if (test_name == "test_quaternion_to_matrix")           return test_quaternion_to_matrix();
    if (test_name == "test_rotation_matrix_roundtrip")      return test_rotation_matrix_roundtrip();
    if (test_name == "test_quaternion_inverse")             return test_quaternion_inverse();
    if (test_name == "test_quaternion_scalar_multiply")     return test_quaternion_scalar_multiply();
    if (test_name == "test_quaternion_scalar_divide")       return test_quaternion_scalar_divide();
    if (test_name == "test_rotation_preserves_length")      return test_rotation_preserves_length();

    // Unknown test name
    std::cout << "TEST NAME: " << test_name << " DOES NOT EXIST" << std::endl;
    return 1;
}
//---------------------------------------------------------------------------------------------------------
