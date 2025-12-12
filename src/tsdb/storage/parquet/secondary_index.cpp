#include "tsdb/storage/parquet/secondary_index.h"
#include "tsdb/storage/parquet/reader.hpp"
#include "tsdb/storage/parquet/schema_mapper.hpp"
#include "tsdb/core/types.h"

#include <arrow/api.h>
#include <arrow/io/file.h>
#include <parquet/arrow/reader.h>
#include <parquet/file_reader.h>
#include <parquet/metadata.h>
#include <parquet/statistics.h>

#include <fstream>
#include <chrono>
#include <set>
#include <unordered_map>
#include <spdlog/spdlog.h>
#include <functional>

namespace tsdb {
namespace storage {
namespace parquet {

// ============================================================================
// SecondaryIndex Implementation
// ============================================================================

SecondaryIndex::SecondaryIndex() {
    stats_.num_series = 0;
    stats_.num_locations = 0;
    stats_.memory_bytes = 0;
    stats_.build_time_us = 0;
}

SecondaryIndex::~SecondaryIndex() = default;

SecondaryIndex::SecondaryIndex(SecondaryIndex&& other) noexcept 
    : index_()
    , stats_() {
    // Move ctor is not expected to be used concurrently; still, take exclusive lock for safety.
    std::unique_lock<std::shared_mutex> lock(other.mutex_);
    index_ = std::move(other.index_);
    stats_ = std::move(other.stats_);
}

SecondaryIndex& SecondaryIndex::operator=(SecondaryIndex&& other) noexcept {
    if (this != &other) {
        // Lock both mutexes without deadlock.
        std::scoped_lock lock(mutex_, other.mutex_);
        index_ = std::move(other.index_);
        stats_ = std::move(other.stats_);
    }
    return *this;
}

core::SeriesID SecondaryIndex::ComputeSeriesID(const std::string& labels_str) {
    // Use std::hash for SeriesID computation
    // This should match how labels are hashed elsewhere in the codebase
    return std::hash<std::string>{}(labels_str);
}

bool SecondaryIndex::BuildFromParquetFile(const std::string& parquet_path) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        // Open Parquet file using Arrow (same pattern as reader.cpp)
        auto infile_result = arrow::io::ReadableFile::Open(parquet_path);
        if (!infile_result.ok()) {
            spdlog::error("Failed to open Parquet file: {}", infile_result.status().ToString());
            return false;
        }
        auto infile = *infile_result;
        
        std::unique_ptr<::parquet::arrow::FileReader> arrow_reader;
        auto status = ::parquet::arrow::FileReader::Make(
            arrow::default_memory_pool(),
            ::parquet::ParquetFileReader::Open(infile),
            &arrow_reader
        );
        if (!status.ok()) {
            spdlog::error("Failed to create Parquet reader: {}", status.ToString());
            return false;
        }
        
        auto file_metadata = arrow_reader->parquet_reader()->metadata();
        int num_row_groups = file_metadata->num_row_groups();
        
        spdlog::debug("Building secondary index for {} with {} row groups", 
                      parquet_path, num_row_groups);
        
        // Build into local structures, then publish with a single swap under exclusive lock.
        std::unordered_map<core::SeriesID, std::vector<RowLocation>> local_index;
        
        // Track all unique series across the file and which row groups they appear in.
        std::unordered_map<core::SeriesID, std::set<int>> series_row_groups;
        std::vector<std::pair<int64_t, int64_t>> row_group_time_bounds;
        row_group_time_bounds.resize(static_cast<size_t>(std::max(0, num_row_groups)));
        
        // Scan each row group and extract ALL series information
        for (int rg = 0; rg < num_row_groups; ++rg) {
            // Get row group statistics for time range
            auto rg_metadata = file_metadata->RowGroup(rg);
            
            int64_t rg_min_ts = 0, rg_max_ts = 0;
            
            // Try to get timestamp stats from column statistics
            // The timestamp column is typically the first column
            if (rg_metadata->num_columns() > 0) {
                auto col_stats = rg_metadata->ColumnChunk(0)->statistics();
                if (col_stats && col_stats->HasMinMax()) {
                    // Assuming Int64 timestamp column
                    auto int_stats = std::dynamic_pointer_cast<::parquet::Int64Statistics>(col_stats);
                    if (int_stats) {
                        rg_min_ts = int_stats->min();
                        rg_max_ts = int_stats->max();
                    }
                }
            }

            row_group_time_bounds[static_cast<size_t>(rg)] = {rg_min_ts, rg_max_ts};
            
            // Read the row group to get labels
            std::shared_ptr<arrow::Table> table;
            auto read_status = arrow_reader->ReadRowGroup(rg, &table);
            if (!read_status.ok()) {
                spdlog::warn("Failed to read row group {}: {}", rg, read_status.ToString());
                continue;
            }
            
            if (!table || table->num_rows() == 0) continue;
            
            auto schema = table->schema();
            int64_t num_rows = table->num_rows();
            
            // Try to find a 'tags' column (stored as map<string, string>)
            auto tags_field = schema->GetFieldByName("tags");
            if (tags_field) {
                auto tags_col = table->GetColumnByName("tags");
                if (tags_col && tags_col->num_chunks() > 0) {
                    // Process ALL rows in this row group
                    for (int chunk_idx = 0; chunk_idx < tags_col->num_chunks(); ++chunk_idx) {
                        auto chunk = tags_col->chunk(chunk_idx);
                        auto map_array = std::dynamic_pointer_cast<arrow::MapArray>(chunk);
                        
                        if (map_array) {
                            auto keys_array = std::static_pointer_cast<arrow::StringArray>(map_array->keys());
                            auto values_array = std::static_pointer_cast<arrow::StringArray>(map_array->items());
                            
                            // Iterate through EACH row in the chunk
                            for (int64_t row = 0; row < map_array->length(); ++row) {
                                if (map_array->IsNull(row)) continue;
                                
                                std::vector<std::pair<std::string, std::string>> label_pairs;
                                int64_t start = map_array->value_offset(row);
                                int64_t end = map_array->value_offset(row + 1);
                                
                                for (int64_t i = start; i < end; ++i) {
                                    if (!keys_array->IsNull(i) && !values_array->IsNull(i)) {
                                        label_pairs.emplace_back(
                                            keys_array->GetString(i),
                                            values_array->GetString(i)
                                        );
                                    }
                                }
                                
                                // Build sorted label string for consistent hashing
                                std::sort(label_pairs.begin(), label_pairs.end());
                                std::string labels_str;
                                for (const auto& [k, v] : label_pairs) {
                                    if (!labels_str.empty()) labels_str += ",";
                                    labels_str += k + "=" + v;
                                }
                                
                                // Compute SeriesID
                                core::SeriesID series_id = ComputeSeriesID(labels_str);
                                
                                // Track this series in this row group
                                series_row_groups[series_id].insert(rg);
                            }
                        } else {
                            // Fallback for string type tags (legacy format)
                            auto str_array = std::dynamic_pointer_cast<arrow::StringArray>(chunk);
                            if (str_array) {
                                for (int64_t row = 0; row < str_array->length(); ++row) {
                                    if (str_array->IsNull(row)) continue;
                                    
                                    std::string labels_str = str_array->GetString(row);
                                    core::SeriesID series_id = ComputeSeriesID(labels_str);
                                    
                                    series_row_groups[series_id].insert(rg);
                                }
                            }
                        }
                    }
                }
            }
        }
        
        // Now build the index from aggregated data
        for (const auto& [series_id, row_groups] : series_row_groups) {
            auto& vec = local_index[series_id];
            vec.reserve(vec.size() + row_groups.size());
            for (int rg : row_groups) {
                const auto& bounds = row_group_time_bounds[static_cast<size_t>(rg)];
                RowLocation location(rg, 0, bounds.first, bounds.second);
                vec.push_back(location);
            }
        }
        
        // Update statistics
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time);
        
        {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            index_.swap(local_index);
            stats_.num_series = index_.size();
            stats_.num_locations = TotalLocations();
            stats_.memory_bytes = stats_.num_series * sizeof(core::SeriesID) +
                                  stats_.num_locations * sizeof(RowLocation);
            stats_.build_time_us = duration.count();
            stats_.source_file = parquet_path;
        }
        
        spdlog::info("Built secondary index for {}: {} series, {} locations in {:.2f}ms",
                     parquet_path, stats_.num_series, stats_.num_locations, 
                     duration.count() / 1000.0);
        
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to build secondary index for {}: {}", 
                      parquet_path, e.what());
        return false;
    }
}

bool SecondaryIndex::LoadFromFile(const std::string& index_path) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        std::ifstream file(index_path, std::ios::binary);
        if (!file.is_open()) {
            spdlog::debug("Index file not found: {}", index_path);
            return false;
        }
        
        // Read and validate header
        uint32_t magic, version;
        file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        file.read(reinterpret_cast<char*>(&version), sizeof(version));
        
        if (magic != INDEX_MAGIC) {
            spdlog::warn("Invalid index file magic: {}", magic);
            return false;
        }
        
        if (version != INDEX_VERSION) {
            spdlog::warn("Unsupported index version: {}", version);
            return false;
        }
        
        // Read number of entries
        size_t num_series;
        file.read(reinterpret_cast<char*>(&num_series), sizeof(num_series));
        
        std::unordered_map<core::SeriesID, std::vector<RowLocation>> local_index;
        local_index.reserve(num_series);

        for (size_t i = 0; i < num_series; ++i) {
            core::SeriesID series_id;
            size_t num_locations;
            
            file.read(reinterpret_cast<char*>(&series_id), sizeof(series_id));
            file.read(reinterpret_cast<char*>(&num_locations), sizeof(num_locations));
            
            std::vector<RowLocation> locations(num_locations);
            for (size_t j = 0; j < num_locations; ++j) {
                file.read(reinterpret_cast<char*>(&locations[j].row_group_id),
                          sizeof(locations[j].row_group_id));
                file.read(reinterpret_cast<char*>(&locations[j].row_offset),
                          sizeof(locations[j].row_offset));
                file.read(reinterpret_cast<char*>(&locations[j].min_timestamp),
                          sizeof(locations[j].min_timestamp));
                file.read(reinterpret_cast<char*>(&locations[j].max_timestamp),
                          sizeof(locations[j].max_timestamp));
            }
            
            local_index[series_id] = std::move(locations);
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time);
        
        {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            index_.swap(local_index);
            stats_.num_series = index_.size();
            stats_.num_locations = TotalLocations();
            stats_.memory_bytes = stats_.num_series * sizeof(core::SeriesID) +
                                  stats_.num_locations * sizeof(RowLocation);
            stats_.build_time_us = duration.count();
        }
        
        spdlog::info("Loaded secondary index from {}: {} series in {:.2f}ms",
                     index_path, stats_.num_series, duration.count() / 1000.0);
        
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to load secondary index from {}: {}", 
                      index_path, e.what());
        return false;
    }
}

bool SecondaryIndex::SaveToFile(const std::string& index_path) const {
    try {
        std::ofstream file(index_path, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            spdlog::error("Cannot create index file: {}", index_path);
            return false;
        }
        
        // Write header
        file.write(reinterpret_cast<const char*>(&INDEX_MAGIC), sizeof(INDEX_MAGIC));
        file.write(reinterpret_cast<const char*>(&INDEX_VERSION), sizeof(INDEX_VERSION));
        
        std::shared_lock<std::shared_mutex> lock(mutex_);
        
        // Write number of entries
        size_t num_series = index_.size();
        file.write(reinterpret_cast<const char*>(&num_series), sizeof(num_series));
        
        // Write entries
        for (const auto& [series_id, locations] : index_) {
            file.write(reinterpret_cast<const char*>(&series_id), sizeof(series_id));
            
            size_t num_locations = locations.size();
            file.write(reinterpret_cast<const char*>(&num_locations), sizeof(num_locations));
            
            for (const auto& loc : locations) {
                file.write(reinterpret_cast<const char*>(&loc.row_group_id), 
                           sizeof(loc.row_group_id));
                file.write(reinterpret_cast<const char*>(&loc.row_offset), 
                           sizeof(loc.row_offset));
                file.write(reinterpret_cast<const char*>(&loc.min_timestamp), 
                           sizeof(loc.min_timestamp));
                file.write(reinterpret_cast<const char*>(&loc.max_timestamp), 
                           sizeof(loc.max_timestamp));
            }
        }
        
        spdlog::debug("Saved secondary index to {}: {} series", 
                      index_path, num_series);
        
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to save secondary index to {}: {}", 
                      index_path, e.what());
        return false;
    }
}

std::vector<RowLocation> SecondaryIndex::Lookup(core::SeriesID series_id) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    auto it = index_.find(series_id);
    if (it != index_.end()) {
        return it->second;
    }
    
    return {};
}

std::vector<RowLocation> SecondaryIndex::LookupInTimeRange(
    core::SeriesID series_id,
    int64_t start_time,
    int64_t end_time) const {
    
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    auto it = index_.find(series_id);
    if (it == index_.end()) {
        return {};
    }
    
    std::vector<RowLocation> result;
    for (const auto& loc : it->second) {
        // Check for time range overlap
        // NOT (end < start OR start > end) => overlap exists
        if (!(loc.max_timestamp < start_time || loc.min_timestamp > end_time)) {
            result.push_back(loc);
        }
    }
    
    return result;
}

void SecondaryIndex::Insert(core::SeriesID series_id, const RowLocation& location) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    index_[series_id].push_back(location);
    
    // Update stats
    stats_.num_series = index_.size();
    stats_.num_locations++;
    stats_.memory_bytes = stats_.num_series * sizeof(core::SeriesID) + 
                          stats_.num_locations * sizeof(RowLocation);
}

bool SecondaryIndex::Contains(core::SeriesID series_id) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return index_.find(series_id) != index_.end();
}

size_t SecondaryIndex::Size() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return index_.size();
}

size_t SecondaryIndex::TotalLocations() const {
    // Note: called with mutex held from other methods
    size_t total = 0;
    for (const auto& [_, locations] : index_) {
        total += locations.size();
    }
    return total;
}

void SecondaryIndex::Clear() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    index_.clear();
    stats_.num_series = 0;
    stats_.num_locations = 0;
    stats_.memory_bytes = 0;
}

bool SecondaryIndex::Empty() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return index_.empty();
}

std::vector<core::SeriesID> SecondaryIndex::GetAllSeriesIDs() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::vector<core::SeriesID> result;
    result.reserve(index_.size());
    
    for (const auto& [series_id, _] : index_) {
        result.push_back(series_id);
    }
    
    return result;
}

SecondaryIndex::IndexStats SecondaryIndex::GetStats() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return stats_;
}

// ============================================================================
// SecondaryIndexCache Implementation
// ============================================================================

SecondaryIndexCache& SecondaryIndexCache::Instance() {
    static SecondaryIndexCache instance;
    return instance;
}

std::shared_ptr<SecondaryIndex> SecondaryIndexCache::GetOrCreate(
    const std::string& parquet_path) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_.find(parquet_path);
    if (it != cache_.end()) {
        hits_++;
        return it->second;
    }
    
    misses_++;
    
    // Create new index
    auto index = std::make_shared<SecondaryIndex>();
    
    // Try to load from sidecar file first
    std::string index_path = parquet_path + ".idx";
    if (!index->LoadFromFile(index_path)) {
        // Build from Parquet file
        if (!index->BuildFromParquetFile(parquet_path)) {
            spdlog::error("Failed to build index for: {}", parquet_path);
            return nullptr;
        }
        
        // Save for next time
        index->SaveToFile(index_path);
    }
    
    cache_[parquet_path] = index;
    return index;
}

void SecondaryIndexCache::Invalidate(const std::string& parquet_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.erase(parquet_path);
}

void SecondaryIndexCache::ClearAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
}

SecondaryIndexCache::CacheStats SecondaryIndexCache::GetStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    CacheStats stats;
    stats.num_cached_indices = cache_.size();
    stats.total_memory_bytes = 0;
    stats.cache_hits = hits_;
    stats.cache_misses = misses_;
    
    for (const auto& [_, index] : cache_) {
        stats.total_memory_bytes += index->GetStats().memory_bytes;
    }
    
    return stats;
}

}  // namespace parquet
}  // namespace storage
}  // namespace tsdb
