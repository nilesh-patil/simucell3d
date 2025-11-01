/**
 * @file benchmark_simple.cpp
 * @brief Simple performance benchmark comparing USPG vs Spatial Hash
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

struct BenchResult {
    double insert_time_ms;
    double search_time_ms;
    size_t occupied_voxels;
};

template<typename GridType>
BenchResult benchmark(size_t num_particles, double domain_size, double voxel_size, int iterations) {
    BenchResult result = {0, 0, 0};

    std::mt19937 rng(42);
    std::uniform_real_distribution<double> dist(-domain_size/2, domain_size/2);

    // Generate random positions
    std::vector<vec3> positions;
    positions.reserve(num_particles);
    for (size_t i = 0; i < num_particles; ++i) {
        positions.push_back(vec3(dist(rng), dist(rng), dist(rng)));
    }

    for (int iter = 0; iter < iterations; ++iter) {
        // Create grid
        GridType grid(-domain_size/2, -domain_size/2, -domain_size/2,
                      domain_size/2, domain_size/2, domain_size/2,
                      voxel_size, num_particles);

        // Measure insertion
        auto t1 = high_resolution_clock::now();
        for (size_t i = 0; i < positions.size(); ++i) {
            grid.place_object(static_cast<int>(i), positions[i]);
        }
        auto t2 = high_resolution_clock::now();
        result.insert_time_ms += duration<double, std::milli>(t2 - t1).count();

        // Measure neighbor search
        auto t3 = high_resolution_clock::now();
        for (size_t i = 0; i < positions.size(); ++i) {
            auto neighbors = grid.get_neighborhood(positions[i]);
            size_t count = 0;
            for (auto n : neighbors) { count++; (void)n; }
        }
        auto t4 = high_resolution_clock::now();
        result.search_time_ms += duration<double, std::milli>(t4 - t3).count();

        // Count occupied voxels (first iteration only)
        if (iter == 0) {
            #ifdef USE_SPATIAL_HASH
            result.occupied_voxels = grid.get_num_occupied_voxels();
            #else
            result.occupied_voxels = grid.get_nb_voxels()[0] * grid.get_nb_voxels()[1] * grid.get_nb_voxels()[2];
            #endif
        }
    }

    result.insert_time_ms /= iterations;
    result.search_time_ms /= iterations;

    return result;
}

int main() {
    std::cout << "Contact Detection Performance Benchmark\n";
    std::cout << "========================================\n\n";

    #ifdef USE_SPATIAL_HASH
    std::cout << "Implementation: Spatial Hash (Optimized)\n\n";
    #else
    std::cout << "Implementation: USPG (Baseline)\n\n";
    #endif

    struct TestCase {
        const char* name;
        size_t num_particles;
        double domain_size;
        double voxel_size;
        int iterations;
    };

    TestCase tests[] = {
        {"Small (1K particles)", 1000, 100.0, 5.0, 50},
        {"Medium (10K particles)", 10000, 100.0, 5.0, 20},
        {"Large (100K particles)", 100000, 100.0, 5.0, 5},
        {"XLarge (200K particles)", 200000, 100.0, 5.0, 3},
    };

    for (const auto& test : tests) {
        std::cout << test.name << ":\n";
        std::cout << "  Domain: " << test.domain_size << "³, Voxel: " << test.voxel_size << "\n";
        std::cout << "  Running " << test.iterations << " iterations...\n";

        BenchResult result;
        #ifdef USE_SPATIAL_HASH
        result = benchmark<spatial_hash<int>>(test.num_particles, test.domain_size, test.voxel_size, test.iterations);
        #else
        result = benchmark<uspg_4d<int>>(test.num_particles, test.domain_size, test.voxel_size, test.iterations);
        #endif

        std::cout << "  Insert:  " << std::fixed << std::setprecision(3) << result.insert_time_ms << " ms\n";
        std::cout << "  Search:  " << result.search_time_ms << " ms\n";
        std::cout << "  Voxels:  " << result.occupied_voxels << "\n";
        std::cout << "\n";
    }

    return 0;
}
