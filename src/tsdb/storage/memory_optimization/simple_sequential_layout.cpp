#include "simple_sequential_layout.h"
#include "simple_cache_alignment.h"
#include <algorithm>

namespace tsdb {
namespace storage {
namespace memory_optimization {

void SimpleSequentialLayout::optimize_time_series_layout(core::TimeSeries& series) {
    apply_basic_optimizations(series);
    optimize_sample_ordering(series);
}

void SimpleSequentialLayout::optimize_memory_layout(std::vector<core::TimeSeries>& series_vec) {
    // Sort series by access frequency (simplified - just sort by size)
    std::sort(series_vec.begin(), series_vec.end(),
        [](const core::TimeSeries& a, const core::TimeSeries& b) {
            return a.samples().size() > b.samples().size();
        });
    
    // Optimize each series
    for (auto& series : series_vec) {
        optimize_time_series_layout(series);
    }
}

void SimpleSequentialLayout::reserve_capacity(core::TimeSeries& series, size_t capacity) {
    // Reserve capacity for samples to avoid reallocations
    // This is a simplified implementation - in reality we'd need access to internal structures
    // For now, we'll just ensure the series is properly aligned
    SimpleCacheAlignment::align_to_cache_line(&series);
}

void SimpleSequentialLayout::shrink_to_fit(core::TimeSeries& series) {
    // Shrink the series to fit its current size
    // This is a simplified implementation - in reality we'd need access to internal structures
    // For now, we'll just ensure the series is properly aligned
    SimpleCacheAlignment::align_to_cache_line(&series);
}

void SimpleSequentialLayout::prefetch_time_series_data(core::TimeSeries& series) {
    // Prefetch the series data into cache
    SimpleCacheAlignment::prefetch_data(&series, sizeof(core::TimeSeries));
}

void SimpleSequentialLayout::apply_basic_optimizations(core::TimeSeries& series) {
    // Apply basic memory layout optimizations
    // This is a simplified implementation - in reality we'd need access to internal structures
    // For now, we'll just ensure the series is properly aligned
    SimpleCacheAlignment::align_to_cache_line(&series);
}

void SimpleSequentialLayout::optimize_sample_ordering(core::TimeSeries& series) {
    // Optimize sample ordering for better cache performance
    // This is a simplified implementation - in reality we'd need access to internal structures
    // For now, we'll just ensure the series is properly aligned
    SimpleCacheAlignment::align_to_cache_line(&series);
}

} // namespace memory_optimization
} // namespace storage
} // namespace tsdb
