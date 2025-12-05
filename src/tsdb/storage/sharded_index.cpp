#include "tsdb/storage/sharded_index.h"
#include <iostream>

namespace tsdb {
namespace storage {

ShardedIndex::ShardedIndex(size_t num_shards)
    : num_shards_(num_shards) {
    
    shards_.reserve(num_shards_);
    for (size_t i = 0; i < num_shards_; ++i) {
        shards_.push_back(std::make_unique<Index>());
    }
}

core::Result<void> ShardedIndex::add_series(core::SeriesID id, const core::Labels& labels) {
    size_t shard_idx = get_shard_index(id);
    auto result = shards_[shard_idx]->add_series(id, labels);
    
    if (result.ok()) {
        metrics_.total_series++;
    }
    
    return result;
    return result;
}

core::Result<void> ShardedIndex::remove_series(core::SeriesID id) {
    size_t shard_idx = get_shard_index(id);
    auto result = shards_[shard_idx]->remove_series(id);
    
    if (result.ok()) {
        metrics_.total_series--;
    }
    
    return result;
}

core::Result<std::vector<core::SeriesID>> ShardedIndex::find_series(
    const std::vector<core::LabelMatcher>& matchers) {
    
    metrics_.total_lookups++;
    
    std::vector<core::SeriesID> result;
    
    // Scatter-Gather: Query all shards
    // In a real implementation, this could be parallelized
    for (const auto& shard : shards_) {
        auto shard_result = shard->find_series(matchers);
        if (shard_result.ok()) {
            const auto& shard_ids = shard_result.value();
            result.insert(result.end(), shard_ids.begin(), shard_ids.end());
        }
    }
    
    return core::Result<std::vector<core::SeriesID>>(result);
}

core::Result<core::Labels> ShardedIndex::get_labels(core::SeriesID id) {
    size_t shard_idx = get_shard_index(id);
    return shards_[shard_idx]->get_labels(id);
}

IndexStats ShardedIndex::get_stats() const {
    return IndexStats{
        metrics_.total_series.load(),
        metrics_.total_lookups.load()
    };
}

size_t ShardedIndex::get_shard_index(core::SeriesID id) const {
    // Simple modulo sharding for integer IDs
    return id % num_shards_;
}

} // namespace storage
} // namespace tsdb
