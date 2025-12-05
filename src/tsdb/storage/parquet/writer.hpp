#pragma once

#include <arrow/api.h>
#include <parquet/arrow/writer.h>
#include <string>
#include <memory>
#include "tsdb/core/result.h"

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



    // Closes the file and writes the footer
    core::Result<void> Close();

private:
    std::shared_ptr<arrow::io::FileOutputStream> outfile_;
    std::unique_ptr<::parquet::arrow::FileWriter> writer_;
    std::shared_ptr<arrow::Schema> schema_;
};

} // namespace parquet
} // namespace storage
} // namespace tsdb
