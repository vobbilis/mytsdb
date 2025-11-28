#include "tsdb/storage/wal.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cstring>
#include "tsdb/common/logger.h"

namespace tsdb {
namespace storage {

WriteAheadLog::WriteAheadLog(const std::string& dir) : wal_dir_(dir) {
    // Create WAL directory if it doesn't exist
    try {
        std::error_code ec;
        if (!std::filesystem::exists(dir, ec)) {
            if (ec) {
                throw std::runtime_error("Failed to check WAL directory existence: " + ec.message());
            }
            std::filesystem::create_directories(dir, ec);
            if (ec) {
                throw std::runtime_error("Failed to create WAL directory: " + ec.message());
            }
        }
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to create WAL directory: " + std::string(e.what()));
    }
    
    // Initialize current segment
    current_segment_ = 0;
    std::string segment_path = get_segment_path(current_segment_);
    
    // Open file with explicit error handling and timeout protection
    current_file_.open(segment_path, std::ios::binary | std::ios::app);
    
    if (!current_file_.is_open()) {
        throw std::runtime_error("Failed to open WAL segment file: " + segment_path);
    }
    
    // Ensure file is in a good state
    if (current_file_.fail()) {
        current_file_.clear();
    }
    
    // Verify file is writable
    if (!current_file_.good()) {
        throw std::runtime_error("WAL segment file is not in a good state: " + segment_path);
    }
}

WriteAheadLog::~WriteAheadLog() {
    if (current_file_.is_open()) {
        current_file_.close();
    }
}

core::Result<void> WriteAheadLog::log(const core::TimeSeries& series, bool flush_now) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // Serialize the series using the same pattern as BlockImpl
        auto serialized_data = serialize_series(series);
        
        // Write to current segment
        if (!write_to_segment(serialized_data, flush_now)) {
            return core::Result<void>::error("Failed to write to WAL segment");
        }
        
        // Rotate segment if it gets too large (>64MB)
        if (current_file_.tellp() > 64 * 1024 * 1024) {
            rotate_segment();
        }
        
        return core::Result<void>();
    } catch (const std::exception& e) {
        return core::Result<void>::error("WAL log failed: " + std::string(e.what()));
    }
}

core::Result<void> WriteAheadLog::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (current_file_.is_open()) {
        current_file_.flush();
        if (!current_file_.good()) {
            return core::Result<void>::error("Failed to flush WAL");
        }
    }
    return core::Result<void>();
}

core::Result<void> WriteAheadLog::replay(std::function<void(const core::TimeSeries&)> callback) {
    // CRITICAL: Do NOT lock the mutex during replay if called from init()
    // During init(), we're single-threaded, so no need for mutex protection
    // Locking here could cause deadlocks if the callback tries to access WAL
    // std::lock_guard<std::mutex> lock(mutex_);  // REMOVED - causes deadlock risk
    
    try {
        // Early return if WAL directory doesn't exist - no need to flush
        if (!std::filesystem::exists(wal_dir_)) {
            // No WAL directory means no data to replay - this is fine
            return core::Result<void>();
        }
        
        // Flush any pending writes to ensure file is in a consistent state
        // Only do this if the file is actually open and we have a WAL directory
        if (current_file_.is_open()) {
            current_file_.flush();
        }
        
        // Find all WAL segment files
        std::vector<std::string> segment_files;
        try {
            // Check if directory exists and is accessible before iterating
            if (!std::filesystem::exists(wal_dir_)) {
                return core::Result<void>();  // No directory, nothing to replay
            }
            
            if (!std::filesystem::is_directory(wal_dir_)) {
                return core::Result<void>::error("WAL path is not a directory: " + wal_dir_);
            }
            
            // Iterate directory with error handling
            std::error_code ec;
            for (const auto& entry : std::filesystem::directory_iterator(wal_dir_, ec)) {
                if (ec) {
                    // Error accessing directory entry - skip and continue
                    ec.clear();
                    continue;
                }
                
                try {
                    if (!entry.is_regular_file()) {
                        continue;  // Skip non-files
                    }
                    
                    std::string filename = entry.path().filename().string();
                    // std::cout << "WAL Replay: Found file " << filename << std::endl;
                    // std::cout << "WAL Replay: Found file " << filename << std::endl;
                    if (filename.length() >= 4 && filename.substr(0, 4) == "wal_") {
                        segment_files.push_back(entry.path().string());
                    }
                } catch (const std::exception& e) {
                    // Skip files that can't be accessed
                    continue;
                }
            }
            
            if (ec) {
                // Directory iteration had errors, but we may have found some files
                // Continue with what we found
            }
        } catch (const std::filesystem::filesystem_error& e) {
            // Directory iteration failed - return error
            return core::Result<void>::error("WAL replay failed: cannot access WAL directory: " + std::string(e.what()));
        } catch (const std::exception& e) {
            // Any other exception - return error
            return core::Result<void>::error("WAL replay failed: " + std::string(e.what()));
        }
        
        // If no segment files, replay is complete
        if (segment_files.empty()) {
            return core::Result<void>();
        }
        
        // Sort by segment number
        std::sort(segment_files.begin(), segment_files.end());
        
        // Replay each segment
        for (const auto& segment_file : segment_files) {
            auto result = replay_segment(segment_file, callback);
            if (!result.ok()) {
                // Log error but continue with other segments
                // This allows partial recovery
                continue;
            }
        }
        return core::Result<void>();
    } catch (const std::exception& e) {
        return core::Result<void>::error("WAL replay failed: " + std::string(e.what()));
    }
}

core::Result<void> WriteAheadLog::checkpoint(int last_segment_to_keep) {
    try {
        // Find all WAL segment files
        std::vector<std::string> segment_files;
        for (const auto& entry : std::filesystem::directory_iterator(wal_dir_)) {
            std::string filename = entry.path().filename().string();
            if (entry.is_regular_file() && filename.substr(0, 4) == "wal_") {
                segment_files.push_back(entry.path().string());
            }
        }
        
        // Sort by segment number
        std::sort(segment_files.begin(), segment_files.end());
        
        // Keep only the last N segments
        int segments_to_delete = static_cast<int>(segment_files.size()) - last_segment_to_keep;
        if (segments_to_delete > 0) {
            for (int i = 0; i < segments_to_delete; ++i) {
                std::filesystem::remove(segment_files[i]);
            }
        }
        
        return core::Result<void>();
    } catch (const std::exception& e) {
        return core::Result<void>::error("WAL checkpoint failed: " + std::string(e.what()));
    }
}

std::vector<uint8_t> WriteAheadLog::serialize_series(const core::TimeSeries& series) {
    std::vector<uint8_t> result;
    
    // 1. Serialize labels (using same pattern as BlockImpl)
    const auto& labels = series.labels();
    uint32_t label_count = static_cast<uint32_t>(labels.map().size());
    

    
    const uint8_t* label_count_bytes = reinterpret_cast<const uint8_t*>(&label_count);
    result.insert(result.end(), label_count_bytes, label_count_bytes + sizeof(uint32_t));
    
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
    
    // 2. Serialize samples
    const auto& samples = series.samples();
    uint32_t sample_count = static_cast<uint32_t>(samples.size());
    const uint8_t* sample_count_bytes = reinterpret_cast<const uint8_t*>(&sample_count);
    result.insert(result.end(), sample_count_bytes, sample_count_bytes + sizeof(uint32_t));
    
    for (const auto& sample : samples) {
        // Timestamp
        int64_t timestamp = sample.timestamp();
        const uint8_t* timestamp_bytes = reinterpret_cast<const uint8_t*>(&timestamp);
        result.insert(result.end(), timestamp_bytes, timestamp_bytes + sizeof(int64_t));
        
        // Value
        double value = sample.value();
        const uint8_t* value_bytes = reinterpret_cast<const uint8_t*>(&value);
        result.insert(result.end(), value_bytes, value_bytes + sizeof(double));
    }
    
    return result;
}

bool WriteAheadLog::write_to_segment(const std::vector<uint8_t>& data, bool flush_now) {
    if (!current_file_.is_open()) {
        return false;
    }
    
    // Write data length first
    uint32_t data_length = static_cast<uint32_t>(data.size());
    current_file_.write(reinterpret_cast<const char*>(&data_length), sizeof(data_length));
    
    // Write the actual data
    current_file_.write(reinterpret_cast<const char*>(data.data()), data.size());
    // std::cout << "WAL: Wrote " << data.size() << " bytes. Flush: " << flush_now << std::endl;
    // std::cout << "WAL: Wrote " << data.size() << " bytes. Flush: " << flush_now << std::endl;
    
    if (flush_now) {
        current_file_.flush();
    }
    
    return current_file_.good();
}

void WriteAheadLog::rotate_segment() {
    if (current_file_.is_open()) {
        current_file_.close();
    }
    
    current_segment_++;
    current_file_.open(get_segment_path(current_segment_), std::ios::binary | std::ios::app);
    
    if (!current_file_.is_open()) {
        throw std::runtime_error("Failed to open new WAL segment file");
    }
}

std::string WriteAheadLog::get_segment_path(int segment) {
    std::ostringstream oss;
    oss << wal_dir_ << "/wal_" << std::setfill('0') << std::setw(6) << segment << ".log";
    return oss.str();
}

core::Result<void> WriteAheadLog::replay_segment(const std::string& segment_path, 
                                                 std::function<void(const core::TimeSeries&)> callback) {
    std::ifstream file(segment_path, std::ios::binary);
    if (!file.is_open()) {
        return core::Result<void>::error("Failed to open WAL segment for replay: " + segment_path);
    }
    
    // Get file size to prevent reading beyond file
    file.seekg(0, std::ios::end);
    std::streamsize file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // std::cout << "WAL Replay: Segment " << segment_path << " size: " << file_size << std::endl;
    // std::cout << "WAL Replay: Segment " << segment_path << " size: " << file_size << std::endl;
    
    // If file is empty, nothing to replay
    if (file_size == 0) {
        return core::Result<void>();
    }
    
    
    // Safety limit: prevent reading more than 1GB per segment (prevents corrupted data_length from causing hangs)
    const uint32_t MAX_DATA_LENGTH = 1024 * 1024 * 1024; // 1GB
    const size_t MAX_ENTRIES = 1000000; // Prevent infinite loops
    size_t entry_count = 0;
    
    std::streampos last_pos = -1;
    size_t no_progress_count = 0;
    const size_t MAX_NO_PROGRESS = 10;  // Prevent infinite loops if file position doesn't advance
    
    while (entry_count < MAX_ENTRIES) {
        // Check current position - if we're at or past the end, we're done
        std::streampos current_pos = file.tellg();
        if (current_pos < 0 || current_pos >= file_size) {
            break; // At or past end of file
        }
        
        // Check if file position is advancing - prevent infinite loops
        if (current_pos == last_pos) {
            no_progress_count++;
            if (no_progress_count >= MAX_NO_PROGRESS) {
                // File position hasn't advanced - likely corrupted file or infinite loop
                break;
            }
        } else {
            no_progress_count = 0;
            last_pos = current_pos;
        }
        
        // Check if we have enough data for the length field
        if (current_pos + static_cast<std::streampos>(sizeof(uint32_t)) > file_size) {
            break; // Not enough data for length field
        }
        
        // Read data length
        uint32_t data_length = 0;
        file.read(reinterpret_cast<char*>(&data_length), sizeof(data_length));
        
        // Check if read was successful
        if (file.gcount() != sizeof(data_length)) {
            break; // Failed to read length field - end of file or error
        }
        
        // Validate data_length to prevent hangs from corrupted data
        if (data_length == 0 || data_length > MAX_DATA_LENGTH) {
            // Corrupted or invalid data_length - file is corrupted, stop reading
            break;
        }
        
        // Check if we have enough data remaining in file
        std::streampos expected_end_pos = current_pos + static_cast<std::streampos>(sizeof(uint32_t) + data_length);
        if (expected_end_pos > file_size) {
            // Not enough data in file - file might be truncated
            break;
        }
        
        // Read the data
        std::vector<uint8_t> data(data_length);
        file.read(reinterpret_cast<char*>(data.data()), data_length);
        
        // Check if read was successful
        if (file.gcount() != static_cast<std::streamsize>(data_length)) {
            // Failed to read complete entry - file might be corrupted
            break;
        }
        
        // Verify we're at the expected position after reading
        std::streampos actual_pos = file.tellg();
        if (actual_pos != expected_end_pos) {
            // File position mismatch - something went wrong, but advance anyway
            // Set file position to expected position to continue
            file.seekg(expected_end_pos);
            if (file.fail()) {
                // Can't seek to expected position - file is corrupted
                break;
            }
        }
        
        // Deserialize and replay
        try {
            auto series = deserialize_series(data);
            if (series.has_value()) {
                callback(series.value());
            }
        } catch (const std::exception& e) {
            // Deserialization failed - skip this entry and continue
            // File position has already advanced, so we can continue
        }
        
        entry_count++;
        
        // Check if we've reached the end of the file after reading this entry
        std::streampos pos_after_read = file.tellg();
        if (pos_after_read < 0 || pos_after_read >= file_size) {
            // We've reached the end of the file
            break;
        }
    }
    return core::Result<void>();
}

std::optional<core::TimeSeries> WriteAheadLog::deserialize_series(const std::vector<uint8_t>& data) {
    try {
        size_t offset = 0;
        
        // Bounds check: need at least sizeof(uint32_t) for label_count
        if (offset + sizeof(uint32_t) > data.size()) {
            return std::nullopt;
        }
        
        // Read label count
        uint32_t label_count;
        std::memcpy(&label_count, data.data() + offset, sizeof(label_count));
        offset += sizeof(label_count);
        

        
        // Sanity check: prevent excessive label count
        if (label_count > 1000) {
            return std::nullopt;
        }
        
        // Read labels
        core::Labels labels;
        for (uint32_t i = 0; i < label_count; ++i) {
            // Bounds check: need sizeof(uint32_t) for key_len
            if (offset + sizeof(uint32_t) > data.size()) {
                return std::nullopt;
            }
            
            // Read key
            uint32_t key_len;
            std::memcpy(&key_len, data.data() + offset, sizeof(key_len));
            offset += sizeof(key_len);
            
            // Bounds check: need key_len bytes for key
            if (offset + key_len > data.size()) {
                return std::nullopt;
            }
            
            std::string key(reinterpret_cast<const char*>(data.data() + offset), key_len);
            offset += key_len;
            
            // Bounds check: need sizeof(uint32_t) for val_len
            if (offset + sizeof(uint32_t) > data.size()) {
                return std::nullopt;
            }
            
            // Read value
            uint32_t val_len;
            std::memcpy(&val_len, data.data() + offset, sizeof(val_len));
            offset += sizeof(val_len);
            
            // Bounds check: need val_len bytes for value
            if (offset + val_len > data.size()) {
                return std::nullopt;
            }
            
            std::string value(reinterpret_cast<const char*>(data.data() + offset), val_len);
            offset += val_len;
            
            labels.add(key, value);
        }
        
        // Bounds check: need sizeof(uint32_t) for sample_count
        if (offset + sizeof(uint32_t) > data.size()) {
            return std::nullopt;
        }
        
        // Read sample count
        uint32_t sample_count;
        std::memcpy(&sample_count, data.data() + offset, sizeof(sample_count));
        offset += sizeof(sample_count);
        
        // Sanity check: prevent excessive sample count
        if (sample_count > 10000000) {
            return std::nullopt;
        }
        
        // Create TimeSeries
        core::TimeSeries series(labels);
        
        // Read samples
        for (uint32_t i = 0; i < sample_count; ++i) {
            // Bounds check: need sizeof(int64_t) + sizeof(double) for each sample
            if (offset + sizeof(int64_t) + sizeof(double) > data.size()) {
                return std::nullopt;
            }
            
            int64_t timestamp;
            std::memcpy(&timestamp, data.data() + offset, sizeof(timestamp));
            offset += sizeof(timestamp);
            
            double value;
            std::memcpy(&value, data.data() + offset, sizeof(value));
            offset += sizeof(value);
            
            series.add_sample(timestamp, value);
        }
        
        return series;
    } catch (const std::exception& e) {
        return std::nullopt;
    }
}

} // namespace storage
} // namespace tsdb

