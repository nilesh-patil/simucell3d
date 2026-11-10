#ifndef DEF_PARAMETER_READER
#define DEF_PARAMETER_READER

#include <string>
#include <fstream>
#include <iostream>
#include <streambuf>
#include <sstream>
#include <cassert>
#include <vector>
#include <map>
#include <optional>
#include <memory>
#include "tinyxml2.h"
#include <algorithm>



#include "utils.hpp"
#include "custom_structures.hpp"
#include "custom_exception.hpp"


/**
 * @file
 * @brief Reader of the XML parameter file.
 * @ingroup io
 */

/**
 * @brief Reads the XML parameter file into global_simulation_parameters and cell_type_parameters.
 *
 * The file has a `<numerical_parameters>` section and a `<cell_types>` section with one `<cell_type>` block
 * per cell type, each holding a `<face_types>` list of `<face_type>` blocks. Tag names are documented with
 * the members of the parameter structures.
 * @ingroup io
 */
class parameter_reader{
   private:
        //The tinyxml2 object that will be used to parse the parameter file
        tinyxml2::XMLDocument xml_doc;

        //Class used for testing
        friend class test_parameter_reader;

   public:
        parameter_reader() = delete;                                    //default constructor
        parameter_reader(const parameter_reader& v) = delete;           //copy constructor
        parameter_reader(parameter_reader&& v) = delete;                //move constructor
        parameter_reader& operator=(const parameter_reader& v) = delete;//copy assignment operator
        parameter_reader& operator=(parameter_reader&& v) = delete;     //move assignment operator 
        
        /**
         * @brief Open and parse the parameter file.
         * @param paramter_file_path Path of the XML file, absolute or relative to the working directory.
         * @throws parameter_reader_exception If the file cannot be opened or parsed.
         */
        parameter_reader(const std::string& paramter_file_path) noexcept(false);

        /**
         * @brief Return the text of the child element `XML_markup` of `e`.
         * @param e Parent element.
         * @param XML_markup Tag name of the child element.
         * @param to_lower_case Convert the text to lower case.
         * @return The text, or `std::nullopt` if the element is missing or empty.
         */
        std::optional<std::string> get_string_value(const tinyxml2::XMLElement* e,  const std::string XML_markup, const bool to_lower_case = false) const noexcept; 

        /**
         * @brief Return the top-level element named `section_name`.
         * @throws parameter_reader_exception If the section does not exist.
         */
        tinyxml2::XMLElement* select_section(const std::string& section_name) noexcept(false);

        /**
         * @brief Read the `<numerical_parameters>` section.
         * @throws parameter_reader_exception If the section or a required tag is missing, a value is not a
         *         number, or `contact_detection_algorithm` is not `uspg`, `sweep_and_prune` or `adaptive`.
         */
        global_simulation_parameters read_numerical_parameters() noexcept(false); 

        /**
         * @brief Read all cell types with their face types, in file order.
         * @return One entry per `<cell_type>` block; the index is the `cell_type_id` of the input mesh.
         * @throws parameter_reader_exception If no cell type is defined, a cell type has no face type, or a
         *         required tag is missing or invalid.
         */
        std::vector<std::shared_ptr<cell_type_parameters>> read_biomechanical_parameters() noexcept(false);

        /**
         * @brief Read one `<face_type>` block.
         * @param cell_type_name Name of the enclosing cell type, used in error messages.
         * @param face_type_id Position of the face type in its cell type.
         * @param face_type_section The `<face_type>` element.
         * @throws parameter_reader_exception If a required tag is missing or invalid.
         */
        face_type_parameters read_face_type_parameters(
            const std::string& cell_type_name,
            const unsigned face_type_id,     
            tinyxml2::XMLElement* face_type_section
        ) noexcept(false);


        /**
         * @brief Read one `<cell_type>` block, without its face types.
         * @param cell_type_id Position of the cell type in the file.
         * @param cell_type_section The `<cell_type>` element.
         * @throws parameter_reader_exception If a required tag is missing or invalid.
         */
        std::shared_ptr<cell_type_parameters> read_cell_type_parameters(
            const unsigned cell_type_id,     
            tinyxml2::XMLElement* cell_type_section
        ) noexcept(false);


};

#endif