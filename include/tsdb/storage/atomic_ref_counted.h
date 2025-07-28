#pragma once

#include <atomic>
#include <memory>
#include <cstdint>
#include <string>
#include <sstream>
#include <iomanip>

namespace tsdb {
namespace storage {

/**
 * @brief Configuration for atomic reference counting behavior
 */
struct AtomicRefCountedConfig {
    bool enable_performance_tracking = true;
    bool use_relaxed_ordering = false;  // Use relaxed ordering for better performance
    bool enable_debug_logging = false;
    size_t max_ref_count = 1000000;     // Safety limit for reference count
};

/**
 * @brief Performance statistics for atomic reference counting
 */
struct AtomicRefCountedStats {
    std::atomic<uint64_t> total_add_refs{0};
    std::atomic<uint64_t> total_releases{0};
    std::atomic<uint64_t> total_destructions{0};
    std::atomic<uint64_t> peak_ref_count{0};
    std::atomic<uint64_t> contention_events{0};
    
    void reset() {
        total_add_refs.store(0, std::memory_order_relaxed);
        total_releases.store(0, std::memory_order_relaxed);
        total_destructions.store(0, std::memory_order_relaxed);
        peak_ref_count.store(0, std::memory_order_relaxed);
        contention_events.store(0, std::memory_order_relaxed);
    }
    
    std::string to_string() const {
        std::ostringstream oss;
        oss << "AtomicRefCounted Statistics:\n";
        oss << "  Total addRef operations: " << total_add_refs.load(std::memory_order_relaxed) << "\n";
        oss << "  Total release operations: " << total_releases.load(std::memory_order_relaxed) << "\n";
        oss << "  Total destructions: " << total_destructions.load(std::memory_order_relaxed) << "\n";
        oss << "  Peak reference count: " << peak_ref_count.load(std::memory_order_relaxed) << "\n";
        oss << "  Contention events: " << contention_events.load(std::memory_order_relaxed) << "\n";
        
        uint64_t total_ops = total_add_refs.load(std::memory_order_relaxed) + 
                           total_releases.load(std::memory_order_relaxed);
        if (total_ops > 0) {
            double contention_rate = static_cast<double>(contention_events.load(std::memory_order_relaxed)) / total_ops * 100.0;
            oss << "  Contention rate: " << std::fixed << std::setprecision(2) << contention_rate << "%\n";
        }
        
        return oss.str();
    }
};

/**
 * @brief Template class for atomic reference counting
 * 
 * This class provides atomic reference counting with configurable memory ordering
 * and performance tracking. It can be used as an alternative to std::shared_ptr
 * for performance-critical scenarios where fine-grained control is needed.
 * 
 * @tparam T The type to be reference counted
 */
template<typename T>
class AtomicRefCounted {
public:
    /**
     * @brief Constructor
     * @param data The data to be reference counted
     * @param config Configuration for reference counting behavior
     */
    explicit AtomicRefCounted(T* data, const AtomicRefCountedConfig& config = AtomicRefCountedConfig{})
        : data_(data)
        , ref_count_(1)
        , config_(config) {
        if (!data_) {
            throw std::invalid_argument("Data pointer cannot be null");
        }
        if (config_.enable_performance_tracking) {
            update_peak_ref_count(1);
        }
    }
    
    /**
     * @brief Destructor
     */
    ~AtomicRefCounted() {
        fprintf(stderr, "[AtomicRefCounted] Destructor called for this=%p\n", static_cast<void*>(this));
        if (config_.enable_performance_tracking) {
            stats_.total_destructions.fetch_add(1, std::memory_order_relaxed);
        }
        if (data_) {
            fprintf(stderr, "[AtomicRefCounted] Deleting data_ at %p\n", static_cast<void*>(data_));
            delete data_;
            fprintf(stderr, "[AtomicRefCounted] Deleted data_ at %p\n", static_cast<void*>(data_));
            data_ = nullptr;
        } else {
            // Debug: destructor called with data_ already nullptr
            fprintf(stderr, "[AtomicRefCounted] Destructor called with data_ == nullptr (double destruction?)\n");
        }
    }
    
    // Disable copy constructor and assignment
    AtomicRefCounted(const AtomicRefCounted&) = delete;
    AtomicRefCounted& operator=(const AtomicRefCounted&) = delete;
    
    /**
     * @brief Add a reference to the object
     * @return The new reference count
     */
    uint32_t addRef() {
        auto memory_order = config_.use_relaxed_ordering ? 
                           std::memory_order_relaxed : std::memory_order_acq_rel;
        
        uint32_t old_count = ref_count_.fetch_add(1, memory_order);
        uint32_t new_count = old_count + 1;
        
        if (config_.enable_performance_tracking) {
            stats_.total_add_refs.fetch_add(1, std::memory_order_relaxed);
            update_peak_ref_count(new_count);
            
            if (new_count > config_.max_ref_count) {
                stats_.contention_events.fetch_add(1, std::memory_order_relaxed);
                if (config_.enable_debug_logging) {
                    // In a real implementation, this would use a proper logging system
                    // For now, we'll just track the event
                }
            }
        }
        
        return new_count;
    }
    
    /**
     * @brief Release a reference to the object
     * @return true if this was the last reference (object should be destroyed)
     */
    bool release() {
        auto memory_order = config_.use_relaxed_ordering ? 
                           std::memory_order_acq_rel : std::memory_order_acq_rel;
        
        uint32_t old_count = ref_count_.fetch_sub(1, memory_order);
        uint32_t new_count = old_count - 1;
        
        if (config_.enable_performance_tracking) {
            stats_.total_releases.fetch_add(1, std::memory_order_relaxed);
        }
        
        if (new_count == 0) {
            // This was the last reference - delete this
            fprintf(stderr, "[AtomicRefCounted] delete this called for this=%p\n", static_cast<void*>(this));
            delete this;
            return true;
        }
        
        return false;
    }
    
    /**
     * @brief Get the current reference count
     * @return Current reference count
     */
    uint32_t refCount() const {
        return ref_count_.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Get access to the underlying data
     * @return Pointer to the data
     */
    T* get() const {
        return data_;
    }
    
    /**
     * @brief Get access to the underlying data with operator syntax
     * @return Pointer to the data
     */
    T* operator->() const {
        return data_;
    }
    
    /**
     * @brief Dereference operator
     * @return Reference to the data
     */
    T& operator*() const {
        return *data_;
    }
    
    /**
     * @brief Get performance statistics
     * @return Reference to the statistics object
     */
    const AtomicRefCountedStats& getStats() const {
        return stats_;
    }
    
    /**
     * @brief Get performance statistics as string
     * @return String representation of statistics
     */
    std::string getStatsString() const {
        return stats_.to_string();
    }
    
    /**
     * @brief Reset performance statistics
     */
    void resetStats() {
        stats_.reset();
    }
    
    /**
     * @brief Update configuration
     * @param new_config New configuration
     */
    void updateConfig(const AtomicRefCountedConfig& new_config) {
        config_ = new_config;
    }
    
    /**
     * @brief Get current configuration
     * @return Current configuration
     */
    const AtomicRefCountedConfig& getConfig() const {
        return config_;
    }
    
    /**
     * @brief Check if the object is unique (ref count == 1)
     * @return true if this is the only reference
     */
    bool unique() const {
        return ref_count_.load(std::memory_order_relaxed) == 1;
    }
    
    /**
     * @brief Check if the object is shared (ref count > 1)
     * @return true if there are multiple references
     */
    bool shared() const {
        return ref_count_.load(std::memory_order_relaxed) > 1;
    }

private:
    T* data_;
    mutable std::atomic<uint32_t> ref_count_;
    AtomicRefCountedConfig config_;
    mutable AtomicRefCountedStats stats_;
    
        void update_peak_ref_count(uint32_t current_count) {
        uint64_t current_peak = stats_.peak_ref_count.load(std::memory_order_relaxed);
        while (current_count > current_peak) {
            if (stats_.peak_ref_count.compare_exchange_weak(current_peak, static_cast<uint64_t>(current_count), 
                                                           std::memory_order_relaxed)) {
                break;
            }
        }
    }
};

/**
 * @brief Helper function to create an AtomicRefCounted object
 * @tparam T The type to be reference counted
 * @tparam Args Constructor argument types for T
 * @param config Configuration for reference counting
 * @param args Constructor arguments for T
 * @return Unique pointer to the AtomicRefCounted object
 */
template<typename T, typename... Args>
std::unique_ptr<AtomicRefCounted<T>> makeAtomicRefCounted(
    const AtomicRefCountedConfig& config = AtomicRefCountedConfig{},
    Args&&... args) {
    return std::unique_ptr<AtomicRefCounted<T>>(
        new AtomicRefCounted<T>(new T(std::forward<Args>(args)...), config));
}

/**
 * @brief Helper function to create an AtomicRefCounted object from existing data
 * @tparam T The type to be reference counted
 * @param data Pointer to existing data (will be owned by the AtomicRefCounted object)
 * @param config Configuration for reference counting
 * @return Unique pointer to the AtomicRefCounted object
 */
template<typename T>
std::unique_ptr<AtomicRefCounted<T>> makeAtomicRefCounted(
    T* data,
    const AtomicRefCountedConfig& config = AtomicRefCountedConfig{}) {
    return std::unique_ptr<AtomicRefCounted<T>>(
        new AtomicRefCounted<T>(data, config));
}

} // namespace storage
} // namespace tsdb 