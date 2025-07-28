#include "tsdb/storage/object_pool.h"
#include <sstream>
#include <iostream>

namespace tsdb {
namespace storage {

// ============================================================================
// TimeSeriesPool Implementation
// ============================================================================

TimeSeriesPool::TimeSeriesPool(size_t initial_size, size_t max_size)
    : max_size_(max_size) {
    // Pre-allocate initial objects
    for (size_t i = 0; i < initial_size; ++i) {
        pool_.push(create_object());
        total_created_.fetch_add(1, std::memory_order_relaxed);
    }
}

TimeSeriesPool::~TimeSeriesPool() = default;

std::unique_ptr<core::TimeSeries> TimeSeriesPool::acquire() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (pool_.empty()) {
        // Create new object if pool is empty
        auto obj = create_object();
        total_created_.fetch_add(1, std::memory_order_relaxed);
        total_acquired_.fetch_add(1, std::memory_order_relaxed);
        return obj;
    }
    
    // Get object from pool
    auto obj = std::move(pool_.top());
    pool_.pop();
    total_acquired_.fetch_add(1, std::memory_order_relaxed);
    
    // Clear the object for reuse
    obj->clear();
    
    return obj;
}

void TimeSeriesPool::release(std::unique_ptr<core::TimeSeries> obj) {
    if (!obj) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Only add back to pool if we haven't reached max size
    if (pool_.size() < max_size_) {
        // Clear the object for reuse
        obj->clear();
        pool_.push(std::move(obj));
        total_released_.fetch_add(1, std::memory_order_relaxed);
    }
    // If pool is full, the object will be destroyed when unique_ptr goes out of scope
}

std::string TimeSeriesPool::stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream oss;
    oss << "TimeSeriesPool Statistics:\n";
    oss << "  Available objects: " << pool_.size() << "\n";
    oss << "  Max pool size: " << max_size_ << "\n";
    oss << "  Total created: " << total_created_.load(std::memory_order_relaxed) << "\n";
    oss << "  Total acquired: " << total_acquired_.load(std::memory_order_relaxed) << "\n";
    oss << "  Total released: " << total_released_.load(std::memory_order_relaxed) << "\n";
    
    size_t total_acquired = total_acquired_.load(std::memory_order_relaxed);
    size_t total_created = total_created_.load(std::memory_order_relaxed);
    if (total_acquired > 0) {
        double reuse_ratio = static_cast<double>(total_acquired - total_created) / total_acquired * 100.0;
        oss << "  Object reuse ratio: " << reuse_ratio << "%\n";
    }
    
    return oss.str();
}

size_t TimeSeriesPool::available() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pool_.size();
}

size_t TimeSeriesPool::total_created() const {
    return total_created_.load(std::memory_order_relaxed);
}

size_t TimeSeriesPool::max_size() const {
    return max_size_;
}

std::unique_ptr<core::TimeSeries> TimeSeriesPool::create_object() {
    return std::make_unique<core::TimeSeries>();
}

// ============================================================================
// LabelsPool Implementation
// ============================================================================

LabelsPool::LabelsPool(size_t initial_size, size_t max_size)
    : max_size_(max_size) {
    // Pre-allocate initial objects
    for (size_t i = 0; i < initial_size; ++i) {
        pool_.push(create_object());
        total_created_.fetch_add(1, std::memory_order_relaxed);
    }
}

LabelsPool::~LabelsPool() = default;

std::unique_ptr<core::Labels> LabelsPool::acquire() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (pool_.empty()) {
        // Create new object if pool is empty
        auto obj = create_object();
        total_created_.fetch_add(1, std::memory_order_relaxed);
        total_acquired_.fetch_add(1, std::memory_order_relaxed);
        return obj;
    }
    
    // Get object from pool
    auto obj = std::move(pool_.top());
    pool_.pop();
    total_acquired_.fetch_add(1, std::memory_order_relaxed);
    
    return obj;
}

void LabelsPool::release(std::unique_ptr<core::Labels> obj) {
    if (!obj) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Only add back to pool if we haven't reached max size
    if (pool_.size() < max_size_) {
        // Clear the object for reuse
        obj->clear();
        pool_.push(std::move(obj));
        total_released_.fetch_add(1, std::memory_order_relaxed);
    }
    // If pool is full, the object will be destroyed when unique_ptr goes out of scope
}

std::string LabelsPool::stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream oss;
    oss << "LabelsPool Statistics:\n";
    oss << "  Available objects: " << pool_.size() << "\n";
    oss << "  Max pool size: " << max_size_ << "\n";
    oss << "  Total created: " << total_created_.load(std::memory_order_relaxed) << "\n";
    oss << "  Total acquired: " << total_acquired_.load(std::memory_order_relaxed) << "\n";
    oss << "  Total released: " << total_released_.load(std::memory_order_relaxed) << "\n";
    
    size_t total_acquired = total_acquired_.load(std::memory_order_relaxed);
    size_t total_created = total_created_.load(std::memory_order_relaxed);
    if (total_acquired > 0) {
        double reuse_ratio = static_cast<double>(total_acquired - total_created) / total_acquired * 100.0;
        oss << "  Object reuse ratio: " << reuse_ratio << "%\n";
    }
    
    return oss.str();
}

size_t LabelsPool::available() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pool_.size();
}

size_t LabelsPool::total_created() const {
    return total_created_.load(std::memory_order_relaxed);
}

std::unique_ptr<core::Labels> LabelsPool::create_object() {
    return std::make_unique<core::Labels>();
}

// ============================================================================
// SamplePool Implementation
// ============================================================================

SamplePool::SamplePool(size_t initial_size, size_t max_size)
    : max_size_(max_size) {
    // Pre-allocate initial objects
    for (size_t i = 0; i < initial_size; ++i) {
        pool_.push(create_object());
        total_created_.fetch_add(1, std::memory_order_relaxed);
    }
}

SamplePool::~SamplePool() = default;

std::unique_ptr<core::Sample> SamplePool::acquire() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (pool_.empty()) {
        // Create new object if pool is empty
        auto obj = create_object();
        total_created_.fetch_add(1, std::memory_order_relaxed);
        total_acquired_.fetch_add(1, std::memory_order_relaxed);
        return obj;
    }
    
    // Get object from pool
    auto obj = std::move(pool_.top());
    pool_.pop();
    total_acquired_.fetch_add(1, std::memory_order_relaxed);
    
    return obj;
}

void SamplePool::release(std::unique_ptr<core::Sample> obj) {
    if (!obj) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Only add back to pool if we haven't reached max size
    if (pool_.size() < max_size_) {
        pool_.push(std::move(obj));
        total_released_.fetch_add(1, std::memory_order_relaxed);
    }
    // If pool is full, the object will be destroyed when unique_ptr goes out of scope
}

std::string SamplePool::stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream oss;
    oss << "SamplePool Statistics:\n";
    oss << "  Available objects: " << pool_.size() << "\n";
    oss << "  Max pool size: " << max_size_ << "\n";
    oss << "  Total created: " << total_created_.load(std::memory_order_relaxed) << "\n";
    oss << "  Total acquired: " << total_acquired_.load(std::memory_order_relaxed) << "\n";
    oss << "  Total released: " << total_released_.load(std::memory_order_relaxed) << "\n";
    
    size_t total_acquired = total_acquired_.load(std::memory_order_relaxed);
    size_t total_created = total_created_.load(std::memory_order_relaxed);
    if (total_acquired > 0) {
        double reuse_ratio = static_cast<double>(total_acquired - total_created) / total_acquired * 100.0;
        oss << "  Object reuse ratio: " << reuse_ratio << "%\n";
    }
    
    return oss.str();
}

size_t SamplePool::available() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pool_.size();
}

size_t SamplePool::total_created() const {
    return total_created_.load(std::memory_order_relaxed);
}

std::unique_ptr<core::Sample> SamplePool::create_object() {
    return std::make_unique<core::Sample>(0, 0.0); // Default values
}

} // namespace storage
} // namespace tsdb 