#include "tsdb/storage/parquet/writer.hpp"
#include <arrow/io/file.h>
#include <parquet/arrow/writer.h>

namespace tsdb {
namespace storage {
namespace parquet {

ParquetWriter::~ParquetWriter() {
    if (writer_) {
        // Ensure closed on destruction, though Close() should be called explicitly
        // We ignore errors in destructor
        (void)writer_->Close();
    }
    if (outfile_) {
        (void)outfile_->Close();
    }
}

core::Result<void> ParquetWriter::Open(const std::string& path, std::shared_ptr<arrow::Schema> schema, int64_t max_row_group_length) {
    schema_ = schema;
    
    // Open file output stream
    auto outfile_result = arrow::io::FileOutputStream::Open(path);
    if (!outfile_result.ok()) {
        return core::Result<void>::error("Failed to open file: " + outfile_result.status().ToString());
    }
    outfile_ = *outfile_result;

    // Create Parquet writer
    // Configure Writer Properties
    ::parquet::WriterProperties::Builder builder;
    builder.compression(::parquet::Compression::ZSTD);
    builder.enable_dictionary();
    builder.max_row_group_length(max_row_group_length);
    
    std::shared_ptr<::parquet::WriterProperties> props = builder.build();

    auto result = ::parquet::arrow::FileWriter::Open(
        *schema_,
        arrow::default_memory_pool(),
        outfile_,
        props,
        ::parquet::default_arrow_writer_properties()
    );

    if (!result.ok()) {
        return core::Result<void>::error("Failed to create Parquet writer: " + result.status().ToString());
    }
    writer_ = std::move(result).ValueOrDie();

    return core::Result<void>();
}

core::Result<void> ParquetWriter::WriteBatch(std::shared_ptr<arrow::RecordBatch> batch) {
    if (!writer_) {
        return core::Result<void>::error("Writer not open");
    }

    auto status = writer_->WriteRecordBatch(*batch);
    if (!status.ok()) {
        std::string err = "Failed to write batch: " + status.ToString();
        fprintf(stderr, "%s\n", err.c_str());
        return core::Result<void>::error(err);
    }

    return core::Result<void>();
}



core::Result<void> ParquetWriter::Close() {
    if (writer_) {
        auto status = writer_->Close();
        if (!status.ok()) {
            return core::Result<void>::error("Failed to close writer: " + status.ToString());
        }
        writer_.reset();
    }
    
    if (outfile_) {
        auto status = outfile_->Close();
        if (!status.ok()) {
            return core::Result<void>::error("Failed to close file: " + status.ToString());
        }
        outfile_.reset();
    }

    return core::Result<void>();
}

} // namespace parquet
} // namespace storage
} // namespace tsdb
