#ifndef DEF_CUSTOM_STRUCTURES
#define DEF_CUSTOM_STRUCTURES


#include <vector> 
#include <memory>
#include <map>
#include <string>
#include <functional>


#include "vec3.hpp"

/**
 * @file
 * @brief Parameter structures read from the parameter file, simulation outputs and light mesh types.
 * @ingroup core
 */

//---------------------------------------------------------------------------------------
/**
 * @brief Contact detection algorithm, set by the optional `contact_detection_algorithm` tag of the parameter file.
 * @ingroup core
 */
enum class ContactDetectionAlgorithm {
    USPG,              ///< Uniform spatial partitioning grid (`uspg`, default).
    SWEEP_AND_PRUNE,   ///< Sweep and prune on the axis-aligned bounding boxes of the cells (`sweep_and_prune`).
    ADAPTIVE           ///< USPG below ADAPTIVE_SAP_CELL_THRESHOLD cells, sweep and prune from that count on (`adaptive`).
};

/// Number of cells from which the `adaptive` contact detection switches from the USPG to sweep and prune.
constexpr size_t ADAPTIVE_SAP_CELL_THRESHOLD = 500;
//---------------------------------------------------------------------------------------



//---------------------------------------------------------------------------------------
/**
 * @brief Numerical parameters of a simulation, from the `<numerical_parameters>` section of the parameter file.
 *
 * Members without a default must be set before the structure is used.
 * @ingroup core
 */
struct global_simulation_parameters{

    /// Folder where all output files are written (`output_mesh_folder_path`). Absolute, or relative to the
    /// working directory. The solver deletes and re-creates it at the start of a run.
    std::string output_folder_path_;

    /// Input mesh file (`input_mesh_file_path`). Tried as given (absolute, or relative to the working
    /// directory), then relative to the source directory recorded at configure time.
    std::string input_mesh_path_;

    /// Re-triangulate the input cell meshes before the run (`perform_initial_triangulation`).
    bool perform_initial_triangulation_ = 1;

    /// Let the local mesh refiner swap the longest edge of triangles with a high aspect ratio
    /// (`enable_edge_swap_operation`).
    bool enable_edge_swap_operation_ = true;

    double damping_coefficient_;   ///< Damping coefficient, kg/s (`damping_coefficient`).

    double simulation_duration_;   ///< Simulated time, s (`simulation_duration`).

    double sampling_period_;       ///< Simulated time between two output meshes, s (`sampling_period`).

    double time_step_;             ///< Time step, s (`time_step`).

    /// Minimum edge length of the cell meshes, m (`min_edge_length`). The maximum edge length is three times this value.
    double min_edge_len_;

    double contact_cutoff_adhesion_;   ///< Distance beyond which adhesion forces are zero, m (`contact_cutoff_adhesion`).
    double contact_cutoff_repulsion_;  ///< Distance beyond which repulsion forces are zero, m (`contact_cutoff_repulsion`).

    /// Contact detection algorithm (`contact_detection_algorithm`, optional). Default: USPG.
    ContactDetectionAlgorithm contact_detection_algorithm_ = ContactDetectionAlgorithm::USPG;
};
//---------------------------------------------------------------------------------------



//---------------------------------------------------------------------------------------
/**
 * @brief Parameters of one face type, from a `<face_type>` block of the parameter file.
 * @ingroup core
 */
struct face_type_parameters{
    
    std::string name_;  ///< Name of the face type (`face_type_name`).

    /// Identifier of the face type written to the output meshes (`global_face_id`). Cells refer to their
    /// face types internally by local ids.
    short face_type_global_id_ = 0;

    double surface_tension_  = 0.;     ///< Surface tension, N/m (`surface_tension`).

    double adherence_strength_  = 0.;  ///< Adherence strength towards faces of adjacent cells, N/m^3 (`adherence_strength`).

    double repulsion_strength_;        ///< Repulsion strength towards faces of adjacent cells, N/m^3 (`repulsion_strength`). No default.

    double bending_modulus_ = 0.;      ///< Bending modulus, J (`bending_modulus`).

    /// Construct a face type with the default values.
    face_type_parameters() {}
};
//---------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------
/**
 * @brief Parameters of one cell type, from a `<cell_type>` block of the parameter file.
 *
 * The position of the block in the file is the `cell_type_id` used by the input mesh; `global_type_id_`
 * selects the cell class.
 * @ingroup core
 */
struct cell_type_parameters{

    std::string name_;  ///< Name of the cell type (`cell_type_name`).

    /// Cell class (`global_cell_id`): 0 epithelial, 1 ECM, 2 lumen, 3 nucleus, 4 static. Also written to the output meshes.
    short global_type_id_ = 0;

    double mass_density_ = 0.;  ///< Mass density, kg/m^3 (`cell_mass_density`).

    double bulk_modulus_ = 1.;  ///< Bulk modulus K of the cytoplasm, Pa (`cell_bulk_modulus`).

    double max_pressure_ = 0.;  ///< Upper bound of the cell pressure, Pa (`max_inner_pressure`; `inf` for no bound).

    /// Pressure at the start of the run, Pa. Sets the initial target volume to V exp(initial_pressure_ / K).
    /// Not read from the parameter file (always 0 there); settable from Python.
    double initial_pressure_ = 0.;

    double area_elasticity_modulus_ = 0.;  ///< Area elasticity modulus, J (`area_elasticity_modulus`).

    /// Mean and standard deviation of the normal distribution of division volumes, m^3
    /// (`avg_division_volume`, `std_division_volume`).
    double avg_division_vol_ = 0.;
    double std_division_vol_ = 0.;  ///< See avg_division_vol_.

    /// Mean and standard deviation of the normal distribution of growth rates of the target volume, m^3/s
    /// (`avg_growth_rate`, `std_growth_rate`).
    double avg_growth_rate_ = 0.;
    double std_growth_rate_ = 0.;  ///< See avg_growth_rate_.

    double min_vol_ = 0.;  ///< Volume below which a cell is removed, m^3 (`min_vol`).

    /// Strength of the force that drives the angles of the triangular faces towards 60 degrees
    /// (`angle_regularization_factor`); 0 disables it.
    double angle_regularization_factor_ = 0.;

    double target_isoperimetric_ratio_ = 0.;  ///< Target isoperimetric ratio A^3/V^2, dimensionless (`target_isoperimetric_ratio`).

    /// Curvature, 1/m, from which a node is no longer coupled to adjacent cells (`surface_coupling_max_curvature`).
    /// Used by contact models 1 and 2.
    double surface_coupling_max_curvature_ = 0.;

    std::vector<face_type_parameters> face_types_;  ///< Face types of the cell type, in file order.

    /// Named extra parameters. Settable from Python; not read from the parameter file.
    std::map<std::string, double> additional_parameters_;

    /// Append a face type to face_types_.
    void add_face_type(const face_type_parameters& face_type) noexcept {face_types_.push_back(face_type);}

    /// Store a named extra parameter in additional_parameters_.
    void add_additional_parameter(const std::string& parameter_name, const double parameter_value) noexcept {additional_parameters_[parameter_name] = parameter_value;}

    /// Construct a cell type with the default values.
    cell_type_parameters() {};
};
//---------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------
class cell; //Forward declaration of the cell class

/**
 * @brief Result of a simulation run through the Python module (simucell3d_wrapper).
 * @ingroup core
 */
struct simulation_outputs{
    
    bool RETURN_CODE_ = 0;  ///< 0 if the run completed, 1 if an exception stopped it.

    std::string error_message_ = "";  ///< Message of the exception that stopped the run; empty on success.

    std::string cell_statistics_ = "";  ///< Per-cell statistics in CSV format (same columns as `simulation_statistics.csv`).

    std::vector<std::shared_ptr<cell>> cell_lst_;  ///< The cells at the end of the run.

    /// Construct an empty result.
    simulation_outputs() {}
};
//---------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------
/**
 * @brief A point with a normal vector, used by the initial triangulation and the cell division.
 * @ingroup core
 */
struct oriented_point{
    unsigned id_;      ///< Identifier of the point.
    vec3 position_;    ///< Position, m.
    vec3 normal_;      ///< Normal vector at the point.

    /// True if the point was created by the Poisson sampling, false if it was already a node of the mesh.
    /// Used during cell division.
    bool created_by_poisson_sampling_ = true; 

    /// Construct a point from its position and normal.
    oriented_point(const vec3& point_pos, const vec3& point_normal) noexcept:
        position_(point_pos), normal_(point_normal){}

    /// Construct a point from its identifier, position and normal.
    oriented_point(const unsigned id, const vec3& point_pos, const vec3& point_normal) noexcept:
        id_(id), position_(point_pos), normal_(point_normal){}

    /// Construct a point at the origin with a zero normal.
    oriented_point():position_(vec3(0., 0., 0.)), normal_(vec3(0., 0., 0.)){}



};
//---------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------
/**
 * @brief Lightweight polygon mesh used to read cell meshes and to divide cells.
 *
 * The faces need not be triangles. Layout:
 * - `face_point_ids = {{f0_n0, f0_n1, f0_n2, f0_n3}, {f1_n0, f1_n1, f1_n2}, ...}`, where `f0_n1` is the
 *   id of the second node of the first face;
 * - `node_pos_lst = {x0, y0, z0, x1, y1, z1, ...}`.
 * @ingroup core
 */
struct mesh{

    std::vector<std::vector<unsigned>> face_point_ids;  ///< Node ids of each face.


    std::vector<double> node_pos_lst;  ///< Node coordinates, flattened as x, y, z per node, m.

    /// Return the number of nodes.
    unsigned get_nb_nodes() const {return node_pos_lst.size() / 3;}
    /// Return the number of faces.
    unsigned get_nb_faces() const {return face_point_ids.size();}


    /// Return the position of the node `node_id`, m.
    std::array<double, 3> get_node_pos(const unsigned node_id){
        assert(node_id < get_nb_nodes());

        return {
            node_pos_lst[node_id * 3    ],
            node_pos_lst[node_id * 3 + 1],
            node_pos_lst[node_id * 3 + 2]
        };
    } 
};
//---------------------------------------------------------------------------------------

#endif