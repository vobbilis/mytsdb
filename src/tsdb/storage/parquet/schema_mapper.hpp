#pragma once

#include <arrow/api.h>
#include <memory>
#include <vector>
#include <map>
#include <string>
#include "tsdb/core/types.h"
#include "tsdb/core/result.h"

namespace tsdb {
namespace storage {
namespace parquet {

class SchemaMapper {
public:
    // Returns the fixed Arrow schema for our time series data
    // Gets the Arrow schema used for conversion
    static std::shared_ptr<arrow::Schema> GetArrowSchema();

    // Converts internal samples to Arrow RecordBatch
    static std::shared_ptr<arrow::RecordBatch> ToRecordBatch(
        const std::vector<tsdb::core::Sample>& samples,
        const std::map<std::string, std::string>& tags
    );

    // Converts Arrow RecordBatch to internal samples
    static core::Result<std::vector<core::Sample>> ToSamples(
        std::shared_ptr<arrow::RecordBatch> batch);

    // Extracts tags from Arrow RecordBatch (assumes all rows have same tags for now)
    static core::Result<std::map<std::string, std::string>> ExtractTags(
        std::shared_ptr<arrow::RecordBatch> batch);
};

} // namespace parquet
} // namespace storage
} // namespace tsdb
