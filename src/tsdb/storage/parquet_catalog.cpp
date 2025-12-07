#include "tsdb/storage/parquet_catalog.h"
#include <spdlog/spdlog.h>

namespace tsdb {
namespace storage {

ParquetCatalog& ParquetCatalog::instance() {
    static ParquetCatalog inst;
    return inst;
}

std::shared_ptr<ParquetCatalog::FileMeta> ParquetCatalog::GetFileMeta(const std::string& path) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(path);
        if (it != cache_.end()) {
            return it->second;
        }
    }
    
    // Not found, index it
    IndexFile(path);
    
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_.find(path);
    if (it != cache_.end()) {
        return it->second;
    }
    return nullptr;
}

void ParquetCatalog::IndexFile(const std::string& path) {
    parquet::ParquetReader reader;
    auto status = reader.Open(path);
    if (!status.ok()) {
        spdlog::error("Failed to index file {}: {}", path, status.error());
        return;
    }
    
    auto meta = std::make_shared<FileMeta>();
    meta->path = path;
    meta->row_groups.reserve(reader.GetNumRowGroups());
    
    int64_t min_t = std::numeric_limits<int64_t>::max();
    int64_t max_t = std::numeric_limits<int64_t>::min();
    
    for (int i = 0; i < reader.GetNumRowGroups(); ++i) {
        auto stats_res = reader.GetRowGroupStats(i);
        if (stats_res.ok()) {
            auto stats = stats_res.value();
            meta->row_groups.push_back(stats);
            if (stats.min_timestamp < min_t) min_t = stats.min_timestamp;
            if (stats.max_timestamp > max_t) max_t = stats.max_timestamp;
        } else {
            // If we can't get stats, we should probably assume full range or fail?
            // For now, push a dummy stat or handle error
            // Let's assume full range for safety if stats missing
            parquet::ParquetReader::RowGroupStats dummy;
            dummy.min_timestamp = std::numeric_limits<int64_t>::min();
            dummy.max_timestamp = std::numeric_limits<int64_t>::max();
            dummy.num_rows = 0; // Unknown
            dummy.total_byte_size = 0;
            meta->row_groups.push_back(dummy);
            min_t = std::numeric_limits<int64_t>::min();
            max_t = std::numeric_limits<int64_t>::max();
        }
    }
    meta->min_time = min_t;
    meta->max_time = max_t;
    
    std::lock_guard<std::mutex> lock(mutex_);
    cache_[path] = meta;
    spdlog::info("Indexed file {}: {} row groups, time range {}-{}", path, meta->row_groups.size(), min_t, max_t);
}

void ParquetCatalog::EvictFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.erase(path);
}

void ParquetCatalog::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
}

} // namespace storage
} // namespace tsdb
