#ifndef TSDB_STORAGE_OBJECT_POOL_H_
#define TSDB_STORAGE_OBJECT_POOL_H_

#include <memory>
#include <stack>
#include <mutex>
#include <atomic>
#include <cstddef>

#include "tsdb/core/types.h"

namespace tsdb {
namespace storage {

/**
 * @brief Thread-safe object pool for TimeSeries objects
 * 
 * This pool reduces memory allocations by reusing TimeSeries objects
 * instead of creating new ones on every write operation.
 */
class TimeSeriesPool {
public:
    /**
     * @brief Constructor
     * @param initial_size Initial number of objects to pre-allocate
     * @param max_size Maximum number of objects in the pool
     */
    explicit TimeSeriesPool(size_t initial_size = 100, size_t max_size = 10000);
    
    /**
     * @brief Destructor
     */
    ~TimeSeriesPool();
    
    /**
     * @brief Acquire a TimeSeries object from the pool
     * @return Unique pointer to a TimeSeries object
     */
    std::unique_ptr<core::TimeSeries> acquire();
    
    /**
     * @brief Release a TimeSeries object back to the pool
     * @param obj Unique pointer to the object to release
     */
    void release(std::unique_ptr<core::TimeSeries> obj);
    
    /**
     * @brief Get current pool statistics
     * @return String containing pool statistics
     */
    std::string stats() const;
    
    /**
     * @brief Get the number of objects currently in the pool
     * @return Number of available objects
     */
    size_t available() const;
    
    /**
     * @brief Get the total number of objects created by the pool
     * @return Total number of objects created
     */
    size_t total_created() const;
    
    /**
     * @brief Get the maximum pool size
     * @return Maximum number of objects allowed in the pool
     */
    size_t max_size() const;

private:
    mutable std::mutex mutex_;
    std::stack<std::unique_ptr<core::TimeSeries>> pool_;
    std::atomic<size_t> total_created_{0};
    std::atomic<size_t> total_acquired_{0};
    std::atomic<size_t> total_released_{0};
    size_t max_size_;
    
    /**
     * @brief Create a new TimeSeries object
     * @return Unique pointer to a new TimeSeries object
     */
    std::unique_ptr<core::TimeSeries> create_object();
};

/**
 * @brief Thread-safe object pool for Labels objects
 * 
 * Labels are frequently created and destroyed during write operations.
 * This pool helps reduce allocations for label objects.
 */
class LabelsPool {
public:
    /**
     * @brief Constructor
     * @param initial_size Initial number of objects to pre-allocate
     * @param max_size Maximum number of objects in the pool
     */
    explicit LabelsPool(size_t initial_size = 200, size_t max_size = 20000);
    
    /**
     * @brief Destructor
     */
    ~LabelsPool();
    
    /**
     * @brief Acquire a Labels object from the pool
     * @return Unique pointer to a Labels object
     */
    std::unique_ptr<core::Labels> acquire();
    
    /**
     * @brief Release a Labels object back to the pool
     * @param obj Unique pointer to the object to release
     */
    void release(std::unique_ptr<core::Labels> obj);
    
    /**
     * @brief Get current pool statistics
     * @return String containing pool statistics
     */
    std::string stats() const;
    
    /**
     * @brief Get the number of objects currently in the pool
     * @return Number of available objects
     */
    size_t available() const;
    
    /**
     * @brief Get the total number of objects created by the pool
     * @return Total number of objects created
     */
    size_t total_created() const;

private:
    mutable std::mutex mutex_;
    std::stack<std::unique_ptr<core::Labels>> pool_;
    std::atomic<size_t> total_created_{0};
    std::atomic<size_t> total_acquired_{0};
    std::atomic<size_t> total_released_{0};
    size_t max_size_;
    
    /**
     * @brief Create a new Labels object
     * @return Unique pointer to a new Labels object
     */
    std::unique_ptr<core::Labels> create_object();
};

/**
 * @brief Thread-safe object pool for Sample objects
 * 
 * Samples are the most frequently allocated objects in time series operations.
 * This pool provides significant performance improvements for high-frequency writes.
 */
class SamplePool {
public:
    /**
     * @brief Constructor
     * @param initial_size Initial number of objects to pre-allocate
     * @param max_size Maximum number of objects in the pool
     */
    explicit SamplePool(size_t initial_size = 1000, size_t max_size = 100000);
    
    /**
     * @brief Destructor
     */
    ~SamplePool();
    
    /**
     * @brief Acquire a Sample object from the pool
     * @return Unique pointer to a Sample object
     */
    std::unique_ptr<core::Sample> acquire();
    
    /**
     * @brief Release a Sample object back to the pool
     * @param obj Unique pointer to the object to release
     */
    void release(std::unique_ptr<core::Sample> obj);
    
    /**
     * @brief Get current pool statistics
     * @return String containing pool statistics
     */
    std::string stats() const;
    
    /**
     * @brief Get the number of objects currently in the pool
     * @return Number of available objects
     */
    size_t available() const;
    
    /**
     * @brief Get the total number of objects created by the pool
     * @return Total number of objects created
     */
    size_t total_created() const;

private:
    mutable std::mutex mutex_;
    std::stack<std::unique_ptr<core::Sample>> pool_;
    std::atomic<size_t> total_created_{0};
    std::atomic<size_t> total_acquired_{0};
    std::atomic<size_t> total_released_{0};
    size_t max_size_;
    
    /**
     * @brief Create a new Sample object
     * @return Unique pointer to a new Sample object
     */
    std::unique_ptr<core::Sample> create_object();
};

} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_OBJECT_POOL_H_ 