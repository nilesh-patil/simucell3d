/**
 * @file checkpoint_reader.hpp
 * @brief Binary checkpoint reader for restoring simulation state
 *
 * Reads .simchk checkpoint files and reconstructs:
 * - Cell meshes (nodes, faces, edges)
 * - Cell properties (volume, area, growth rates)
 * - Simulation state (iteration, time)
 *
 * @author SimuCell3D Team
 * @date 2025-10-31
 */

#ifndef DEF_CHECKPOINT_READER
#define DEF_CHECKPOINT_READER

#include <string>
#include <fstream>
#include <vector>
#include <ctime>
#include <stdexcept>

#include "cell.hpp"
#include "custom_structures.hpp"

/**
 * @brief Reads simulation checkpoints from binary files
 *
 * Validates checkpoint integrity and reconstructs full simulation state.
 * Compatible with checkpoint_writer format.
 */
class checkpoint_reader {
private:
    static constexpr uint32_t MAGIC_NUMBER = 0x53494D43;  // "SIMC" in ASCII
    static constexpr uint16_t VERSION_MAJOR = 1;
    static constexpr uint16_t VERSION_MINOR = 0;

    /**
     * @brief Read a length-prefixed string from binary file
     */
    std::string read_string(std::ifstream& file) const {
        uint32_t length;
        file.read(reinterpret_cast<char*>(&length), sizeof(length));

        if (length > 100000) {  // Sanity check
            throw std::runtime_error("Invalid string length in checkpoint (file may be corrupt)");
        }

        std::string str(length, '\0');
        file.read(&str[0], length);
        return str;
    }

    /**
     * @brief Read vec3 from binary file
     */
    vec3 read_vec3(std::ifstream& file) const {
        double coords[3];
        file.read(reinterpret_cast<char*>(coords), sizeof(coords));
        return vec3(coords[0], coords[1], coords[2]);
    }

public:
    checkpoint_reader() = default;

    struct CheckpointMetadata {
        uint16_t version_major;
        uint16_t version_minor;
        std::time_t timestamp;
        unsigned int iteration;
        double simulation_time;
        std::string parameter_file_path;
        uint32_t num_cells;
    };

    /**
     * @brief Read checkpoint metadata without loading full state
     *
     * Useful for inspecting checkpoint files.
     *
     * @param filename Checkpoint file path
     * @return Metadata structure
     */
    CheckpointMetadata read_metadata(const std::string& filename) const {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open checkpoint file: " + filename);
        }

        CheckpointMetadata meta;

        // Verify magic number
        uint32_t magic;
        file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        if (magic != MAGIC_NUMBER) {
            throw std::runtime_error("Invalid checkpoint file format (bad magic number)");
        }

        // Read version
        file.read(reinterpret_cast<char*>(&meta.version_major), sizeof(meta.version_major));
        file.read(reinterpret_cast<char*>(&meta.version_minor), sizeof(meta.version_minor));

        if (meta.version_major != VERSION_MAJOR) {
            throw std::runtime_error("Incompatible checkpoint version");
        }

        // Read metadata
        file.read(reinterpret_cast<char*>(&meta.timestamp), sizeof(meta.timestamp));
        file.read(reinterpret_cast<char*>(&meta.iteration), sizeof(meta.iteration));
        file.read(reinterpret_cast<char*>(&meta.simulation_time), sizeof(meta.simulation_time));

        meta.parameter_file_path = read_string(file);

        file.read(reinterpret_cast<char*>(&meta.num_cells), sizeof(meta.num_cells));

        file.close();
        return meta;
    }

    /**
     * @brief Read full checkpoint and reconstruct cell list
     *
     * @param filename Checkpoint file path
     * @param cell_type_params Cell type parameters (needed for reconstruction)
     * @param[out] iteration Restored iteration number
     * @param[out] simulation_time Restored simulation time
     * @return Vector of restored cells
     */
    std::vector<cell_ptr> read_checkpoint(
        const std::string& filename,
        const std::vector<cell_type_param_ptr>& cell_type_params,
        unsigned int& iteration,
        double& simulation_time) const {

        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open checkpoint file: " + filename);
        }

        // Read and validate header
        uint32_t magic;
        uint16_t ver_major, ver_minor;
        std::time_t timestamp;

        file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        if (magic != MAGIC_NUMBER) {
            throw std::runtime_error("Invalid checkpoint file format");
        }

        file.read(reinterpret_cast<char*>(&ver_major), sizeof(ver_major));
        file.read(reinterpret_cast<char*>(&ver_minor), sizeof(ver_minor));

        if (ver_major != VERSION_MAJOR) {
            throw std::runtime_error("Incompatible checkpoint version: " +
                                   std::to_string(ver_major) + "." + std::to_string(ver_minor));
        }

        file.read(reinterpret_cast<char*>(&timestamp), sizeof(timestamp));

        // Read simulation state
        file.read(reinterpret_cast<char*>(&iteration), sizeof(iteration));
        file.read(reinterpret_cast<char*>(&simulation_time), sizeof(simulation_time));

        // Read parameter file path (for reference)
        std::string param_path = read_string(file);
        (void)param_path;  // Not used currently, but stored for reference

        // Read number of cells
        uint32_t num_cells;
        file.read(reinterpret_cast<char*>(&num_cells), sizeof(num_cells));

        // Reconstruct cells
        std::vector<cell_ptr> cell_lst;
        cell_lst.reserve(num_cells);

        for (uint32_t i = 0; i < num_cells; ++i) {
            cell_ptr restored_cell = read_cell(file, cell_type_params);
            cell_lst.push_back(restored_cell);
        }

        file.close();

        return cell_lst;
    }

private:
    /**
     * @brief Read single cell from binary stream
     */
    cell_ptr read_cell(std::ifstream& file, const std::vector<cell_type_param_ptr>& cell_type_params) const {
        // Read cell metadata
        unsigned cell_id, cell_type_id;
        bool is_static;

        file.read(reinterpret_cast<char*>(&cell_id), sizeof(cell_id));
        file.read(reinterpret_cast<char*>(&cell_type_id), sizeof(cell_type_id));
        file.read(reinterpret_cast<char*>(&is_static), sizeof(is_static));

        // Find cell type parameters
        if (cell_type_id >= cell_type_params.size()) {
            throw std::runtime_error("Invalid cell type ID in checkpoint: " + std::to_string(cell_type_id));
        }

        // Read cell properties
        double volume, area, target_volume, target_area, growth_rate, division_volume;

        file.read(reinterpret_cast<char*>(&volume), sizeof(volume));
        file.read(reinterpret_cast<char*>(&area), sizeof(area));
        file.read(reinterpret_cast<char*>(&target_volume), sizeof(target_volume));
        file.read(reinterpret_cast<char*>(&target_area), sizeof(target_area));
        file.read(reinterpret_cast<char*>(&growth_rate), sizeof(growth_rate));
        file.read(reinterpret_cast<char*>(&division_volume), sizeof(division_volume));

        // Read centroid
        vec3 centroid = read_vec3(file);

        // Create cell
        cell_ptr c = std::make_shared<cell>(cell_type_params[cell_type_id], cell_id);

        // Read nodes
        uint32_t num_nodes;
        file.read(reinterpret_cast<char*>(&num_nodes), sizeof(num_nodes));

        std::vector<vec3> node_positions;
        std::vector<vec3> node_forces;
        std::vector<unsigned> node_ids;

        #if DYNAMIC_MODEL_INDEX == 1
        std::vector<vec3> node_momenta;
        #endif

        for (uint32_t i = 0; i < num_nodes; ++i) {
            unsigned node_id;
            file.read(reinterpret_cast<char*>(&node_id), sizeof(node_id));

            vec3 pos = read_vec3(file);
            vec3 force = read_vec3(file);

            node_ids.push_back(node_id);
            node_positions.push_back(pos);
            node_forces.push_back(force);

            #if DYNAMIC_MODEL_INDEX == 1
            vec3 momentum = read_vec3(file);
            node_momenta.push_back(momentum);
            #endif
        }

        // Read faces
        uint32_t num_faces;
        file.read(reinterpret_cast<char*>(&num_faces), sizeof(num_faces));

        std::vector<std::array<unsigned, 3>> face_node_indices;
        std::vector<unsigned short> face_type_ids;

        for (uint32_t i = 0; i < num_faces; ++i) {
            unsigned face_id, n1, n2, n3;
            unsigned short type_id;

            file.read(reinterpret_cast<char*>(&face_id), sizeof(face_id));
            file.read(reinterpret_cast<char*>(&n1), sizeof(n1));
            file.read(reinterpret_cast<char*>(&n2), sizeof(n2));
            file.read(reinterpret_cast<char*>(&n3), sizeof(n3));
            file.read(reinterpret_cast<char*>(&type_id), sizeof(type_id));

            face_node_indices.push_back({n1, n2, n3});
            face_type_ids.push_back(type_id);
        }

        // Reconstruct cell mesh
        c->restore_from_checkpoint(node_positions, face_node_indices, face_type_ids,
                                   growth_rate, division_volume, target_volume, is_static);

        return c;
    }
};

#endif // DEF_CHECKPOINT_READER
