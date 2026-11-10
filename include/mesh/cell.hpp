#ifndef DEF_CELL
#define DEF_CELL

/**
 * @file cell.hpp
 * @brief Base class of all cells: a closed triangulated surface and its mechanics.
 * @ingroup mesh
 */

#define _USE_MATH_DEFINES

#include <cassert>
#include <cmath>
#include <limits>
#include <vector>
#include <map>
#include <iostream>
#include <algorithm>
#include <type_traits>
#include <memory>
#include <list>
#include <set>
#include <numeric>
#include <chrono> 
#include <random>

#include "global_configuration.hpp"

class face;
class edge;
class node;


#include "custom_structures.hpp"
#include "custom_exception.hpp"
#include "node.hpp"
#include "edge.hpp"
#include "face.hpp"
#include "vec3.hpp"
#include "mat33.hpp"



/*
    The cell class is a mediator: it manages the relationships between the nodes, the edges and the faces
    and keeps the cell surface consistent, so that nodes and faces do not reference each other directly.
*/

//typedef std::unordered_set<edge, edge_hasher,  edge_comparator> edge_set;
typedef std::set<edge> edge_set;



typedef std::shared_ptr<cell_type_parameters> cell_type_param_ptr;

/**
 * @brief A cell: a closed, oriented, triangulated surface with its own mechanics.
 *
 * The cell owns its nodes and faces (stored in vectors whose free slots are reused) and the
 * set of its edges. It computes its geometry (area, volume, centroid, face normals, node
 * curvature) and, in apply_internal_forces(), the forces of its own membrane:
 * - pressure p = -K ln(V/V0), capped at the cell type's maximum pressure, applied along
 *   the face normals (K bulk modulus, V0 target volume);
 * - surface tension, from the surface tension of each face type;
 * - area elasticity with energy (ka/2)(A/A0 - 1)^2, where the target area
 *   A0 = (r V^2)^(1/3) follows from the target isoperimetric ratio r = A^3/V^2;
 * - bending, from the bending modulus of each face type;
 * - a regularization force that keeps triangle angles near 60 degrees.
 *
 * The target volume grows at the cell's growth rate each time step. Growth rate and division
 * volume are drawn once per cell from normal distributions (clipped to 3 standard
 * deviations) given by the cell type.
 *
 * Derived classes (epithelial_cell, ecm_cell, lumen_cell, nucleus_cell, static_cell) override
 * division, polarization and motion; see @ref cell_classes.
 *
 * @ingroup mesh
 */
class cell: public std::enable_shared_from_this<cell> {

    protected: 
        
        //Store all the nodes in an vector
        std::vector<node> node_lst_;

        //Store all the faces in an vector
        std::vector<face> face_lst_;

        //The centroid of the cell which is equal to the center of mass
        vec3 centroid_;

        double area_ = 0.;   //  Cell area
        double volume_ = 0.; //  Cell volume
        //Signed volume enclosed by the surface at the last volume update: positive when the faces are wound outward,
        //negative when the surface is inside out. NaN until the first update (the stability check then ignores it)
        double signed_volume_ = std::numeric_limits<double>::quiet_NaN();
        double pressure_ = 0.; //Pressure difference between the inside and outside of the cell
        double target_volume_ = 0.; //Target volume of the cell
        double target_area_ = 0.; //Target area of the cell

        //Each cell has a unique identifier, note that the cell_id might not be the position of the cell
        //in the cell_lst vector. 
        unsigned cell_id_;

        //The index of the cell in the cell_lst vector
        unsigned local_id_; 

        //Pointer to the structure that stores all the cell type parameters
        cell_type_param_ptr cell_type_; 

        //Stores in this vector the IDs of all the nodes or faces that are not used to represent the mesh of the cell
        std::vector<unsigned> free_node_queue_;
        std::vector<unsigned> free_face_queue_;

        //The different contributions to the potential energy of the cell
        //We use float values since they are only meant for plotting
        float surface_tension_energy_ = 0.;
        float membrane_elasticity_energy_ = 0.;
        float bending_energy_ = 0.;
        float pressure_energy_ = 0.;
        float kinetic_energy_ = 0.;

        //The growth rate and division volumes of the cells are drawn from a normal distribution
        double growth_rate_ = 0.;
        double division_volume_ = 0.;

        //If this flag is set to true, the cell will not move
        bool is_static_ = false; 
        
        //Store all the edges of the cell in this unordered set
        edge_set edge_set_; 

        //We use a lot of friend classes too directly acces the attributes of the cell class
        //and thus avoid the use of getters and setters which would slow down the code.
        friend class node;
        friend class time_integration_scheme;
        friend class dynamic_motion_semi_implicit_euler;       //The time integration scheme
        friend class overdamped_motion_forward_euler;             //The time integration scheme
        friend class overdamped_motion_improved_euler;             //The time integration scheme
        friend class contact_model_abstract;
        friend class contact_node_node_via_coupling;
        friend class contact_face_face_via_coupling;
        friend class contact_node_face_via_spring;
        friend class cell_divider;
        friend class automatic_polarizer;

        //The following classes are used for testing purposes
        friend class cell_tester;
        friend class node_tester;
        friend class local_mesh_refiner;
        friend class local_mesh_refiner_tester;
        friend class time_integration_test_helper;

 
        void add_free_node(const unsigned node_id) noexcept;
        void add_free_face(const unsigned face_id) noexcept;

        void update_target_volume(const double time_step) noexcept;

        //Apply the pressure on the nodes of the cell mesh
        void apply_pressure_on_surface() noexcept;

        //Apply at the same time the surface tension forces and the membrane elasticity forces
        void apply_surface_tension_and_membrane_elasticity() noexcept;

        void apply_bending_forces() noexcept;


        node& get_node(const unsigned node_id) noexcept{
            assert(node_id < node_lst_.size()); 
            assert(node_lst_[node_id].is_used());
            return node_lst_[node_id]; 
        }
        face& get_face(const unsigned face_id) noexcept{
            assert(face_id < face_lst_.size()); 
            assert(face_lst_[face_id].is_used());
            return face_lst_[face_id]; 
        }
        
        //Translate all the used nodes of the cell
        void translate(const vec3& translation) noexcept;
       
    public:

        //Make sure the cell cannot be default instantiated
        cell() = delete;                         //default constructor
        cell(const cell& c) = default;           //copy constructor
        cell(cell&& c) = delete;                 //move constructor
        cell& operator=(const cell& c) = delete; //copy assignment operator
        cell& operator=(cell&& c) = delete;      //move assignment operator 


        
        /**
         * @brief Create a cell from copies of its nodes and faces.
         *
         * Call initialize_cell_properties() before using the cell.
         * @param cell_id Unique id of the cell.
         * @param cell_type_parameters Cell type; may be null for a bare mesh.
         */
        cell(
            const std::vector<node>& node_lst,
            const std::vector<face>& face_lst,
            unsigned cell_id, 
            cell_type_param_ptr cell_type_parameters = nullptr
        ) noexcept;



        /**
         * @brief Create a cell from flat node coordinates and face node ids.
         *
         * Call initialize_cell_properties() before using the cell.
         * @param node_position Node coordinates [x0, y0, z0, x1, ...], m.
         * @param face_node_ids Node ids of the faces, three per face [f0_n0, f0_n1, f0_n2, f1_n0, ...].
         * @param cell_id Unique id of the cell.
         * @param cell_type_parameters Cell type; may be null for a bare mesh.
         */
        cell(
            const std::vector<double>&  node_position,
            const std::vector<unsigned>& face_node_ids,
            unsigned cell_id,
            cell_type_param_ptr cell_type_parameters = nullptr
        ) noexcept;


        /**
         * @brief Create a cell from a triangulated mesh (every face must have three nodes).
         *
         * Call initialize_cell_properties() before using the cell.
         */
        cell(
            const mesh& m,
            unsigned cell_id,
            cell_type_param_ptr cell_type_parameters = nullptr
        ) noexcept;


        /** @brief Set the index of the cell in the solver's cell list. */
        void set_local_id(const unsigned local_id) noexcept {local_id_ = local_id;}
        /** @brief Return the index of the cell in the solver's cell list (not the cell id). */
        unsigned get_local_id() const noexcept {return local_id_;}


        /** @brief Return the number of nodes of the mesh (free slots excluded). */
        size_t get_nb_of_nodes() const noexcept {return node_lst_.size() - free_node_queue_.size();}
        /** @brief Return the number of faces of the mesh (free slots excluded). */
        size_t get_nb_of_faces() const noexcept {return face_lst_.size() - free_face_queue_.size();}

        /** @brief Remove all nodes, faces and edges and reset area, volume and target volume. */
        void clear_data() noexcept;

        /**
         * @brief Update the pressure p = -K ln(V/V0), Pa, capped at the cell type's maximum pressure.
         *
         * With a bulk modulus of 0 the pressure is 0. Also updates the pressure energy.
         */
        void update_pressure() noexcept;

        /** @brief Return the face whose nodes are the three given nodes, or a null pointer if there is none. */
        face* get_face(const unsigned n1_id, const unsigned n2_id, const unsigned n3_id) noexcept;

        /**
         * @brief Prepare a newly created cell for the simulation.
         *
         * Sets the local ids of nodes and faces, frees unused nodes and, if @p check_cell_integrity
         * is true, builds the edge set, checks that the surface is a closed manifold and orients
         * the faces outward. Then computes face normals and areas, the cell area and volume, sets
         * the target area and target volume to the current values and, if the cell has a cell
         * type, draws its growth rate and division volume.
         * @throws mesh_integrity_exception If an edge is shared by more than two faces.
         * @throws initial_triangulation_exception If the surface is not a closed manifold.
         */
        void initialize_cell_properties(bool check_cell_integrity = true) noexcept(false);

        /**
         * @brief Draw the growth rate (m^3/s) and division volume (m^3) of the cell.
         *
         * Each is drawn from a normal distribution with the mean and standard deviation of the
         * cell type and clipped to the mean plus or minus 3 standard deviations; with a standard
         * deviation of 0 (or an infinite mean division volume) the mean is used.
         */
        void initialize_random_properties() noexcept;

        /**
         * @brief Update the geometry of the cell and add its membrane forces to its nodes.
         *
         * In order: face normals and areas, cell area and volume, target volume
         * (grows by @p time_step times the growth rate, not below the minimum volume), pressure,
         * pressure forces, surface tension and area elasticity forces, bending forces, angle
         * regularization and, with CONTACT_MODEL_INDEX 1 or 2, node curvatures and normals.
         * @param time_step Time step, s.
         * @note Overridden by ecm_cell (no forces).
         */
        virtual void apply_internal_forces(const double time_step) noexcept;

        /** @brief Mark as free the nodes that no face uses. */
        void remove_unused_nodes() noexcept;

        /** @brief Return true if the volume is below the minimum volume of the cell type; the solver then removes the cell. */
        bool is_below_min_vol() const noexcept{assert(cell_type_ != nullptr); return volume_ < cell_type_->min_vol_;}
        /** @brief Return true if the cell does not move (ECM and static cells). */
        bool is_static() const noexcept {return is_static_;}

        /** @brief Result of check_numerical_stability(): whether, and why, the mesh of a cell has diverged. */
        struct stability_report{
            bool unstable = false;           ///< True if any of the conditions below holds.
            double extent = 0.;              ///< Largest side of the cell's axis-aligned bounding box (AABB), m.
            double max_extent = 0.;          ///< Extent above which the cell is unstable, m.
            unsigned nb_non_finite_nodes = 0;///< Number of nodes with a NaN or infinite coordinate.
            double signed_volume = 0.;       ///< Signed volume at the last volume update, m^3.
            double aabb_volume = 0.;         ///< Volume of the AABB, m^3.
            bool negative_volume = false;    ///< signed_volume <= 0: the surface is inside out.
            bool volume_exceeds_aabb = false;///< |signed_volume| > aabb_volume * (1 + tolerance): the surface intersects itself.
        };

        /**
         * @brief Check whether the mesh of the cell has diverged.
         *
         * A cell is unstable if a node has a non-finite coordinate, or if its AABB is wider than
         * @p max_extent_ratio times the cube root of its target volume (a healthy cell of target
         * volume V0 spans about 1.2 V0^(1/3)). A non-static cell is also unstable if its
         * surface is inside out (signed volume <= 0) or intersects itself (|signed volume| larger
         * than the AABB volume times 1 + @p aabb_volume_tolerance; a sphere fills pi/6 of its AABB).
         * The signed volume is the one of the last volume update. The solver removes unstable
         * cells (a 2.1 safeguard).
         * @param max_extent_ratio Maximum AABB extent in units of V0^(1/3), dimensionless.
         * @param aabb_volume_tolerance Relative tolerance of the AABB volume test, dimensionless.
         */
        stability_report check_numerical_stability(const double max_extent_ratio = 10., const double aabb_volume_tolerance = 1e-3) const noexcept;

        /** @brief Add @p force (N) to every used node of the cell. */
        void add_force(const vec3& force) noexcept{
            std::for_each(node_lst_.begin(), node_lst_.end(), [&force](node& n){
                if(n.is_used()) n.add_force(force);
            });
        }

        /**
         * @brief Make the winding of face @p f consistent with the adjacent face @p ref_face.
         *
         * The two faces must share an edge. If they traverse it in the same direction, the first and
         * third nodes of @p f are swapped.
         */
        void check_face_winding_order(const face& ref_face, face& f) noexcept;

        /** @brief Set the unique id of the cell. */
        void set_id(const unsigned cell_id) noexcept {cell_id_ = cell_id;}
        /** @brief Return the unique id of the cell (written to the output files). */
        unsigned get_id() const noexcept {return cell_id_;};

        /** @brief Return the parameters of the cell type (may be null). */
        cell_type_param_ptr get_cell_type() const noexcept {return cell_type_;}
        /** @brief Return the `global_cell_id` of the cell type (the cell class, 0-4). */
        unsigned short get_cell_type_id() const noexcept {assert(cell_type_ != nullptr); return cell_type_->global_type_id_;}

        /**
         * @brief Build the edge set from the faces.
         * @throws mesh_integrity_exception If an edge is shared by more than two faces.
         */
        void generate_edge_set() noexcept(false);

        /** @brief Return a copy of the edge between two nodes, or an empty optional if there is none. */
        std::optional<edge> get_edge(const unsigned n1_id, const unsigned n2_id) const noexcept;

        /** @brief Return the node with local id @p node_id. */
        const node& get_const_ref_node(const unsigned node_id) const noexcept;
        /** @brief Return the face with local id @p face_id. */
        const face& get_const_ref_face(const unsigned face_id) const noexcept;

        /** @brief Return the mass of the cell, kg: mass density times volume. */
        double get_mass() const noexcept {
            assert(cell_type_ != nullptr);
            return cell_type_->mass_density_ * volume_;
        }

        /** @brief Return the mass of one node, kg: the cell mass divided by the number of nodes. */
        double get_node_mass() const noexcept{
            assert(get_nb_of_nodes() > 0);
            return get_mass() / static_cast<double>(get_nb_of_nodes());
        }

        /** @brief Return the parameters of the face type of face @p face_id. */
        const face_type_parameters& get_face_type(const unsigned face_id) const noexcept{
            assert(face_id < face_lst_.size());
            assert(cell_type_ != nullptr);
            assert(face_lst_[face_id].type_id_ < cell_type_->face_types_.size());
            return cell_type_->face_types_[face_lst_[face_id].type_id_];
        }


        /** @brief Set the target volume, m^3 (used at initialization and after division). */
        void set_target_volume(const double target_volume) noexcept {target_volume_ = target_volume;}

        /**
         * @brief Remove a face from the mesh and from its edges; its slot is marked free for reuse.
         * @throws mesh_integrity_exception If an edge of the face is inconsistent with it.
         */
        void delete_face(const unsigned face_id) noexcept(false);
        /** @copydoc delete_face(const unsigned) */
        void delete_face(face& f) noexcept(false);

        /** @brief Mark a node as free for reuse. Delete the faces that use it first. */
        void delete_node(const unsigned node_id) noexcept;
        /** @copydoc delete_node(const unsigned) */
        void delete_node(node& n) noexcept;

        /** @brief Add a node at @p pos (m), reusing a free slot if there is one, and return its local id. */
        unsigned create_node(const vec3& pos) noexcept;
        /** @brief Add a node at (@p x, @p y, @p z), m, and return its local id. */
        unsigned create_node(const double x, const double y, const double z) noexcept;
        /** @brief Add a copy of @p n, reusing a free slot if there is one, and return its local id. */
        unsigned add_node(const node& n) noexcept;

        /**
         * @brief Replace node @p old_node_id by @p new_node_id in all faces and edges around it, then delete the old node.
         * @param start_edge An edge of the old node from which the faces around it are visited.
         * @return The deleted edges and the created edges.
         */
        std::pair<std::vector<edge>, std::vector<edge>> replace_node(const edge& start_edge, const unsigned old_node_id, const unsigned new_node_id) noexcept;

        /**
         * @brief Add a copy of @p f, reusing a free slot if there is one; update the edges, normal and area.
         * @return The local id of the new face.
         * @throws mesh_integrity_exception If one of its edges would then have more than two faces.
         */
        unsigned add_face(const face& f) noexcept(false);

        /**
         * @brief Add the face with the given nodes (in winding order) and return its local id.
         * @throws mesh_integrity_exception If one of its edges would then have more than two faces.
         */
        unsigned create_face(
            const unsigned n1_id, 
            const unsigned n2_id, 
            const unsigned n3_id  
        ) noexcept(false);

        /** @brief Return the edges of the cell. */
        const edge_set& get_edge_set() const noexcept {return edge_set_;}

        /** @brief Return the node list, including free slots (check node::is_used()). */
        const std::vector<node>& get_node_lst() const noexcept {return node_lst_;};
        /** @brief Return the face list, including free slots (check face::is_used()). */
        const std::vector<face>& get_face_lst() const noexcept {return face_lst_;};

        /**
         * @brief Remove the free slots from the node and face lists, renumber them and rebuild the edge set.
         *
         * Expensive; called before a cell is written or divided.
         * @throws mesh_integrity_exception If the rebuilt edge set is not consistent.
         */
        void rebase() noexcept(false);


        /** @brief Return the cell surface as a mesh (node coordinates and face node ids, free slots included). */
        mesh get_mesh() const noexcept;

        /** @brief Return the node ids of every face, free slots included, as lists of three ids. */
        std::vector<std::vector<unsigned>> get_face_point_ids() const noexcept{
            std::vector<std::vector<unsigned>> face_point_ids;
            std::for_each(face_lst_.begin(), face_lst_.end(), [&face_point_ids](const face& f){
                auto [n1, n2, n3] = f.get_node_ids();
                face_point_ids.push_back({n1, n2, n3});
            });
            return face_point_ids;
        }



        /** @brief Return the node coordinates [x0, y0, z0, x1, ...], m, free slots included. */
        std::vector<double> get_flat_node_coord_lst() const noexcept;

        /** @brief Return the node coordinates as [x, y, z] lists, m, free slots included. */
        std::vector<std::vector<double>> get_node_coord_lst() const noexcept{
            std::vector<std::vector<double>> node_coord_lst;
            std::for_each(node_lst_.begin(), node_lst_.end(), [&node_coord_lst](const node& n){
                const vec3& node_pos = n.pos();
                node_coord_lst.push_back({node_pos.dx(), node_pos.dy(), node_pos.dz()});
            });
            return node_coord_lst;
        }

        /** @brief Orient all faces consistently, with their normals pointing outward. */
        void check_face_normal_orientation() noexcept;

        /** @brief Return the stored unit normal of face @p face_id (updated by update_all_face_normals_and_areas()). */
        vec3 get_face_normal(const unsigned face_id) const noexcept;

        /**
         * @brief Return the gradient of the angle at node @p i between the edges i-j and i-k.
         * @return Gradients with respect to the positions of @p i, @p j and @p k, 1/m.
         */
        static std::array<vec3, 3> get_angle_gradient(const vec3& i, const vec3& j, const vec3& k) noexcept;

        /** @brief Add the angle regularization forces of all faces (no effect if the regularization factor is 0). */
        void regularize_all_face_angles() noexcept;
        /** @brief Add forces that push the angles of face @p f towards 60 degrees. */
        void regularize_face_angles(const face& f) noexcept;

        /** @brief Recompute the unit normal and the area of every used face. */
        void update_all_face_normals_and_areas() noexcept;

        /** @brief Recompute the unit normal and the area (m^2) of one face. */
        void update_face_normal_and_area(const unsigned face_id) noexcept;
        /** @copydoc update_face_normal_and_area(const unsigned) */
        void update_face_normal_and_area(face& f) noexcept;

        /** @brief Recompute and store the centroid. */
        void update_centroid() noexcept{centroid_ = compute_centroid();}

        /** @brief Return the volume enclosed by the surface, m^3. */
        double compute_volume() const noexcept;

        /** @brief Return the signed volume enclosed by the surface, m^3: positive if the faces are wound outward. */
        double compute_signed_volume() const noexcept;

        /** @brief Return the surface area, m^2 (sum of the stored face areas). */
        double compute_area() const noexcept;

        /** @brief Return the centroid of the surface, m. */
        vec3 compute_centroid() const noexcept;

        /** @brief Return true if every edge has two faces and the mesh satisfies V - E + F = 2 (closed, genus 0). */
        bool is_manifold() const noexcept;

        /** @brief Return true if @p point lies on the side of face @p face_id that its normal points to. */
        bool point_is_on_positive_side_of_face(const unsigned face_id, const vec3& point) const noexcept;

        /** @brief Return the axis-aligned bounding box of the used nodes as {min_x, min_y, min_z, max_x, max_y, max_z}, m. */
        std::array<double, 6> get_aabb() const noexcept;

        /**
         * @brief Return a new cell of the same class, built from @p m, with the same id and cell type.
         *
         * Used by the cell divider to create the daughters. Every cell class overrides it.
         * @throws std::runtime_error If called on the base class.
         */
        virtual cell_ptr get_cell_same_type(const mesh& m) noexcept(false){
            throw std::runtime_error("Cannot call get_cell_same_type on the base class cell");
        };


        /** @brief Set this cell as the owner of all its faces. The cell must be held by a cell_ptr. */
        void set_face_owner_cell() noexcept{
            std::for_each(face_lst_.begin(), face_lst_.end(), [this](auto& f){f.owner_cell_ = shared_from_this();});
        }

        /** @brief Set the local id of every node and face to its index in its list. */
        void set_local_ids() noexcept{
            for(unsigned i = 0; i < face_lst_.size(); ++i){face_lst_[i].local_face_id_ = i;}
            for(unsigned i = 0; i < node_lst_.size(); ++i){node_lst_[i].node_id_ = i;}
        }

        /** @brief Return the nodes joined to node @p node_id by an edge, visiting its faces from @p e_start. */
        std::vector<unsigned> get_connected_nodes(const unsigned node_id, const edge& e_start) const noexcept;

        /**
         * @brief Return true if the cell should divide now. The base class never divides.
         * @note Overridden by epithelial_cell (volume >= division volume). Checked every 5 iterations.
         */
        virtual bool is_ready_to_divide() const noexcept {return false;}

        /** @brief Return the unit normal of the division plane; by default the longest axis of the cell. */
        virtual vec3 get_cell_division_axis() const noexcept {return get_cell_longest_axis();}

        /**
         * @brief Return the longest axis of the cell as a unit vector.
         *
         * The eigenvector with the largest eigenvalue of the covariance matrix of the node positions.
         */
        vec3 get_cell_longest_axis() const noexcept;

        /** @brief Called when face @p face_local_id touches @p other_face of @p other_cell. No effect in the base class. */
        virtual void face_is_in_contact(const unsigned face_local_id, const cell_ptr other_cell, const face& other_face) noexcept {}
        /**
         * @brief Called by contact model 0, with POLARIZATION_MODE_INDEX 1, when face @p face_local_id touches @p other_cell.
         * No effect in the base class; epithelial_cell sets the face type.
         */
        virtual void face_is_in_contact(const unsigned face_local_id, const cell_ptr other_cell) noexcept {}

    

        /** @brief Called by the solver at the start of each iteration to reset face types. No effect in the base class. */
        virtual void update_face_types() noexcept {}
  
        /** @brief Return the fraction of the surface in contact with other cells (output array). 0 in the base class. */
        virtual double get_contact_area_fraction() {return 0.;}

        /** @brief Return the centroid stored by update_centroid(), m. */
        const vec3& get_centroid() const noexcept {return centroid_;}
        /** @brief Return the division volume drawn for this cell, m^3. */
        double get_division_volume() const noexcept {return division_volume_;}
        /** @brief Return the growth rate of the target volume, m^3/s. */
        double get_growth_rate() const noexcept {return growth_rate_;}
        /** @brief Set the growth rate of the target volume, m^3/s. */
        void set_growth_rate(const double growth_rate) noexcept {growth_rate_ = growth_rate;}
        /** @brief Return the surface area at the last update, m^2. */
        double get_area() const noexcept {return area_;}
        /** @brief Return the volume at the last update, m^3. */
        double get_volume() const noexcept {return volume_;}
        /** @brief Return the signed volume at the last update, m^3 (NaN before the first update). */
        double get_signed_volume() const noexcept {return signed_volume_;}
        /** @brief Return the pressure at the last update, Pa. */
        double get_pressure() const noexcept {return pressure_;}
        /** @brief Return the target volume V0, m^3. */
        double get_target_volume() const noexcept {return target_volume_;}
        /** @brief Return the surface tension energy, J. */
        float get_surface_tension_energy() const noexcept {return surface_tension_energy_;}
        /** @brief Return the area elasticity energy (ka/2)(A/A0 - 1)^2, J. */
        float get_membrane_elasticity_energy() const noexcept {return membrane_elasticity_energy_;}
        /** @brief Return the bending energy, J. */
        float get_bending_energy() const noexcept {return bending_energy_;}
        /** @brief Return the pressure energy |K V (ln(V/V0) - 1)|, J. */
        float get_pressure_energy() const noexcept {return pressure_energy_;}
        /** @brief Return the kinetic energy of the nodes, J. */
        float get_kinetic_energy() const noexcept {return kinetic_energy_;}


        /**
         * @brief Called by the solver after contact detection to update face types from node couplings.
         * No effect in the base class; see epithelial_cell.
         */
        virtual void special_polarization_update(const std::vector<cell_ptr>& cell_lst) noexcept{};


        #if CONTACT_MODEL_INDEX == 1  || CONTACT_MODEL_INDEX == 2
            /**
             * @brief Compute the curvature and unit normal of every node with the cotangent Laplacian.
             * @note Only with CONTACT_MODEL_INDEX 1 or 2.
             */
            void compute_node_curvature_and_normals() noexcept;
        #endif


        /** @name Contact energies
         *  Sums of the adhesion and repulsion energies of the faces, J.
         *  @note Present only when FACE_STORE_CONTACT_ENERGY is true. */
        ///@{
        #if FACE_STORE_CONTACT_ENERGY
            float get_adhesion_energy() const noexcept {
                return std::accumulate(face_lst_.begin(), face_lst_.end(), 0., [](auto acc, const auto& f){
                    return acc + f.get_adhesion_energy();
                });
            }

            float get_repulsion_energy() const noexcept {
                return std::accumulate(face_lst_.begin(), face_lst_.end(), 0., [](auto acc, const auto& f){
                    return acc + f.get_repulsion_energy();
                });
            } 
        #endif
        ///@}




};

#endif