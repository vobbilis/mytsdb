#include "simple_cache_alignment.h"
#include <cstring>

namespace tsdb {
namespace storage {
namespace memory_optimization {

void* SimpleCacheAlignment::align_to_cache_line(void* ptr) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t aligned_addr = (addr + CACHE_LINE_SIZE - 1) & ~(CACHE_LINE_SIZE - 1);
    return reinterpret_cast<void*>(aligned_addr);
}

bool SimpleCacheAlignment::is_cache_aligned(void* ptr) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    return (addr & (CACHE_LINE_SIZE - 1)) == 0;
}

void SimpleCacheAlignment::prefetch_data(void* ptr, size_t size) {
    // Simple prefetch implementation
    // In a real implementation, this would use platform-specific prefetch instructions
    volatile char* data = static_cast<volatile char*>(ptr);
    for (size_t i = 0; i < size; i += CACHE_LINE_SIZE) {
        // Touch the data to bring it into cache
        volatile char dummy = data[i];
        (void)dummy; // Suppress unused variable warning
    }
}

size_t SimpleCacheAlignment::get_cache_line_size() {
    return CACHE_LINE_SIZE;
}

size_t SimpleCacheAlignment::align_size_to_cache_line(size_t size) {
    return (size + CACHE_LINE_SIZE - 1) & ~(CACHE_LINE_SIZE - 1);
}

} // namespace memory_optimization
} // namespace storage
} // namespace tsdb
