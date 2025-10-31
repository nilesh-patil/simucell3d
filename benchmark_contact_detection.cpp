/**
 * @file benchmark_contact_detection.cpp
 * @brief Performance benchmark comparing USPG vs Spatial Hash contact detection
 *
 * Measures:
 * - Grid update time
 * - Neighbor search time
 * - Memory usage
 * - Overall performance
 */

#include <iostream>
#include <chrono>
#include <iomanip>
#include <vector>
#include <random>

#include "uspg_4d.hpp"

#ifdef USE_SPATIAL_HASH
#include "spatial_hash.hpp"
#endif

#include "vec3.hpp"

using namespace std::chrono;

// Benchmark configuration
struct BenchmarkConfig {
    size_t num_cells;
    size_t particles_per_cell;
    size_t num_iterations;
    double domain_size;
    double voxel_size;
};

// Timing results
struct TimingResults {
    double grid_update_ms;
    double neighbor_search_ms;
    double total_ms;
    size_t num_occupied_voxels;
    size_t total_voxels;
    double occupancy_percent;
};

// Mock particle structure
struct Particle {
    vec3 position;
    int id;
};

template<typename GridType>
TimingResults run_benchmark(const BenchmarkConfig& config) {
    TimingResults results = {0, 0, 0, 0, 0, 0};

    // Random number generator
    std::mt19937 rng(42);  // Fixed seed for reproducibility
    std::uniform_real_distribution<double> dist(-config.domain_size/2, config.domain_size/2);

    // Generate random particles (using vector that won't reallocate)
    const size_t total_particles = config.num_cells * config.particles_per_cell;
    std::vector<Particle> particles;
    particles.reserve(total_particles * 2);  // Extra space to prevent reallocation

    for (size_t cell_idx = 0; cell_idx < config.num_cells; ++cell_idx) {
        // Cell center
        vec3 cell_center(dist(rng), dist(rng), dist(rng));

        // Particles around cell center
        for (size_t p = 0; p < config.particles_per_cell; ++p) {
            Particle particle;
            particle.position = cell_center + vec3(dist(rng) * 0.1,
                                                   dist(rng) * 0.1,
                                                   dist(rng) * 0.1);
            particle.id = cell_idx * config.particles_per_cell + p;
            particles.push_back(particle);
        }
    }

    // Create stable pointers to particles
    std::vector<const Particle*> particle_ptrs;
    particle_ptrs.reserve(particles.size());
    for (const auto& p : particles) {
        particle_ptrs.push_back(&p);
    }

    // Total voxels
    size_t voxels_per_dim = (size_t)(config.domain_size / config.voxel_size);
    results.total_voxels = voxels_per_dim * voxels_per_dim * voxels_per_dim;

    // Benchmark loop
    auto total_start = high_resolution_clock::now();

    for (size_t iter = 0; iter < config.num_iterations; ++iter) {
        // Create grid
        GridType grid(-config.domain_size/2, -config.domain_size/2, -config.domain_size/2,
                      config.domain_size/2, config.domain_size/2, config.domain_size/2,
                      config.voxel_size, particles.size());

        // Measure grid update (insertion)
        auto grid_start = high_resolution_clock::now();

        for (size_t i = 0; i < particle_ptrs.size(); ++i) {
            grid.place_object(static_cast<int>(i), particle_ptrs[i]->position);
        }

        auto grid_end = high_resolution_clock::now();
        results.grid_update_ms += duration<double, std::milli>(grid_end - grid_start).count();

        // Measure neighbor searches
        auto search_start = high_resolution_clock::now();

        size_t total_neighbors = 0;
        for (size_t i = 0; i < particle_ptrs.size(); ++i) {
            auto neighbors = grid.get_neighborhood(particle_ptrs[i]->position);
            // Count neighbors (forces evaluation of forward_list)
            for (auto n : neighbors) {
                total_neighbors++;
                (void)n; // Suppress unused warning
            }
        }

        auto search_end = high_resolution_clock::now();
        results.neighbor_search_ms += duration<double, std::milli>(search_end - search_start).count();

        // Count occupied voxels (only for spatial_hash, estimate for USPG)
        #ifdef USE_SPATIAL_HASH
        if (iter == 0) {
            results.num_occupied_voxels = grid.get_num_occupied_voxels();
        }
        #else
        // Estimate for USPG (all voxels are allocated)
        if (iter == 0) {
            results.num_occupied_voxels = results.total_voxels;
        }
        #endif
    }

    auto total_end = high_resolution_clock::now();
    results.total_ms = duration<double, std::milli>(total_end - total_start).count();

    // Average over iterations
    results.grid_update_ms /= config.num_iterations;
    results.neighbor_search_ms /= config.num_iterations;

    // Calculate occupancy
    results.occupancy_percent = 100.0 * results.num_occupied_voxels / results.total_voxels;

    return results;
}

void print_separator() {
    std::cout << "+------------------------+----------------+----------------+\n";
}

void print_header() {
    print_separator();
    std::cout << "| Metric                 | USPG (Baseline)| Spatial Hash   |\n";
    print_separator();
}

int main(int argc, char** argv) {
    std::cout << "======================================================\n";
    std::cout << "  Contact Detection Performance Benchmark\n";
    std::cout << "======================================================\n\n";

    #ifdef USE_SPATIAL_HASH
    std::cout << "Running with: Spatial Hash (Optimized)\n\n";
    #else
    std::cout << "Running with: USPG (Baseline)\n\n";
    #endif

    // Test configurations
    std::vector<BenchmarkConfig> configs = {
        {100, 100, 50, 100.0, 5.0},      // Small: 100 cells, 10K particles
        {1000, 100, 20, 100.0, 5.0},     // Medium: 1K cells, 100K particles
        {10000, 100, 5, 100.0, 5.0},     // Large: 10K cells, 1M particles
    };

    const char* size_names[] = {"Small (100 cells)", "Medium (1K cells)", "Large (10K cells)"};

    for (size_t i = 0; i < configs.size(); ++i) {
        std::cout << "\n" << size_names[i] << "\n";
        std::cout << "  Particles: " << configs[i].num_cells * configs[i].particles_per_cell << "\n";
        std::cout << "  Iterations: " << configs[i].num_iterations << "\n";
        std::cout << "  Domain: " << configs[i].domain_size << "³\n";
        std::cout << "  Voxel size: " << configs[i].voxel_size << "\n";

        #ifdef USE_SPATIAL_HASH
        TimingResults results = run_benchmark<spatial_hash<int>>(configs[i]);
        #else
        TimingResults results = run_benchmark<uspg_4d<int>>(configs[i]);
        #endif

        std::cout << "\n  Results:\n";
        std::cout << "    Grid update:     " << std::fixed << std::setprecision(3)
                  << results.grid_update_ms << " ms/iter\n";
        std::cout << "    Neighbor search: " << results.neighbor_search_ms << " ms/iter\n";
        std::cout << "    Total time:      " << results.total_ms << " ms\n";
        std::cout << "    Occupied voxels: " << results.num_occupied_voxels
                  << " / " << results.total_voxels
                  << " (" << std::setprecision(2) << results.occupancy_percent << "%)\n";
        std::cout << std::string(50, '-') << "\n";
    }

    return 0;
}
