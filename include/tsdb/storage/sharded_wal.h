#ifndef TSDB_STORAGE_SHARDED_WAL_H_
#define TSDB_STORAGE_SHARDED_WAL_H_

#include <vector>
#include <memory>
#include <string>
#include <atomic>
#include <functional>
#include "tsdb/storage/async_wal_shard.h"
#include "tsdb/core/result.h"
#include "tsdb/core/types.h"

namespace tsdb {
namespace storage {

struct WALStats {
    uint64_t total_writes;
    uint64_t total_bytes;
    uint64_t total_errors;
};

struct WALMetrics {
    std::atomic<uint64_t> total_writes{0};
    std::atomic<uint64_t> total_bytes{0};
    std::atomic<uint64_t> total_errors{0};
};

class ShardedWAL {
public:
    // Initialize with base directory and number of shards
    ShardedWAL(const std::string& base_dir, size_t num_shards = 16);
    
    // Log series to the appropriate shard
    core::Result<void> log(const core::TimeSeries& series);
    
    // Replay all shards
    core::Result<void> replay(std::function<void(const core::TimeSeries&)> callback);
    
    // Checkpoint all shards
    core::Result<void> checkpoint(int last_segment_to_keep);
    
    // Get stats
    WALStats get_stats() const;
    
    // Flush all shards (force immediate persistence)
    void flush();


private:
    const size_t num_shards_;
    std::string base_dir_;
    std::vector<std::unique_ptr<AsyncWALShard>> shards_;
    mutable WALMetrics metrics_;
    
    // Helper to determine shard index
    size_t get_shard_index(const core::TimeSeries& series) const;
};

} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_SHARDED_WAL_H_
