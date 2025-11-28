#include "tsdb/storage/sharded_wal.h"
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <iostream>

namespace tsdb {
namespace storage {

ShardedWAL::ShardedWAL(const std::string& base_dir, size_t num_shards)
    : num_shards_(num_shards), base_dir_(base_dir) {
    
    // Create base directory if it doesn't exist
    std::filesystem::create_directories(base_dir);
    
    // Initialize shards
    shards_.reserve(num_shards_);
    for (size_t i = 0; i < num_shards_; ++i) {
        std::ostringstream oss;
        oss << base_dir_ << "/shard_" << std::setfill('0') << std::setw(3) << i;
        shards_.push_back(std::make_unique<AsyncWALShard>(oss.str()));
    }
}

core::Result<void> ShardedWAL::log(const core::TimeSeries& series) {
    size_t shard_idx = get_shard_index(series);
    
    auto result = shards_[shard_idx]->log(series);
    
    if (result.ok()) {
        metrics_.total_writes++;
        // We don't easily know bytes here without serializing, 
        // but we could add it if WAL::log returned bytes written.
    } else {
        metrics_.total_errors++;
    }
    
    return result;
}

core::Result<void> ShardedWAL::replay(std::function<void(const core::TimeSeries&)> callback) {
    // Replay all shards
    // In a real implementation, this could be parallelized
    for (const auto& shard : shards_) {
        auto result = shard->replay(callback);
        if (!result.ok()) {
            return result;
        }
    }
    return core::Result<void>();
}

core::Result<void> ShardedWAL::checkpoint(int last_segment_to_keep) {
    for (const auto& shard : shards_) {
        auto result = shard->checkpoint(last_segment_to_keep);
        if (!result.ok()) {
            return result;
        }
    }
    return core::Result<void>();
}

WALStats ShardedWAL::get_stats() const {
    return WALStats{
        metrics_.total_writes.load(),
        metrics_.total_bytes.load(),
        metrics_.total_errors.load()
    };
}

void ShardedWAL::flush() {
    for (const auto& shard : shards_) {
        shard->flush();
    }
}

size_t ShardedWAL::get_shard_index(const core::TimeSeries& series) const {
    // Simple hash of labels to pick a shard
    // We want the same series to always go to the same shard
    // to ensure ordering if we were to rely on it (though WAL is mostly for recovery)
    
    size_t hash = 0;
    for (const auto& [key, value] : series.labels().map()) {
        // Combine hash of key and value
        hash ^= std::hash<std::string>{}(key) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<std::string>{}(value) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
    
    return hash % num_shards_;
}

} // namespace storage
} // namespace tsdb
