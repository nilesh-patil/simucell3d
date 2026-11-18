#ifndef DEF_UTILS
#define DEF_UTILS


#include <type_traits>
#include <functional>
#include <limits>
#include <algorithm>
#include <exception>
#include <string>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <cassert>
#include <array>
#include <omp.h>

/**
 * @file
 * @brief Small helper templates: floating-point comparison, container utilities, number
 *        formatting and a parallel loop that propagates exceptions.
 * @ingroup runtime
 */





/**
 * @brief Compare two floating-point numbers with a tolerance in units in the last place.
 *
 * The machine epsilon is scaled by the magnitude |x + y| and by @p ulp. Values whose
 * difference is subnormal are also considered equal.
 *
 * @param x First value.
 * @param y Second value.
 * @param ulp Tolerance in units in the last place.
 * @return true if x and y are equal within the tolerance.
 * @ingroup runtime
 */
//---------------------------------------------------------------------------------------
template<class T>
typename std::enable_if<!std::numeric_limits<T>::is_integer, bool>::type
inline almost_equal(const T x, const T y, const int ulp =  2)
{
    // the machine epsilon has to be scaled to the magnitude of the values used
    // and multiplied by the desired precision in ULPs (units in the last place)
    return std::fabs(x-y) <= std::numeric_limits<T>::epsilon() * std::fabs(x+y) * ulp
        // unless the result is subnormal
        || std::fabs(x-y) < std::numeric_limits<T>::min();
}
//---------------------------------------------------------------------------------------




//---------------------------------------------------------------------------------------
/**
 * @brief Print the elements of a container to standard output, separated by spaces.
 * @param cont Container whose elements support operator<<.
 * @ingroup runtime
 */
template<typename T>
inline void print_container(T const& cont) 
{
    for (typename T::const_iterator it = cont.begin(); it != cont.end(); ++it) {
        std::cout << *it << " "; 
    }

    std::cout << std::endl; 
};
//---------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------
/**
 * @brief Running sums of func(element) over a vector.
 * @param lst Input vector; must not be empty.
 * @param func Maps an element to a summable scalar of type T2.
 * @return Vector whose element i is func(lst[0]) + ... + func(lst[i]).
 * @ingroup runtime
 */
template<typename T1, typename T2>
inline  std::vector<T2> partial_sum_vector(const std::vector<T1>& lst, const std::function<T2(const T1&)>& func){

    //Make sure there are things to add
    assert(lst.size() != 0);

    //Store the sequence of partial sums in this vector
    std::vector<T2> partial_sum_lst{func(lst.front())}; 
    partial_sum_lst.reserve(lst.size());

    //Compute the partial sums
    for (size_t i = 0, j = 1; j < lst.size();  i = j++){
        partial_sum_lst.push_back(partial_sum_lst[i] + func(lst[j]));
    }

    return partial_sum_lst;
};
//---------------------------------------------------------------------------------------




//---------------------------------------------------------------------------------------
/**
 * @brief Remove the elements at the given indices from a vector, keeping the order of the rest.
 * @param vector Vector to modify.
 * @param to_remove Indices to remove; must be unique and sorted in ascending order.
 * @ingroup runtime
 */
template<typename T, typename U>
void remove_index(std::vector<T>& vector, std::vector<U>& to_remove){
    auto vector_base = vector.begin();

    typename std::vector<T>::size_type down_by = 0;
    for (auto iter = to_remove.cbegin(); 
                iter < to_remove.cend(); 
                iter++, down_by++){
        typename std::vector<T>::size_type next = iter + 1 == to_remove.cend() ? vector.size() : *(iter + 1);

        std::move(vector_base + *iter + 1, vector_base + next, vector_base + *iter - down_by);
    }
    vector.resize(vector.size() - to_remove.size());
}
//---------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------
/**
 * @brief Concatenate the inner vectors of a 2D vector.
 * @param orig Vector of vectors.
 * @return The elements of all inner vectors, in order.
 * @ingroup runtime
 */
template<typename T>
std::vector<T> flatten(const std::vector<std::vector<T>> &orig)
{   
    std::vector<T> ret;
    for(const auto &v: orig)
        ret.insert(ret.end(), v.begin(), v.end());                                                                                         
    return ret;
}  
//---------------------------------------------------------------------------------------



//---------------------------------------------------------------------------------------
/**
 * @brief Format a number with a printf-style format string.
 * @param number Value to format.
 * @param fmt printf format, for example "%.3e".
 * @return The formatted text (at most 29 characters).
 * @ingroup runtime
 */
template<typename T>
inline std::string format_number(T number, std::string fmt){
    char buffer_char[30];
    sprintf(buffer_char, fmt.c_str(), number);
    return std::string(buffer_char);
}
//---------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------
/**
 * @brief Convert a string to lower case in place.
 * @param str String to convert; it is modified.
 * @return A copy of the converted string.
 * @ingroup runtime
 */
inline std::string lower_string(std::string& str){
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c){ return std::tolower(c); });
    return str;
}
//---------------------------------------------------------------------------------------




//---------------------------------------------------------------------------------------
/**
 * @brief Apply a function to every element of a vector in an OpenMP parallel loop and
 *        propagate exceptions.
 *
 * An exception thrown inside the loop is stored, the other iterations run to completion,
 * and the stored exception is then rethrown on the calling thread. This prevents an
 * exception from terminating the program inside the parallel region. If several iterations
 * throw, one of the exceptions is rethrown. The loop uses schedule(runtime).
 *
 * @param vec Elements to iterate over.
 * @param func Function applied to each element; it must not change the size of @p vec.
 * @throws Any exception thrown by @p func.
 * @ingroup runtime
 */
template<typename T>
inline void parallel_exception_handler(
    const std::vector<T>& vec,             //Contains the vector of objects to iterate over
    const std::function<void(T)>& func     //Note that the function should not modify the size of the vector
) noexcept(false){

    //Create an exception pointer
    std::exception_ptr e_ptr;

    // Use runtime scheduling to benefit from adaptive chunk sizing
    #pragma omp parallel for schedule(runtime)
    for(size_t i = 0; i < vec.size(); i++){

        try{
            //This might throw an exception
            func(vec[i]);
        }

        //Capture the exception thrown by func
        catch(...){

            #pragma omp critical
            {
                //Store the exception
                e_ptr = std::current_exception();
            }
        }
    }

    //If an exception was thrown during the execution of the parallel loop, rethrow it
    if (e_ptr) std::rethrow_exception(e_ptr);
}
//---------------------------------------------------------------------------------------



//---------------------------------------------------------------------------------------------------------
/**
 * @brief Cotangent.
 * @param angle Angle in radians.
 * @return 1 / tan(angle).
 * @ingroup runtime
 */
inline double cot(const double angle){return 1. / std::tan(angle);}
//---------------------------------------------------------------------------------------------------------








#endif
