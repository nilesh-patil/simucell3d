#ifndef DEF_MAT33
#define DEF_MAT33


#include <iostream>
#include <limits>
#include <iomanip>
#include <cassert>
#include <cmath>
#include <array>

#include "utils.hpp"
#include "vec3.hpp"
#include "eigen_solver.hpp"



/**
 * @file
 * @brief 3 x 3 matrix of doubles.
 * @ingroup math
 */

/**
 * @brief 3 x 3 matrix of doubles stored by rows; the zero matrix by default.
 * @ingroup math
 */
class mat33 
{

    private: 

        //Initialize the 3 rows of the matrix
        std::array<double, 3> row_1_ = {0., 0., 0.};
        std::array<double, 3> row_2_ = {0., 0., 0.};
        std::array<double, 3> row_3_ = {0., 0., 0.};

    public:

        //Matrix is a zero matrix with the default consyucor 
        mat33() = default;                        //Default constructor
        mat33(const mat33& m) = default;           //copy constructor
        mat33(mat33&& m) = default;                //move constructor
        mat33& operator=(const mat33& m) = default;//copy assignment operator
        mat33& operator=(mat33&& m) = default;     //move assignment operator 


        /** @brief Construct the matrix from its three rows. */
        mat33(const std::array<double, 3>& row_1,
              const std::array<double, 3>& row_2,
              const std::array<double, 3>& row_3
            )noexcept;


        mat33(
            std::array<double, 3>&& row_1,
            std::array<double, 3>&& row_2,
            std::array<double, 3>&& row_3
        )noexcept;


        /** @brief The identity matrix. */
        static mat33 identity() noexcept{
            return mat33(
                {1., 0., 0.},
                {0., 1., 0.},
                {0., 0., 1.}
            );
        }

        /** @brief Print the matrix to standard output, one row per line. */
        void print() const noexcept; 


        /** @brief Row @p i (0, 1 or 2) as a vector. */
        vec3 get_row(const int i) const noexcept;
        /** @brief Column @p i (0, 1 or 2) as a vector. */
        vec3 get_col(const int i) const noexcept;

        /** @brief Matrix product (this * m). */
        mat33 dot(const mat33& m) const noexcept;
        /** @brief Matrix product (this * m). */
        mat33 dot(      mat33&& m) const noexcept;

        /** @brief Matrix-vector product (this * v). */
        vec3 dot(const vec3& v) const noexcept;
        /** @brief Matrix-vector product (this * v). */
        vec3 dot(      vec3&& v) const noexcept;
       

        /** @brief Transposed matrix. */
        mat33 transpose() const noexcept;
        /** @brief Determinant. */
        double determinant() const noexcept;

        /**
         * @brief Eigenvalues and eigenvectors of a symmetric matrix.
         *
         * Uses gte::SymmetricEigensolver3x3. The matrix must be symmetric (checked by
         * assertions in debug builds).
         *
         * @return The eigenvalues in increasing order, and a matrix whose column i is the unit
         *         eigenvector of eigenvalue i.
         */
        std::pair<vec3, mat33> eigen_decomposition() const noexcept;


        /**
         * @brief Inverse matrix.
         * @throws std::domain_error if |determinant| < 1e-12.
         */
        mat33 inverse() const;


        /** @brief Element-wise difference. */
        mat33 operator-(mat33&& m) const noexcept;         //rvalue arg
        mat33 operator-(const mat33& m) const noexcept;    //lvalue arg

        /** @brief Element-wise sum. */
        mat33 operator+(mat33&& m) const noexcept;         //rvalue arg
        mat33 operator+(const mat33& m) const noexcept;    //lvalue arg

        /** @brief Product with a scalar. */
        mat33 operator*(const double scalar) const noexcept;         //rvalue arg
        /** @brief Copy of row @p i (0, 1 or 2). */
        std::array<double, 3> operator[](const int i) const noexcept;    //lvalue arg


};

#endif