#ifndef DEF_USPG_ABSTRACT
#define DEF_USPG_ABSTRACT

/**
 * @file uspg_abstract.hpp
 * @brief Base class of the uniform spatial partitioning grids.
 * @ingroup spatial_partitioning
 */

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


/**
 * @brief Base class of the uniform spatial partitioning grids (USPG).
 *
 * A USPG divides an axis-aligned box into cubic voxels of equal edge length, so that a
 * neighbour search only visits the voxels near a point (C. Ericson, *Real-Time Collision
 * Detection*, section 7.1). Voxels are stored in a flat array indexed
 * x + y n_x + z n_x n_y. The derived classes define what a voxel holds: uspg_4d any number of
 * objects, uspg_3d at most one.
 *
 * @ingroup spatial_partitioning
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
    /** @brief Return the voxel edge length, m. */
    double get_voxel_size() const noexcept {return voxel_size_;}
    /** @brief Return the minimum corner (x, y, z) of the grid, m. */
    std::array<double, 3>   get_min_corner() const noexcept {return {min_x_, min_y_, min_z_};}
    /** @brief Return the maximum corner (x, y, z) of the grid, m. */
    std::array<double, 3>   get_max_corner() const noexcept {return {max_x_, max_y_, max_z_};}
    /** @brief Return the number of voxels along x, y and z. */
    std::array<unsigned, 3> get_nb_voxels() const noexcept {return {nb_voxels_x_, nb_voxels_y_, nb_voxels_z_};}
//----------------------------------------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------------------------------------
    /**
     * @brief Return the (i, j, k) index of the voxel that contains a point.
     * @param pos Position, m. Must lie inside the grid (checked by assertions only).
     */
    std::array<unsigned, 3> get_3d_voxel_index(const vec3& pos) const noexcept{
        return get_3d_voxel_index(pos.dx(), pos.dy(), pos.dz());
    }

    /** @brief Overload of get_3d_voxel_index() taking the three coordinates, m. */
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
    /**
     * @brief Return the flat index of the voxel that contains a point.
     * @param pos Position, m. Must lie inside the grid.
     */
    size_t get_voxel_index(const vec3& pos) const noexcept{
        const auto [pos_x, pos_y, pos_z] = pos.to_array();
        return get_voxel_index(pos_x, pos_y, pos_z);
    }

    
    /** @brief Overload of get_voxel_index() taking the three coordinates, m. */
    size_t get_voxel_index(const double pos_x, const double pos_y, const double pos_z) const noexcept{

        const auto [voxel_x_id, voxel_y_id, voxel_z_id] = get_3d_voxel_index(pos_x, pos_y, pos_z);

        return get_voxel_index(voxel_x_id, voxel_y_id, voxel_z_id);
    }
    
    /** @brief Return the flat index of the voxel (i, j, k). */
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
    /**
     * @brief Empty the grid and resize it to cover a box.
     * @param nb_objects Number of objects that will be stored (0 with an empty box gives a
     *        single-voxel grid, see set_empty_grid()).
     * @param min_x,min_y,min_z Minimum corner of the box, m.
     * @param max_x,max_y,max_z Maximum corner of the box, m.
     * @throws unstable_simulation_exception If the box is not finite, needs too many voxels, or
     *         the grid cannot be allocated (a cell has diverged).
     */
    virtual void update_dimensions(const size_t nb_objects, const double min_x, const double min_y, const double min_z,
    const double max_x, const double max_y, const double max_z) noexcept(false) = 0;
//----------------------------------------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------------------------------------
    /**
     * @brief Return the number of voxels needed to cover [min, max] along one axis.
     *
     * A cell whose nodes have diverged gives an extent that does not fit in an unsigned count;
     * the method throws instead of letting the count wrap, and the contact models let the
     * exception reach the solver's caller, which ends the run with the extent in the message.
     *
     * @param min  Lower bound, m.
     * @param max  Upper bound, m.
     * @param axis Axis name used in the error message ('x', 'y' or 'z').
     * @throws unstable_simulation_exception If a bound is not finite or more than 2^31 voxels
     *         would be needed.
     */
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

    /**
     * @brief Collapse the grid to a single voxel at the origin when there is nothing to store.
     *
     * With no objects (for example after every cell has been removed), the contact model passes
     * the inverted (+inf, -inf) box of an empty face list; this is not treated as a diverged
     * domain.
     *
     * @return True if `nb_objects` is 0 and the box is not a proper finite box, in which case the
     *         grid was set to one voxel.
     */
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