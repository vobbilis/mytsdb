#include "tsdb/storage/parquet/bloom_filter_manager.h"
#include <arrow/io/file.h>
#include <fstream>
#include <spdlog/spdlog.h>

namespace tsdb {
namespace storage {
namespace parquet {

// ============================================================================
// BloomFilterManager Implementation
// ============================================================================

void BloomFilterManager::CreateFilter(uint32_t estimated_entries, double fpp) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Calculate optimal filter size
    uint32_t num_bytes = ::parquet::BlockSplitBloomFilter::OptimalNumOfBytes(
        estimated_entries, fpp);
    
    // Create the filter
    filter_ = std::make_unique<::parquet::BlockSplitBloomFilter>();
    filter_->Init(num_bytes);
    entries_added_ = 0;
    
    spdlog::debug("[BloomFilter] Created filter: {} bytes for {} entries, FPP={}",
                  num_bytes, estimated_entries, fpp);
}

void BloomFilterManager::AddSeriesId(core::SeriesID series_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!filter_) {
        spdlog::warn("[BloomFilter] Cannot add - filter not created");
        return;
    }
    
    // Hash the series_id using the filter's hash function
    uint64_t hash = filter_->Hash(static_cast<int64_t>(series_id));
    filter_->InsertHash(hash);
    entries_added_++;
}

void BloomFilterManager::AddSeriesByLabels(const std::string& labels_str) {
    AddSeriesId(ComputeSeriesId(labels_str));
}

bool BloomFilterManager::SaveFilter(const std::string& parquet_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!filter_) {
        spdlog::warn("[BloomFilter] Cannot save - filter not created");
        return false;
    }
    
    std::string bloom_path = GetBloomPath(parquet_path);
    
    // Open output file
    auto outfile_result = arrow::io::FileOutputStream::Open(bloom_path);
    if (!outfile_result.ok()) {
        spdlog::error("[BloomFilter] Failed to open {} for writing: {}",
                      bloom_path, outfile_result.status().ToString());
        return false;
    }
    auto outfile = *outfile_result;
    
    // Write filter
    filter_->WriteTo(outfile.get());
    
    auto close_status = outfile->Close();
    if (!close_status.ok()) {
        spdlog::error("[BloomFilter] Failed to close {}: {}",
                      bloom_path, close_status.ToString());
        return false;
    }
    
    spdlog::info("[BloomFilter] Saved filter: {} ({} entries, {} bytes)",
                 bloom_path, entries_added_, filter_->GetBitsetSize());
    return true;
}

bool BloomFilterManager::LoadFilter(const std::string& parquet_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string bloom_path = GetBloomPath(parquet_path);
    
    // Check if file exists
    std::ifstream check(bloom_path);
    if (!check.good()) {
        spdlog::debug("[BloomFilter] No filter file found: {}", bloom_path);
        return false;
    }
    check.close();
    
    // Open input file
    auto infile_result = arrow::io::ReadableFile::Open(bloom_path);
    if (!infile_result.ok()) {
        spdlog::error("[BloomFilter] Failed to open {} for reading: {}",
                      bloom_path, infile_result.status().ToString());
        return false;
    }
    auto infile = *infile_result;
    
    // Get file size
    auto size_result = infile->GetSize();
    if (!size_result.ok()) {
        spdlog::error("[BloomFilter] Failed to get file size: {}",
                      size_result.status().ToString());
        return false;
    }
    int64_t file_size = *size_result;
    
    // Deserialize using default reader properties
    ::parquet::ReaderProperties reader_props;
    
    try {
        // Use the Deserialize method - it reads directly from stream
        // Returns BlockSplitBloomFilter directly (not a Result)
        auto loaded_filter = ::parquet::BlockSplitBloomFilter::Deserialize(
            reader_props, infile.get(), file_size);
        
        filter_ = std::make_unique<::parquet::BlockSplitBloomFilter>(
            std::move(loaded_filter));
    } catch (const std::exception& e) {
        spdlog::error("[BloomFilter] Failed to deserialize filter: {}", e.what());
        infile->Close();
        return false;
    }
    
    infile->Close();
    
    spdlog::info("[BloomFilter] Loaded filter: {} ({} bytes)",
                 bloom_path, filter_->GetBitsetSize());
    return true;
}

bool BloomFilterManager::MightContain(core::SeriesID series_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!filter_) {
        // No filter = assume might be present
        return true;
    }
    
    uint64_t hash = filter_->Hash(static_cast<int64_t>(series_id));
    return filter_->FindHash(hash);
}

bool BloomFilterManager::MightContainLabels(const std::string& labels_str) const {
    return MightContain(ComputeSeriesId(labels_str));
}

size_t BloomFilterManager::GetFilterSizeBytes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return filter_ ? filter_->GetBitsetSize() : 0;
}

std::string BloomFilterManager::GetBloomPath(const std::string& parquet_path) {
    // Replace .parquet with .bloom, or append .bloom if no extension
    std::string bloom_path = parquet_path;
    size_t ext_pos = bloom_path.rfind(".parquet");
    if (ext_pos != std::string::npos) {
        bloom_path.replace(ext_pos, 8, ".bloom");
    } else {
        bloom_path += ".bloom";
    }
    return bloom_path;
}

core::SeriesID BloomFilterManager::ComputeSeriesId(const std::string& labels_str) {
    // Use same hash function as rest of codebase
    return std::hash<std::string>{}(labels_str);
}

// ============================================================================
// BloomFilterCache Implementation
// ============================================================================

BloomFilterCache& BloomFilterCache::Instance() {
    static BloomFilterCache instance;
    return instance;
}

std::shared_ptr<BloomFilterManager> BloomFilterCache::GetOrLoad(
    const std::string& parquet_path) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check cache first
    auto it = cache_.find(parquet_path);
    if (it != cache_.end()) {
        return it->second;
    }
    
    // Try to load from disk
    auto manager = std::make_shared<BloomFilterManager>();
    if (manager->LoadFilter(parquet_path)) {
        cache_[parquet_path] = manager;
        return manager;
    }
    
    // No filter available - cache nullptr to avoid repeated disk checks
    cache_[parquet_path] = nullptr;
    return nullptr;
}

void BloomFilterCache::Invalidate(const std::string& parquet_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.erase(parquet_path);
}

void BloomFilterCache::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
}

size_t BloomFilterCache::Size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.size();
}

size_t BloomFilterCache::TotalMemoryBytes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t total = 0;
    for (const auto& [path, manager] : cache_) {
        if (manager) {
            total += manager->GetFilterSizeBytes();
        }
    }
    return total;
}

}  // namespace parquet
}  // namespace storage
}  // namespace tsdb
