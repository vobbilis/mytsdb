#ifndef TSDB_STORAGE_READ_PERFORMANCE_INSTRUMENTATION_H_
#define TSDB_STORAGE_READ_PERFORMANCE_INSTRUMENTATION_H_

#include <chrono>
#include <string>
#include <atomic>
#include <mutex>
#include <sstream>
#include <iomanip>
#include "tsdb/storage/atomic_metrics.h"

namespace tsdb {
namespace storage {

/**
 * @brief Performance instrumentation for read operations
 */
class ReadPerformanceInstrumentation {
public:
    struct ReadMetrics {
        double index_search_us = 0.0;
        double block_lookup_us = 0.0;
        double block_read_us = 0.0;
        double decompression_us = 0.0;
        double total_us = 0.0;
        size_t samples_scanned = 0;
        size_t blocks_accessed = 0;
        bool cache_hit = false;

        double active_series_lookup_us = 0.0;
        double active_series_read_us = 0.0;
        double series_id_calc_us = 0.0;
        double access_pattern_us = 0.0;
        double cache_get_us = 0.0;
        
        double sorting_us = 0.0;
        double block_lock_wait_us = 0.0;

        // Parquet Metrics
        size_t row_groups_total = 0;
        size_t row_groups_pruned_time = 0;
        size_t row_groups_pruned_tags = 0;
        size_t row_groups_read = 0;
        
        size_t bytes_total = 0;
        size_t bytes_skipped = 0;
        size_t bytes_read = 0;
        
        double pruning_time_us = 0.0;
        
        // Detailed Breakdown
        double row_group_read_us = 0.0;      // Time spent in ReadRowGroup (I/O)
        double decoding_us = 0.0;            // Time spent in ToSeriesMap (CPU/Decoding)
        double processing_us = 0.0;          // Time spent filtering/collecting samples (CPU)

        // New granular metrics for StorageImpl/BlockImpl
        double block_filter_us = 0.0;
        double data_extraction_us = 0.0;
        double result_construction_us = 0.0;
        double data_copy_us = 0.0;

        // Secondary Index Metrics (Phase A: B+ Tree)
        bool secondary_index_used = false;           // Whether index was used for this query
        double secondary_index_lookup_us = 0.0;      // Time spent in index lookup
        double secondary_index_build_us = 0.0;       // Time to build index (if newly built)
        size_t secondary_index_hits = 0;             // Number of series found in index
        size_t secondary_index_row_groups_selected = 0; // Row groups selected via index
        
        // Bloom Filter Metrics (Phase 0: Pre-B+ Tree filtering)
        bool bloom_filter_used = false;              // Whether bloom filter was checked
        double bloom_filter_lookup_us = 0.0;         // Time spent checking bloom filter
        size_t bloom_filter_checks = 0;              // Number of bloom filter checks
        size_t bloom_filter_skips = 0;               // Files skipped (definite not present)
        size_t bloom_filter_passes = 0;              // Files passed to B+ Tree (might be present)

        void reset() {
            index_search_us = 0.0;
            block_lookup_us = 0.0;
            block_read_us = 0.0;
            decompression_us = 0.0;
            total_us = 0.0;
            samples_scanned = 0;
            blocks_accessed = 0;
            cache_hit = false;
            active_series_lookup_us = 0.0;
            active_series_read_us = 0.0;
            series_id_calc_us = 0.0;
            access_pattern_us = 0.0;
            cache_get_us = 0.0;
            sorting_us = 0.0;
            block_lock_wait_us = 0.0;
            
            row_groups_total = 0;
            row_groups_pruned_time = 0;
            row_groups_pruned_tags = 0;
            row_groups_read = 0;
            
            bytes_total = 0;
            bytes_skipped = 0;
            bytes_read = 0;
            
            pruning_time_us = 0.0;
            row_group_read_us = 0.0;
            decoding_us = 0.0;
            processing_us = 0.0;
            
            block_filter_us = 0.0;
            data_extraction_us = 0.0;
            result_construction_us = 0.0;
            data_copy_us = 0.0;
            
            // Secondary Index reset
            secondary_index_used = false;
            secondary_index_lookup_us = 0.0;
            secondary_index_build_us = 0.0;
            secondary_index_hits = 0;
            secondary_index_row_groups_selected = 0;
            
            // Bloom Filter reset
            bloom_filter_used = false;
            bloom_filter_lookup_us = 0.0;
            bloom_filter_checks = 0;
            bloom_filter_skips = 0;
            bloom_filter_passes = 0;
        }

        std::string to_string() const {
            std::stringstream ss;
            ss << std::fixed << std::setprecision(3);
            ss << "Total: " << total_us / 1000.0 << "ms";
            ss << " (Index: " << index_search_us / 1000.0 << "ms";
            ss << ", ID: " << series_id_calc_us / 1000.0 << "ms";
            ss << ", Access: " << access_pattern_us / 1000.0 << "ms";
            ss << ", CacheGet: " << cache_get_us / 1000.0 << "ms";
            ss << ", ActiveLookup: " << active_series_lookup_us / 1000.0 << "ms";
            ss << ", ActiveRead: " << active_series_read_us / 1000.0 << "ms";
            ss << ", BlockLookup: " << block_lookup_us / 1000.0 << "ms";
            ss << ", BlockRead: " << block_read_us / 1000.0 << "ms";
            ss << ", LockWait: " << block_lock_wait_us / 1000.0 << "ms";
            ss << ", Decomp: " << decompression_us / 1000.0 << "ms";
            ss << ", Sort: " << sorting_us / 1000.0 << "ms";
            ss << ", Pruning: " << pruning_time_us / 1000.0 << "ms)";
            ss << ", Samples: " << samples_scanned;
            ss << ", Blocks: " << blocks_accessed;
            ss << ", RG(Total/Time/Tags/Read): " << row_groups_total << "/" << row_groups_pruned_time << "/" << row_groups_pruned_tags << "/" << row_groups_read;
            ss << ", Bytes(Skip/Read): " << bytes_skipped << "/" << bytes_read;
            ss << ", Samples: " << samples_scanned;
            ss << ", Blocks: " << blocks_accessed;
            ss << ", CacheHit: " << (cache_hit ? "Yes" : "No");
            ss << ", BloomFilter: " << (bloom_filter_used ? "Yes" : "No");
            if (bloom_filter_used) {
                ss << " (lookup: " << bloom_filter_lookup_us / 1000.0 << "ms";
                ss << ", checks: " << bloom_filter_checks;
                ss << ", skips: " << bloom_filter_skips;
                ss << ", passes: " << bloom_filter_passes << ")";
            }
            ss << ", SecondaryIdx: " << (secondary_index_used ? "Yes" : "No");
            if (secondary_index_used) {
                ss << " (lookup: " << secondary_index_lookup_us / 1000.0 << "ms";
                ss << ", hits: " << secondary_index_hits;
                ss << ", rg_selected: " << secondary_index_row_groups_selected << ")";
            }
            return ss.str();
        }
    };

    static ReadPerformanceInstrumentation& instance() {
        static ReadPerformanceInstrumentation inst;
        return inst;
    }

    // Thread-local context for detailed query tracing without interface changes
    static void SetCurrentMetrics(ReadMetrics* metrics) {
        GetTLSMetrics() = metrics;
    }
    
    static ReadMetrics* GetCurrentMetrics() {
        return GetTLSMetrics();
    }

private:
    static ReadMetrics*& GetTLSMetrics() {
        static thread_local ReadMetrics* metrics = nullptr;
        return metrics;
    }

public:

    void enable() { enabled_ = true; }
    void disable() { enabled_ = false; }
    bool is_enabled() const { return enabled_; }


    void record_read(const ReadMetrics& metrics) {
        if (!enabled_) return;

        std::lock_guard<std::mutex> lock(stats_mutex_);
        total_reads_++;
        total_time_us_ += metrics.total_us;
        total_index_search_us_ += metrics.index_search_us;
        total_block_lookup_us_ += metrics.block_lookup_us;
        total_block_read_us_ += metrics.block_read_us;
        total_decompression_us_ += metrics.decompression_us;
        total_samples_scanned_ += metrics.samples_scanned;
        total_blocks_accessed_ += metrics.blocks_accessed;
        if (metrics.cache_hit) cache_hits_++;
        
        row_groups_total_ += metrics.row_groups_total;
        row_groups_pruned_time_ += metrics.row_groups_pruned_time;
        row_groups_pruned_tags_ += metrics.row_groups_pruned_tags;
        row_groups_read_ += metrics.row_groups_read;
        bytes_skipped_ += metrics.bytes_skipped;
        bytes_read_ += metrics.bytes_read;
        
        // Active Series Metrics
        total_active_series_lookup_us_ += metrics.active_series_lookup_us;
        total_active_series_read_us_ += metrics.active_series_read_us;
        
        // Detailed Breakdown
        total_row_group_read_us_ += metrics.row_group_read_us;
        total_decoding_us_ += metrics.decoding_us;
        total_processing_us_ += metrics.processing_us;
        
        total_block_filter_us_ += metrics.block_filter_us;
        total_data_extraction_us_ += metrics.data_extraction_us;
        total_result_construction_us_ += metrics.result_construction_us;
        total_data_copy_us_ += metrics.data_copy_us;
        
        // Secondary Index aggregation
        if (metrics.secondary_index_used) {
            secondary_index_lookups_++;
            secondary_index_hits_ += metrics.secondary_index_hits;
            secondary_index_lookup_time_us_ += metrics.secondary_index_lookup_us;
            secondary_index_build_time_us_ += metrics.secondary_index_build_us;
            secondary_index_row_groups_selected_ += metrics.secondary_index_row_groups_selected;
        } else if (metrics.row_groups_total > 0) {
            // Index was not used but we had row groups to scan
            secondary_index_misses_++;
        }
        
        // Bloom Filter aggregation
        if (metrics.bloom_filter_used) {
            bloom_filter_checks_ += metrics.bloom_filter_checks;
            bloom_filter_skips_ += metrics.bloom_filter_skips;
            bloom_filter_passes_ += metrics.bloom_filter_passes;
            bloom_filter_lookup_time_us_ += metrics.bloom_filter_lookup_us;
        }
        
        // Record to GlobalMetrics for self-monitoring
        size_t bytes = metrics.bytes_read > 0 ? metrics.bytes_read : metrics.samples_scanned * sizeof(double);
        tsdb::storage::internal::GlobalMetrics::getInstance().recordRead(
            bytes,
            static_cast<uint64_t>(metrics.total_us * 1000) // Convert us to ns
        );
        
        if (metrics.cache_hit) {
            tsdb::storage::internal::GlobalMetrics::getInstance().recordCacheHit();
        } else {
            tsdb::storage::internal::GlobalMetrics::getInstance().recordCacheMiss();
        }
    }
    
    /**
     * @brief Record secondary index usage directly (for read_columns path)
     * This is called when secondary index is used in read_columns() which doesn't
     * use the full ReadPerformanceMetrics struct.
     * 
     * @param hit True if index found the series, false if miss
     * @param lookup_time_us Time spent in index lookup
     * @param row_groups_selected Number of row groups selected by index
     */
    void recordSecondaryIndexUsage(bool hit, double lookup_time_us = 0, size_t row_groups_selected = 0) {
        if (!enabled_.load(std::memory_order_relaxed)) return;
        std::lock_guard<std::mutex> lock(stats_mutex_);
        secondary_index_lookups_++;
        if (hit) {
            secondary_index_hits_++;
            secondary_index_row_groups_selected_ += row_groups_selected;
        } else {
            secondary_index_misses_++;
        }
        secondary_index_lookup_time_us_ += lookup_time_us;
    }
    
    /**
     * @brief Record Bloom filter usage directly (for Phase 0 filtering)
     * This is called when bloom filter is checked before B+ Tree lookup.
     * 
     * @param skipped True if series was definitely not present (file skipped)
     * @param lookup_time_us Time spent checking bloom filter
     */
    void recordBloomFilterUsage(bool skipped, double lookup_time_us = 0) {
        if (!enabled_.load(std::memory_order_relaxed)) return;
        std::lock_guard<std::mutex> lock(stats_mutex_);
        bloom_filter_checks_++;
        if (skipped) {
            bloom_filter_skips_++;  // Definite not present - file skipped
        } else {
            bloom_filter_passes_++;  // Might be present - proceed to B+ Tree
        }
        bloom_filter_lookup_time_us_ += lookup_time_us;
    }

    // Getters for SelfMonitor
    struct AggregateStats {
        uint64_t total_reads;
        double total_time_us;
        double total_index_search_us;
        double total_block_lookup_us;
        double total_block_read_us;
        double total_decompression_us;
        uint64_t total_samples_scanned;
        uint64_t total_blocks_accessed;
        uint64_t cache_hits;
        
        // Parquet Metrics
        uint64_t row_groups_total;
        uint64_t row_groups_pruned_time;
        uint64_t row_groups_pruned_tags;
        uint64_t row_groups_read;
        uint64_t bytes_skipped;
        uint64_t bytes_read;
        
        // Active Series Metrics
        double total_active_series_lookup_us;
        double total_active_series_read_us;
        
        // Detailed Breakdown
        double total_row_group_read_us;
        double total_decoding_us;
        double total_processing_us;
        
        double total_block_filter_us;
        double total_data_extraction_us;
        double total_result_construction_us;
        double total_data_copy_us;
        
        // Secondary Index Metrics
        uint64_t secondary_index_lookups;
        uint64_t secondary_index_hits;
        uint64_t secondary_index_misses;
        double secondary_index_lookup_time_us;
        double secondary_index_build_time_us;
        uint64_t secondary_index_row_groups_selected;
        
        // Bloom Filter Metrics (Phase 0: Pre-B+ Tree filtering)
        uint64_t bloom_filter_checks;
        uint64_t bloom_filter_skips;
        uint64_t bloom_filter_passes;
        double bloom_filter_lookup_time_us;
    };

    AggregateStats get_stats() const {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        return {
            total_reads_,
            total_time_us_,
            total_index_search_us_,
            total_block_lookup_us_,
            total_block_read_us_,
            total_decompression_us_,
            total_samples_scanned_,
            total_blocks_accessed_,
            cache_hits_,
            row_groups_total_,
            row_groups_pruned_time_,
            row_groups_pruned_tags_,
            row_groups_read_,
            bytes_skipped_,
            bytes_read_,
            total_active_series_lookup_us_,
            total_active_series_read_us_,
            total_row_group_read_us_,
            total_decoding_us_,
            total_processing_us_,
            total_block_filter_us_,
            total_data_extraction_us_,
            total_result_construction_us_,
            total_data_copy_us_,
            secondary_index_lookups_,
            secondary_index_hits_,
            secondary_index_misses_,
            secondary_index_lookup_time_us_,
            secondary_index_build_time_us_,
            secondary_index_row_groups_selected_,
            bloom_filter_checks_,
            bloom_filter_skips_,
            bloom_filter_passes_,
            bloom_filter_lookup_time_us_
        };
    }

    void reset_stats() {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        total_reads_ = 0;
        total_time_us_ = 0;
        total_index_search_us_ = 0;
        total_block_lookup_us_ = 0;
        total_block_read_us_ = 0;
        total_decompression_us_ = 0;
        total_row_group_read_us_ = 0;
        total_decoding_us_ = 0;
        total_processing_us_ = 0;
        
        total_block_filter_us_ = 0;
        total_data_extraction_us_ = 0;
        total_result_construction_us_ = 0;
        total_data_copy_us_ = 0;
        
        total_samples_scanned_ = 0;
        total_blocks_accessed_ = 0;
        cache_hits_ = 0;
        
        row_groups_total_ = 0;
        row_groups_pruned_time_ = 0;
        row_groups_pruned_tags_ = 0;
        row_groups_read_ = 0;
        bytes_skipped_ = 0;
        bytes_read_ = 0;
        
        secondary_index_lookups_ = 0;
        secondary_index_hits_ = 0;
        secondary_index_misses_ = 0;
        secondary_index_lookup_time_us_ = 0;
        secondary_index_build_time_us_ = 0;
        secondary_index_row_groups_selected_ = 0;
        
        bloom_filter_checks_ = 0;
        bloom_filter_skips_ = 0;
        bloom_filter_passes_ = 0;
        bloom_filter_lookup_time_us_ = 0;
    }

private:
    ReadPerformanceInstrumentation() : enabled_(true) {}

    std::atomic<bool> enabled_;
    mutable std::mutex stats_mutex_;
    
    uint64_t total_reads_ = 0;
    double total_time_us_ = 0;
    double total_index_search_us_ = 0;
    double total_block_lookup_us_ = 0;
    double total_block_read_us_ = 0;
    double total_decompression_us_ = 0;
    double total_row_group_read_us_ = 0;
    double total_decoding_us_ = 0;
    double total_processing_us_ = 0;
    
    double total_block_filter_us_ = 0;
    double total_data_extraction_us_ = 0;
    double total_result_construction_us_ = 0;
    double total_data_copy_us_ = 0;
    
    uint64_t total_samples_scanned_ = 0;
    uint64_t total_blocks_accessed_ = 0;
    uint64_t cache_hits_ = 0;
    
    // Parquet Metrics
    uint64_t row_groups_total_ = 0;
    uint64_t row_groups_pruned_time_ = 0;
    uint64_t row_groups_pruned_tags_ = 0;
    uint64_t row_groups_read_ = 0;
    uint64_t bytes_skipped_ = 0;
    uint64_t bytes_read_ = 0;
    
    // Active Series Metrics
    double total_active_series_lookup_us_ = 0;
    double total_active_series_read_us_ = 0;
    
    // Secondary Index Metrics
    uint64_t secondary_index_lookups_ = 0;
    uint64_t secondary_index_hits_ = 0;
    uint64_t secondary_index_misses_ = 0;
    double secondary_index_lookup_time_us_ = 0;
    double secondary_index_build_time_us_ = 0;
    uint64_t secondary_index_row_groups_selected_ = 0;
    
    // Bloom Filter Metrics (Phase 0: Pre-B+ Tree filtering)
    uint64_t bloom_filter_checks_ = 0;
    uint64_t bloom_filter_skips_ = 0;
    uint64_t bloom_filter_passes_ = 0;
    double bloom_filter_lookup_time_us_ = 0;
};

class ReadScopedTimer {
public:
    ReadScopedTimer(double& output_us, bool enabled = true)
        : output_us_(output_us), enabled_(enabled), start_(std::chrono::high_resolution_clock::now()) {}
    
    ~ReadScopedTimer() {
        stop();
    }

    void stop() {
        if (enabled_ && !stopped_) {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_);
            output_us_ += duration.count() / 1000.0;  // Add to existing value
            stopped_ = true;
        }
    }
    
private:
    double& output_us_;
    bool enabled_;
    std::chrono::high_resolution_clock::time_point start_;
    bool stopped_ = false;
};

} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_READ_PERFORMANCE_INSTRUMENTATION_H_
