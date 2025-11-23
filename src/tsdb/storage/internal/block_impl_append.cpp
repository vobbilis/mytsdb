#include "tsdb/storage/internal/block_impl.h"
#include <cstring>

namespace tsdb {
namespace storage {
namespace internal {

void BlockImpl::update_time_range(int64_t ts) {
    if (ts < header_.start_time) {
        header_.start_time = ts;
    }
    if (ts > header_.end_time) {
        header_.end_time = ts;
    }
}

void BlockImpl::append(const core::Labels& labels, const core::Sample& sample) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Find the data for this specific series within the block
    auto& series_data = series_[labels];

    // Use the block's compressors to process the new sample.
    // In a real implementation, the compressors would return byte arrays
    // that we would append to the series_data vectors.
    // For now, we simulate this by calling the append methods.
    ts_compressor_->append(sample.timestamp());
    val_compressor_->append(sample.value());
    
    // This is a placeholder for where we would actually append the compressed bytes.
    // For example:
    // auto ts_bytes = ts_compressor_->bytes();
    // series_data.timestamps.insert(series_data.timestamps.end(), ts_bytes.begin(), ts_bytes.end());
    // auto val_bytes = val_compressor_->bytes();
    // series_data.values.insert(series_data.values.end(), val_bytes.begin(), val_bytes.end());

    update_time_range(sample.timestamp());
    dirty_ = true;
}

std::vector<uint8_t> BlockImpl::serialize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<uint8_t> result;
    
    // 1. Serialize the block header
    const uint8_t* header_bytes = reinterpret_cast<const uint8_t*>(&header_);
    result.insert(result.end(), header_bytes, header_bytes + sizeof(BlockHeader));
    
    // 2. Serialize the number of series in this block
    uint32_t num_series = static_cast<uint32_t>(series_.size());
    const uint8_t* num_series_bytes = reinterpret_cast<const uint8_t*>(&num_series);
    result.insert(result.end(), num_series_bytes, num_series_bytes + sizeof(uint32_t));
    
    // 3. Serialize each series' data
    for (const auto& [labels, data] : series_) {
        // Serialize label count
        uint32_t label_count = static_cast<uint32_t>(labels.map().size());
        const uint8_t* label_count_bytes = reinterpret_cast<const uint8_t*>(&label_count);
        result.insert(result.end(), label_count_bytes, label_count_bytes + sizeof(uint32_t));
        
        // Serialize each label key-value pair
        for (const auto& [key, value] : labels.map()) {
            // Key length + key
            uint32_t key_len = static_cast<uint32_t>(key.size());
            const uint8_t* key_len_bytes = reinterpret_cast<const uint8_t*>(&key_len);
            result.insert(result.end(), key_len_bytes, key_len_bytes + sizeof(uint32_t));
            result.insert(result.end(), key.begin(), key.end());
            
            // Value length + value
            uint32_t val_len = static_cast<uint32_t>(value.size());
            const uint8_t* val_len_bytes = reinterpret_cast<const uint8_t*>(&val_len);
            result.insert(result.end(), val_len_bytes, val_len_bytes + sizeof(uint32_t));
            result.insert(result.end(), value.begin(), value.end());
        }
        
        // Serialize timestamp data
        uint32_t ts_size = static_cast<uint32_t>(data.timestamps.size());
        const uint8_t* ts_size_bytes = reinterpret_cast<const uint8_t*>(&ts_size);
        result.insert(result.end(), ts_size_bytes, ts_size_bytes + sizeof(uint32_t));
        result.insert(result.end(), data.timestamps.begin(), data.timestamps.end());
        
        // Serialize value data
        uint32_t val_size = static_cast<uint32_t>(data.values.size());
        const uint8_t* val_size_bytes = reinterpret_cast<const uint8_t*>(&val_size);
        result.insert(result.end(), val_size_bytes, val_size_bytes + sizeof(uint32_t));
        result.insert(result.end(), data.values.begin(), data.values.end());
    }
    
    return result;
}

} // namespace internal
} // namespace storage
} // namespace tsdb
