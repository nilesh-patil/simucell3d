#ifndef DEF_GLOBAL_CONFIGURATION
#define DEF_GLOBAL_CONFIGURATION


/**
 * @file
 * @brief Compile-time configuration of the simulation model.
 *
 * The macros in this file select the contact model, the dynamic model and the polarization mode, and
 * switch optional per-face and output data on or off. Changing a value requires rebuilding SimuCell3D.
 * @ingroup core
 */


/**
 * @brief Store the adhesion and repulsion energies on each face (`true` or `false`).
 *
 * When true, every face holds two extra `float` members (8 bytes) for its contact energies.
 * @ingroup core
 */
#define FACE_STORE_CONTACT_ENERGY false


/**
 * @brief Write the face and node normals of the cell surfaces in the output meshes (`true` or `false`).
 *
 * Enabling it makes the output mesh files considerably larger.
 * @ingroup core
 */
#define WRITE_NORMALS_IN_OUTPUT_MESH_FILE true


/**
 * @brief Polarization mode: how the faces of the cells receive their face types.
 *
 * - 0: no polarization.
 * - 1: polarization based on the contacts of the cell (default).
 * - 2: polarization based on a discretization of the simulation space (automatic_polarizer); the
 *   regions are also written to the `region_data/` output folder.
 * @ingroup core
 */
#define POLARIZATION_MODE_INDEX 1

/**
 * @brief Contact model used to compute the forces between cells.
 *
 * - 0: springs between nodes and faces of adjacent cells (contact_node_face_via_spring).
 * - 1: mechanical coupling of pairs of adjacent nodes (contact_node_node_via_coupling).
 * - 2: mechanical coupling of pairs of adjacent faces (contact_face_face_via_coupling, default).
 * @ingroup core
 */
#define CONTACT_MODEL_INDEX 2


/**
 * @brief Dynamic model and time integration scheme.
 *
 * - 0: full equations of motion, semi-implicit Euler scheme (first order, default).
 * - 1: overdamped equations of motion, forward Euler scheme (first order).
 * @ingroup core
 */
#define DYNAMIC_MODEL_INDEX 0



#endif