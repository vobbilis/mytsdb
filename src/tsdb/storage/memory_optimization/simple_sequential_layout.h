#ifndef TSDB_STORAGE_MEMORY_OPTIMIZATION_SIMPLE_SEQUENTIAL_LAYOUT_H
#define TSDB_STORAGE_MEMORY_OPTIMIZATION_SIMPLE_SEQUENTIAL_LAYOUT_H

#include "tsdb/core/types.h"
#include <vector>

namespace tsdb {
namespace storage {
namespace memory_optimization {

/**
 * @brief Simple sequential layout optimizer for memory optimization
 * 
 * This class provides basic sequential layout optimization without complex dependencies.
 * It focuses on optimizing memory layout for better cache performance.
 */
class SimpleSequentialLayout {
public:
    /**
     * @brief Optimize the layout of a TimeSeries for better cache performance
     * @param series The TimeSeries to optimize
     */
    static void optimize_time_series_layout(core::TimeSeries& series);
    
    /**
     * @brief Optimize the layout of a vector of TimeSeries
     * @param series_vec Vector of TimeSeries to optimize
     */
    static void optimize_memory_layout(std::vector<core::TimeSeries>& series_vec);
    
    /**
     * @brief Reserve capacity for a TimeSeries to avoid reallocations
     * @param series The TimeSeries to reserve capacity for
     * @param capacity The capacity to reserve
     */
    static void reserve_capacity(core::TimeSeries& series, size_t capacity);
    
    /**
     * @brief Shrink a TimeSeries to fit its current size
     * @param series The TimeSeries to shrink
     */
    static void shrink_to_fit(core::TimeSeries& series);
    
    /**
     * @brief Prefetch data for a TimeSeries
     * @param series The TimeSeries to prefetch data for
     */
    static void prefetch_time_series_data(core::TimeSeries& series);

private:
    /**
     * @brief Apply basic memory layout optimizations
     * @param series The TimeSeries to optimize
     */
    static void apply_basic_optimizations(core::TimeSeries& series);
    
    /**
     * @brief Optimize sample ordering for better cache performance
     * @param series The TimeSeries to optimize
     */
    static void optimize_sample_ordering(core::TimeSeries& series);
};

} // namespace memory_optimization
} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_MEMORY_OPTIMIZATION_SIMPLE_SEQUENTIAL_LAYOUT_H
