#pragma once

#include <arrow/api.h>
#include <parquet/arrow/reader.h>
#include <string>
#include <memory>
#include "tsdb/core/result.h"

namespace tsdb {
namespace storage {
namespace parquet {

class ParquetReader {
public:
    ParquetReader() = default;
    ~ParquetReader();

    // Opens a Parquet file for reading
    core::Result<void> Open(const std::string& path);

    // Reads the next RecordBatch from the file
    // Returns nullptr in out_batch if end of file
    core::Result<void> ReadBatch(std::shared_ptr<arrow::RecordBatch>* out_batch);

    // Closes the file
    core::Result<void> Close();
    
    // Predicate pushdown support
    int GetNumRowGroups() const;
    core::Result<std::shared_ptr<arrow::RecordBatch>> ReadRowGroupTags(int row_group_index);
    core::Result<std::shared_ptr<arrow::RecordBatch>> ReadRowGroup(int row_group_index);

private:
    std::shared_ptr<arrow::io::ReadableFile> infile_;
    std::unique_ptr<::parquet::arrow::FileReader> reader_;
    int current_row_group_ = 0;
};

} // namespace parquet
} // namespace storage
} // namespace tsdb
