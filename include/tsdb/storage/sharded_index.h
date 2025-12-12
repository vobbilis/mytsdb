#ifndef TSDB_STORAGE_SHARDED_INDEX_H_
#define TSDB_STORAGE_SHARDED_INDEX_H_

#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>
#include "tsdb/storage/index.h"
#include "tsdb/storage/index_metrics.h"
#include "tsdb/core/result.h"
#include "tsdb/core/types.h"
#include "tsdb/core/matcher.h"

namespace tsdb {
namespace storage {

// Basic stats for external API compatibility
struct IndexStats {
    uint64_t total_series;
    uint64_t total_lookups;
};

// Aggregated metrics from all shards
struct AggregatedIndexMetrics {
    // Counts
    uint64_t total_add_count{0};
    uint64_t total_lookup_count{0};
    uint64_t total_intersect_count{0};
    
    // Timing in microseconds
    uint64_t total_add_time_us{0};
    uint64_t total_lookup_time_us{0};
    uint64_t total_intersect_time_us{0};
    
    // Computed averages in microseconds
    double avg_add_time_us() const { 
        return total_add_count > 0 ? static_cast<double>(total_add_time_us) / total_add_count : 0.0; 
    }
    double avg_lookup_time_us() const { 
        return total_lookup_count > 0 ? static_cast<double>(total_lookup_time_us) / total_lookup_count : 0.0; 
    }
    double avg_intersect_time_us() const { 
        return total_intersect_count > 0 ? static_cast<double>(total_intersect_time_us) / total_intersect_count : 0.0; 
    }
};

class ShardedIndex {
public:
    ShardedIndex(size_t num_shards = 16);
    
    // Add series to appropriate shard
    core::Result<void> add_series(core::SeriesID id, const core::Labels& labels);
    
    // Remove series from appropriate shard
    core::Result<void> remove_series(core::SeriesID id);
    
    // Find series matching matchers (scatter-gather)
    core::Result<std::vector<core::SeriesID>> find_series(
        const std::vector<core::LabelMatcher>& matchers);
        
    // Get labels for a series ID
    core::Result<core::Labels> get_labels(core::SeriesID id);
    
    // Optimized: Returns series IDs with their labels in a single operation
    core::Result<std::vector<std::pair<core::SeriesID, core::Labels>>> find_series_with_labels(
        const std::vector<core::LabelMatcher>& matchers);
    
    // Get basic stats (for backward compatibility)
    IndexStats get_stats() const;
    
    // Get detailed aggregated metrics from all shards
    AggregatedIndexMetrics get_aggregated_metrics() const;
    
    // Reset metrics across all shards
    void reset_metrics();

    // ---- Test/observability helpers ----
    // Counts how many shard-level queries were actually executed by ShardedIndex.
    // Useful to validate shard routing optimizations.
    std::vector<uint64_t> get_shard_query_counts() const;
    void reset_shard_query_counts();

private:
    const size_t num_shards_;
    std::vector<std::unique_ptr<Index>> shards_;
    mutable std::atomic<uint64_t> total_series_{0};
    mutable std::atomic<uint64_t> total_lookups_{0};

    // Step 1.3: routing structure for __name__="metric" queries.
    // Maintain per-metric per-shard counts so we can route queries to only shards
    // that contain the metric.
    mutable std::mutex routing_mutex_;
    std::unordered_map<std::string, std::vector<uint32_t>> metric_shard_counts_;

    // Per-shard query counters (for tests/observability)
    std::unique_ptr<std::atomic<uint64_t>[]> shard_query_counts_;
    
    size_t get_shard_index(core::SeriesID id) const;
    std::vector<size_t> get_routed_shards_for_metric(const std::string& metric_name) const;
};

} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_SHARDED_INDEX_H_
