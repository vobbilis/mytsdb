#ifndef TSDB_STORAGE_PARQUET_CATALOG_H_
#define TSDB_STORAGE_PARQUET_CATALOG_H_

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <memory>
#include "tsdb/storage/parquet/reader.hpp"

namespace tsdb {
namespace storage {

class ParquetCatalog {
public:
    struct FileMeta {
        std::string path;
        int64_t min_time;
        int64_t max_time;
        std::vector<parquet::ParquetReader::RowGroupStats> row_groups;
        int64_t file_size;
    };

    static ParquetCatalog& instance();

    // Get metadata for a file. If not cached, it will be indexed.
    std::shared_ptr<FileMeta> GetFileMeta(const std::string& path);

    // Explicitly index a file (useful for background indexing)
    void IndexFile(const std::string& path);

    // Remove file from catalog (e.g. on compaction/deletion)
    void EvictFile(const std::string& path);
    
    // Clear entire catalog
    void Clear();

private:
    ParquetCatalog() = default;
    
    std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<FileMeta>> cache_;
};

} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_PARQUET_CATALOG_H_
