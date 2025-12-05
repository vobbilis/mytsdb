#include "tsdb/storage/parquet/parquet_block.hpp"
#include "tsdb/storage/parquet/schema_mapper.hpp"
#include "tsdb/core/matcher.h"
#include <stdexcept>
#include <map>

namespace tsdb {
namespace storage {
namespace parquet {

ParquetBlock::ParquetBlock(const internal::BlockHeader& header, const std::string& path)
    : header_(header), path_(path) {}

size_t ParquetBlock::size() const {
    // This is an approximation or we could use file size
    // For now, return 0 or file size if we checked it
    return 0; // TODO: Implement proper size tracking
}

size_t ParquetBlock::num_series() const {
    // We don't know without scanning
    return 0; // TODO: Store metadata in footer
}

size_t ParquetBlock::num_samples() const {
    return 0; // TODO: Store metadata in footer
}

int64_t ParquetBlock::start_time() const {
    return header_.start_time;
}

int64_t ParquetBlock::end_time() const {
    return header_.end_time;
}

const internal::BlockHeader& ParquetBlock::header() const {
    return header_;
}

void ParquetBlock::write(const core::TimeSeries& series) {
    throw std::runtime_error("ParquetBlock is read-only");
}

void ParquetBlock::flush() {
    // No-op
}

void ParquetBlock::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (reader_) {
        reader_->Close();
        reader_.reset();
    }
}

ParquetReader* ParquetBlock::get_reader() const {
    if (!reader_) {
        reader_ = std::make_unique<ParquetReader>();
        auto result = reader_->Open(path_);
        if (!result.ok()) {
            throw std::runtime_error("Failed to open Parquet file: " + result.error());
        }
    }
    return reader_.get();
}

core::TimeSeries ParquetBlock::read(const core::Labels& labels) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Scan the file and filter by labels
    // Note: This is inefficient (O(N)), optimization needed in Phase 5
    
    // Re-open reader to start from beginning
    if (reader_) {
        reader_->Close();
    }
    reader_ = std::make_unique<ParquetReader>();
    auto open_result = reader_->Open(path_);
    if (!open_result.ok()) {
        throw std::runtime_error("Failed to open Parquet file: " + open_result.error());
    }
    
    core::TimeSeries series(labels);
    
    while (true) {
        std::shared_ptr<arrow::RecordBatch> batch;
        auto result = reader_->ReadBatch(&batch);
        if (!result.ok()) {
            throw std::runtime_error("Failed to read batch: " + result.error());
        }
        if (!batch) break; // EOF
        
        auto samples_res = SchemaMapper::ToSamples(batch);
        if (!samples_res.ok()) continue;
        auto samples = samples_res.value();
        
        auto tags_res = SchemaMapper::ExtractTags(batch);
        if (!tags_res.ok()) continue;
        core::Labels batch_labels(tags_res.value());
        
        // Check if labels match
        if (batch_labels == labels) {
            for (const auto& sample : samples) {
                // Check time range (block level check is already done by caller, but safe to double check)
                if (sample.timestamp() >= header_.start_time && sample.timestamp() <= header_.end_time) {
                    series.add_sample(sample);
                }
            }
        }
    }
    
    return series;
}

std::vector<core::TimeSeries> ParquetBlock::query(
    const std::vector<std::pair<std::string, std::string>>& matchers,
    int64_t start_time,
    int64_t end_time) const {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Re-open reader to start from beginning
    if (reader_) {
        reader_->Close();
    }
    reader_ = std::make_unique<ParquetReader>();
    auto open_result = reader_->Open(path_);
    if (!open_result.ok()) {
        throw std::runtime_error("Failed to open Parquet file: " + open_result.error());
    }
    
    std::map<std::string, core::TimeSeries> series_map; // Keyed by labels string
    
    // Construct LabelMatcher
    // Note: We are using a simplified matcher here. 
    // In a real implementation, we should use the LabelMatcher class.
    
    int num_row_groups = reader_->GetNumRowGroups();
    
    for (int i = 0; i < num_row_groups; ++i) {
        // 1. Predicate Pushdown: Read only tags first
        auto tags_batch_res = reader_->ReadRowGroupTags(i);
        if (!tags_batch_res.ok()) {
            // Log warning? For now throw or continue
            continue;
        }
        auto tags_batch = tags_batch_res.value();

        
        auto tags_res = SchemaMapper::ExtractTags(tags_batch);
        if (!tags_res.ok()) continue;
        core::Labels batch_labels(tags_res.value());
        
        // Check matchers
        bool match = true;
        for (const auto& m : matchers) {
            // Simple exact match for now
            if (batch_labels.get(m.first) != m.second) {
                match = false;
                break;
            }
        }
        
        if (!match) {
            // Skip this row group!
            continue;
        }
        
        // 2. Read full data for matching row group
        auto batch_res = reader_->ReadRowGroup(i);
        if (!batch_res.ok()) {
            throw std::runtime_error("Failed to read row group: " + batch_res.error());
        }
        auto batch = batch_res.value();
        
        auto samples_res = SchemaMapper::ToSamples(batch);
        if (!samples_res.ok()) continue;
        auto samples = samples_res.value();
        
        std::string label_str = batch_labels.to_string();
        if (series_map.find(label_str) == series_map.end()) {
            series_map.emplace(label_str, core::TimeSeries(batch_labels));
        }
        
        for (const auto& sample : samples) {
            if (sample.timestamp() >= start_time && sample.timestamp() <= end_time) {
                series_map.at(label_str).add_sample(sample);
            }
        }
    }
    
    std::vector<core::TimeSeries> result;
    for (auto& pair : series_map) {
        if (pair.second.samples().size() > 0) {
            result.push_back(std::move(pair.second));
        }
    }
    return result;
}

} // namespace parquet
} // namespace storage
} // namespace tsdb
