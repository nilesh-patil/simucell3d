#ifndef DEF_VEC3
#define DEF_VEC3


#include <iostream>
#include <limits>
#include <iomanip>
#include <cassert>
#include <cmath>


#include "utils.hpp"



/**
 * @file
 * @brief Three-component vector of doubles.
 * @ingroup math
 */

/**
 * @brief Three-component vector of doubles, used for positions (m), forces (N), momenta and
 *        normals.
 *
 * Components are 0 by default. The constructor and translate() check in debug builds that
 * the components are finite.
 *
 * @ingroup math
 */
class vec3 
{

    private: 

        //Initialize all vectors to (0., 0., 0.)
        double dx_ = 0., dy_ = 0., dz_ = 0.;

    public:

        //Vector is (0., 0., 0.)
        vec3() = default;                        //Default constructor
        vec3(const vec3& v) = default;           //copy constructor
        vec3(vec3&& v) = default;                //move constructor
        vec3& operator=(const vec3& v) = default;//copy assignment operator
        vec3& operator=(vec3&& v) = default;     //move assignment operator 


        /** @brief Construct the vector (dx, dy, dz). */
        vec3(const double dx,  const double dy, const double dz);

        inline double dx() const {return dx_;};  ///< x component.
        inline double dy() const {return dy_;};  ///< y component.
        inline double dz() const {return dz_;};  ///< z component.


        /** @brief Set all components to 0. */
        void reset() {dx_ = 0., dy_ = 0., dz_ = 0.;};
        /** @brief Copy the components of @p v. */
        void reset(const vec3& v) {dx_ = v.dx(), dy_ = v.dy(), dz_ = v.dz();};
        void reset(vec3&& v) {dx_ = v.dx(), dy_ = v.dy(), dz_ = v.dz();};
        /** @brief Set the components to (dx, dy, dz). */
        void reset(const double dx, const double dy, const double dz) {dx_ = dx, dy_ = dy, dz_ = dz;}

        /**
         * @brief Add @p v to this vector in place.
         * @note Not safe for concurrent updates of the same vector: the math_modules library
         *       is compiled without OpenMP, so the atomic pragmas in the implementation have no
         *       effect. Use atomic_translate() for shared accumulators.
         */
        void translate(const vec3& v);
        /** @brief Add (dx, dy, dz) to this vector in place. Same thread-safety as translate(const vec3&). */
        void translate(const double dx, const double dy, const double dz);

        /**
         * @brief Add @p v with one atomic read-modify-write per component.
         *
         * For accumulators that several threads update concurrently, such as the contact
         * forces on a node. The vector as a whole is not updated atomically, which is
         * sufficient for a sum. Uses the GCC/Clang __atomic builtins (a compare-exchange loop)
         * rather than "#pragma omp atomic", whose effect depends on the translation unit being
         * compiled with -fopenmp; math_modules is built without OpenMP.
         *
         * @note Thread-safe with respect to other atomic_translate() calls on the same vector.
         */
        void atomic_translate(const vec3& v) noexcept {
            atomic_add(dx_, v.dx_);
            atomic_add(dy_, v.dy_);
            atomic_add(dz_, v.dz_);
        }

    private:
        static void atomic_add(double& target, const double increment) noexcept {
            double expected;
            __atomic_load(&target, &expected, __ATOMIC_RELAXED);
            double desired;
            do{ desired = expected + increment; }
            while(!__atomic_compare_exchange(&target, &expected, &desired, true, __ATOMIC_RELAXED, __ATOMIC_RELAXED));
        }

    public:


        /** @brief Print the components to standard output in scientific notation. */
        void print() const; 

        /** @brief The components as {dx, dy, dz}. */
        std::array<double, 3> to_array() const {return {dx_, dy_, dz_};}

        /** @brief Squared Euclidean norm. */
        inline double squared_norm() const {return dx_*dx_ + dy_*dy_ + dz_*dz_;};

        /** @brief Euclidean norm. */
        inline double norm() const {return std::sqrt(dx_*dx_ + dy_*dy_ + dz_*dz_);};

        /**
         * @brief Scale the vector down to @p max_norm if its norm exceeds it.
         * @param max_norm Maximum norm, in the unit of the vector.
         * @return true if the vector was scaled.
         */
        bool cap(const double max_norm);

        /** @brief Dot product with @p v. */
        double dot(const vec3& v) const;

        /** @brief Cross product (this x v). */
        vec3 cross(const vec3& v) const;

        /** @brief Unit vector with the same direction, or the zero vector if the norm is 0. */
        vec3 normalize() const;

        /**
         * @brief Angle between this vector and @p v, in radians, in [0, pi].
         * @return The angle; 0 if either vector has a norm below 1e-14.
         */
        double get_angle_with(vec3&& v) const; 
        /** @copydoc get_angle_with(vec3&&) const */
        double get_angle_with(const vec3& v) const; 

        /**
         * @brief Rotate this vector around an axis with the Rodrigues formula.
         * @param axis Rotation axis; must be a unit vector.
         * @param angle Rotation angle, in radians.
         * @return The rotated vector.
         */
        vec3 rotate_around_axis(const vec3& axis, const double angle) const;

        //Overload substraction operator
        vec3 operator-(vec3&& v) const;         //rvalue arg
        vec3 operator-(const vec3& v) const;    //lvalue arg

        //Overload addition operator
        vec3 operator+(vec3&& v) const;         //rvalue arg
        vec3 operator+(const vec3& v) const;    //lvalue arg

        //Overload multiplicatuion operator
        vec3 operator*(const double scalar) const; 

        //Overload division operator
        vec3 operator/(const double scalar) const;

        /** @brief Component-wise comparison with almost_equal() (2 units in the last place). */
        bool operator==(const vec3& v) const; 
        bool operator!=(const vec3& v) const; 



};

#endif