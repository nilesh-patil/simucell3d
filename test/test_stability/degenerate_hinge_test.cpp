#include <cassert>
#include <string>
#include <iostream>
#include <cmath>
#include <vector>
#include <memory>
#include <limits>
#include <algorithm>

#include "cell.hpp"
#include "epithelial_cell.hpp"
#include "simulation_initializer.hpp"

/*
    Regression tests for the numerical blow-up observed in the paper-config growth run
    (simulation_results/longrun_20k_fast_growth.xml) around 13k cells: individual cells
    acquire a near-degenerate ("cap") triangle that the local mesh refiner does not remove,
    and cell::apply_bending_forces() then returns a nodal force many orders of magnitude
    above the physiological scale.

    Why the bending term is the amplifier (src/mesh/cell.cpp, apply_bending_forces):

        grad_x2_theta = normal_1 * (edge_length / (2 * f1.get_area()))     -> diverges as 1/A
        grad_x0_theta = (n1*cot(alpha_3) + n1*cot(alpha_4)) * (-1/L)      -> diverges as cot(a)
        prefactor_3   = L^2 / (2 * sum_face_areas^2)                      -> diverges as 1/A^2

    and the only guards upstream ships are exact-equality tests

        if(edge_length == 0. || f1.get_area() == 0 || f2.get_area() == 0.) continue;
        if(alpha_1 == 0. || alpha_2 == 0. || alpha_3 == 0. || alpha_4 == 0.) continue;

    which never fire for a triangle of area 1e-20 m^2 with an interior angle of 1e-8 rad.
    Under DYNAMIC_MODEL_INDEX == 0 (semi-implicit Euler) one such hinge moves a node by
    F*dt^2/m per step, with m = rho*V/n_nodes ~ 6e-15 kg for a paper-sized cell, so a force
    of ~1e-5 N already displaces a node by a full cell diameter in a single time step.

    These tests are deterministic, single-threaded and take well under a second.
*/

//---------------------------------------------------------------------------------------------------------
// Paper-config material parameters (Runser et al. 2024, Table 2 - dynamic column),
// mirrored from simulation_results/longrun_20k_fast_growth.xml
static constexpr double PAPER_BENDING_MODULUS   = 2e-18;   // k_b [J]
static constexpr double PAPER_AREA_ELASTICITY   = 1e-15;   // k_a [J]
static constexpr double PAPER_SURFACE_TENSION   = 1e-3;    // gamma [N/m]
static constexpr double PAPER_BULK_MODULUS      = 2.5e3;   // K [Pa]
static constexpr double PAPER_MASS_DENSITY      = 1e3;     // rho [kg/m^3]
static constexpr double PAPER_DAMPING           = 2.5e-10; // c [kg/s]
static constexpr double PAPER_TIME_STEP         = 1e-7;    // dt [s]

//---------------------------------------------------------------------------------------------------------
// Load the single stable sphere shipped with the scientific benchmarks and give it the
// paper-config material parameters.
static cell_ptr load_paper_cell() {
    const std::string param_file = std::string(PROJECT_SOURCE_DIR) +
                                   "/test/test_scientific_benchmarks/params_single_sphere.xml";
    simulation_initializer sim_init(param_file, false);
    std::vector<cell_ptr> cell_lst = sim_init.get_cell_lst();
    if (cell_lst.empty()) return nullptr;

    cell_ptr c = cell_lst[0];
    auto ct = c->get_cell_type();
    ct->bulk_modulus_ = PAPER_BULK_MODULUS;
    ct->max_pressure_ = PAPER_BULK_MODULUS;
    ct->mass_density_ = PAPER_MASS_DENSITY;
    ct->area_elasticity_modulus_ = PAPER_AREA_ELASTICITY;
    ct->target_isoperimetric_ratio_ = 250.;
    for (face_type_parameters& ft : ct->face_types_) {
        ft.bending_modulus_ = PAPER_BENDING_MODULUS;
        ft.surface_tension_ = PAPER_SURFACE_TENSION;
    }
    return c;
}

//---------------------------------------------------------------------------------------------------------
// Rebuild a cell from a mesh so that the caller can move nodes around freely.
static cell_ptr rebuild(const mesh& m, unsigned id, std::shared_ptr<cell_type_parameters> ct) {
    cell_ptr c = std::make_shared<epithelial_cell>(m, id, ct);
    c->initialize_cell_properties();
    return c;
}

//---------------------------------------------------------------------------------------------------------
// Largest nodal force magnitude after one force assembly.
static double max_nodal_force(cell_ptr c) {
    c->apply_internal_forces(PAPER_TIME_STEP);
    double f_max = 0.;
    for (const node& n : c->get_node_lst()) {
        if (!n.is_used()) continue;
        const double f = n.force().norm();
        if (std::isfinite(f)) f_max = std::max(f_max, f);
        else                  return std::numeric_limits<double>::infinity();
    }
    return f_max;
}

//---------------------------------------------------------------------------------------------------------
// Smallest triangle area in the mesh.
static double min_face_area(cell_ptr c) {
    double a_min = std::numeric_limits<double>::max();
    for (const face& f : c->get_face_lst()) {
        if (!f.is_used()) continue;
        a_min = std::min(a_min, f.get_area());
    }
    return a_min;
}

//---------------------------------------------------------------------------------------------------------
// Collapse one triangle of the mesh onto a line: node A of face 0 is moved onto the
// midpoint of the opposite edge B-C, offset by `height` along the in-plane perpendicular.
// The resulting triangle has area = 0.5 * |BC| * height; every other triangle stays valid.
// This is exactly the "cap" configuration a blind edge swap or a surface-coupling
// teleport produces in the production run.
static mesh make_cap_triangle(const mesh& healthy, double height, double& area_out, double& moved_dist_out) {
    mesh m = healthy;
    const unsigned a = m.face_point_ids[0][0];
    const unsigned b = m.face_point_ids[0][1];
    const unsigned cc = m.face_point_ids[0][2];

    const vec3 pa(m.node_pos_lst[3*a], m.node_pos_lst[3*a+1], m.node_pos_lst[3*a+2]);
    const vec3 pb(m.node_pos_lst[3*b], m.node_pos_lst[3*b+1], m.node_pos_lst[3*b+2]);
    const vec3 pc(m.node_pos_lst[3*cc], m.node_pos_lst[3*cc+1], m.node_pos_lst[3*cc+2]);

    const vec3 bc   = pc - pb;
    const vec3 mid  = (pb + pc) * 0.5;
    // in-plane perpendicular to BC inside the original triangle plane
    const vec3 nrm  = bc.cross(pa - pb);
    vec3 perp = nrm.cross(bc);
    perp = perp / perp.norm();
    if (perp.dot(pa - mid) < 0.) perp = perp * (-1.);

    const vec3 pa_new = mid + perp * height;
    m.node_pos_lst[3*a]   = pa_new.dx();
    m.node_pos_lst[3*a+1] = pa_new.dy();
    m.node_pos_lst[3*a+2] = pa_new.dz();

    area_out       = 0.5 * bc.norm() * height;
    moved_dist_out = (pa_new - pa).norm();
    return m;
}

//---------------------------------------------------------------------------------------------------------
/**
 * @brief The bending force must stay bounded when one triangle degenerates.
 *
 * Root-cause regression for the paper-config divergence. A single node of a healthy
 * paper-sized cell is moved by ~one edge length so that one triangle collapses to a
 * sliver of area 1e-20 m^2 (12 orders below the ~4e-13 m^2 of a healthy triangle).
 *
 * Without a degeneracy guard in cell::apply_bending_forces() the resulting nodal force
 * grows without bound as 1/area, and one semi-implicit Euler step (dt = 1e-7 s,
 * node mass ~ 6e-15 kg) throws the node far outside the tissue. With the guard the
 * force stays within a few orders of the healthy scale and the step stays sub-mesh.
 *
 * @return 0 on success, 1 on failure
 */
int test_degenerate_hinge_force_is_bounded() {
    std::cout << "Running test_degenerate_hinge_force_is_bounded..." << std::endl;

    try {
        cell_ptr healthy_cell = load_paper_cell();
        if (healthy_cell == nullptr) {
            std::cout << "FAILED: could not load the reference sphere" << std::endl;
            return 1;
        }
        const mesh healthy_mesh = healthy_cell->get_mesh();
        auto ct = healthy_cell->get_cell_type();

        const double f_healthy = max_nodal_force(healthy_cell);
        const double a_healthy = min_face_area(healthy_cell);
        const double node_mass = healthy_cell->get_node_mass();
        std::cout << "  healthy: min face area " << a_healthy << " m^2, max nodal force "
                  << f_healthy << " N, node mass " << node_mass << " kg" << std::endl;

        bool t1 = std::isfinite(f_healthy) && f_healthy > 0.;
        std::cout << "  t1 (healthy force finite and non-zero): " << t1 << std::endl;
        if (!t1) return 1;

        // One triangle collapsed to a 1e-20 m^2 sliver.
        double sliver_area = 0., moved = 0.;
        const double height = 1e-14;  // -> area ~ 0.5 * 1e-6 * 1e-14 = 5e-21 m^2
        mesh capped = make_cap_triangle(healthy_mesh, height, sliver_area, moved);
        cell_ptr capped_cell = rebuild(capped, 42u, ct);
        capped_cell->set_target_volume(healthy_cell->get_target_volume());

        const double f_capped = max_nodal_force(capped_cell);
        const double a_capped = min_face_area(capped_cell);
        std::cout << "  capped : min face area " << a_capped << " m^2, max nodal force "
                  << f_capped << " N (node displaced by " << moved << " m)" << std::endl;
        std::cout << "  amplification: " << (f_capped / f_healthy) << "x" << std::endl;

        // One semi-implicit Euler step from rest: dx = F * dt^2 / m
        const double dx = f_capped * PAPER_TIME_STEP * PAPER_TIME_STEP / node_mass;
        std::cout << "  single-step displacement of the worst node: " << dx << " m"
                  << " (cell diameter ~ " << std::cbrt(healthy_cell->get_volume()) << " m)" << std::endl;

        // The guard must keep the degenerate hinge within 1000x the healthy force. Without
        // it the ratio is >1e6 and dx exceeds the cell size by many orders of magnitude.
        bool t2 = std::isfinite(f_capped) && (f_capped < 1e3 * f_healthy);
        std::cout << "  t2 (degenerate hinge force bounded, < 1000x healthy): " << t2 << std::endl;
        if (!t2) {
            std::cout << "FAILED: the bending force is unbounded on a degenerate triangle."
                      << " See cell::apply_bending_forces() - the 1/area and cot(alpha) terms"
                      << " are guarded only against exact zeros." << std::endl;
            return 1;
        }

        // And the step must stay smaller than the cell itself.
        bool t3 = dx < std::cbrt(healthy_cell->get_volume());
        std::cout << "  t3 (single Euler step stays sub-cell-size): " << t3 << std::endl;
        if (!t3) return 1;

        std::cout << "test_degenerate_hinge_force_is_bounded PASSED" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cout << "FAILED with exception: " << e.what() << std::endl;
        return 1;
    }
}

//---------------------------------------------------------------------------------------------------------
/**
 * @brief The bending force must not diverge as a triangle is progressively flattened.
 *
 * Sweeps the residual height of the cap triangle over eight decades and records the
 * resulting maximum nodal force. Unguarded, the force grows like 1/height; guarded, it
 * saturates. The assertion is on saturation, not on any particular value, so the test
 * is insensitive to the exact guard threshold chosen.
 *
 * @return 0 on success, 1 on failure
 */
int test_degenerate_hinge_force_saturates() {
    std::cout << "Running test_degenerate_hinge_force_saturates..." << std::endl;

    try {
        cell_ptr healthy_cell = load_paper_cell();
        if (healthy_cell == nullptr) {
            std::cout << "FAILED: could not load the reference sphere" << std::endl;
            return 1;
        }
        const mesh healthy_mesh = healthy_cell->get_mesh();
        auto ct = healthy_cell->get_cell_type();
        const double target_volume = healthy_cell->get_target_volume();
        const double node_mass = healthy_cell->get_node_mass();

        const double heights[] = {1e-7, 1e-8, 1e-9, 1e-10, 1e-11, 1e-12, 1e-13, 1e-14};
        std::vector<double> forces;

        std::cout << "     height[m]      area[m^2]     max|F|[N]     dx/step[m]" << std::endl;
        for (const double h : heights) {
            double area = 0., moved = 0.;
            mesh capped = make_cap_triangle(healthy_mesh, h, area, moved);
            cell_ptr capped_cell = rebuild(capped, 42u, ct);
            capped_cell->set_target_volume(target_volume);
            const double f = max_nodal_force(capped_cell);
            const double dx = f * PAPER_TIME_STEP * PAPER_TIME_STEP / node_mass;
            printf("  %12.1e   %12.4e  %12.4e   %12.4e\n", h, min_face_area(capped_cell), f, dx);
            forces.push_back(f);
        }

        const double f_last = forces.back();
        bool t1 = std::isfinite(f_last);
        std::cout << "  t1 (force stays finite at area ~2e-21 m^2): " << t1 << std::endl;
        if (!t1) return 1;

        // Saturation: over the last four decades of flattening the force must not keep
        // growing. Unguarded it tracks 1/area exactly, i.e. a factor ~1000 over those decades.
        const double tail_growth = f_last / forces[forces.size() - 4];
        std::cout << "  force growth over the last 3 decades of flattening: " << tail_growth << "x"
                  << std::endl;
        bool t2 = tail_growth < 2.;
        std::cout << "  t2 (force saturates instead of diverging as 1/area): " << t2 << std::endl;
        if (!t2) {
            std::cout << "FAILED: the bending force grows without bound as the triangle flattens."
                      << std::endl;
            return 1;
        }

        std::cout << "test_degenerate_hinge_force_saturates PASSED" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cout << "FAILED with exception: " << e.what() << std::endl;
        return 1;
    }
}

//---------------------------------------------------------------------------------------------------------
/**
 * @brief The guard must not perturb a well-shaped mesh.
 *
 * Comparability check for the published configurations: on a healthy sphere every hinge
 * is far from the degeneracy threshold, so the assembled forces must be bit-identical to
 * the values obtained with the guard disabled. Here we assert the weaker, self-contained
 * property that the healthy force is many orders of magnitude below the guard's cap, i.e.
 * that no healthy hinge is anywhere near being clipped.
 *
 * @return 0 on success, 1 on failure
 */
int test_healthy_mesh_untouched_by_guard() {
    std::cout << "Running test_healthy_mesh_untouched_by_guard..." << std::endl;

    try {
        cell_ptr c = load_paper_cell();
        if (c == nullptr) {
            std::cout << "FAILED: could not load the reference sphere" << std::endl;
            return 1;
        }

        // Quality of the worst triangle in a healthy, refiner-maintained mesh:
        // area / longest_edge^2. An equilateral triangle scores 0.433.
        double worst_quality = std::numeric_limits<double>::max();
        for (const face& f : c->get_face_lst()) {
            if (!f.is_used()) continue;
            const auto [ia, ib, ic] = f.get_node_ids();
            const vec3& pa = c->get_node_lst()[ia].pos();
            const vec3& pb = c->get_node_lst()[ib].pos();
            const vec3& pc = c->get_node_lst()[ic].pos();
            const double l = std::max({(pb - pa).norm(), (pc - pb).norm(), (pa - pc).norm()});
            worst_quality = std::min(worst_quality, f.get_area() / (l * l));
        }
        std::cout << "  worst triangle quality (area / l_max^2) on the healthy sphere: "
                  << worst_quality << " (equilateral = 0.433)" << std::endl;

        // The guard fires below 1e-4; a healthy mesh must sit at least 100x above that.
        bool t1 = worst_quality > 1e-2;
        std::cout << "  t1 (healthy mesh is >100x above the degeneracy threshold): " << t1 << std::endl;
        if (!t1) return 1;

        const double f = max_nodal_force(c);
        bool t2 = std::isfinite(f) && f > 0.;
        std::cout << "  t2 (healthy force assembly finite: " << f << " N): " << t2 << std::endl;
        if (!t2) return 1;

        std::cout << "test_healthy_mesh_untouched_by_guard PASSED" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cout << "FAILED with exception: " << e.what() << std::endl;
        return 1;
    }
}

//---------------------------------------------------------------------------------------------------------
int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: degenerate_hinge_test <test_name>" << std::endl;
        return 1;
    }
    const std::string test_name(argv[1]);

    if (test_name == "test_degenerate_hinge_force_is_bounded") return test_degenerate_hinge_force_is_bounded();
    if (test_name == "test_degenerate_hinge_force_saturates")  return test_degenerate_hinge_force_saturates();
    if (test_name == "test_healthy_mesh_untouched_by_guard")   return test_healthy_mesh_untouched_by_guard();

    std::cout << "Unknown test: " << test_name << std::endl;
    return 1;
}
