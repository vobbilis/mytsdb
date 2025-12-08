#ifndef TSDB_STORAGE_SHARDED_INDEX_H_
#define TSDB_STORAGE_SHARDED_INDEX_H_

#include <vector>
#include <memory>
#include <atomic>
#include "tsdb/storage/index.h"
#include "tsdb/core/result.h"
#include "tsdb/core/types.h"

namespace tsdb {
namespace storage {

struct IndexStats {
    uint64_t total_series;
    uint64_t total_lookups;
};

struct IndexMetrics {
    std::atomic<uint64_t> total_series{0};
    std::atomic<uint64_t> total_lookups{0};
};

class ShardedIndex {
public:
    ShardedIndex(size_t num_shards = 16);
    
    // Add series to appropriate shard
    core::Result<void> add_series(core::SeriesID id, const core::Labels& labels);
    
    // Remove series from appropriate shard
    core::Result<void> remove_series(core::SeriesID id);
    
#include "tsdb/core/matcher.h"

// ...

    // Find series matching matchers (scatter-gather)
    core::Result<std::vector<core::SeriesID>> find_series(
        const std::vector<core::LabelMatcher>& matchers);
        
    // Get labels for a series ID
    core::Result<core::Labels> get_labels(core::SeriesID id);
    
    // Optimized: Returns series IDs with their labels in a single operation
    core::Result<std::vector<std::pair<core::SeriesID, core::Labels>>> find_series_with_labels(
        const std::vector<core::LabelMatcher>& matchers);
    
    // Get stats
    IndexStats get_stats() const;

private:
    const size_t num_shards_;
    std::vector<std::unique_ptr<Index>> shards_;
    mutable IndexMetrics metrics_;
    
    size_t get_shard_index(core::SeriesID id) const;
};

} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_SHARDED_INDEX_H_
