#ifndef DEF_MESH_WRITER
#define DEF_MESH_WRITER


#include <string>
#include <fstream>
#include <iostream>
#include <cassert>
#include <vector>
#include <numeric>
#include <omp.h>


#include "cell.hpp"
#include "utils.hpp"
#include "custom_structures.hpp"
#include "custom_exception.hpp"
#include "mesh_data.hpp"

/**
 * @file
 * @brief Writer of the output meshes (legacy VTK, ASCII).
 * @ingroup io
 */



/**
 * @brief Writes the cell and face meshes as legacy ASCII VTK files (version 4.2, unstructured grid).
 *
 * The cell mesh has one polyhedron (VTK type 42) per cell with the arrays of cell_data_mapper_lst; the face
 * mesh has one triangle (VTK type 7) per face with the arrays of face_data_mapper_lst and
 * node_data_mapper_lst (mesh_data.hpp). All members are static; the class cannot be instantiated.
 * @ingroup io
 */
class mesh_writer{


    public:

        mesh_writer() = delete;                               //default constructor
        mesh_writer(const mesh_writer& v) = delete;           //copy constructor
        mesh_writer(mesh_writer&& v) = delete;                //move constructor
        mesh_writer& operator=(const mesh_writer& v) = delete;//copy assignment operator
        mesh_writer& operator=(mesh_writer&& v) = delete;     //move assignment operator 

        /**
         * @brief Write the cell mesh and the face mesh of the given cells.
         *
         * The cells are rebased first (unused node and face slots removed); the two files are written in
         * parallel.
         * @param output_path_cell_data Path of the cell mesh file.
         * @param output_path_face_data Path of the face mesh file.
         * @param cell_lst Cells to write.
         * @throws mesh_writer_exception If a file cannot be opened or a node coordinate is not finite.
         */
        static void write( 
            const std::string& output_path_cell_data, 
            const std::string& output_path_face_data, 
            std::vector<cell_ptr>& cell_lst
        ) noexcept(false);

        /// Write light meshes (for example cells read from an input mesh) as a cell mesh to an open stream.
        static void write_cell_data_file(   std::ofstream&  cell_data_file,
                                            const std::vector<mesh>& mesh_lst) noexcept(false);

        /// Write cells as a cell mesh to an open stream; rebase the cells first if `rebase_bool` is true.
        static void write_cell_data_file( std::ofstream&  cell_data_file,
                                        const std::vector<cell_ptr>& cell_lst,
                                        bool rebase_bool = true) noexcept(false);



        /**
         * @brief Write light meshes as a cell mesh to `file_path`.
         * @throws mesh_writer_exception If the file cannot be opened.
         */
        static void write_cell_data_file(const std::string& file_path,
                                        const std::vector<mesh>& mesh_lst
                                    ) noexcept(false);

        
       /**
        * @brief Write cells as a cell mesh to `file_path`; rebase the cells first if `rebase` is true.
        * @throws mesh_writer_exception If the file cannot be opened or a node coordinate is not finite.
        */
       static void write_cell_data_file(const std::string& file_path,
                                        const std::vector<cell_ptr>& cell_lst,
                                        const bool rebase = true
                                    ) noexcept(false);

        /**
         * @brief Write cells as a face mesh to `file_path`.
         * @throws mesh_writer_exception If the file cannot be opened or a node coordinate is not finite.
         */
        static void write_face_data_file(const std::string& file_path,
                                const std::vector<cell_ptr>& cell_lst
                            ) noexcept(false);



        /// Write cells as a face mesh to an open stream.
        static void write_face_data_file(   std::ofstream&  face_data_file,
                                            const std::vector<cell_ptr>& cell_lst) noexcept(false);

        /**
         * @brief Write the POINTS section for a list of cells or light meshes.
         * @param data_file Open output stream.
         * @param object_lst Cells or meshes.
         * @param node_pos_extractor Returns the flattened node coordinates (m) of one object.
         * @throws mesh_writer_exception If a coordinate is not finite (the simulation has diverged).
         */
        template<typename T1>
        static void write_point_data(   std::ofstream& data_file, 
                                        const std::vector<T1>& object_lst,
                                        const std::function<std::vector<double>(const T1&)> node_pos_extractor  
                                    ) noexcept(false);




        /// Write the CELLS and CELL_TYPES sections (one polyhedron per mesh); node ids are shifted by `node_id_offset_lst`.
        static void write_cell_data(    std::ofstream& data_file, 
                                        const std::vector<mesh>& mesh_lst,
                                        const std::vector<size_t>& node_id_offset_lst
                                    ) noexcept(false);

        /// Write the CELLS and CELL_TYPES sections (one polyhedron per cell); node ids are shifted by `node_id_offset_lst`.
        static void write_cell_data(    std::ofstream& data_file, 
                                        const std::vector<cell_ptr>& cell_lst,
                                        const std::vector<size_t>& node_id_offset_lst
                            ) noexcept(false);



        /// Write the CELLS and CELL_TYPES sections of a face mesh (one triangle per face).
        static void write_face_data(    std::ofstream& data_file, 
                                        const std::vector<cell_ptr>& cell_lst,
                                        const std::vector<size_t>& node_id_offset_lst
                            ) noexcept(false);



        /// Write the cell-data arrays defined by cell_data_mapper_lst.
        static void add_cell_data_arrays_to_mesh(
            std::ofstream& data_file,                    
            const std::vector<cell_ptr>& cell_lst
        ) noexcept;


        /// Write the per-face arrays defined by face_data_mapper_lst.
        static void add_face_data_arrays_to_mesh(
            std::ofstream& data_file,                    
            const std::vector<cell_ptr>& cell_lst
        ) noexcept;


        /// Write the point-data arrays defined by node_data_mapper_lst.
        static void add_node_data_arrays_to_mesh(
            std::ofstream& data_file,                    
            const std::vector<cell_ptr>& cell_lst
        ) noexcept;


        /// No arrays are written for light meshes.
        static void add_cell_data_arrays_to_mesh(std::ofstream& data_file, const std::vector<mesh>& mesh_lst) noexcept{}
        /// No arrays are written for light meshes.
        static void add_face_data_arrays_to_mesh(std::ofstream& data_file, const std::vector<mesh>& mesh_lst) noexcept{}



};

#endif