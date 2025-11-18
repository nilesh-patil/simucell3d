#include <cassert>
#include <string>
#include "utils.hpp"

#include "mat33.hpp"




/*
Contains all the tests run on the vec3 class which is contained in the vec3.hpp file
*/


//---------------------------------------------------------------------------------------------------------
//Test the trivial constructor
int test_constructors(){
    mat33 m1(
        {1., 2., 3.},
        {4., 5., 6.},
        {7., 8., 9.}
    );


    bool t1 = m1[0][0] == 1 && m1[0][1] == 2 && m1[0][2] == 3;
    bool t2 = m1[1][0] == 4 && m1[1][1] == 5 && m1[1][2] == 6;
    bool t3 = m1[2][0] == 7 && m1[2][1] == 8 && m1[2][2] == 9;

    return !(t1 && t2 && t3);
}
//---------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------
int get_row_col_test(){

 
    //Test the default constructor
    mat33 m1(
        {1., 2., 3.},
        {4., 5., 6.},
        {7., 8., 9.}
    );


    bool t1 = m1.get_row(0) == vec3(1., 2., 3.);
    bool t2 = m1.get_row(1) == vec3(4., 5., 6.);
    bool t3 = m1.get_row(2) == vec3(7., 8., 9.);

    bool t4 = m1.get_col(0) == vec3(1., 4., 7.);
    bool t5 = m1.get_col(1) == vec3(2., 5., 8.);
    bool t6 = m1.get_col(2) == vec3(3., 6., 9.);

    return !(t1 && t2 && t3 && t4 && t5 && t6);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
int matrix_dot_test(){

    //Test the default constructor
    mat33 m1(
        {1., 2., 3.},
        {4., 5., 6.},
        {7., 8., 9.}
    );

    mat33 m2(
        {1., 2., 3.},
        {4., 5., 6.},
        {7., 8., 9.}
    );

    mat33 m3 = m1.dot(m2);

    bool t1 = m3.get_col(0) == m1.dot(m2.get_col(0));
    bool t2 = m3.get_col(1) == m1.dot(m2.get_col(1));
    bool t3 = m3.get_col(2) == m1.dot(m2.get_col(2));

    std::cout << t1 << std::endl;
    std::cout << t2 << std::endl;
    std::cout << t3 << std::endl;


    return !(t1 && t2 && t3);
}
//---------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------
int vector_dot_test(){
    
    mat33 m1(        
        {1., 2., 3.},
        {4., 5., 6.},
        {7., 8., 9.}
    );

    vec3 v1(1., 2., 3.);
    vec3 v2 = m1.dot(v1);


    bool t1 = v2 == m1.get_col(0) * v1.dx() + m1.get_col(1) * v1.dy() + m1.get_col(2) * v1.dz();
    return !t1;
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
int transpose_test(){
    mat33 m1(        
        {1., 2., 3.},
        {4., 5., 6.},
        {7., 8., 9.}
    );

    mat33 m2 = m1.transpose();


    bool t1 = m2.get_col(0) == m1.get_row(0);
    bool t2 = m2.get_col(1) == m1.get_row(1);
    bool t3 = m2.get_col(2) == m1.get_row(2);

    return !(t1 && t2 && t3);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
int determinant_test(){

    mat33 m1(        
        {6., 1. , 1.},
        {4., -2., 5.},
        {2., 8. , 7.}
    );

    double det = m1.determinant();
    return !(det == -306.);
}
//---------------------------------------------------------------------------------------------------------




//---------------------------------------------------------------------------------------------------------
int inverse_test(){

    mat33 m1(        
        {6., 1. , 1.},
        {4., -2., 5.},
        {2., 8. , 7.}
    );

    mat33 m2 = m1.inverse();

    m2.print();



    bool t1 = m2.get_row(0) == vec3( 3./17.,	 -1./306.,	-7./306.);
    bool t2 = m2.get_row(1) == vec3( 1./17.,	-20./153.,    13./153.);
    bool t3 = m2.get_row(2) == vec3(-2./17.,	 23./153.,	 8./153.);

    std::cout << t1 << std::endl;
    std::cout << t2 << std::endl;
    std::cout << t3 << std::endl;

    return !(t1 && t2 && t3);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------

int operator_minus_test(){
    mat33 m1(
        {1., 2., 3.},
        {4., 5., 6.},
        {7., 8., 9.}
    );
    mat33 m2 = m1;
    mat33 m3 = m1 - m2;


    bool t1 = m3.get_row(0) == vec3(0., 0., 0.);
    bool t2 = m3.get_row(1) == vec3(0., 0., 0.);
    bool t3 = m3.get_row(2) == vec3(0., 0., 0.);

    return !(t1 && t2 && t3);
}

//---------------------------------------------------------------------------------------------------------




//---------------------------------------------------------------------------------------------------------

int operator_plus_test(){
    mat33 m1(
        {1., 2., 3.},
        {4., 5., 6.},
        {7., 8., 9.}
    );
    mat33 m2 = m1;
    mat33 m3 = m1 + m2;


    bool t1 = m3.get_row(0) == vec3(2., 4., 6.);
    bool t2 = m3.get_row(1) == vec3(8., 10., 12.);
    bool t3 = m3.get_row(2) == vec3(14., 16., 18.);

    return !(t1 && t2 && t3);
}

//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
int operator_multiply_test(){
    mat33 m1(
        {1., 2., 3.},
        {4., 5., 6.},
        {7., 8., 9.}
    );
    mat33 m2 = m1 * 2.;


    bool t1 = m2.get_row(0) == vec3(2., 4., 6.);
    bool t2 = m2.get_row(1) == vec3(8., 10., 12.);
    bool t3 = m2.get_row(2) == vec3(14., 16., 18.);

    return !(t1 && t2 && t3);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
int identity_test(){
    mat33 m1 = mat33::identity();

    bool t1 = m1.get_row(0) == vec3(1., 0., 0.);
    bool t2 = m1.get_row(1) == vec3(0., 1., 0.);
    bool t3 = m1.get_row(2) == vec3(0., 0., 1.);
    return !(t1 && t2 && t3);
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Bug #2: Test that inverse() throws for singular matrices (det = 0)
int inverse_singular_matrix_test(){
    // Create a singular matrix (rows are linearly dependent, det = 0)
    mat33 singular(
        {1., 2., 3.},
        {2., 4., 6.},  // Row 2 = 2 * Row 1
        {1., 1., 1.}
    );

    // Verify determinant is zero
    double det = singular.determinant();
    if (std::abs(det) > 1e-10) {
        std::cerr << "Test setup error: matrix is not singular, det = " << det << std::endl;
        return 1;
    }

    // Try to invert - should throw std::domain_error
    try {
        mat33 inv = singular.inverse();
        // If we get here, the function didn't throw - this is the bug!
        std::cerr << "FAIL: inverse() should throw for singular matrix but didn't" << std::endl;
        return 1;
    } catch (const std::domain_error& e) {
        // Expected behavior - test passes
        std::cout << "PASS: inverse() correctly threw std::domain_error: " << e.what() << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: Wrong exception type: " << e.what() << std::endl;
        return 1;
    }
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Bug #2: Test that inverse() throws for near-singular matrices
int inverse_near_singular_matrix_test(){
    // Create a near-singular matrix (determinant very close to zero)
    mat33 near_singular(
        {1., 2., 3.},
        {2., 4.0000000001, 6.},  // Almost linearly dependent
        {1., 1., 1.}
    );

    double det = near_singular.determinant();
    std::cout << "Near-singular matrix determinant: " << det << std::endl;

    // The determinant should be very small
    if (std::abs(det) > 1e-8) {
        std::cerr << "Test setup warning: matrix may not be near-singular enough" << std::endl;
    }

    // Try to invert - should throw for numerical stability
    try {
        mat33 inv = near_singular.inverse();
        // Check if result is reasonable (not inf/nan)
        vec3 row0 = inv.get_row(0);
        if (!std::isfinite(row0.dx()) || !std::isfinite(row0.dy()) || !std::isfinite(row0.dz())) {
            std::cerr << "FAIL: inverse() produced inf/nan values" << std::endl;
            return 1;
        }
        // Even if finite, values should not be astronomically large
        if (std::abs(row0.dx()) > 1e12) {
            std::cerr << "FAIL: inverse() produced numerically unstable values" << std::endl;
            return 1;
        }
        std::cout << "PASS: inverse() handled near-singular matrix safely" << std::endl;
        return 0;
    } catch (const std::domain_error& e) {
        // Also acceptable - throwing for numerical safety
        std::cout << "PASS: inverse() correctly threw for near-singular: " << e.what() << std::endl;
        return 0;
    }
}
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// The main function
int main (int argc, char** argv){

    //Check that the command line input is correctly formatted
    assert(argc == 2); 

    //Get the name of the test to run
    std::string test_name = argv[1];
    
    //Run the selected test
    if (test_name == "test_constructors")               return test_constructors();
    if (test_name == "get_row_col_test")                return get_row_col_test();
    if (test_name == "matrix_dot_test")                 return matrix_dot_test();
    if (test_name == "vector_dot_test")                 return vector_dot_test();
    if (test_name == "transpose_test")                  return transpose_test();
    if (test_name == "determinant_test")                return determinant_test();
    if (test_name == "inverse_test")                    return inverse_test();
    if (test_name == "operator_minus_test")             return operator_minus_test();
    if (test_name == "operator_plus_test")              return operator_plus_test();
    if (test_name == "operator_multiply_test")          return operator_multiply_test();
    if (test_name == "identity_test")                   return identity_test();
    if (test_name == "inverse_singular_matrix_test")    return inverse_singular_matrix_test();
    if (test_name == "inverse_near_singular_matrix_test") return inverse_near_singular_matrix_test();







    

    


    




    
    std::cout << "TEST NAME :" << test_name << " DOES NOT EXIST" << std::endl;
    return 1;
}
//---------------------------------------------------------------------------------------------------------



