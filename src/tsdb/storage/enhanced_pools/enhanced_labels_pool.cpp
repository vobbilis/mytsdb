#include "enhanced_labels_pool.h"
#include <chrono>
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace tsdb {
namespace storage {

EnhancedLabelsPool::EnhancedLabelsPool(size_t initial_size, size_t max_size)
    : LabelsPool(initial_size, max_size)
    , next_block_(0)
    , cache_hits_(0)
    , cache_misses_(0)
    , hot_objects_count_(0) {
    
    // Pre-allocate cache-aligned blocks
    cache_aligned_blocks_.reserve(initial_size * 2);
    for (size_t i = 0; i < initial_size * 2; ++i) {
        cache_aligned_blocks_.emplace_back();
    }
}

EnhancedLabelsPool::~EnhancedLabelsPool() {
    // All blocks will be automatically destroyed
}

std::unique_ptr<core::Labels> EnhancedLabelsPool::acquire_aligned() {
    std::unique_ptr<core::Labels> obj = acquire();
    
    if (obj) {
        cache_hits_.fetch_add(1);
    } else {
        cache_misses_.fetch_add(1);
    }
    
    return obj;
}

std::vector<std::unique_ptr<core::Labels>> EnhancedLabelsPool::acquire_bulk(size_t count) {
    std::vector<std::unique_ptr<core::Labels>> objects;
    objects.reserve(count);
    
    for (size_t i = 0; i < count; ++i) {
        auto obj = acquire_aligned();
        if (obj) {
            objects.push_back(std::move(obj));
        }
    }
    
    return objects;
}

void EnhancedLabelsPool::release_bulk(std::vector<std::unique_ptr<core::Labels>>& objects) {
    for (auto& obj : objects) {
        if (obj) {
            release(std::move(obj));
        }
    }
    objects.clear();
}

void EnhancedLabelsPool::optimize_cache_layout() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    // Note: Cannot sort blocks with atomic members, so we'll use a different optimization strategy
    // TODO: Implement alternative optimization that doesn't require sorting
    
    hot_objects_count_.store(
        std::count_if(cache_aligned_blocks_.begin(), cache_aligned_blocks_.end(),
            [](const CacheAlignedBlock& block) {
                return block.access_count.load() > 10;
            })
    );
}

void EnhancedLabelsPool::prefetch_hot_objects() {
    auto hot_objects = identify_hot_objects();
    
    for (auto* block : hot_objects) {
        if (block && !block->in_use.load()) {
            volatile char* data = block->data;
            volatile char dummy = data[0];
            (void)dummy;
        }
    }
}

std::string EnhancedLabelsPool::cache_stats() const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    size_t total_accesses = cache_hits_.load() + cache_misses_.load();
    double hit_ratio = total_accesses > 0 ? 
        (static_cast<double>(cache_hits_.load()) / total_accesses) * 100.0 : 0.0;
    
    std::ostringstream oss;
    oss << "Labels Pool Cache Statistics:\n"
        << "  Cache Hits: " << cache_hits_.load() << "\n"
        << "  Cache Misses: " << cache_misses_.load() << "\n"
        << "  Hit Ratio: " << std::fixed << std::setprecision(2) << hit_ratio << "%\n"
        << "  Hot Objects: " << hot_objects_count_.load() << "\n"
        << "  Cache-Aligned Blocks: " << cache_aligned_blocks_.size();
    
    return oss.str();
}

size_t EnhancedLabelsPool::cache_aligned_blocks() const {
    return cache_aligned_blocks_.size();
}

double EnhancedLabelsPool::cache_hit_ratio() const {
    size_t total_accesses = cache_hits_.load() + cache_misses_.load();
    return total_accesses > 0 ? 
        (static_cast<double>(cache_hits_.load()) / total_accesses) * 100.0 : 0.0;
}

EnhancedLabelsPool::CacheAlignedBlock* EnhancedLabelsPool::get_next_available_block() {
    size_t start_index = next_block_.load();
    size_t current_index = start_index;
    
    do {
        if (current_index >= cache_aligned_blocks_.size()) {
            current_index = 0;
        }
        
        auto& block = cache_aligned_blocks_[current_index];
        if (!block.in_use.load()) {
            if (next_block_.compare_exchange_weak(start_index, current_index + 1)) {
                return &block;
            }
        }
        
        current_index++;
    } while (current_index != start_index);
    
    return nullptr;
}

void EnhancedLabelsPool::mark_block_in_use(CacheAlignedBlock* block) {
    if (block) {
        block->in_use.store(true);
        block->last_access_time.store(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()
        );
    }
}

void EnhancedLabelsPool::mark_block_free(CacheAlignedBlock* block) {
    if (block) {
        block->in_use.store(false);
    }
}

void EnhancedLabelsPool::update_access_stats(CacheAlignedBlock* block) {
    if (block) {
        block->access_count.fetch_add(1);
        block->last_access_time.store(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()
        );
    }
}

std::vector<EnhancedLabelsPool::CacheAlignedBlock*> EnhancedLabelsPool::identify_hot_objects() {
    std::vector<EnhancedLabelsPool::CacheAlignedBlock*> hot_objects;
    
    for (auto& block : cache_aligned_blocks_) {
        if (block.access_count.load() > 10) {
            hot_objects.push_back(&block);
        }
    }
    
    std::sort(hot_objects.begin(), hot_objects.end(),
        [](const EnhancedLabelsPool::CacheAlignedBlock* a, const EnhancedLabelsPool::CacheAlignedBlock* b) {
            return a->access_count.load() > b->access_count.load();
        });
    
    return hot_objects;
}

} // namespace storage
} // namespace tsdb
