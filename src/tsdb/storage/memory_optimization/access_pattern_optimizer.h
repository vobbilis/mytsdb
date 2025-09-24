#ifndef TSDB_STORAGE_ACCESS_PATTERN_OPTIMIZER_H_
#define TSDB_STORAGE_ACCESS_PATTERN_OPTIMIZER_H_

#include "tsdb/core/types.h"
#include "tsdb/core/result.h"
#include "tsdb/core/config.h"
#include <memory>
#include <vector>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <chrono>
#include <queue>

namespace tsdb {
namespace storage {

/**
 * @brief Access Pattern Optimizer for memory access optimization
 * 
 * This class analyzes and optimizes memory access patterns to improve
 * cache performance and reduce memory access latency.
 */
class AccessPatternOptimizer {
public:
    /**
     * @brief Constructor
     * @param config Storage configuration
     */
    explicit AccessPatternOptimizer(const core::StorageConfig& config);
    
    /**
     * @brief Destructor
     */
    ~AccessPatternOptimizer();
    
    /**
     * @brief Initialize the optimizer
     * @return Result indicating success or failure
     */
    core::Result<void> initialize();
    
    /**
     * @brief Record access pattern for a series
     * @param series_id Series ID to record
     * @param access_type Type of access (sequential, random, etc.)
     * @return Result indicating success or failure
     */
    core::Result<void> record_access(const core::SeriesID& series_id, const std::string& access_type);
    
    /**
     * @brief Record bulk access pattern
     * @param series_ids Vector of series IDs to record
     * @param access_type Type of access
     * @return Result indicating success or failure
     */
    core::Result<void> record_bulk_access(const std::vector<core::SeriesID>& series_ids, const std::string& access_type);
    
    /**
     * @brief Analyze access patterns
     * @return Result indicating success or failure
     */
    core::Result<void> analyze_access_patterns();
    
    /**
     * @brief Suggest prefetch addresses based on access patterns
     * @param series_id Series ID to analyze
     * @return Result containing suggested prefetch addresses
     */
    core::Result<std::vector<void*>> suggest_prefetch_addresses(const core::SeriesID& series_id);
    
    /**
     * @brief Execute prefetch based on suggestions
     * @param addresses Vector of addresses to prefetch
     * @return Result indicating success or failure
     */
    core::Result<void> execute_prefetch(const std::vector<void*>& addresses);
    
    /**
     * @brief Optimize access pattern for a series
     * @param series_id Series ID to optimize
     * @return Result indicating success or failure
     */
    core::Result<void> optimize_access_pattern(const core::SeriesID& series_id);
    
    /**
     * @brief Get access pattern statistics
     * @return String containing access pattern statistics
     */
    std::string get_access_pattern_stats() const;
    
    /**
     * @brief Get optimization statistics
     * @return String containing optimization statistics
     */
    std::string get_optimization_stats() const;
    
    /**
     * @brief Get prefetch statistics
     * @return String containing prefetch statistics
     */
    std::string get_prefetch_stats() const;

private:
    // Configuration
    core::StorageConfig config_;
    
    // Access pattern tracking
    struct AccessRecord {
        std::atomic<size_t> access_count;
        std::atomic<size_t> sequential_accesses;
        std::atomic<size_t> random_accesses;
        std::atomic<uint64_t> last_access_time;
        std::atomic<bool> is_sequential;
        std::string access_type;
        std::chrono::system_clock::time_point first_access;
        
        // Default constructor
        AccessRecord() : access_count(0), sequential_accesses(0), random_accesses(0), 
                        last_access_time(0), is_sequential(false), access_type("unknown") {}
        
        // Constructor with parameters
        AccessRecord(size_t access_count, size_t sequential_accesses, size_t random_accesses,
                    uint64_t last_access_time, bool is_sequential, const std::string& access_type,
                    const std::chrono::system_clock::time_point& first_access)
            : access_count(access_count), sequential_accesses(sequential_accesses), 
              random_accesses(random_accesses), last_access_time(last_access_time),
              is_sequential(is_sequential), access_type(access_type), first_access(first_access) {}
        
        // Copy constructor
        AccessRecord(const AccessRecord& other) 
            : access_count(other.access_count.load()), 
              sequential_accesses(other.sequential_accesses.load()),
              random_accesses(other.random_accesses.load()),
              last_access_time(other.last_access_time.load()),
              is_sequential(other.is_sequential.load()),
              access_type(other.access_type),
              first_access(other.first_access) {}
        
        // Move constructor
        AccessRecord(AccessRecord&& other) noexcept
            : access_count(other.access_count.load()), 
              sequential_accesses(other.sequential_accesses.load()),
              random_accesses(other.random_accesses.load()),
              last_access_time(other.last_access_time.load()),
              is_sequential(other.is_sequential.load()),
              access_type(std::move(other.access_type)),
              first_access(other.first_access) {}
        
        // Copy assignment operator
        AccessRecord& operator=(const AccessRecord& other) {
            if (this != &other) {
                access_count.store(other.access_count.load());
                sequential_accesses.store(other.sequential_accesses.load());
                random_accesses.store(other.random_accesses.load());
                last_access_time.store(other.last_access_time.load());
                is_sequential.store(other.is_sequential.load());
                access_type = other.access_type;
                first_access = other.first_access;
            }
            return *this;
        }
        
        // Move assignment operator
        AccessRecord& operator=(AccessRecord&& other) noexcept {
            if (this != &other) {
                access_count.store(other.access_count.load());
                sequential_accesses.store(other.sequential_accesses.load());
                random_accesses.store(other.random_accesses.load());
                last_access_time.store(other.last_access_time.load());
                is_sequential.store(other.is_sequential.load());
                access_type = std::move(other.access_type);
                first_access = other.first_access;
            }
            return *this;
        }
    };
    
    std::unordered_map<core::SeriesID, AccessRecord> access_records_;
    mutable std::mutex access_mutex_;
    
    // Optimization tracking
    struct OptimizationInfo {
        std::atomic<size_t> total_optimizations;
        std::atomic<size_t> successful_optimizations;
        std::atomic<size_t> failed_optimizations;
        std::atomic<size_t> prefetch_suggestions;
        std::atomic<size_t> prefetch_executions;
        std::chrono::system_clock::time_point last_optimization;
    };
    
    OptimizationInfo optimization_info_;
    
    // Prefetch tracking
    std::atomic<size_t> total_prefetches_;
    std::atomic<size_t> successful_prefetches_;
    std::atomic<size_t> failed_prefetches_;
    
    /**
     * @brief Update access record for a series
     * @param series_id Series ID to update
     * @param access_type Type of access
     */
    void update_access_record(const core::SeriesID& series_id, const std::string& access_type);
    
    /**
     * @brief Calculate spatial locality for a series
     * @param series_id Series ID to analyze
     * @return Spatial locality score (0.0 to 1.0)
     */
    double calculate_spatial_locality(const core::SeriesID& series_id);
    
    /**
     * @brief Calculate temporal locality for a series
     * @param series_id Series ID to analyze
     * @return Temporal locality score (0.0 to 1.0)
     */
    double calculate_temporal_locality(const core::SeriesID& series_id);
    
    /**
     * @brief Identify sequential access patterns
     * @param series_id Series ID to analyze
     * @return True if access pattern is sequential
     */
    bool is_sequential_access(const core::SeriesID& series_id);
    
    /**
     * @brief Identify random access patterns
     * @param series_id Series ID to analyze
     * @return True if access pattern is random
     */
    bool is_random_access(const core::SeriesID& series_id);
    
    /**
     * @brief Generate prefetch suggestions based on access patterns
     * @param series_id Series ID to analyze
     * @return Vector of suggested prefetch addresses
     */
    std::vector<void*> generate_prefetch_suggestions(const core::SeriesID& series_id);
    
    /**
     * @brief Execute prefetch for a single address
     * @param address Address to prefetch
     * @return Result indicating success or failure
     */
    core::Result<void> execute_single_prefetch(void* address);
    
    /**
     * @brief Update optimization statistics
     * @param success Whether the optimization was successful
     */
    void update_optimization_stats(bool success);
    
    /**
     * @brief Update prefetch statistics
     * @param success Whether the prefetch was successful
     */
    void update_prefetch_stats(bool success);
};

} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_ACCESS_PATTERN_OPTIMIZER_H_
