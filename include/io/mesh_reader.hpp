#ifndef DEF_MESH_READER
#define DEF_MESH_READER


#include <string>
#include <fstream>
#include <iostream>
#include <streambuf>
#include <sstream>
#include <regex>
#include <cassert>
#include <vector>
#include <set>
#include <map>

#include "utils.hpp"
#include "custom_structures.hpp"
#include "custom_exception.hpp"


/**
 * @file
 * @brief Reader of the input mesh (legacy VTK, ASCII).
 * @ingroup io
 */

/**
 * @brief Reads the input mesh: a legacy ASCII VTK file (version 4.2) with one polyhedron cell (VTK type 42)
 * per biological cell and a `cell_type_id` cell-data array.
 * @ingroup io
 */
class mesh_reader{
   private:
      std::string file_content_;

   public:
      mesh_reader() = delete;                               //default constructor
      mesh_reader(const mesh_reader& v) = delete;           //copy constructor
      mesh_reader(mesh_reader&& v) = delete;                //move constructor
      mesh_reader& operator=(const mesh_reader& v) = delete;//copy assignment operator
      mesh_reader& operator=(mesh_reader&& v) = delete;     //move assignment operator 
      
      /**
       * @brief Load the file into memory.
       *
       * The path is tried as given (absolute, or relative to the working directory), then relative to the
       * source directory recorded at configure time (`PROJECT_SOURCE_DIR`). A file version other than 4.2 only
       * prints a warning.
       * @param mesh_file_path Path of the VTK file.
       * @param verbose Print warnings about the path and the file version.
       * @throws mesh_reader_exception If the file is found at neither location or has no VTK version header.
       */
      mesh_reader(const std::string& mesh_file_path, const bool verbose = true) noexcept(false);

      /**
       * @brief Parse the file into one mesh per cell.
       * @throws mesh_reader_exception If the points or cells sections are missing or malformed.
       */
      std::vector<mesh> read() const noexcept(false);

      /**
       * @brief Return the coordinates of all points, flattened as x, y, z per point, m.
       * @throws mesh_reader_exception If the points section is missing or contains invalid values.
       */
      std::vector<double> get_node_pos() const noexcept(false);

      /**
       * @brief Return the face connectivity of each cell, as stored in the polyhedron cells of the file.
       * @throws mesh_reader_exception If a cell is not a polyhedron or the cells section is corrupted.
       */
      std::vector<std::vector<unsigned>>  read_cell_faces() const noexcept(false);

      /**
       * @brief Build one mesh per cell from the point coordinates and the per-cell face connectivity.
       * @throws mesh_reader_exception If the connectivity data is inconsistent.
       */
      static std::vector<mesh>  get_cell_mesh(
         const std::vector<double>& node_pos, 
         const std::vector<std::vector<unsigned>>& face_connectivity_lst_lst
      ) noexcept(false);

      /**
       * @brief Return the `cell_type_id` of each cell, read from the cell-data array of that name.
       * @throws mesh_reader_exception If the array is missing or contains invalid values.
       */
      std::vector<short>  get_cell_types() const noexcept(false);

};

#endif