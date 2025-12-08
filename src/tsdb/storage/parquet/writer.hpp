#pragma once

#include <arrow/api.h>
#include <parquet/arrow/writer.h>
#include <string>
#include <memory>
#include "tsdb/core/result.h"
#include "tsdb/storage/parquet/bloom_filter_manager.h"

namespace tsdb {
namespace storage {
namespace parquet {

class ParquetWriter {
public:
    ParquetWriter() = default;
    ~ParquetWriter();

    // Opens a new Parquet file for writing
    core::Result<void> Open(const std::string& path, std::shared_ptr<arrow::Schema> schema, int64_t max_row_group_length = 64 * 1024 * 1024);

    // Writes a RecordBatch to the file
    core::Result<void> WriteBatch(std::shared_ptr<arrow::RecordBatch> batch);

    /**
     * @brief Add a series to the Bloom filter (call for each series written)
     * @param series_id The series ID to add
     */
    void AddSeriesToBloomFilter(core::SeriesID series_id);
    
    /**
     * @brief Add a series by labels string
     * @param labels_str Canonical label string (e.g., "name=cpu,pod=p1")
     */
    void AddSeriesToBloomFilterByLabels(const std::string& labels_str);

    // Closes the file and writes the footer (also saves Bloom filter)
    core::Result<void> Close();
    
    /**
     * @brief Get the path of the file being written
     */
    const std::string& GetPath() const { return path_; }

private:
    std::string path_;
    std::shared_ptr<arrow::io::FileOutputStream> outfile_;
    std::unique_ptr<::parquet::arrow::FileWriter> writer_;
    std::shared_ptr<arrow::Schema> schema_;
    
    // Bloom filter for this file
    std::unique_ptr<BloomFilterManager> bloom_filter_;
};

} // namespace parquet
} // namespace storage
} // namespace tsdb
