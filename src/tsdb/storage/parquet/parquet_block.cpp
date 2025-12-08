#include "tsdb/storage/parquet/parquet_block.hpp"
#include "tsdb/storage/parquet/schema_mapper.hpp"
#include "tsdb/storage/parquet/secondary_index.h"
#include "tsdb/storage/parquet/bloom_filter_manager.h"
#include "tsdb/storage/read_performance_instrumentation.h"
#include "tsdb/storage/parquet_catalog.h"
#include "tsdb/core/matcher.h"
#include <stdexcept>
#include <map>
#include <set>
#include <chrono>
#include <algorithm>
#include <spdlog/spdlog.h>
#include <functional>

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
    std::vector<core::Sample> collected_samples; // Collect samples before sorting
    
    while (true) {
        std::shared_ptr<arrow::RecordBatch> batch;
        auto result = reader_->ReadBatch(&batch);
        if (!result.ok()) {
            throw std::runtime_error("Failed to read batch: " + result.error());
        }
        if (!batch) break; // EOF
        
        // Use ToSeriesMap to properly extract samples grouped by their actual per-row tags
        auto series_map_res = SchemaMapper::ToSeriesMap(batch);
        if (!series_map_res.ok()) continue;
        auto batch_series_map = series_map_res.value();
        
        // Find the series matching the requested labels
        for (const auto& [tags, samples] : batch_series_map) {
            core::Labels batch_labels(tags);
            
            // Check if labels match
            if (batch_labels == labels) {
                for (const auto& sample : samples) {
                    // Check time range (block level check is already done by caller, but safe to double check)
                    if (sample.timestamp() >= header_.start_time && sample.timestamp() <= header_.end_time) {
                        collected_samples.push_back(sample);
                    }
                }
            }
        }
    }
    
    // Sort samples by timestamp before adding to series
    std::sort(collected_samples.begin(), collected_samples.end(),
        [](const core::Sample& a, const core::Sample& b) {
            return a.timestamp() < b.timestamp();
        });
    
    // Add sorted samples to series
    for (const auto& sample : collected_samples) {
        series.add_sample(sample);
    }
    
    return series;
}

std::pair<std::vector<int64_t>, std::vector<double>> ParquetBlock::read_columns(const core::Labels& labels) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Build canonical label string for SeriesID computation (needed by both Bloom and B+ Tree)
    std::vector<std::pair<std::string, std::string>> sorted_labels;
    for (const auto& [k, v] : labels.map()) {
        sorted_labels.emplace_back(k, v);
    }
    std::sort(sorted_labels.begin(), sorted_labels.end());
    
    std::string labels_str;
    for (const auto& [k, v] : sorted_labels) {
        if (!labels_str.empty()) labels_str += ",";
        labels_str += k + "=" + v;
    }
    core::SeriesID series_id = std::hash<std::string>{}(labels_str);
    
    // =========================================================================
    // PHASE 0: Bloom Filter Check - O(1) "definitely not present" test
    // =========================================================================
    auto bloom_start = std::chrono::high_resolution_clock::now();
    auto bloom_filter = BloomFilterCache::Instance().GetOrLoad(path_);
    if (bloom_filter && bloom_filter->IsValid()) {
        if (!bloom_filter->MightContain(series_id)) {
            // Series is DEFINITELY NOT in this file - skip entirely!
            auto bloom_end = std::chrono::high_resolution_clock::now();
            double bloom_time_us = std::chrono::duration_cast<std::chrono::nanoseconds>(bloom_end - bloom_start).count() / 1000.0;
            
            // Record metrics: skipped (definite not present)
            ReadPerformanceInstrumentation::instance().recordBloomFilterUsage(true, bloom_time_us);
            
            return {}; // Empty result - series not present
        }
        // Bloom says "might be present" - continue to B+ Tree lookup
        auto bloom_end = std::chrono::high_resolution_clock::now();
        double bloom_time_us = std::chrono::duration_cast<std::chrono::nanoseconds>(bloom_end - bloom_start).count() / 1000.0;
        
        // Record metrics: passed (might be present)
        ReadPerformanceInstrumentation::instance().recordBloomFilterUsage(false, bloom_time_us);
    }
    
    // =========================================================================
    // PHASE 1: B+ Tree Secondary Index - O(log n) row group selection
    // =========================================================================
    
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
    
    // Try to use secondary index for efficient lookup
    auto index = SecondaryIndexCache::Instance().GetOrCreate(path_);
    std::set<int> target_row_groups;
    bool use_index = false;
    
    if (index) {
        // Time the index lookup
        auto lookup_start = std::chrono::high_resolution_clock::now();
        auto locations = index->Lookup(series_id);
        auto lookup_end = std::chrono::high_resolution_clock::now();
        double lookup_time_us = std::chrono::duration<double, std::micro>(lookup_end - lookup_start).count();
        
        if (!locations.empty()) {
            use_index = true;
            for (const auto& loc : locations) {
                target_row_groups.insert(loc.row_group_id);
            }
            // Record secondary index HIT in global stats
            ReadPerformanceInstrumentation::instance().recordSecondaryIndexUsage(
                true, lookup_time_us, target_row_groups.size());
        } else {
            // Record secondary index MISS in global stats
            ReadPerformanceInstrumentation::instance().recordSecondaryIndexUsage(
                false, lookup_time_us, 0);
        }
    }
    
    // Get current metrics for detailed instrumentation
    auto* metrics = ReadPerformanceInstrumentation::GetCurrentMetrics();

    // Read batches - either from targeted row groups or all
    int current_row_group = 0;
    while (true) {
        std::shared_ptr<arrow::RecordBatch> batch;
        
        // Instrument I/O
        auto io_start = std::chrono::high_resolution_clock::now();
        auto result = reader_->ReadBatch(&batch);
        if (metrics) {
             auto io_end = std::chrono::high_resolution_clock::now();
             metrics->row_group_read_us += std::chrono::duration<double, std::micro>(io_end - io_start).count();
        }

        if (!result.ok()) {
            throw std::runtime_error("Failed to read batch: " + result.error());
        }
        if (!batch) break; // EOF
        
        int this_row_group = current_row_group++;

        // If using index, skip row groups not in our target set
        if (use_index) {
             bool is_target = target_row_groups.find(this_row_group) != target_row_groups.end();
             if (!is_target) {
                continue;
             }
        }
        
        if (metrics) {
            metrics->row_groups_read++;
        }
        
        // Use ToSeriesMap to handle batches containing multiple series
        // Instrument Decoding
        auto decode_start = std::chrono::high_resolution_clock::now();
        auto series_map_res = SchemaMapper::ToSeriesMap(batch);
        if (metrics) {
             auto decode_end = std::chrono::high_resolution_clock::now();
             metrics->decoding_us += std::chrono::duration<double, std::micro>(decode_end - decode_start).count();
        }

        if (!series_map_res.ok()) {
             continue;
        }
        auto batch_series_map = series_map_res.value();

        // Instrument Processing
        auto proc_start = std::chrono::high_resolution_clock::now();
        for (const auto& [tags, samples] : batch_series_map) {
            core::Labels batch_labels(tags);
            
            if (batch_labels == labels) {
                if (metrics) {
                    metrics->samples_scanned += samples.size();
                }
                for (const auto& sample : samples) {
                    if (sample.timestamp() >= header_.start_time && sample.timestamp() <= header_.end_time) {
                        timestamps.push_back(sample.timestamp());
                        values.push_back(sample.value());
                    }
                }
            }
        }
        if (metrics) {
             auto proc_end = std::chrono::high_resolution_clock::now();
             metrics->processing_us += std::chrono::duration<double, std::micro>(proc_end - proc_start).count();
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
    std::map<std::string, std::vector<core::Sample>> collected_samples; // Collect samples before sorting
    
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
    
    // =========================================================================
    // PHASE A: Secondary Index Optimization
    // If we have matchers, try to use secondary index for O(log n) lookup
    // instead of scanning all row groups O(n)
    // =========================================================================
    
    std::set<int> candidate_row_groups;
    bool use_index = false;
    
    // Try to get secondary index - measure time
    auto index_lookup_start = std::chrono::high_resolution_clock::now();
    auto index = SecondaryIndexCache::Instance().GetOrCreate(path_);
    
    if (index && !matchers.empty()) {
        // Compute SeriesID from matchers
        // Build a canonical label string from matchers (sorted by key)
        std::vector<std::pair<std::string, std::string>> sorted_matchers = matchers;
        std::sort(sorted_matchers.begin(), sorted_matchers.end());
        
        std::string labels_str;
        for (const auto& [k, v] : sorted_matchers) {
            if (!labels_str.empty()) labels_str += ",";
            labels_str += k + "=" + v;
        }
        
        // Compute SeriesID using same hash as secondary_index.cpp
        core::SeriesID series_id = std::hash<std::string>{}(labels_str);
        
        spdlog::info("Secondary index lookup: matchers={}, labels_str='{}', series_id={}", 
                     matchers.size(), labels_str, series_id);
        
        // Lookup in secondary index with time range filtering
        auto locations = index->LookupInTimeRange(series_id, start_time, end_time);
        
        // Record index lookup time
        auto index_lookup_end = std::chrono::high_resolution_clock::now();
        m.secondary_index_lookup_us = std::chrono::duration<double, std::micro>(
            index_lookup_end - index_lookup_start).count();
        
        if (!locations.empty()) {
            // We found the series in the index!
            use_index = true;
            m.secondary_index_used = true;
            m.secondary_index_hits = 1;  // Found the series
            
            for (const auto& loc : locations) {
                candidate_row_groups.insert(loc.row_group_id);
            }
            
            m.secondary_index_row_groups_selected = candidate_row_groups.size();
            
            spdlog::debug("Secondary index hit: series_id={}, {} row groups (vs {} total), lookup took {:.3f}us",
                          series_id, candidate_row_groups.size(), num_row_groups, m.secondary_index_lookup_us);
            
            // Update metrics for index usage
            m.row_groups_pruned_tags = num_row_groups - candidate_row_groups.size();
        } else {
            spdlog::debug("Secondary index miss: series_id={}, falling back to scan", series_id);
            m.secondary_index_used = false;
        }
    } else {
        // No index or no matchers - record why
        auto index_lookup_end = std::chrono::high_resolution_clock::now();
        m.secondary_index_lookup_us = std::chrono::duration<double, std::micro>(
            index_lookup_end - index_lookup_start).count();
    }
    
    // Determine which row groups to scan
    // If we have index hits, only scan those; otherwise scan all
    std::vector<int> row_groups_to_scan;
    if (use_index) {
        row_groups_to_scan.assign(candidate_row_groups.begin(), candidate_row_groups.end());
    } else {
        row_groups_to_scan.reserve(num_row_groups);
        for (int i = 0; i < num_row_groups; ++i) {
            row_groups_to_scan.push_back(i);
        }
    }
    
    for (int i : row_groups_to_scan) {
        int64_t rg_byte_size = 0;
        
        // 0. Time-based Pruning (Catalog Optimized)
        // Skip this if we already did time-based filtering via secondary index
        if (!use_index) {
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
        } else {
            // For index-based lookup, we already filtered by time
            const auto& stats = meta->row_groups[i];
            rg_byte_size = stats.total_byte_size;
        }

        // Lazy Open: Only open reader if we actually need to read something
        if (!reader_) {
            reader_ = std::make_unique<ParquetReader>();
            auto open_result = reader_->Open(path_);
            if (!open_result.ok()) {
                throw std::runtime_error("Failed to open Parquet file: " + open_result.error());
            }
        }

        // 2. Read full data for matching row group
        // Note: Per-row tag filtering is done after reading via ToSeriesMap
        auto rg_read_start = std::chrono::high_resolution_clock::now();
        auto batch_res = reader_->ReadRowGroup(i);
        if (!batch_res.ok()) {
            throw std::runtime_error("Failed to read row group: " + batch_res.error());
        }
        auto batch = batch_res.value();
        auto rg_read_end = std::chrono::high_resolution_clock::now();
        m.row_group_read_us += std::chrono::duration<double, std::micro>(rg_read_end - rg_read_start).count();

        m.row_groups_read++;
        m.bytes_read += rg_byte_size;
        
        // Use ToSeriesMap to properly extract samples grouped by their actual per-row tags
        auto decode_start = std::chrono::high_resolution_clock::now();
        auto series_map_res = SchemaMapper::ToSeriesMap(batch);
        if (!series_map_res.ok()) continue;
        auto batch_series_map = series_map_res.value();
        auto decode_end = std::chrono::high_resolution_clock::now();
        m.decoding_us += std::chrono::duration<double, std::micro>(decode_end - decode_start).count();
        
        // Process each series in the batch
        auto proc_start = std::chrono::high_resolution_clock::now();
        for (auto& [tags, samples] : batch_series_map) {
            m.samples_scanned += samples.size();
            
            core::Labels series_labels(tags);
            
            // Apply matchers filter if we're not using index
            // (if using index, we've already filtered by series_id)
            if (!use_index) {
                bool match = true;
                for (const auto& matcher : matchers) {
                    if (series_labels.get(matcher.first) != matcher.second) {
                        match = false;
                        break;
                    }
                }
                if (!match) continue;
            }
            
            std::string label_str = series_labels.to_string();
            if (series_map.find(label_str) == series_map.end()) {
                series_map.emplace(label_str, core::TimeSeries(series_labels));
            }
            
            // Collect samples that match the time range (will sort later)
            for (const auto& sample : samples) {
                if (sample.timestamp() >= start_time && sample.timestamp() <= end_time) {
                    collected_samples[label_str].push_back(sample);
                }
            }
        }
        auto proc_end = std::chrono::high_resolution_clock::now();
        m.processing_us += std::chrono::duration<double, std::micro>(proc_end - proc_start).count();
    }
    
    // Sort and add samples to time series in chronological order
    for (auto& [label_str, samples] : collected_samples) {
        // Sort samples by timestamp
        std::sort(samples.begin(), samples.end(), 
            [](const core::Sample& a, const core::Sample& b) {
                return a.timestamp() < b.timestamp();
            });
        
        // Add sorted samples to time series
        for (const auto& sample : samples) {
            series_map.at(label_str).add_sample(sample);
        }
    }
    
    metrics.record_read(m);
    
    // Log performance evidence for verification
    if (use_index) {
        spdlog::info("Parquet Secondary Index: Total RG: {}, Index Selected: {}, Read: {}, Bytes Read: {}",
            m.row_groups_total, candidate_row_groups.size(), m.row_groups_read, m.bytes_read);
    } else if (m.row_groups_pruned_time > 0) {
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
