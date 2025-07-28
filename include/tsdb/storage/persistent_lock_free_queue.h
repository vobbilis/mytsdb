#pragma once

#include "tsdb/storage/lock_free_queue.h"
#include <fstream>
#include <filesystem>
#include <mutex>
#include <queue>
#include <atomic>

namespace tsdb {
namespace storage {

/**
 * @brief Persistent lock-free queue implementation with file-based storage.
 * 
 * This implementation follows AdvPerf's approach:
 * - Fast in-memory operations using the base LockFreeQueue
 * - File-based persistence when memory is full
 * - Background flushing and loading capabilities
 * - Configurable storage limits and policies
 */
template<typename T>
class PersistentLockFreeQueue : public LockFreeQueue<T> {
public:
    explicit PersistentLockFreeQueue(size_t capacity, 
                                    const PersistentQueueConfig& config = PersistentQueueConfig{})
        : LockFreeQueue<T>(capacity, config) {
        if (config.enable_persistence) {
            initialize_persistence();
        }
    }
    
    ~PersistentLockFreeQueue() {
        if (this->config_.enable_persistence) {
            flush_all_to_disk();
        }
    }

protected:
    // Override virtual methods for actual persistence implementation
    bool persist_item(const T& item) override {
        if (!this->config_.enable_persistence) {
            return false;
        }
        
        // Check if we're at the persistent storage limit
        if (this->config_.max_persistent_size > 0 && 
            get_persistent_storage_size() >= this->config_.max_persistent_size) {
            if (this->config_.drop_on_persistent_full) {
                return false; // Drop the item
            }
            // Could implement LRU eviction here
        }
        
        // Add to in-memory buffer for batch writing
        {
            std::lock_guard<std::mutex> lock(persistent_buffer_mutex_);
            persistent_buffer_.push(item);
        }
        
        // Trigger background flush if buffer is getting large
        if (persistent_buffer_.size() >= flush_threshold_) {
            flush_buffer_to_disk();
        }
        
        // Update counters
        this->persistent_item_count_.fetch_add(1, std::memory_order_relaxed);
        this->persistent_storage_size_.fetch_add(sizeof(T), std::memory_order_relaxed);
        
        return true;
    }
    
    bool load_persistent_item(T& item) override {
        if (!this->config_.enable_persistence) {
            return false;
        }
        
        // Try to load from disk if memory buffer is empty
        if (persistent_buffer_.empty()) {
            load_buffer_from_disk();
        }
        
        // Get item from buffer
        {
            std::lock_guard<std::mutex> lock(persistent_buffer_mutex_);
            if (persistent_buffer_.empty()) {
                return false; // No items available
            }
            
            item = persistent_buffer_.front();
            persistent_buffer_.pop();
        }
        
        // Update counters
        this->persistent_item_count_.fetch_sub(1, std::memory_order_relaxed);
        this->persistent_storage_size_.fetch_sub(sizeof(T), std::memory_order_relaxed);
        
        return true;
    }
    
    bool clear_persistent_storage() override {
        if (!this->config_.enable_persistence) {
            return false;
        }
        
        // Clear in-memory buffer
        {
            std::lock_guard<std::mutex> lock(persistent_buffer_mutex_);
            while (!persistent_buffer_.empty()) {
                persistent_buffer_.pop();
            }
        }
        
        // Clear disk storage
        try {
            std::filesystem::remove_all(this->config_.persistence_path);
            std::filesystem::create_directories(this->config_.persistence_path);
        } catch (const std::exception& e) {
            // Log error but don't fail
            return false;
        }
        
        // Reset counters
        this->persistent_item_count_.store(0, std::memory_order_relaxed);
        this->persistent_storage_size_.store(0, std::memory_order_relaxed);
        
        return true;
    }
    
    size_t get_persistent_item_count() const override {
        if (!this->config_.enable_persistence) {
            return 0;
        }
        
        // Count includes both in-memory buffer and disk storage
        size_t buffer_count = 0;
        {
            std::lock_guard<std::mutex> lock(persistent_buffer_mutex_);
            buffer_count = persistent_buffer_.size();
        }
        
        return this->persistent_item_count_.load(std::memory_order_relaxed) + buffer_count;
    }
    
    size_t get_persistent_storage_size() const override {
        if (!this->config_.enable_persistence) {
            return 0;
        }
        
        // Size includes both in-memory buffer and disk storage
        size_t buffer_size = 0;
        {
            std::lock_guard<std::mutex> lock(persistent_buffer_mutex_);
            buffer_size = persistent_buffer_.size() * sizeof(T);
        }
        
        return this->persistent_storage_size_.load(std::memory_order_relaxed) + buffer_size;
    }

private:
    void initialize_persistence() {
        try {
            std::filesystem::create_directories(this->config_.persistence_path);
        } catch (const std::exception& e) {
            // Log error but don't fail initialization
        }
    }
    
    void flush_buffer_to_disk() {
        std::queue<T> items_to_flush;
        
        // Move items from buffer to local queue
        {
            std::lock_guard<std::mutex> lock(persistent_buffer_mutex_);
            items_to_flush = std::move(persistent_buffer_);
        }
        
        // Write items to disk
        std::string filename = this->config_.persistence_path + "/queue_" + 
                              std::to_string(file_counter_++) + ".dat";
        
        std::ofstream file(filename, std::ios::binary | std::ios::app);
        if (file.is_open()) {
            while (!items_to_flush.empty()) {
                T item = items_to_flush.front();
                items_to_flush.pop();
                
                // Simple binary serialization (could be optimized)
                file.write(reinterpret_cast<const char*>(&item), sizeof(T));
            }
            file.close();
        }
    }
    
    void load_buffer_from_disk() {
        // Find the oldest file and load from it
        std::string oldest_file;
        try {
            for (const auto& entry : std::filesystem::directory_iterator(this->config_.persistence_path)) {
                if (entry.path().extension() == ".dat") {
                    if (oldest_file.empty() || entry.path() < oldest_file) {
                        oldest_file = entry.path().string();
                    }
                }
            }
        } catch (const std::exception& e) {
            return; // No files to load
        }
        
        if (oldest_file.empty()) {
            return; // No files found
        }
        
        // Load items from the oldest file
        std::ifstream file(oldest_file, std::ios::binary);
        if (file.is_open()) {
            T item;
            while (file.read(reinterpret_cast<char*>(&item), sizeof(T))) {
                std::lock_guard<std::mutex> lock(persistent_buffer_mutex_);
                persistent_buffer_.push(item);
                
                // Stop if buffer is getting too large
                if (persistent_buffer_.size() >= load_threshold_) {
                    break;
                }
            }
            file.close();
            
            // Remove the file after loading
            std::filesystem::remove(oldest_file);
        }
    }
    
    void flush_all_to_disk() {
        flush_buffer_to_disk();
    }
    
    // Configuration
    static constexpr size_t flush_threshold_ = 1000;  // Flush when buffer reaches this size
    static constexpr size_t load_threshold_ = 500;    // Load up to this many items at once
    
    // Persistent storage state
    mutable std::mutex persistent_buffer_mutex_;
    std::queue<T> persistent_buffer_;
    std::atomic<size_t> file_counter_{0};
};

} // namespace storage
} // namespace tsdb 