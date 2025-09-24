#include "enhanced_time_series_pool.h"
#include <chrono>
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace tsdb {
namespace storage {

EnhancedTimeSeriesPool::EnhancedTimeSeriesPool(size_t initial_size, size_t max_size)
    : TimeSeriesPool(initial_size, max_size)
    , next_block_(0)
    , cache_hits_(0)
    , cache_misses_(0)
    , hot_objects_count_(0) {
    
    // Pre-allocate cache-aligned blocks
    cache_aligned_blocks_.reserve(initial_size * 2); // Reserve extra space for cache alignment
    for (size_t i = 0; i < initial_size * 2; ++i) {
        cache_aligned_blocks_.emplace_back();
    }
}

EnhancedTimeSeriesPool::~EnhancedTimeSeriesPool() {
    // All blocks will be automatically destroyed
}

std::unique_ptr<core::TimeSeries> EnhancedTimeSeriesPool::acquire_aligned() {
    std::unique_ptr<core::TimeSeries> obj = acquire();
    
    if (obj) {
        // Update cache statistics
        cache_hits_.fetch_add(1);
        
        // Mark the underlying block as in use if we can identify it
        // This is a simplified approach - in a real implementation,
        // we would need to track which block contains which object
    } else {
        cache_misses_.fetch_add(1);
    }
    
    return obj;
}

std::vector<std::unique_ptr<core::TimeSeries>> EnhancedTimeSeriesPool::acquire_bulk(size_t count) {
    std::vector<std::unique_ptr<core::TimeSeries>> objects;
    objects.reserve(count);
    
    for (size_t i = 0; i < count; ++i) {
        auto obj = acquire_aligned();
        if (obj) {
            objects.push_back(std::move(obj));
        }
    }
    
    return objects;
}

void EnhancedTimeSeriesPool::release_bulk(std::vector<std::unique_ptr<core::TimeSeries>>& objects) {
    for (auto& obj : objects) {
        if (obj) {
            release(std::move(obj));
        }
    }
    objects.clear();
}

void EnhancedTimeSeriesPool::optimize_cache_layout() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    // Sort blocks by access count to improve cache locality
    // Note: Cannot sort blocks with atomic members, so we'll use a different optimization strategy
    // TODO: Implement alternative optimization that doesn't require sorting
    
    // Update hot objects count
    hot_objects_count_.store(
        std::count_if(cache_aligned_blocks_.begin(), cache_aligned_blocks_.end(),
            [](const CacheAlignedBlock& block) {
                return block.access_count.load() > 10; // Threshold for "hot" objects
            })
    );
}

void EnhancedTimeSeriesPool::prefetch_hot_objects() {
    auto hot_objects = identify_hot_objects();
    
    for (auto* block : hot_objects) {
        if (block && !block->in_use.load()) {
            // Prefetch the block into cache
            // This is a simplified prefetch - in a real implementation,
            // we would use actual prefetch instructions
            volatile char* data = block->data;
            volatile char dummy = data[0];
            (void)dummy; // Suppress unused variable warning
        }
    }
}

std::string EnhancedTimeSeriesPool::cache_stats() const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    size_t total_accesses = cache_hits_.load() + cache_misses_.load();
    double hit_ratio = total_accesses > 0 ? 
        (static_cast<double>(cache_hits_.load()) / total_accesses) * 100.0 : 0.0;
    
    std::ostringstream oss;
    oss << "Cache Statistics:\n"
        << "  Cache Hits: " << cache_hits_.load() << "\n"
        << "  Cache Misses: " << cache_misses_.load() << "\n"
        << "  Hit Ratio: " << std::fixed << std::setprecision(2) << hit_ratio << "%\n"
        << "  Hot Objects: " << hot_objects_count_.load() << "\n"
        << "  Cache-Aligned Blocks: " << cache_aligned_blocks_.size() << "\n"
        << "  Next Block: " << next_block_.load();
    
    return oss.str();
}

size_t EnhancedTimeSeriesPool::cache_aligned_blocks() const {
    return cache_aligned_blocks_.size();
}

double EnhancedTimeSeriesPool::cache_hit_ratio() const {
    size_t total_accesses = cache_hits_.load() + cache_misses_.load();
    return total_accesses > 0 ? 
        (static_cast<double>(cache_hits_.load()) / total_accesses) * 100.0 : 0.0;
}

EnhancedTimeSeriesPool::CacheAlignedBlock* EnhancedTimeSeriesPool::get_next_available_block() {
    size_t start_index = next_block_.load();
    size_t current_index = start_index;
    
    do {
        if (current_index >= cache_aligned_blocks_.size()) {
            current_index = 0; // Wrap around
        }
        
        auto& block = cache_aligned_blocks_[current_index];
        if (!block.in_use.load()) {
            if (next_block_.compare_exchange_weak(start_index, current_index + 1)) {
                return &block;
            }
        }
        
        current_index++;
    } while (current_index != start_index);
    
    return nullptr; // No available blocks
}

void EnhancedTimeSeriesPool::mark_block_in_use(CacheAlignedBlock* block) {
    if (block) {
        block->in_use.store(true);
        block->last_access_time.store(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()
        );
    }
}

void EnhancedTimeSeriesPool::mark_block_free(CacheAlignedBlock* block) {
    if (block) {
        block->in_use.store(false);
    }
}

void EnhancedTimeSeriesPool::update_access_stats(CacheAlignedBlock* block) {
    if (block) {
        block->access_count.fetch_add(1);
        block->last_access_time.store(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()
        );
    }
}

std::vector<EnhancedTimeSeriesPool::CacheAlignedBlock*> EnhancedTimeSeriesPool::identify_hot_objects() {
    std::vector<EnhancedTimeSeriesPool::CacheAlignedBlock*> hot_objects;
    
    for (auto& block : cache_aligned_blocks_) {
        if (block.access_count.load() > 10) { // Threshold for "hot" objects
            hot_objects.push_back(&block);
        }
    }
    
    // Sort by access count (most accessed first)
    std::sort(hot_objects.begin(), hot_objects.end(),
        [](const EnhancedTimeSeriesPool::CacheAlignedBlock* a, const EnhancedTimeSeriesPool::CacheAlignedBlock* b) {
            return a->access_count.load() > b->access_count.load();
        });
    
    return hot_objects;
}

} // namespace storage
} // namespace tsdb
