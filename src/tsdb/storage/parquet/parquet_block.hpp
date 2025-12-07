#ifndef TSDB_STORAGE_PARQUET_BLOCK_HPP
#define TSDB_STORAGE_PARQUET_BLOCK_HPP

#include "tsdb/storage/internal/block_internal.h"
#include "tsdb/storage/parquet/reader.hpp"
#include <string>
#include <memory>
#include <mutex>

namespace tsdb {
namespace storage {
namespace parquet {

/**
 * @brief Block implementation backed by a Parquet file.
 * 
 * This class implements the BlockInternal interface but delegates read operations
 * to a ParquetReader. Write operations are not supported (read-only).
 */
class ParquetBlock : public internal::BlockInternal {
public:
    ParquetBlock(const internal::BlockHeader& header, const std::string& path);
    ~ParquetBlock() override = default;

    // Block interface
    size_t size() const override;
    size_t num_series() const override;
    size_t num_samples() const override;
    int64_t start_time() const override;
    int64_t end_time() const override;
    
    core::TimeSeries read(const core::Labels& labels) const override;
    
    std::vector<core::TimeSeries> query(
        const std::vector<std::pair<std::string, std::string>>& matchers,
        int64_t start_time,
        int64_t end_time) const override;
        
    void flush() override;
    void close() override;

    // BlockInternal interface
    std::pair<std::vector<int64_t>, std::vector<double>> read_columns(const core::Labels& labels) const override;
    void write(const core::TimeSeries& series) override;
    const internal::BlockHeader& header() const override;
    
    // Parquet specific
    const std::string& path() const { return path_; }

private:
    internal::BlockHeader header_;
    std::string path_;
    mutable std::unique_ptr<ParquetReader> reader_;
    mutable std::mutex mutex_;
    
    // Helper to get or create reader
    ParquetReader* get_reader() const;
};

} // namespace parquet
} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_PARQUET_BLOCK_HPP
