#include "tsdb/storage/parquet/parquet_block.hpp"
#include "tsdb/storage/parquet/schema_mapper.hpp"
#include "tsdb/storage/read_performance_instrumentation.h"
#include "tsdb/storage/parquet_catalog.h"
#include "tsdb/core/matcher.h"
#include <stdexcept>
#include <map>
#include <spdlog/spdlog.h>

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

std::pair<std::vector<int64_t>, std::vector<double>> ParquetBlock::read_columns(const core::Labels& labels) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Inefficient implementation for now, similar to read()
    // Ideally we would push down column selection to Parquet reader
    
    if (reader_) {
        reader_->Close();
    }
    reader_ = std::make_unique<ParquetReader>();
    auto open_result = reader_->Open(path_);
    if (!open_result.ok()) {
        throw std::runtime_error("Failed to open Parquet file: " + open_result.error());
    }
    
    std::vector<int64_t> timestamps;
    std::vector<double> values;
    
    while (true) {
        std::shared_ptr<arrow::RecordBatch> batch;
        auto result = reader_->ReadBatch(&batch);
        if (!result.ok()) {
            throw std::runtime_error("Failed to read batch: " + result.error());
        }
        if (!batch) break; // EOF
        
        auto tags_res = SchemaMapper::ExtractTags(batch);
        if (!tags_res.ok()) continue;
        core::Labels batch_labels(tags_res.value());
        
        if (batch_labels == labels) {
             // We found the series, extract columns
             // Note: SchemaMapper::ToSamples extracts all columns, we should optimize this later
            auto samples_res = SchemaMapper::ToSamples(batch);
            if (!samples_res.ok()) continue;
            auto samples = samples_res.value();
            
            for (const auto& sample : samples) {
                if (sample.timestamp() >= header_.start_time && sample.timestamp() <= header_.end_time) {
                    timestamps.push_back(sample.timestamp());
                    values.push_back(sample.value());
                }
            }
        }
    }
    
    return {std::move(timestamps), std::move(values)};
}


std::vector<core::TimeSeries> ParquetBlock::query(
    const std::vector<std::pair<std::string, std::string>>& matchers,
    int64_t start_time,
    int64_t end_time) const {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Reader will be opened lazily if needed
    // We reuse the existing reader if it's already open (Reader Caching)
    
    std::map<std::string, core::TimeSeries> series_map; // Keyed by labels string
    
    // Construct LabelMatcher
    // Note: We are using a simplified matcher here. 
    // In a real implementation, we should use the LabelMatcher class.
    
    auto& metrics = ReadPerformanceInstrumentation::instance();
    ReadPerformanceInstrumentation::ReadMetrics m;
    
    // Use Catalog to get metadata without opening file
    auto meta = ParquetCatalog::instance().GetFileMeta(path_);
    if (!meta) {
        spdlog::error("Failed to get metadata for file: {}", path_);
        return {}; 
    }
    
    int num_row_groups = meta->row_groups.size();
    m.row_groups_total = num_row_groups;
    
    for (int i = 0; i < num_row_groups; ++i) {
        int64_t rg_byte_size = 0;
        
        // 0. Time-based Pruning (Catalog Optimized)
        {
            auto start_prune = std::chrono::high_resolution_clock::now();
            const auto& stats = meta->row_groups[i];
            rg_byte_size = stats.total_byte_size;
            
            // Check for overlap: NOT (end < start OR start > end)
            if (stats.max_timestamp < start_time || stats.min_timestamp > end_time) {
                m.row_groups_pruned_time++;
                m.bytes_skipped += stats.total_byte_size;
                auto end_prune = std::chrono::high_resolution_clock::now();
                m.pruning_time_us += std::chrono::duration<double, std::micro>(end_prune - start_prune).count();
                continue;
            }
            auto end_prune = std::chrono::high_resolution_clock::now();
            m.pruning_time_us += std::chrono::duration<double, std::micro>(end_prune - start_prune).count();
        }

        // Lazy Open: Only open reader if we actually need to read something
        if (!reader_) {
            reader_ = std::make_unique<ParquetReader>();
            auto open_result = reader_->Open(path_);
            if (!open_result.ok()) {
                throw std::runtime_error("Failed to open Parquet file: " + open_result.error());
            }
        }

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
            m.row_groups_pruned_tags++;
            // Estimate bytes skipped (rough approximation)
            // Ideally we get this from RowGroup metadata
            continue;
        }
        
        // 2. Read full data for matching row group
        auto batch_res = reader_->ReadRowGroup(i);
        if (!batch_res.ok()) {
            throw std::runtime_error("Failed to read row group: " + batch_res.error());
        }
        auto batch = batch_res.value();
        m.row_groups_read++;
        m.bytes_read += rg_byte_size;
        
        auto samples_res = SchemaMapper::ToSamples(batch);
        if (!samples_res.ok()) continue;
        auto samples = samples_res.value();
        m.samples_scanned += samples.size();
        
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
    
    metrics.record_read(m);
    
    // Log performance evidence for verification
    if (m.row_groups_pruned_time > 0) {
        spdlog::info("Parquet Pruning Evidence: Total RG: {}, Pruned(Time): {}, Pruned(Tags): {}, Read: {}, Bytes Skipped: {}, Pruning Time: {:.3f}ms",
            m.row_groups_total, m.row_groups_pruned_time, m.row_groups_pruned_tags, m.row_groups_read, m.bytes_skipped, m.pruning_time_us / 1000.0);
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
