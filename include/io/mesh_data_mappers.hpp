#ifndef DEF_MESH_DATA_MAPPERS
#define DEF_MESH_DATA_MAPPERS

#include <vector>
#include <string>
#include <functional>


#include "utils.hpp"
#include "cell.hpp"
#include "face.hpp"

/**
 * @file
 * @brief Data mappers: named, typed extractors of per-cell, per-face and per-node output values.
 * @ingroup io
 */
//---------------------------------------------------------------------------------------
/**
 * @brief Maps one per-cell quantity to an array of the output meshes or a column of the statistics.
 *
 * Instances are listed in mesh_data.hpp.
 * @ingroup io
 */
struct cell_data_mapper{

    std::string value_name_;  ///< Name of the array or column, for example `cell_volume`.

    std::string value_type_;  ///< VTK data type of the array, for example `float` or `int`.

    std::function<std::string(cell_ptr)>  value_extractor_;  ///< Returns the formatted value for a cell.

    /// Construct an empty mapper.
    cell_data_mapper() = default; 

    /// Construct a mapper from its name, VTK type and extractor.
    cell_data_mapper(const std::string value_name, const std::string value_type, std::function<std::string(cell_ptr)> value_extractor):
        value_name_(value_name), value_type_(value_type), value_extractor_(value_extractor){};

};

/**
 * @brief Maps one per-face quantity to an array of the face meshes. Instances are listed in mesh_data.hpp.
 * @ingroup io
 */
struct face_data_mapper{

    std::string value_name_;  ///< Name of the array.

    std::string value_type_;  ///< VTK data type of the array.

    std::function<std::string(const face& f)>  value_extractor_;  ///< Returns the formatted value for a face.

    /// Construct an empty mapper.
    face_data_mapper() = default; 

    /// Construct a mapper from its name, VTK type and extractor.
    face_data_mapper(const std::string value_name, const std::string value_type, std::function<std::string(const face& f)> value_extractor):
        value_name_(value_name), value_type_(value_type), value_extractor_(value_extractor){};

};
//---------------------------------------------------------------------------------------




/**
 * @brief Maps one per-node quantity to a point-data array of the face meshes. Instances are listed in
 * mesh_data.hpp.
 * @ingroup io
 */
struct node_data_mapper{

    std::string value_name_;  ///< Name of the array.

    std::string value_type_;  ///< VTK data type of the array.

    std::function<std::string(const node& n)>  value_extractor_;  ///< Returns the formatted value for a node.

    /// Construct an empty mapper.
    node_data_mapper() = default; 

    /// Construct a mapper from its name, VTK type and extractor.
    node_data_mapper(const std::string value_name, const std::string value_type, std::function<std::string(const node& n)> value_extractor):
        value_name_(value_name), value_type_(value_type), value_extractor_(value_extractor){};

};
//---------------------------------------------------------------------------------------




#endif