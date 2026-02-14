#ifndef DEF_PERFORMANCE_MONITOR
#define DEF_PERFORMANCE_MONITOR

#include <string>
#include <map>
#include <vector>
#include <chrono>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <limits>
#include <algorithm>
#include <omp.h>

/**
 * @brief Lightweight performance monitoring infrastructure
 *
 * Provides scoped timing with minimal overhead (<1%) for diagnosing
 * performance bottlenecks in SimuCell3D iterations.
 *
 * Usage:
 *   PerformanceMonitor monitor;
 *   {
 *       PerformanceMonitor::ScopedTimer timer(monitor, "contact_forces");
 *       contact_model->run(cells);
 *   }
 *   monitor.report();
 */
class PerformanceMonitor {
public:
    struct TimingStats {
        double total_time = 0.0;       // Total accumulated time
        double min_time = std::numeric_limits<double>::max();
        double max_time = 0.0;
        size_t count = 0;              // Number of samples

        void update(double time) {
            total_time += time;
            min_time = std::min(min_time, time);
            max_time = std::max(max_time, time);
            count++;
        }

        double average() const {
            return (count > 0) ? (total_time / count) : 0.0;
        }

        double std_dev() const {
            // Simple estimate: (max - min) / 4 for normal distribution
            return (max_time - min_time) / 4.0;
        }
    };

    /**
     * @brief RAII-based scoped timer
     *
     * Automatically measures elapsed time from construction to destruction
     */
    class ScopedTimer {
    private:
        PerformanceMonitor& monitor_;
        std::string name_;
        std::chrono::high_resolution_clock::time_point start_;

    public:
        ScopedTimer(PerformanceMonitor& monitor, const std::string& name)
            : monitor_(monitor), name_(name),
              start_(std::chrono::high_resolution_clock::now()) {}

        ~ScopedTimer() {
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed = end - start_;
            monitor_.record(name_, elapsed.count());
        }

        // Disable copy/move to prevent double-recording
        ScopedTimer(const ScopedTimer&) = delete;
        ScopedTimer& operator=(const ScopedTimer&) = delete;
    };

    /**
     * @brief Record a timing measurement
     */
    void record(const std::string& name, double time_seconds) {
        timings_[name].update(time_seconds);
    }

    /**
     * @brief Get statistics for a specific timer
     */
    const TimingStats* get_stats(const std::string& name) const {
        auto it = timings_.find(name);
        return (it != timings_.end()) ? &it->second : nullptr;
    }

    /**
     * @brief Print performance report to stdout
     */
    void report(bool detailed = false) const {
        std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
        std::cout << "║           Performance Monitoring Report                   ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";

        if (timings_.empty()) {
            std::cout << "  No timing data collected.\n\n";
            return;
        }

        // Calculate total time
        double total_time = 0.0;
        for (const auto& [name, stats] : timings_) {
            total_time += stats.total_time;
        }

        // Print header
        std::cout << std::left << std::setw(25) << "  Phase"
                  << std::right << std::setw(12) << "Avg (ms)"
                  << std::setw(12) << "Min (ms)"
                  << std::setw(12) << "Max (ms)"
                  << std::setw(10) << "Count"
                  << std::setw(10) << "% Total"
                  << "\n";
        std::cout << "  " << std::string(80, '-') << "\n";

        // Sort by total time (most expensive first)
        std::vector<std::pair<std::string, TimingStats>> sorted_timings(
            timings_.begin(), timings_.end());
        std::sort(sorted_timings.begin(), sorted_timings.end(),
            [](const auto& a, const auto& b) {
                return a.second.total_time > b.second.total_time;
            });

        // Print stats
        for (const auto& [name, stats] : sorted_timings) {
            double percent = (total_time > 0) ? (stats.total_time / total_time * 100.0) : 0.0;

            std::cout << std::left << std::setw(25) << ("  " + name)
                      << std::right << std::fixed << std::setprecision(2)
                      << std::setw(12) << (stats.average() * 1000.0)
                      << std::setw(12) << (stats.min_time * 1000.0)
                      << std::setw(12) << (stats.max_time * 1000.0)
                      << std::setw(10) << stats.count
                      << std::setw(9) << percent << "%"
                      << "\n";
        }

        std::cout << "  " << std::string(80, '-') << "\n";
        std::cout << std::left << std::setw(25) << "  TOTAL"
                  << std::right << std::fixed << std::setprecision(2)
                  << std::setw(12) << (total_time * 1000.0)
                  << "\n\n";

        if (detailed) {
            std::cout << "  Detailed Analysis:\n";
            for (const auto& [name, stats] : sorted_timings) {
                double variability = (stats.average() > 0) ?
                    (stats.std_dev() / stats.average() * 100.0) : 0.0;
                std::cout << "    " << name << ":\n";
                std::cout << "      Variability: " << std::setprecision(1)
                          << variability << "% (std/avg)\n";
            }
            std::cout << "\n";
        }
    }

    /**
     * @brief Reset all timing data
     */
    void reset() {
        timings_.clear();
    }

    /**
     * @brief Get total accumulated time across all phases
     */
    double get_total_time() const {
        double total = 0.0;
        for (const auto& [name, stats] : timings_) {
            total += stats.total_time;
        }
        return total;
    }

    /**
     * @brief Monitor thread-level load balance for a parallel region
     *
     * Records per-thread execution time to detect imbalance.
     * Call this INSIDE a parallel region to capture thread-specific data.
     *
     * Example:
     *   #pragma omp parallel
     *   {
     *       auto start = std::chrono::high_resolution_clock::now();
     *       #pragma omp for schedule(runtime)
     *       for (int i = 0; i < n; i++) { work(i); }
     *       monitor.record_thread_balance("my_loop", start);
     *   }
     */
    struct ThreadBalanceStats {
        double avg_thread_time = 0.0;
        double min_thread_time = std::numeric_limits<double>::max();
        double max_thread_time = 0.0;
        double imbalance_percent = 0.0;  // (max-min)/max * 100
        double efficiency_percent = 0.0;  // avg/max * 100
        int num_threads = 0;
    };

    void record_thread_balance(const std::string& name,
                               std::chrono::high_resolution_clock::time_point start_time) {
        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end_time - start_time;
        double thread_time = elapsed.count();

        // Thread-safe update of per-thread statistics
        #pragma omp critical
        {
            thread_timings_[name].push_back(thread_time);
        }
    }

    /**
     * @brief Analyze and report thread imbalance for all monitored regions
     */
    void analyze_thread_balance() const {
        if (thread_timings_.empty()) return;

        std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
        std::cout << "║         Thread Load Balance Analysis                     ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";

        for (const auto& [name, times] : thread_timings_) {
            if (times.empty()) continue;

            ThreadBalanceStats stats = calculate_balance_stats(times);

            std::cout << "  " << name << ":\n";
            std::cout << "    Threads: " << stats.num_threads << "\n";
            std::cout << "    Min/Avg/Max time: "
                      << std::fixed << std::setprecision(2)
                      << (stats.min_thread_time * 1000.0) << "/"
                      << (stats.avg_thread_time * 1000.0) << "/"
                      << (stats.max_thread_time * 1000.0) << " ms\n";
            std::cout << "    Imbalance: " << std::setprecision(1)
                      << stats.imbalance_percent << "%\n";
            std::cout << "    Efficiency: " << stats.efficiency_percent << "%\n";

            if (stats.imbalance_percent > 20.0) {
                std::cout << "    ⚠ WARNING: High thread imbalance detected!\n";
                std::cout << "      Consider: reducing chunk size or using dynamic scheduling\n";
            }
            std::cout << "\n";
        }
    }

    /**
     * @brief Export all timing data to CSV file
     *
     * @param filename Path to output CSV file
     * @param iteration Current iteration number (optional, for time-series analysis)
     * @param num_cells Current cell count (optional, for correlation analysis)
     */
    void export_to_csv(const std::string& filename, int iteration = -1, size_t num_cells = 0) const {
        bool file_exists = std::ifstream(filename).good();
        std::ofstream csv_file(filename, std::ios::app);

        if (!csv_file.is_open()) {
            std::cerr << "Error: Could not open CSV file: " << filename << std::endl;
            return;
        }

        // Write header if file is new
        if (!file_exists) {
            csv_file << "iteration,num_cells,phase,avg_ms,min_ms,max_ms,count,total_ms\n";
        }

        // Write data for each phase
        for (const auto& [name, stats] : timings_) {
            csv_file << iteration << ","
                     << num_cells << ","
                     << name << ","
                     << std::fixed << std::setprecision(6)
                     << (stats.average() * 1000.0) << ","
                     << (stats.min_time * 1000.0) << ","
                     << (stats.max_time * 1000.0) << ","
                     << stats.count << ","
                     << (stats.total_time * 1000.0) << "\n";
        }

        csv_file.flush();
    }

private:
    std::map<std::string, TimingStats> timings_;
    mutable std::map<std::string, std::vector<double>> thread_timings_;

    ThreadBalanceStats calculate_balance_stats(const std::vector<double>& times) const {
        ThreadBalanceStats stats;
        stats.num_threads = times.size();

        if (times.empty()) return stats;

        double sum = 0.0;
        for (double t : times) {
            sum += t;
            stats.min_thread_time = std::min(stats.min_thread_time, t);
            stats.max_thread_time = std::max(stats.max_thread_time, t);
        }

        stats.avg_thread_time = sum / times.size();
        stats.imbalance_percent = (stats.max_thread_time > 0) ?
            ((stats.max_thread_time - stats.min_thread_time) / stats.max_thread_time * 100.0) : 0.0;
        stats.efficiency_percent = (stats.max_thread_time > 0) ?
            (stats.avg_thread_time / stats.max_thread_time * 100.0) : 0.0;

        return stats;
    }
};

#endif
