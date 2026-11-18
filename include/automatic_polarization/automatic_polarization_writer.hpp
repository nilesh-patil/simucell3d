#ifndef DEF_AUTOMATIC_POLARIZATION_WRITER
#define DEF_AUTOMATIC_POLARIZATION_WRITER

/**
 * @file automatic_polarization_writer.hpp
 * @brief Writer of the voxel regions of the automatic polarizer (`region_data/` output).
 * @ingroup polarization
 */


#include <iostream>
#include <vector>
#include <array>
#include <omp.h>
#include <string>


#include "global_configuration.hpp"

#include "utils.hpp"
#include "custom_structures.hpp"
#include "custom_exception.hpp"

#include "mesh_writer.hpp"

#include "cell.hpp"
#include "face.hpp"
#include "node.hpp"
#include "uspg_3d.hpp"
#include "vec3.hpp"


#include "automatic_polarizer.hpp"


/**
 * @brief Writes the voxel grid of an automatic_polarizer to a legacy VTK file.
 *
 * With `POLARIZATION_MODE_INDEX 2`, the solver calls write() each time it writes the output
 * meshes after the first iteration, into `<output>/region_data/result_<n>.vtk`. Each voxel is written as a cube with the
 * cell array `voxel_type_id`: -1 unlabelled, 0 boundary, 1 cytoplasm, 2 exterior space,
 * 3 lumen.
 *
 * @ingroup polarization
 */
class automatic_polarization_writer{

        protected:

        public:
                automatic_polarization_writer() = default;                                         //default constructor
                automatic_polarization_writer(const automatic_polarization_writer& v) = delete;           //copy constructor
                automatic_polarization_writer(automatic_polarization_writer&& v) = delete;                //move constructor
                automatic_polarization_writer& operator=(const automatic_polarization_writer& v) = delete;//copy assignment operator
                automatic_polarization_writer& operator=(automatic_polarization_writer&& v) = default;    //move assignment operator 

                /**
                 * @brief Create a cube-shaped cell of 8 nodes and 12 triangles.
                 * @param min_corner  Corner with the smallest coordinates, m.
                 * @param edge_length Edge length of the cube, m.
                 * @param cell_id     Identifier of the new cell.
                 */
                static cell_ptr generate_cubic_cell(const vec3& min_corner, const double edge_length, const unsigned cell_id) noexcept;

                /**
                 * @brief Write the labelled voxel grid of a polarizer to a VTK file.
                 * @param output_path Path of the file to create.
                 * @param polarizer   Polarizer whose grid is written.
                 * @throws mesh_writer_exception If the file cannot be opened.
                 */
                static void write(const std::string& output_path, const automatic_polarizer& polarizer) noexcept(false);

                /** @brief Create one cube cell per voxel of the grid (loop order: x outer, then y, z inner). */
                static std::vector<cell_ptr> convert_grid_voxels_to_cells(const uspg_3d<unsigned short>& grid) noexcept;

                /** @brief Append the `voxel_type_id` cell array (one value per voxel) to an open VTK file. */
                static void write_voxel_type_data(
                    std::ofstream& polarization_data_file,
                    const uspg_3d<unsigned short>& grid
                );


};

#endif