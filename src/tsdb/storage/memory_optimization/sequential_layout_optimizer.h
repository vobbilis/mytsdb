#ifndef TSDB_STORAGE_SEQUENTIAL_LAYOUT_OPTIMIZER_H_
#define TSDB_STORAGE_SEQUENTIAL_LAYOUT_OPTIMIZER_H_

#include "tsdb/core/types.h"
#include "tsdb/core/result.h"
#include "tsdb/core/config.h"
#include "tsdb/storage/block.h"
#include "tsdb/storage/internal/block_internal.h"
#include <memory>
#include <vector>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <chrono>

namespace tsdb {
namespace storage {

/**
 * @brief Sequential Layout Optimizer for memory access pattern optimization
 * 
 * This class optimizes memory layouts for sequential access patterns,
 * improving cache performance and reducing memory access latency.
 */
class SequentialLayoutOptimizer {
public:
    /**
     * @brief Constructor
     * @param config Storage configuration
     */
    explicit SequentialLayoutOptimizer(const core::StorageConfig& config);
    
    /**
     * @brief Destructor
     */
    ~SequentialLayoutOptimizer();
    
    /**
     * @brief Initialize the optimizer
     * @return Result indicating success or failure
     */
    core::Result<void> initialize();
    
    /**
     * @brief Optimize TimeSeries layout for sequential access
     * @param time_series TimeSeries to optimize
     * @return Result indicating success or failure
     */
    core::Result<void> optimize_time_series_layout(core::TimeSeries& time_series);
    
    /**
     * @brief Optimize block layout for sequential access
     * @param blocks Vector of blocks to optimize
     * @return Result indicating success or failure
     */
    core::Result<void> optimize_block_layout(std::vector<std::shared_ptr<storage::internal::BlockInternal>>& blocks);
    
    /**
     * @brief Reserve capacity for sequential access
     * @param time_series TimeSeries to reserve capacity for
     * @param capacity Required capacity
     * @return Result indicating success or failure
     */
    core::Result<void> reserve_capacity(core::TimeSeries& time_series, size_t capacity);
    
    /**
     * @brief Shrink TimeSeries to fit actual usage
     * @param time_series TimeSeries to shrink
     * @return Result indicating success or failure
     */
    core::Result<void> shrink_to_fit(core::TimeSeries& time_series);
    
    /**
     * @brief Prefetch data for sequential access
     * @param time_series TimeSeries to prefetch
     * @return Result indicating success or failure
     */
    core::Result<void> prefetch_data(core::TimeSeries& time_series);
    
    /**
     * @brief Optimize access pattern for a series
     * @param series_id Series ID to optimize
     * @return Result indicating success or failure
     */
    core::Result<void> optimize_access_pattern(const core::SeriesID& series_id);
    
    /**
     * @brief Get optimization statistics
     * @return String containing optimization statistics
     */
    std::string get_optimization_stats() const;
    
    /**
     * @brief Get memory usage statistics
     * @return String containing memory usage statistics
     */
    std::string get_memory_stats() const;
    
    /**
     * @brief Get access pattern statistics
     * @return String containing access pattern statistics
     */
    std::string get_access_pattern_stats() const;

private:
    // Configuration
    core::StorageConfig config_;
    
    // Optimization tracking
    struct OptimizationInfo {
        std::atomic<size_t> optimization_count;
        std::atomic<size_t> prefetch_count;
        std::atomic<size_t> capacity_reservations;
        std::atomic<size_t> shrink_operations;
        std::chrono::system_clock::time_point last_optimization;
    };
    
    OptimizationInfo optimization_info_;
    
    // Access pattern tracking
    struct AccessPattern {
        std::atomic<size_t> sequential_accesses;
        std::atomic<size_t> random_accesses;
        std::atomic<uint64_t> last_access_time;
        std::atomic<bool> is_sequential;
        
        // Default constructor
        AccessPattern() : sequential_accesses(0), random_accesses(0), last_access_time(0), is_sequential(false) {}
        
        // Copy constructor
        AccessPattern(const AccessPattern& other) 
            : sequential_accesses(other.sequential_accesses.load()), 
              random_accesses(other.random_accesses.load()),
              last_access_time(other.last_access_time.load()),
              is_sequential(other.is_sequential.load()) {}
        
        // Move constructor
        AccessPattern(AccessPattern&& other) noexcept
            : sequential_accesses(other.sequential_accesses.load()), 
              random_accesses(other.random_accesses.load()),
              last_access_time(other.last_access_time.load()),
              is_sequential(other.is_sequential.load()) {}
        
        // Copy assignment operator
        AccessPattern& operator=(const AccessPattern& other) {
            if (this != &other) {
                sequential_accesses.store(other.sequential_accesses.load());
                random_accesses.store(other.random_accesses.load());
                last_access_time.store(other.last_access_time.load());
                is_sequential.store(other.is_sequential.load());
            }
            return *this;
        }
        
        // Move assignment operator
        AccessPattern& operator=(AccessPattern&& other) noexcept {
            if (this != &other) {
                sequential_accesses.store(other.sequential_accesses.load());
                random_accesses.store(other.random_accesses.load());
                last_access_time.store(other.last_access_time.load());
                is_sequential.store(other.is_sequential.load());
            }
            return *this;
        }
    };
    
    std::unordered_map<core::SeriesID, AccessPattern> access_patterns_;
    mutable std::mutex access_mutex_;
    
    // Memory usage tracking
    std::atomic<size_t> total_memory_usage_;
    std::atomic<size_t> optimized_memory_usage_;
    std::atomic<size_t> memory_savings_;
    
    /**
     * @brief Analyze access pattern for a series
     * @param series_id Series ID to analyze
     * @return Result indicating success or failure
     */
    core::Result<void> analyze_access_pattern(const core::SeriesID& series_id);
    
    /**
     * @brief Apply sequential layout optimization
     * @param time_series TimeSeries to optimize
     * @return Result indicating success or failure
     */
    core::Result<void> apply_sequential_optimization(core::TimeSeries& time_series);
    
    /**
     * @brief Apply block layout optimization
     * @param blocks Vector of blocks to optimize
     * @return Result indicating success or failure
     */
    core::Result<void> apply_block_optimization(std::vector<std::shared_ptr<storage::internal::BlockInternal>>& blocks);
    
    /**
     * @brief Calculate optimal capacity for a TimeSeries
     * @param time_series TimeSeries to calculate capacity for
     * @return Optimal capacity
     */
    size_t calculate_optimal_capacity(const core::TimeSeries& time_series);
    
    /**
     * @brief Identify sequential access patterns
     * @param series_id Series ID to analyze
     * @return True if access pattern is sequential
     */
    bool is_sequential_access(const core::SeriesID& series_id);
    
    /**
     * @brief Update access pattern statistics
     * @param series_id Series ID to update
     * @param is_sequential Whether the access was sequential
     */
    void update_access_pattern(const core::SeriesID& series_id, bool is_sequential);
    
    /**
     * @brief Calculate memory savings from optimization
     * @param original_size Original memory size
     * @param optimized_size Optimized memory size
     * @return Memory savings in bytes
     */
    size_t calculate_memory_savings(size_t original_size, size_t optimized_size);
};

} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_SEQUENTIAL_LAYOUT_OPTIMIZER_H_
