#ifndef TSDB_STORAGE_WAL_H_
#define TSDB_STORAGE_WAL_H_

#include <string>
#include <functional>
#include <fstream>
#include <vector>
#include <optional>
#include <mutex>
#include "tsdb/core/types.h"
#include "tsdb/core/result.h"

namespace tsdb {
namespace storage {

class WriteAheadLog {
public:
    WriteAheadLog(const std::string& dir);
    ~WriteAheadLog();
    
    core::Result<void> log(const core::TimeSeries& series, bool flush_now = true); // Logs new samples
    core::Result<void> flush(); // Force flush to disk
    core::Result<void> replay(std::function<void(const core::TimeSeries&)> callback); // Used on startup
    core::Result<void> checkpoint(int last_segment_to_keep); // Deletes old log files

private:
    std::string wal_dir_;
    int current_segment_;
    std::ofstream current_file_;
    mutable std::mutex mutex_;  // Protects concurrent access to WAL
    
    std::vector<uint8_t> serialize_series(const core::TimeSeries& series);
    bool write_to_segment(const std::vector<uint8_t>& data, bool flush_now);
    void rotate_segment();
    std::string get_segment_path(int segment);
    core::Result<void> replay_segment(const std::string& segment_path, 
                                     std::function<void(const core::TimeSeries&)> callback);
    std::optional<core::TimeSeries> deserialize_series(const std::vector<uint8_t>& data);
};

} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_WAL_H_
