#ifndef DEF_USPG_ABSTRACT
#define DEF_USPG_ABSTRACT

#include <vector>
#include <forward_list>
#include <string>
#include <iterator>
#include <cmath>
#include <limits>
#include <sstream>

#include "utils.hpp"
#include "custom_exception.hpp"
#include "vec3.hpp"


/*
    USPG stands for Uniform Space Partitionning Grid. This is a classical optimization technique 
    to find neighborhing objects in a space. To learn more about the theory, please refer to the excellent 
    book: Real time collision detection by Christer Ericson (Chapter 7.1) 
*/

class uspg_abstract
{

    protected: 
        //The origin of the grid
        double min_x_, min_y_, min_z_;
        double max_x_, max_y_, max_z_;

        //The number of voxels in the different directions
        unsigned nb_voxels_x_, nb_voxels_y_, nb_voxels_z_;

        //The side length of the voxels
        double voxel_size_;

        friend class contact_model_face_face;

    public:

//----------------------------------------------------------------------------------------------------------------------------------------------------
    //Make sure the uspg_abstract cannot be default instantiated
    uspg_abstract() = default;                        //default constructor
    uspg_abstract(const uspg_abstract& c) = delete;           //copy constructor
    uspg_abstract(uspg_abstract&& c) = delete;                //move constructor
    uspg_abstract& operator=(const uspg_abstract& c) = default;//copy assignment operator
    uspg_abstract& operator=(uspg_abstract&& c) = default;     //move assignment operator 

//----------------------------------------------------------------------------------------------------------------------------------------------------
    double get_voxel_size() const noexcept {return voxel_size_;}
    std::array<double, 3>   get_min_corner() const noexcept {return {min_x_, min_y_, min_z_};}
    std::array<double, 3>   get_max_corner() const noexcept {return {max_x_, max_y_, max_z_};}
    std::array<unsigned, 3> get_nb_voxels() const noexcept {return {nb_voxels_x_, nb_voxels_y_, nb_voxels_z_};}
//----------------------------------------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------------------------------------
    std::array<unsigned, 3> get_3d_voxel_index(const vec3& pos) const noexcept{
        return get_3d_voxel_index(pos.dx(), pos.dy(), pos.dz());
    }

    std::array<unsigned, 3> get_3d_voxel_index(const double pos_x, const double pos_y, const double pos_z) const noexcept{
        //Make sure the object is in the grid
        assert(pos_x >= min_x_ && pos_x <= max_x_);
        assert(pos_y >= min_y_ && pos_y <= max_y_);
        assert(pos_z >= min_z_ && pos_z <= max_z_);

        //Get the voxel in the grid that contains the point. The first part with almost equal is needed in case the 
        //points is located at the boundary of the grid
        const unsigned voxel_x_id = static_cast<unsigned>(std::floor((pos_x -  min_x_) / voxel_size_));
        const unsigned voxel_y_id = static_cast<unsigned>(std::floor((pos_y -  min_y_) / voxel_size_));
        const unsigned voxel_z_id = static_cast<unsigned>(std::floor((pos_z -  min_z_) / voxel_size_));

        return {voxel_x_id, voxel_y_id, voxel_z_id};
        
    }

//----------------------------------------------------------------------------------------------------------------------------------------------------


//----------------------------------------------------------------------------------------------------------------------------------------------------
    //Given a position in space, indicate in which voxel the object is contained
    size_t get_voxel_index(const vec3& pos) const noexcept{
        const auto [pos_x, pos_y, pos_z] = pos.to_array();
        return get_voxel_index(pos_x, pos_y, pos_z);
    }

    
    size_t get_voxel_index(const double pos_x, const double pos_y, const double pos_z) const noexcept{

        const auto [voxel_x_id, voxel_y_id, voxel_z_id] = get_3d_voxel_index(pos_x, pos_y, pos_z);

        return get_voxel_index(voxel_x_id, voxel_y_id, voxel_z_id);
    }
    
    size_t get_voxel_index(const unsigned voxel_x_id, const unsigned voxel_y_id, const unsigned voxel_z_id) const noexcept{
        assert(voxel_x_id < nb_voxels_x_);
        assert(voxel_y_id < nb_voxels_y_);
        assert(voxel_z_id < nb_voxels_z_);

        //The position of the object in the 1D grid
        //Cast first operand to size_t to prevent 32-bit overflow when multiplying
        const size_t voxel_id = static_cast<size_t>(voxel_z_id) * nb_voxels_x_ * nb_voxels_y_ +
                                static_cast<size_t>(voxel_y_id) * nb_voxels_x_ + voxel_x_id;
        return voxel_id;
    }
//----------------------------------------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------------------------------------
    virtual void update_dimensions(const size_t nb_objects, const double min_x, const double min_y, const double min_z,
    const double max_x, const double max_y, const double max_z) noexcept(false) = 0;
//----------------------------------------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------------------------------------
    //Number of voxels needed to cover [min, max] along one axis. Throws unstable_simulation_exception instead of
    //letting the count wrap: a cell whose nodes have diverged to ~1e19 m gives an extent / voxel_size that does not
    //fit in an unsigned, and static_cast<unsigned> of such a value is UB (in practice 0 voxels, after which every
    //voxel lookup indexes an empty vector). Called from noexcept contact-model code, so the throw terminates the run
    //with the extent in the message rather than hanging in an O(N^2) fallback.
    unsigned compute_nb_voxels(const double min, const double max, const char axis) const noexcept(false){
        //Add a small padding to make sure that the point (max_x, max_y, max_z) is in the grid
        constexpr double delta = std::numeric_limits<double>::epsilon();
        const double extent = max + delta - min;
        const double nb_voxels = std::ceil(extent / voxel_size_);

        //Anything past this is not a tissue, it is a diverging mesh (2^31 voxels of 3.4 um = 7 km)
        constexpr double max_nb_voxels = static_cast<double>(std::numeric_limits<unsigned>::max() / 2);
        if(!std::isfinite(min) || !std::isfinite(max) || !std::isfinite(nb_voxels) || nb_voxels < 0. || nb_voxels > max_nb_voxels){
            std::ostringstream msg;
            msg << "The bounding box of the simulation along the " << axis << " axis is [" << min << ", " << max
                << "] (" << nb_voxels << " voxels of size " << voxel_size_
                << "). The simulation is unstable: at least one cell has diverged. Reducing the time step might help.";
            throw unstable_simulation_exception(msg.str());
        }
        return static_cast<unsigned>(nb_voxels);
    }

    //With nothing to store (e.g. every cell has been removed) the contact model passes the inverted (+inf, -inf)
    //bounding box of an empty face list: collapse the grid to a single voxel at the origin instead of treating
    //that as a diverged domain. Returns true if the grid was set up this way.
    bool set_empty_grid(const size_t nb_objects, const double min_x, const double min_y, const double min_z,
    const double max_x, const double max_y, const double max_z) noexcept{
        const bool proper_box = std::isfinite(min_x) && std::isfinite(min_y) && std::isfinite(min_z) &&
                                std::isfinite(max_x) && std::isfinite(max_y) && std::isfinite(max_z) &&
                                min_x <= max_x && min_y <= max_y && min_z <= max_z;
        if(nb_objects != 0 || proper_box) return false;
        nb_voxels_x_ = nb_voxels_y_ = nb_voxels_z_ = 1;
        min_x_ = min_y_ = min_z_ = 0.;
        max_x_ = max_y_ = max_z_ = voxel_size_;
        return true;
    }
//----------------------------------------------------------------------------------------------------------------------------------------------------
        
//----------------------------------------------------------------------------------------------------------------------------------------------------
    //Make sure the memory usage does not exceed a certain amount of the available
    //memory RAM
    //virtual void check_memory_limit(const size_t nb_objects, const double fraction_of_RAM = 1.) const noexcept(false) = 0; 
//----------------------------------------------------------------------------------------------------------------------------------------------------




};

#endif