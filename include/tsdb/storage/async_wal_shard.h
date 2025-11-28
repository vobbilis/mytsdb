#ifndef TSDB_STORAGE_ASYNC_WAL_SHARD_H_
#define TSDB_STORAGE_ASYNC_WAL_SHARD_H_

#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include "tsdb/storage/wal.h"
#include "tsdb/core/types.h"
#include "tsdb/core/result.h"

namespace tsdb {
namespace storage {

class AsyncWALShard {
public:
    AsyncWALShard(const std::string& dir);
    ~AsyncWALShard();

    core::Result<void> log(const core::TimeSeries& series);
    core::Result<void> replay(std::function<void(const core::TimeSeries&)> callback);
    core::Result<void> checkpoint(int last_segment_to_keep);
    
    // Flush pending writes (for testing)
    void flush();


private:
    void worker_loop();

    std::unique_ptr<WriteAheadLog> wal_;
    std::string dir_;
    
    // Queue
    std::queue<core::TimeSeries> queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    
    // Worker
    std::thread worker_;
    std::atomic<bool> running_;
};

} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_ASYNC_WAL_SHARD_H_
