#ifndef TSDB_STORAGE_INDEX_METRICS_H_
#define TSDB_STORAGE_INDEX_METRICS_H_

#include <atomic>
#include <chrono>
#include <string>
#include <sstream>

namespace tsdb {
namespace storage {

/**
 * @brief Performance metrics for index operations
 * 
 * Tracks timing and counts for index lookups, intersections, and iterations.
 * Used to validate performance improvements from Roaring Bitmaps.
 */
class IndexMetrics {
public:
    static IndexMetrics& instance() {
        static IndexMetrics inst;
        return inst;
    }

    // Lookup metrics
    std::atomic<uint64_t> total_lookups_{0};
    std::atomic<uint64_t> total_lookup_time_ns_{0};
    
    // Intersection metrics (key for Roaring performance)
    std::atomic<uint64_t> total_intersections_{0};
    std::atomic<uint64_t> total_intersection_time_ns_{0};
    std::atomic<uint64_t> total_intersection_input_size_{0};  // Sum of input sizes
    std::atomic<uint64_t> total_intersection_output_size_{0}; // Sum of output sizes
    
    // Iteration metrics
    std::atomic<uint64_t> total_iterations_{0};
    std::atomic<uint64_t> total_iteration_time_ns_{0};
    std::atomic<uint64_t> total_items_iterated_{0};
    
    // Memory metrics
    std::atomic<uint64_t> posting_list_memory_bytes_{0};
    std::atomic<uint64_t> total_posting_lists_{0};
    std::atomic<uint64_t> total_series_in_index_{0};

    void reset() {
        total_lookups_ = 0;
        total_lookup_time_ns_ = 0;
        total_intersections_ = 0;
        total_intersection_time_ns_ = 0;
        total_intersection_input_size_ = 0;
        total_intersection_output_size_ = 0;
        total_iterations_ = 0;
        total_iteration_time_ns_ = 0;
        total_items_iterated_ = 0;
        posting_list_memory_bytes_ = 0;
        total_posting_lists_ = 0;
        total_series_in_index_ = 0;
    }

    void record_lookup(uint64_t time_ns) {
        total_lookups_.fetch_add(1, std::memory_order_relaxed);
        total_lookup_time_ns_.fetch_add(time_ns, std::memory_order_relaxed);
    }

    void record_intersection(uint64_t time_ns, uint64_t input_size, uint64_t output_size) {
        total_intersections_.fetch_add(1, std::memory_order_relaxed);
        total_intersection_time_ns_.fetch_add(time_ns, std::memory_order_relaxed);
        total_intersection_input_size_.fetch_add(input_size, std::memory_order_relaxed);
        total_intersection_output_size_.fetch_add(output_size, std::memory_order_relaxed);
    }

    void record_iteration(uint64_t time_ns, uint64_t items) {
        total_iterations_.fetch_add(1, std::memory_order_relaxed);
        total_iteration_time_ns_.fetch_add(time_ns, std::memory_order_relaxed);
        total_items_iterated_.fetch_add(items, std::memory_order_relaxed);
    }

    // Computed metrics
    double avg_lookup_time_us() const {
        uint64_t lookups = total_lookups_.load();
        if (lookups == 0) return 0.0;
        return static_cast<double>(total_lookup_time_ns_.load()) / lookups / 1000.0;
    }

    double avg_intersection_time_us() const {
        uint64_t intersections = total_intersections_.load();
        if (intersections == 0) return 0.0;
        return static_cast<double>(total_intersection_time_ns_.load()) / intersections / 1000.0;
    }

    double avg_iteration_time_us() const {
        uint64_t iterations = total_iterations_.load();
        if (iterations == 0) return 0.0;
        return static_cast<double>(total_iteration_time_ns_.load()) / iterations / 1000.0;
    }

    double intersection_selectivity() const {
        uint64_t input = total_intersection_input_size_.load();
        if (input == 0) return 0.0;
        return static_cast<double>(total_intersection_output_size_.load()) / input;
    }

    std::string to_string() const {
        std::ostringstream ss;
        ss << "IndexMetrics:\n"
           << "  Lookups: " << total_lookups_.load() 
           << " (avg " << avg_lookup_time_us() << " us)\n"
           << "  Intersections: " << total_intersections_.load() 
           << " (avg " << avg_intersection_time_us() << " us)\n"
           << "  Intersection selectivity: " << (intersection_selectivity() * 100) << "%\n"
           << "  Iterations: " << total_iterations_.load() 
           << " (avg " << avg_iteration_time_us() << " us, " 
           << total_items_iterated_.load() << " items)\n"
           << "  Posting lists: " << total_posting_lists_.load() 
           << " (" << (posting_list_memory_bytes_.load() / 1024.0) << " KB)\n"
           << "  Series in index: " << total_series_in_index_.load();
        return ss.str();
    }

private:
    IndexMetrics() = default;
};

// RAII timer for measuring operations
class ScopedIndexTimer {
public:
    ScopedIndexTimer(std::atomic<uint64_t>& counter, std::atomic<uint64_t>& time_ns)
        : counter_(counter), time_ns_(time_ns), 
          start_(std::chrono::high_resolution_clock::now()) {}
    
    ~ScopedIndexTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_).count();
        counter_.fetch_add(1, std::memory_order_relaxed);
        time_ns_.fetch_add(duration, std::memory_order_relaxed);
    }

    uint64_t elapsed_ns() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(now - start_).count();
    }

private:
    std::atomic<uint64_t>& counter_;
    std::atomic<uint64_t>& time_ns_;
    std::chrono::high_resolution_clock::time_point start_;
};

} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_INDEX_METRICS_H_
