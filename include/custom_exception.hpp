#ifndef DEF_CUSTOM_EXCEPTION
#define DEF_CUSTOM_EXCEPTION


#include <exception>
#include <string>

/**
 * @file
 * @brief Exception classes thrown by SimuCell3D.
 * @ingroup core
 */




//---------------------------------------------------------------------------------------
/**
 * @brief Thrown by the automatic_polarizer when the faces cannot be polarized.
 * @ingroup core
 */
class automatic_polarization_exception: public std::exception
{
    private:
        std::string error_msg_;

public:
    /// Construct the exception with the message returned by what().
    automatic_polarization_exception(const std::string& error_msg) throw() :error_msg_(error_msg) {}

    /// Return the error message.
    virtual const char* what() const throw() {return error_msg_.c_str();}
     
    //Destructor is probably not needed
    virtual ~automatic_polarization_exception() throw(){}
};

//---------------------------------------------------------------------------------------






//---------------------------------------------------------------------------------------
/**
 * @brief Thrown when the simulation has diverged and cannot continue.
 *
 * Raised when the bounding box of the tissue is not finite or too large for a spatial grid, and by
 * solver::handle_refinement_failures() when too many mesh refinements fail in one iteration.
 * @ingroup core
 */
class unstable_simulation_exception: public std::exception
{
    private:
        std::string error_msg_;

public:
    /// Construct the exception with the message returned by what().
    unstable_simulation_exception(const std::string& error_msg) throw() :error_msg_(error_msg) {}

    /// Return the error message.
    virtual const char* what() const throw() {return error_msg_.c_str();}
     
    //Destructor is probably not needed
    virtual ~unstable_simulation_exception() throw(){}
};

//---------------------------------------------------------------------------------------




//---------------------------------------------------------------------------------------
/**
 * @brief Thrown when a simulation cannot be set up or its output cannot be written.
 *
 * Examples: inconsistent cell types and input mesh, an invalid scheduling mode, an output folder or
 * statistics file that cannot be created or written. The class name keeps its historical spelling.
 * @ingroup core
 */
class intialization_exception: public std::exception
{
    private:
        std::string error_msg_;

public:
    /// Construct the exception with the message returned by what().
    intialization_exception(const std::string& error_msg) throw() :error_msg_(error_msg) {}

    /// Return the error message.
    virtual const char* what() const throw() {return error_msg_.c_str();}
     
    //Destructor is probably not needed
    virtual ~intialization_exception() throw(){}
};

//---------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------
/**
 * @brief Thrown when a cell division fails (cell_divider, poisson_sampling).
 * @ingroup core
 */
class division_exception: public std::exception
{
    private:
        std::string error_msg_;

public:
    /// Construct the exception with the message returned by what().
    division_exception(const std::string& error_msg) throw() :error_msg_(error_msg) {}

    /// Return the error message.
    virtual const char* what() const throw() {return error_msg_.c_str();}
     
    //Destructor is probably not needed
    virtual ~division_exception() throw(){}
};

//---------------------------------------------------------------------------------------




//---------------------------------------------------------------------------------------
/**
 * @brief Thrown when the parameter file cannot be opened, is malformed or misses a required tag.
 * @ingroup core
 */
class parameter_reader_exception: public std::exception
{
    private:
        std::string error_msg_;

public:
    /// Construct the exception with the message returned by what().
    parameter_reader_exception(const std::string& error_msg) throw() :error_msg_(error_msg) {}

    /// Return the error message.
    virtual const char* what() const throw() {return error_msg_.c_str();}
     
    //Destructor is probably not needed
    virtual ~parameter_reader_exception() throw(){}
};

//---------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------
/**
 * @brief Thrown when the ball pivoting algorithm cannot triangulate a point cloud.
 * @ingroup core
 */
class bpa_exception: public std::exception
{
    private:
        std::string error_msg_;

public:
    /// Construct the exception with the message returned by what().
    bpa_exception(const std::string& error_msg) throw() :error_msg_(error_msg) {}

    /// Return the error message.
    virtual const char* what() const throw() {return error_msg_.c_str();}
     
    //Destructor is probably not needed
    virtual ~bpa_exception() throw(){}
};
//---------------------------------------------------------------------------------------




//---------------------------------------------------------------------------------------
/**
 * @brief Thrown when a cell mesh is not a valid closed manifold surface.
 *
 * For example a hole in the mesh or an edge shared by more than two faces.
 * @ingroup core
 */
class mesh_integrity_exception: public std::exception
{
    private:
        std::string error_msg_;

public:
    /// Construct the exception with the message returned by what().
    mesh_integrity_exception(const std::string& error_msg) throw() :error_msg_(error_msg) {}

    /// Return the error message.
    virtual const char* what() const throw() {return error_msg_.c_str();}
     
    //Destructor is probably not needed
    virtual ~mesh_integrity_exception() throw(){}
};
//---------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------
/**
 * @brief Thrown when the initial triangulation of an input cell mesh fails.
 * @ingroup core
 */
class initial_triangulation_exception: public std::exception
{
    private:
        std::string error_msg_;

public:
    /// Construct the exception with the message returned by what().
    initial_triangulation_exception(const std::string& error_msg) throw() :error_msg_(error_msg) {}

    /// Return the error message.
    virtual const char* what() const throw() {return error_msg_.c_str();}
     
    //Destructor is probably not needed
    virtual ~initial_triangulation_exception() throw(){}
};
//---------------------------------------------------------------------------------------



//---------------------------------------------------------------------------------------
/**
 * @brief Thrown when the input mesh file cannot be opened or is not in the expected VTK format.
 * @ingroup core
 */
class mesh_reader_exception: public std::exception
{
    private:
        std::string error_msg_;

public:
    /// Construct the exception with the message returned by what().
    mesh_reader_exception(const std::string& error_msg) throw() :error_msg_(error_msg) {}

    /// Return the error message.
    virtual const char* what() const throw() {return error_msg_.c_str();}
     
    //Destructor is probably not needed
    virtual ~mesh_reader_exception() throw(){}
};
//---------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------
/**
 * @brief Thrown when an output mesh file cannot be written.
 * @ingroup core
 */
class mesh_writer_exception: public std::exception
{
    private:
        std::string error_msg_;

public:
    /// Construct the exception with the message returned by what().
    mesh_writer_exception(const std::string& error_msg) throw() :error_msg_(error_msg) {}

    /// Return the error message.
    virtual const char* what() const throw() {return error_msg_.c_str();}
     
    //Destructor is probably not needed
    virtual ~mesh_writer_exception() throw(){}
};
//---------------------------------------------------------------------------------------




//---------------------------------------------------------------------------------------
/**
 * @brief Exception reserved for the uniform spatial partitioning grids.
 *
 * Not thrown by the current code: the grids report an oversized domain with unstable_simulation_exception.
 * @ingroup core
 */
class uspg_exception: public std::exception
{
    private:
        std::string error_msg_;

public:
    /// Construct the exception with the message returned by what().
    uspg_exception(const std::string& error_msg) throw() :error_msg_(error_msg) {}

    /// Return the error message.
    virtual const char* what() const throw() {return error_msg_.c_str();}
     
    //Destructor is probably not needed
    virtual ~uspg_exception() throw(){}
};
//---------------------------------------------------------------------------------------








#endif