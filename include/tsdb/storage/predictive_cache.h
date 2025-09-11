#pragma once

#include "tsdb/core/types.h"
#include "tsdb/storage/cache_hierarchy.h"
#include <unordered_map>
#include <deque>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>

namespace tsdb {
namespace storage {

/**
 * @brief Configuration for predictive caching behavior
 */
struct PredictiveCacheConfig {
    // Pattern recognition settings
    size_t max_pattern_length = 10;           // Maximum sequence length to track
    size_t min_pattern_confidence = 3;        // Minimum occurrences to consider a pattern
    double confidence_threshold = 0.7;        // Minimum confidence ratio for predictions
    
    // Prefetching settings
    size_t max_prefetch_size = 5;             // Maximum items to prefetch
    bool enable_adaptive_prefetch = true;     // Adjust prefetch size based on success rate
    size_t prefetch_window_size = 100;        // Window size for success rate calculation
    
    // Performance settings
    size_t max_tracked_series = 10000;        // Maximum series to track patterns for
    size_t cleanup_interval_ms = 60000;       // Cleanup interval for old patterns (ms)
    bool enable_background_cleanup = false;   // DISABLED BY DEFAULT FOR TESTING
    
    // Integration settings
    bool integrate_with_cache_hierarchy = true; // Integrate with existing cache hierarchy
    double prefetch_priority_boost = 1.5;     // Priority boost for prefetched items
};

/**
 * @brief Represents a detected access pattern
 */
struct AccessPattern {
    std::vector<core::SeriesID> sequence;     // The sequence of series IDs
    size_t occurrences = 0;                   // Number of times this pattern occurred
    double confidence = 0.0;                  // Confidence score (0.0 to 1.0)
    std::chrono::steady_clock::time_point last_seen; // Last time this pattern was observed
    
    AccessPattern() = default;
    AccessPattern(const std::vector<core::SeriesID>& seq, size_t occ = 1)
        : sequence(seq), occurrences(occ), confidence(0.0) {
        last_seen = std::chrono::steady_clock::now();
    }
};

/**
 * @brief Tracks prefetch success rates for adaptive behavior
 */
struct PrefetchStats {
    std::atomic<size_t> total_prefetches{0};
    std::atomic<size_t> successful_prefetches{0};
    std::atomic<size_t> failed_prefetches{0};
    std::deque<bool> recent_results;          // Recent prefetch results for window calculation
    std::mutex results_mutex;
    
    double get_success_rate() const;
    void record_result(bool success);
    void cleanup_old_results(size_t window_size);
};

/**
 * @brief Predictive cache that learns access patterns and prefetches likely-to-be-accessed data
 * 
 * This class implements intelligent cache warming by:
 * 1. Tracking access patterns for each series
 * 2. Detecting common sequences of accesses
 * 3. Prefetching data that is likely to be accessed next
 * 4. Adapting prefetch behavior based on success rates
 */
class PredictiveCache {
public:
    explicit PredictiveCache(const PredictiveCacheConfig& config = PredictiveCacheConfig{});
    ~PredictiveCache();
    
    // Disable copy constructor and assignment
    PredictiveCache(const PredictiveCache&) = delete;
    PredictiveCache& operator=(const PredictiveCache&) = delete;
    
    /**
     * @brief Record an access to a series for pattern learning
     * @param series_id The series that was accessed
     */
    void record_access(core::SeriesID series_id);
    
    /**
     * @brief Get predictions for likely next accesses
     * @param current_series The currently accessed series
     * @return Vector of predicted series IDs with confidence scores
     */
    std::vector<std::pair<core::SeriesID, double>> get_predictions(core::SeriesID current_series) const;
    
    /**
     * @brief Prefetch predicted data into the cache hierarchy
     * @param cache_hierarchy The cache hierarchy to prefetch into
     * @param current_series The currently accessed series
     * @return Number of items prefetched
     */
    size_t prefetch_predictions(CacheHierarchy& cache_hierarchy, core::SeriesID current_series);
    
    /**
     * @brief Record whether a prefetched item was actually accessed
     * @param series_id The series that was prefetched
     * @param was_accessed Whether it was actually accessed
     */
    void record_prefetch_result(core::SeriesID series_id, bool was_accessed);
    
    /**
     * @brief Get statistics about predictive cache performance
     * @return String containing performance statistics
     */
    std::string get_stats() const;
    
    /**
     * @brief Clear all learned patterns and statistics
     */
    void clear();
    
    /**
     * @brief Check if predictive caching is enabled
     * @return True if enabled
     */
    bool is_enabled() const { return config_.enable_adaptive_prefetch; }
    
    /**
     * @brief Get the current configuration
     * @return Current configuration
     */
    const PredictiveCacheConfig& get_config() const { return config_; }
    
    /**
     * @brief Update configuration
     * @param new_config New configuration
     */
    void update_config(const PredictiveCacheConfig& new_config);

private:
    PredictiveCacheConfig config_;
    
    // Pattern tracking
    mutable std::mutex patterns_mutex_;
    std::unordered_map<core::SeriesID, std::deque<core::SeriesID>> access_sequences_;
    std::deque<core::SeriesID> global_access_sequence_;
    std::unordered_map<std::string, AccessPattern> detected_patterns_;
    
    // Prefetch statistics
    PrefetchStats prefetch_stats_;
    
    // Background cleanup
    std::atomic<bool> shutdown_flag_{false};
    std::unique_ptr<std::thread> cleanup_thread_;
    
    // Helper methods
    void detect_patterns();
    std::string pattern_to_string(const std::vector<core::SeriesID>& pattern) const;
    std::vector<core::SeriesID> string_to_pattern(const std::string& pattern_str) const;
    void cleanup_old_patterns();
    void background_cleanup_worker();
    double calculate_confidence(const AccessPattern& pattern) const;
    size_t get_adaptive_prefetch_size() const;
    
    /**
     * @brief Find patterns that start with the given series
     * @param series_id The series to find patterns for
     * @return Vector of matching patterns with confidence scores
     */
    std::vector<std::pair<AccessPattern, double>> find_matching_patterns(core::SeriesID series_id) const;
};

} // namespace storage
} // namespace tsdb 