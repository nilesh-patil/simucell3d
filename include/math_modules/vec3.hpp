#ifndef DEF_VEC3
#define DEF_VEC3


#include <iostream>
#include <limits>
#include <iomanip>
#include <cassert>
#include <cmath>


#include "utils.hpp"



/*
    Class for a 3D vector object
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


        //Vector is instantiated to an arbitrary value 
        vec3(const double dx,  const double dy, const double dz);

        //Return the coordinates of the vector
        inline double dx() const {return dx_;};
        inline double dy() const {return dy_;};
        inline double dz() const {return dz_;};


        //Reset the values of the vector to 0.
        void reset() {dx_ = 0., dy_ = 0., dz_ = 0.;};
        void reset(const vec3& v) {dx_ = v.dx(), dy_ = v.dy(), dz_ = v.dz();};
        void reset(vec3&& v) {dx_ = v.dx(), dy_ = v.dy(), dz_ = v.dz();};
        void reset(const double dx, const double dy, const double dz) {dx_ = dx, dy_ = dy, dz_ = dz;}

        //Translate the values by a certain amount
        void translate(const vec3& v);
        void translate(const double dx, const double dy, const double dz);

        //Translate by v with one atomic read-modify-write per component, for an accumulator that several threads
        //update concurrently (e.g. the contact forces on a node). The vector as a whole is not updated atomically,
        //which is all a sum needs. Uses the GCC/Clang __atomic builtins (a lock cmpxchg loop on x86-64) rather than
        //"#pragma omp atomic", whose effect depends on the translation unit being compiled with -fopenmp: the
        //pragmas in translate() are ignored because math_modules is built without OpenMP.
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


        //Print the content of the vector
        void print() const; 

        //Get the coordinates of the vector into an std:array
        std::array<double, 3> to_array() const {return {dx_, dy_, dz_};}

        //Compute the squared norm of the vector
        inline double squared_norm() const {return dx_*dx_ + dy_*dy_ + dz_*dz_;};

        //Compute the norm of the vector
        inline double norm() const {return std::sqrt(dx_*dx_ + dy_*dy_ + dz_*dz_);};

        //Cap the vector to a maximum norm
        bool cap(const double max_norm);

        //Return the dot product
        double dot(const vec3& v) const;

        //Compute the dot product of 2 vectors
        vec3 cross(const vec3& v) const;

        //Return a new vector with the same direction but unit norm
        vec3 normalize() const;

        //Get the angle between 2 vectors, the angle is in the range [0, pi]
        double get_angle_with(vec3&& v) const; 
        double get_angle_with(const vec3& v) const; 

        //Rotate this vector around the given axis by the given angle
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

        //Overload the comparison operator
        bool operator==(const vec3& v) const; 
        bool operator!=(const vec3& v) const; 



};

#endif