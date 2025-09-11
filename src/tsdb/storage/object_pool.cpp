/**
 * @file object_pool.cpp
 * @brief Object pooling implementation for memory-efficient resource management
 * 
 * This file implements object pools for frequently allocated/deallocated
 * objects in the TSDB system. Object pooling reduces memory allocation
 * overhead and improves performance by reusing objects instead of
 * constantly creating and destroying them.
 * 
 * The implementation provides three main object pools:
 * 1. TimeSeriesPool - For core::TimeSeries objects
 * 2. LabelsPool - For core::Labels objects  
 * 3. SamplePool - For core::Sample objects
 * 
 * Each pool features:
 * - Thread-safe operations with mutex protection
 * - Configurable initial and maximum pool sizes
 * - Statistics tracking for performance monitoring
 * - Automatic object cleanup and reuse
 * - Memory-efficient object management
 * 
 * Benefits:
 * - Reduced memory allocation overhead
 * - Improved cache locality
 * - Better garbage collection performance
 * - Predictable memory usage patterns
 */

#include "tsdb/storage/object_pool.h"
#include <sstream>
#include <iostream>

namespace tsdb {
namespace storage {

// ============================================================================
// TimeSeriesPool Implementation
// ============================================================================

/**
 * @brief Constructs a TimeSeriesPool with specified initial and maximum sizes
 * 
 * @param initial_size Number of objects to pre-allocate in the pool
 * @param max_size Maximum number of objects the pool can hold
 * 
 * This constructor initializes the object pool by:
 * - Pre-allocating the specified number of TimeSeries objects
 * - Setting the maximum pool size limit
 * - Initializing atomic counters for statistics tracking
 * 
 * Pre-allocation reduces allocation overhead during peak usage by
 * having objects ready for immediate use.
 * 
 * Thread Safety: Constructor is not thread-safe, should be called
 * before any concurrent access to the pool.
 */
TimeSeriesPool::TimeSeriesPool(size_t initial_size, size_t max_size)
    : max_size_(max_size) {
    // Pre-allocate initial objects
    for (size_t i = 0; i < initial_size; ++i) {
        pool_.push(create_object());
        total_created_.fetch_add(1, std::memory_order_relaxed);
    }
}

TimeSeriesPool::~TimeSeriesPool() = default;

/**
 * @brief Acquires a TimeSeries object from the pool
 * 
 * @return Unique pointer to a TimeSeries object ready for use
 * 
 * This method provides a TimeSeries object for use. It:
 * - Returns a pre-allocated object from the pool if available
 * - Creates a new object if the pool is empty
 * - Clears the object's state for safe reuse
 * - Updates acquisition statistics
 * 
 * The returned object is guaranteed to be in a clean state
 * and ready for immediate use. The caller owns the object
 * and is responsible for releasing it back to the pool when done.
 * 
 * Thread Safety: Uses mutex locking to ensure thread-safe access
 * to the pool's internal data structures.
 */
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

/**
 * @brief Releases a TimeSeries object back to the pool
 * 
 * @param obj Unique pointer to the TimeSeries object to release
 * 
 * This method returns a TimeSeries object to the pool for reuse. It:
 * - Clears the object's state for safe reuse
 * - Adds the object back to the pool if under maximum size
 * - Destroys the object if the pool is at maximum capacity
 * - Updates release statistics
 * 
 * The pool enforces a maximum size limit to prevent unbounded
 * memory growth. When the pool is full, objects are destroyed
 * rather than stored, maintaining memory efficiency.
 * 
 * Thread Safety: Uses mutex locking to ensure thread-safe access
 * to the pool's internal data structures.
 */
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

/**
 * @brief Returns detailed statistics about the TimeSeriesPool
 * 
 * @return String containing formatted pool statistics
 * 
 * This method provides comprehensive statistics about the pool's
 * performance and usage patterns, including:
 * - Current number of available objects in the pool
 * - Maximum pool size limit
 * - Total number of objects created since pool initialization
 * - Total number of objects acquired from the pool
 * - Total number of objects released back to the pool
 * - Object reuse ratio (percentage of reused vs. newly created objects)
 * 
 * The reuse ratio is a key performance metric that indicates
 * how effectively the pool is reducing memory allocation overhead.
 * Higher reuse ratios indicate better performance benefits.
 * 
 * Thread Safety: Uses mutex locking to ensure consistent statistics
 * during concurrent access.
 */
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
        // Calculate reuse ratio: (acquired - created) / acquired * 100
        // This represents the percentage of requests that were served from the pool
        if (total_acquired > total_created) {
            double reuse_ratio = static_cast<double>(total_acquired - total_created) / total_acquired * 100.0;
            oss << "  Object reuse ratio: " << reuse_ratio << "%\n";
        } else {
            oss << "  Object reuse ratio: 0.00%\n";
        }
    }
    
    return oss.str();
}

/**
 * @brief Returns the number of available objects in the pool
 * @return Number of objects currently available for acquisition
 * 
 * Thread Safety: Uses mutex locking to ensure accurate count
 * during concurrent access.
 */
size_t TimeSeriesPool::available() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pool_.size();
}

/**
 * @brief Returns the total number of objects created since pool initialization
 * @return Total count of objects created (including those currently in use)
 * 
 * This includes all objects ever created, not just those currently
 * in the pool. Objects currently in use are included in this count.
 * 
 * Thread Safety: Uses atomic load for thread-safe access.
 */
size_t TimeSeriesPool::total_created() const {
    return total_created_.load(std::memory_order_relaxed);
}

/**
 * @brief Returns the maximum size limit of the pool
 * @return Maximum number of objects the pool can hold
 */
size_t TimeSeriesPool::max_size() const {
    return max_size_;
}

/**
 * @brief Creates a new TimeSeries object
 * @return Unique pointer to a newly created TimeSeries object
 * 
 * This factory method creates new TimeSeries objects when the pool
 * is empty and new objects are needed.
 */
std::unique_ptr<core::TimeSeries> TimeSeriesPool::create_object() {
    return std::make_unique<core::TimeSeries>();
}

// ============================================================================
// LabelsPool Implementation
// ============================================================================

/**
 * @brief Constructs a LabelsPool with specified initial and maximum sizes
 * 
 * @param initial_size Number of Labels objects to pre-allocate in the pool
 * @param max_size Maximum number of Labels objects the pool can hold
 * 
 * This constructor initializes the Labels object pool by:
 * - Pre-allocating the specified number of Labels objects
 * - Setting the maximum pool size limit
 * - Initializing atomic counters for statistics tracking
 * 
 * Labels objects are frequently created and destroyed during
 * time series operations, making them ideal candidates for pooling.
 * 
 * Thread Safety: Constructor is not thread-safe, should be called
 * before any concurrent access to the pool.
 */
LabelsPool::LabelsPool(size_t initial_size, size_t max_size)
    : max_size_(max_size) {
    // Pre-allocate initial objects
    for (size_t i = 0; i < initial_size; ++i) {
        pool_.push(create_object());
        total_created_.fetch_add(1, std::memory_order_relaxed);
    }
}

LabelsPool::~LabelsPool() = default;

/**
 * @brief Acquires a Labels object from the pool
 * 
 * @return Unique pointer to a Labels object ready for use
 * 
 * This method provides a Labels object for use. It:
 * - Returns a pre-allocated object from the pool if available
 * - Creates a new object if the pool is empty
 * - Updates acquisition statistics
 * 
 * Unlike TimeSeries objects, Labels objects don't need explicit
 * clearing since they start in a clean state and are typically
 * populated with new label data.
 * 
 * The returned object is guaranteed to be ready for immediate use.
 * The caller owns the object and is responsible for releasing it
 * back to the pool when done.
 * 
 * Thread Safety: Uses mutex locking to ensure thread-safe access
 * to the pool's internal data structures.
 */
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

/**
 * @brief Releases a Labels object back to the pool
 * 
 * @param obj Unique pointer to the Labels object to release
 * 
 * This method returns a Labels object to the pool for reuse. It:
 * - Clears the object's state for safe reuse
 * - Adds the object back to the pool if under maximum size
 * - Destroys the object if the pool is at maximum capacity
 * - Updates release statistics
 * 
 * The pool enforces a maximum size limit to prevent unbounded
 * memory growth. When the pool is full, objects are destroyed
 * rather than stored, maintaining memory efficiency.
 * 
 * Thread Safety: Uses mutex locking to ensure thread-safe access
 * to the pool's internal data structures.
 */
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

/**
 * @brief Returns detailed statistics about the LabelsPool
 * 
 * @return String containing formatted pool statistics
 * 
 * This method provides comprehensive statistics about the LabelsPool's
 * performance and usage patterns, including:
 * - Current number of available objects in the pool
 * - Maximum pool size limit
 * - Total number of objects created since pool initialization
 * - Total number of objects acquired from the pool
 * - Total number of objects released back to the pool
 * - Object reuse ratio (percentage of reused vs. newly created objects)
 * 
 * The reuse ratio indicates how effectively the pool is reducing
 * memory allocation overhead for Labels objects.
 * 
 * Thread Safety: Uses mutex locking to ensure consistent statistics
 * during concurrent access.
 */
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
        // Calculate reuse ratio: (acquired - created) / acquired * 100
        // This represents the percentage of requests that were served from the pool
        if (total_acquired > total_created) {
            double reuse_ratio = static_cast<double>(total_acquired - total_created) / total_acquired * 100.0;
            oss << "  Object reuse ratio: " << reuse_ratio << "%\n";
        } else {
            oss << "  Object reuse ratio: 0.00%\n";
        }
    }
    
    return oss.str();
}

/**
 * @brief Returns the number of available Labels objects in the pool
 * @return Number of objects currently available for acquisition
 * 
 * Thread Safety: Uses mutex locking to ensure accurate count
 * during concurrent access.
 */
size_t LabelsPool::available() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pool_.size();
}

/**
 * @brief Returns the total number of Labels objects created since pool initialization
 * @return Total count of objects created (including those currently in use)
 * 
 * Thread Safety: Uses atomic load for thread-safe access.
 */
size_t LabelsPool::total_created() const {
    return total_created_.load(std::memory_order_relaxed);
}

/**
 * @brief Creates a new Labels object
 * @return Unique pointer to a newly created Labels object
 * 
 * This factory method creates new Labels objects when the pool
 * is empty and new objects are needed.
 */
std::unique_ptr<core::Labels> LabelsPool::create_object() {
    return std::make_unique<core::Labels>();
}

// ============================================================================
// SamplePool Implementation
// ============================================================================

/**
 * @brief Constructs a SamplePool with specified initial and maximum sizes
 * 
 * @param initial_size Number of Sample objects to pre-allocate in the pool
 * @param max_size Maximum number of Sample objects the pool can hold
 * 
 * This constructor initializes the Sample object pool by:
 * - Pre-allocating the specified number of Sample objects
 * - Setting the maximum pool size limit
 * - Initializing atomic counters for statistics tracking
 * 
 * Sample objects are the most frequently allocated objects in the TSDB,
 * representing individual data points. Pooling them provides significant
 * performance benefits due to their high allocation frequency.
 * 
 * Thread Safety: Constructor is not thread-safe, should be called
 * before any concurrent access to the pool.
 */
SamplePool::SamplePool(size_t initial_size, size_t max_size)
    : max_size_(max_size) {
    // Pre-allocate initial objects
    for (size_t i = 0; i < initial_size; ++i) {
        pool_.push(create_object());
        total_created_.fetch_add(1, std::memory_order_relaxed);
    }
}

SamplePool::~SamplePool() = default;

/**
 * @brief Acquires a Sample object from the pool
 * 
 * @return Unique pointer to a Sample object ready for use
 * 
 * This method provides a Sample object for use. It:
 * - Returns a pre-allocated object from the pool if available
 * - Creates a new object if the pool is empty
 * - Updates acquisition statistics
 * 
 * Sample objects represent individual data points in time series.
 * They are the most frequently allocated objects in the TSDB,
 * making pooling particularly beneficial for performance.
 * 
 * The returned object is guaranteed to be ready for immediate use.
 * The caller owns the object and is responsible for releasing it
 * back to the pool when done.
 * 
 * Thread Safety: Uses mutex locking to ensure thread-safe access
 * to the pool's internal data structures.
 */
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

/**
 * @brief Releases a Sample object back to the pool
 * 
 * @param obj Unique pointer to the Sample object to release
 * 
 * This method returns a Sample object to the pool for reuse. It:
 * - Adds the object back to the pool if under maximum size
 * - Destroys the object if the pool is at maximum capacity
 * - Updates release statistics
 * 
 * Unlike TimeSeries and Labels objects, Sample objects don't need
 * explicit clearing since they are simple value objects that are
 * typically overwritten with new data.
 * 
 * The pool enforces a maximum size limit to prevent unbounded
 * memory growth. When the pool is full, objects are destroyed
 * rather than stored, maintaining memory efficiency.
 * 
 * Thread Safety: Uses mutex locking to ensure thread-safe access
 * to the pool's internal data structures.
 */
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

/**
 * @brief Returns detailed statistics about the SamplePool
 * 
 * @return String containing formatted pool statistics
 * 
 * This method provides comprehensive statistics about the SamplePool's
 * performance and usage patterns, including:
 * - Current number of available objects in the pool
 * - Maximum pool size limit
 * - Total number of objects created since pool initialization
 * - Total number of objects acquired from the pool
 * - Total number of objects released back to the pool
 * - Object reuse ratio (percentage of reused vs. newly created objects)
 * 
 * Sample objects have the highest allocation frequency in the TSDB,
 * making their reuse ratio a critical performance metric.
 * 
 * Thread Safety: Uses mutex locking to ensure consistent statistics
 * during concurrent access.
 */
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
        // Calculate reuse ratio: (acquired - created) / acquired * 100
        // This represents the percentage of requests that were served from the pool
        if (total_acquired > total_created) {
            double reuse_ratio = static_cast<double>(total_acquired - total_created) / total_acquired * 100.0;
            oss << "  Object reuse ratio: " << reuse_ratio << "%\n";
        } else {
            oss << "  Object reuse ratio: 0.00%\n";
        }
    }
    
    return oss.str();
}

/**
 * @brief Returns the number of available Sample objects in the pool
 * @return Number of objects currently available for acquisition
 * 
 * Thread Safety: Uses mutex locking to ensure accurate count
 * during concurrent access.
 */
size_t SamplePool::available() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pool_.size();
}

/**
 * @brief Returns the total number of Sample objects created since pool initialization
 * @return Total count of objects created (including those currently in use)
 * 
 * Thread Safety: Uses atomic load for thread-safe access.
 */
size_t SamplePool::total_created() const {
    return total_created_.load(std::memory_order_relaxed);
}

/**
 * @brief Creates a new Sample object with default values
 * @return Unique pointer to a newly created Sample object
 * 
 * This factory method creates new Sample objects when the pool
 * is empty and new objects are needed. Creates objects with
 * default timestamp (0) and value (0.0).
 */
std::unique_ptr<core::Sample> SamplePool::create_object() {
    return std::make_unique<core::Sample>(0, 0.0); // Default values
}

} // namespace storage
} // namespace tsdb 