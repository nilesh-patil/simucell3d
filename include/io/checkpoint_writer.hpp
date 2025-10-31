/**
 * @file checkpoint_writer.hpp
 * @brief Binary checkpoint writer for saving simulation state
 *
 * Checkpoint file format (binary):
 * - Header: Version, timestamp, step, time, parameter file name
 * - Cells: All cell data (mesh, properties, state)
 * - Global state: Iteration, simulation time, RNG state
 *
 * File extension: .simchk (SimuCell3D Checkpoint)
 *
 * @author SimuCell3D Team
 * @date 2025-10-31
 */

#ifndef DEF_CHECKPOINT_WRITER
#define DEF_CHECKPOINT_WRITER

#include <string>
#include <fstream>
#include <vector>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "cell.hpp"
#include "custom_structures.hpp"

/**
 * @brief Writes simulation checkpoints to binary files
 *
 * Binary format for efficiency and portability.
 * Checkpoints can be used to:
 * - Resume interrupted simulations
 * - Branch simulations from common state
 * - Debug simulation issues
 * - Share reproducible states
 */
class checkpoint_writer {
private:
    static constexpr uint32_t MAGIC_NUMBER = 0x53494D43;  // "SIMC" in ASCII
    static constexpr uint16_t VERSION_MAJOR = 1;
    static constexpr uint16_t VERSION_MINOR = 0;

    /**
     * @brief Write a string to binary file (length-prefixed)
     */
    void write_string(std::ofstream& file, const std::string& str) const {
        uint32_t length = static_cast<uint32_t>(str.size());
        file.write(reinterpret_cast<const char*>(&length), sizeof(length));
        file.write(str.data(), length);
    }

    /**
     * @brief Write vec3 to binary file
     */
    void write_vec3(std::ofstream& file, const vec3& v) const {
        double coords[3] = {v.dx(), v.dy(), v.dz()};
        file.write(reinterpret_cast<const char*>(coords), sizeof(coords));
    }

public:
    checkpoint_writer() = default;

    /**
     * @brief Write checkpoint to file
     *
     * @param filename Output checkpoint file path
     * @param cell_lst List of cells to checkpoint
     * @param iteration Current iteration number
     * @param simulation_time Current simulation time
     * @param parameter_file_path Path to parameter file (for reference)
     */
    void write_checkpoint(
        const std::string& filename,
        const std::vector<cell_ptr>& cell_lst,
        unsigned int iteration,
        double simulation_time,
        const std::string& parameter_file_path) const {

        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open checkpoint file for writing: " + filename);
        }

        // Write header
        file.write(reinterpret_cast<const char*>(&MAGIC_NUMBER), sizeof(MAGIC_NUMBER));
        file.write(reinterpret_cast<const char*>(&VERSION_MAJOR), sizeof(VERSION_MAJOR));
        file.write(reinterpret_cast<const char*>(&VERSION_MINOR), sizeof(VERSION_MINOR));

        // Write timestamp
        std::time_t now = std::time(nullptr);
        file.write(reinterpret_cast<const char*>(&now), sizeof(now));

        // Write simulation state
        file.write(reinterpret_cast<const char*>(&iteration), sizeof(iteration));
        file.write(reinterpret_cast<const char*>(&simulation_time), sizeof(simulation_time));

        // Write parameter file reference
        write_string(file, parameter_file_path);

        // Write number of cells
        uint32_t num_cells = static_cast<uint32_t>(cell_lst.size());
        file.write(reinterpret_cast<const char*>(&num_cells), sizeof(num_cells));

        // Write each cell
        for (const auto& cell_ptr : cell_lst) {
            write_cell(file, *cell_ptr);
        }

        file.close();
    }

    /**
     * @brief Write single cell to binary stream
     */
    void write_cell(std::ofstream& file, const cell& c) const {
        // Cell metadata
        unsigned cell_id = c.get_id();
        unsigned cell_type_id = c.get_cell_type_id();
        bool is_static = c.is_static();

        file.write(reinterpret_cast<const char*>(&cell_id), sizeof(cell_id));
        file.write(reinterpret_cast<const char*>(&cell_type_id), sizeof(cell_type_id));
        file.write(reinterpret_cast<const char*>(&is_static), sizeof(is_static));

        // Cell properties
        double volume = c.get_volume();
        double area = c.get_area();
        double target_volume = c.get_target_volume();
        double growth_rate = c.get_growth_rate();
        double division_volume = c.get_division_volume();

        file.write(reinterpret_cast<const char*>(&volume), sizeof(volume));
        file.write(reinterpret_cast<const char*>(&area), sizeof(area));
        file.write(reinterpret_cast<const char*>(&target_volume), sizeof(target_volume));
        file.write(reinterpret_cast<const char*>(&growth_rate), sizeof(growth_rate));
        file.write(reinterpret_cast<const char*>(&division_volume), sizeof(division_volume));

        // Centroid
        write_vec3(file, c.get_centroid());

        // Mesh data - nodes
        const auto& nodes = c.get_node_lst();
        uint32_t num_used_nodes = 0;

        // Count used nodes
        for (const auto& node : nodes) {
            if (node.is_used()) num_used_nodes++;
        }

        file.write(reinterpret_cast<const char*>(&num_used_nodes), sizeof(num_used_nodes));

        // Write used nodes
        for (unsigned i = 0; i < nodes.size(); ++i) {
            if (!nodes[i].is_used()) continue;

            // Node ID
            unsigned node_id = nodes[i].get_local_id();
            file.write(reinterpret_cast<const char*>(&node_id), sizeof(node_id));

            // Position and force
            write_vec3(file, nodes[i].pos());
            write_vec3(file, nodes[i].force());

            #if DYNAMIC_MODEL_INDEX == 1
            // Momentum for dynamic model
            write_vec3(file, nodes[i].momentum());
            #endif
        }

        // Mesh data - faces
        const auto& faces = c.get_face_lst();
        uint32_t num_used_faces = 0;

        // Count used faces
        for (const auto& face : faces) {
            if (face.is_used()) num_used_faces++;
        }

        file.write(reinterpret_cast<const char*>(&num_used_faces), sizeof(num_used_faces));

        // Write used faces
        for (unsigned i = 0; i < faces.size(); ++i) {
            if (!faces[i].is_used()) continue;

            // Face ID and node indices
            unsigned face_id = faces[i].get_local_id();
            unsigned n1 = faces[i].n1_id();
            unsigned n2 = faces[i].n2_id();
            unsigned n3 = faces[i].n3_id();
            unsigned short type_id = faces[i].get_local_face_type_id();

            file.write(reinterpret_cast<const char*>(&face_id), sizeof(face_id));
            file.write(reinterpret_cast<const char*>(&n1), sizeof(n1));
            file.write(reinterpret_cast<const char*>(&n2), sizeof(n2));
            file.write(reinterpret_cast<const char*>(&n3), sizeof(n3));
            file.write(reinterpret_cast<const char*>(&type_id), sizeof(type_id));
        }
    }
};

#endif // DEF_CHECKPOINT_WRITER
