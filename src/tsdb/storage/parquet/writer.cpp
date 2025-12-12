#include "tsdb/storage/parquet/writer.hpp"
#include "tsdb/storage/parquet/secondary_index.h"
#include <arrow/io/file.h>
#include <parquet/arrow/writer.h>
#include <spdlog/spdlog.h>

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
    path_ = path;
    schema_ = schema;
    
    // Open file output stream
    auto outfile_result = arrow::io::FileOutputStream::Open(path);
    if (!outfile_result.ok()) {
        return core::Result<void>::error("Failed to open file: " + outfile_result.status().ToString());
    }
    outfile_ = *outfile_result;

    // Create Parquet writer with optimized properties
    ::parquet::WriterProperties::Builder builder;
    builder.compression(::parquet::Compression::ZSTD);
    builder.enable_dictionary();
    builder.max_row_group_length(max_row_group_length);
    
    // Enable statistics for all columns - helps with min/max pruning for time range queries
    builder.enable_statistics();
    
    // Enable page index for better predicate pushdown
    builder.enable_write_page_index();
    
    std::shared_ptr<::parquet::WriterProperties> props = builder.build();
    
    // Configure Arrow writer properties for better statistics
    auto arrow_props = ::parquet::ArrowWriterProperties::Builder()
        .store_schema()  // Store Arrow schema for better interop
        ->build();

    auto result = ::parquet::arrow::FileWriter::Open(
        *schema_,
        arrow::default_memory_pool(),
        outfile_,
        props,
        arrow_props
    );

    if (!result.ok()) {
        return core::Result<void>::error("Failed to create Parquet writer: " + result.status().ToString());
    }
    writer_ = std::move(result).ValueOrDie();
    
    // Create Bloom filter for this file
    bloom_filter_ = std::make_unique<BloomFilterManager>();
    bloom_filter_->CreateFilter();  // Use default NDV and FPP

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

void ParquetWriter::AddSeriesToBloomFilter(core::SeriesID series_id) {
    if (bloom_filter_) {
        bloom_filter_->AddSeriesId(series_id);
    }
}

void ParquetWriter::AddSeriesToBloomFilterByLabels(const std::string& labels_str) {
    if (bloom_filter_) {
        bloom_filter_->AddSeriesByLabels(labels_str);
    }
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
    
    // Save Bloom filter alongside Parquet file
    if (bloom_filter_ && !path_.empty()) {
        if (bloom_filter_->GetEntriesAdded() > 0) {
            if (!bloom_filter_->SaveFilter(path_)) {
                spdlog::warn("[ParquetWriter] Failed to save Bloom filter for {}", path_);
            }
        }
        bloom_filter_.reset();
    }

    // Phase 2 Step 2.4: Build and persist SecondaryIndex sidecar at write time.
    // This avoids a cold-start scan on first read.
    if (!path_.empty()) {
        try {
            const std::string index_path = path_ + ".idx";
            SecondaryIndex idx;
            if (!idx.BuildFromParquetFile(path_)) {
                spdlog::warn("[ParquetWriter] Failed to build secondary index for {}", path_);
            } else if (!idx.SaveToFile(index_path)) {
                spdlog::warn("[ParquetWriter] Failed to save secondary index sidecar {}", index_path);
            } else {
                spdlog::debug("[ParquetWriter] Wrote secondary index sidecar {}", index_path);
            }
        } catch (const std::exception& e) {
            spdlog::warn("[ParquetWriter] Exception building secondary index sidecar: {}", e.what());
        } catch (...) {
            spdlog::warn("[ParquetWriter] Unknown exception building secondary index sidecar");
        }
    }

    return core::Result<void>();
}

} // namespace parquet
} // namespace storage
} // namespace tsdb
