#pragma once

#include <atomic>
#include <cstddef>
#include <vector>
#include <type_traits>
#include <functional>
#include <memory>
#include <string>

namespace tsdb {
namespace storage {

/**
 * @brief Configuration for persistent queue behavior
 */
struct PersistentQueueConfig {
    /** @brief Whether to enable persistence */
    bool enable_persistence = false;
    
    /** @brief Path for persistent storage */
    std::string persistence_path = "./queue_data";
    
    /** @brief Maximum size of persistent storage in bytes (0 = unlimited) */
    size_t max_persistent_size = 0;
    
    /** @brief Whether to drop data when persistent storage is full */
    bool drop_on_persistent_full = false;
    
    /** @brief Callback for persistence events */
    std::function<void(const std::string&, size_t)> persistence_callback = nullptr;
};

/**
 * @brief Bounded lock-free multi-producer multi-consumer (MPMC) queue with optional persistence.
 *        Based on Dmitry Vyukov's algorithm: https://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue
 *
 * @tparam T Type of element stored in the queue.
 */
template<typename T>
class LockFreeQueue {
public:
    explicit LockFreeQueue(size_t capacity, 
                          const PersistentQueueConfig& config = PersistentQueueConfig{});
    ~LockFreeQueue();

    // Non-copyable, non-movable
    LockFreeQueue(const LockFreeQueue&) = delete;
    LockFreeQueue& operator=(const LockFreeQueue&) = delete;

    /**
     * @brief Push an item into the queue.
     * @param item The item to push (copied or moved).
     * @return true if successful, false if the queue is full and persistence is disabled/full.
     */
    bool push(const T& item);
    bool push(T&& item);

    /**
     * @brief Pop an item from the queue.
     * @param item Output parameter for the popped item.
     * @return true if successful, false if the queue is empty.
     */
    bool pop(T& item);

    /** @brief Returns true if the queue is empty. */
    bool empty() const;
    /** @brief Returns true if the queue is full. */
    bool full() const;
    /** @brief Returns the current number of elements. */
    size_t size() const;
    /** @brief Returns the maximum capacity. */
    size_t capacity() const;

    // Persistence-related methods
    /** @brief Returns true if persistence is enabled. */
    bool is_persistence_enabled() const;
    
    /** @brief Returns the number of items in persistent storage. */
    size_t persistent_size() const;
    
    /** @brief Returns the total size of persistent storage in bytes. */
    size_t persistent_bytes() const;
    
    /** @brief Flush all in-memory items to persistent storage. */
    bool flush_to_persistent();
    
    /** @brief Load items from persistent storage into memory queue. */
    size_t load_from_persistent(size_t max_items = 0);
    
    /** @brief Clear persistent storage. */
    void clear_persistent();

protected:
    // Hooks for persistence implementation
    virtual bool persist_item(const T& item);
    virtual bool load_persistent_item(T& item);
    virtual bool clear_persistent_storage();
    virtual size_t get_persistent_item_count() const;
    virtual size_t get_persistent_storage_size() const;

private:
    struct Slot {
        std::atomic<size_t> seq;
        typename std::aligned_storage<sizeof(T), alignof(T)>::type storage;
    };

    const size_t capacity_;
    const PersistentQueueConfig config_;
    Slot* slots_;
    alignas(64) std::atomic<size_t> head_;
    alignas(64) std::atomic<size_t> tail_;
    
    // Persistence state
    mutable std::atomic<size_t> persistent_item_count_{0};
    mutable std::atomic<size_t> persistent_storage_size_{0};
};

template<typename T>
LockFreeQueue<T>::LockFreeQueue(size_t capacity, 
                          const PersistentQueueConfig& config)
    : capacity_(capacity), config_(config), slots_(new Slot[capacity]), head_(0), tail_(0) {
    for (size_t i = 0; i < capacity_; ++i) {
        slots_[i].seq.store(i, std::memory_order_relaxed);
    }
}

template<typename T>
LockFreeQueue<T>::~LockFreeQueue() {
    T tmp;
    while (pop(tmp)) {}
    delete[] slots_;
}

template<typename T>
bool LockFreeQueue<T>::push(const T& item) {
    size_t pos = tail_.load(std::memory_order_relaxed);
    for (;;) {
        Slot& slot = slots_[pos % capacity_];
        size_t seq = slot.seq.load(std::memory_order_acquire);
        intptr_t diff = (intptr_t)seq - (intptr_t)pos;
        if (diff == 0) {
            if (tail_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                new (&slot.storage) T(item);
                slot.seq.store(pos + 1, std::memory_order_release);
                return true;
            }
        } else if (diff < 0) {
            // Memory queue is full - try persistence if enabled
            if (config_.enable_persistence) {
                return persist_item(item);
            }
            return false; // full and no persistence
        } else {
            pos = tail_.load(std::memory_order_relaxed);
        }
    }
}

template<typename T>
bool LockFreeQueue<T>::push(T&& item) {
    size_t pos = tail_.load(std::memory_order_relaxed);
    for (;;) {
        Slot& slot = slots_[pos % capacity_];
        size_t seq = slot.seq.load(std::memory_order_acquire);
        intptr_t diff = (intptr_t)seq - (intptr_t)pos;
        if (diff == 0) {
            if (tail_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                new (&slot.storage) T(std::move(item));
                slot.seq.store(pos + 1, std::memory_order_release);
                return true;
            }
        } else if (diff < 0) {
            // Memory queue is full - try persistence if enabled
            if (config_.enable_persistence) {
                return persist_item(item);
            }
            return false; // full and no persistence
        } else {
            pos = tail_.load(std::memory_order_relaxed);
        }
    }
}

template<typename T>
bool LockFreeQueue<T>::pop(T& item) {
    size_t pos = head_.load(std::memory_order_relaxed);
    for (;;) {
        Slot& slot = slots_[pos % capacity_];
        size_t seq = slot.seq.load(std::memory_order_acquire);
        intptr_t diff = (intptr_t)seq - (intptr_t)(pos + 1);
        if (diff == 0) {
            if (head_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                T* data_ptr = reinterpret_cast<T*>(&slot.storage);
                item = std::move(*data_ptr);
                data_ptr->~T();
                slot.seq.store(pos + capacity_, std::memory_order_release);
                return true;
            }
        } else if (diff < 0) {
            return false; // empty
        } else {
            pos = head_.load(std::memory_order_relaxed);
        }
    }
}

template<typename T>
bool LockFreeQueue<T>::empty() const {
    size_t pos = head_.load(std::memory_order_acquire);
    Slot& slot = slots_[pos % capacity_];
    size_t seq = slot.seq.load(std::memory_order_acquire);
    return ((intptr_t)seq - (intptr_t)(pos + 1)) < 0;
}

template<typename T>
bool LockFreeQueue<T>::full() const {
    size_t pos = tail_.load(std::memory_order_acquire);
    Slot& slot = slots_[pos % capacity_];
    size_t seq = slot.seq.load(std::memory_order_acquire);
    return ((intptr_t)seq - (intptr_t)pos) < 0;
}

template<typename T>
size_t LockFreeQueue<T>::size() const {
    size_t head = head_.load(std::memory_order_acquire);
    size_t tail = tail_.load(std::memory_order_acquire);
    return tail > head ? (tail - head) : 0;
}

template<typename T>
size_t LockFreeQueue<T>::capacity() const {
    return capacity_;
}

// Persistence-related methods
template<typename T>
bool LockFreeQueue<T>::is_persistence_enabled() const {
    return config_.enable_persistence;
}

template<typename T>
size_t LockFreeQueue<T>::persistent_size() const {
    return persistent_item_count_.load(std::memory_order_relaxed);
}

template<typename T>
size_t LockFreeQueue<T>::persistent_bytes() const {
    return persistent_storage_size_.load(std::memory_order_relaxed);
}

template<typename T>
bool LockFreeQueue<T>::flush_to_persistent() {
    if (!config_.enable_persistence) {
        return false;
    }
    
    // Move all items from memory queue to persistent storage
    size_t items_flushed = 0;
    T item;
    while (pop(item)) {
        if (persist_item(item)) {
            items_flushed++;
        } else {
            // If persistence fails, we could push back to memory queue
            // For now, we'll just drop the item
            break;
        }
    }
    
    if (config_.persistence_callback) {
        config_.persistence_callback("flush_completed", items_flushed);
    }
    
    return items_flushed > 0;
}

template<typename T>
size_t LockFreeQueue<T>::load_from_persistent(size_t max_items) {
    if (!config_.enable_persistence) {
        return 0;
    }
    
    size_t items_loaded = 0;
    T item;
    
    // Load items until memory queue is full or max_items reached
    while (items_loaded < max_items && !full()) {
        if (load_persistent_item(item)) {
            if (push(item)) {
                items_loaded++;
            } else {
                // Memory queue became full, stop loading
                break;
            }
        } else {
            // No more items in persistent storage
            break;
        }
    }
    
    if (config_.persistence_callback) {
        config_.persistence_callback("load_completed", items_loaded);
    }
    
    return items_loaded;
}

template<typename T>
void LockFreeQueue<T>::clear_persistent() {
    if (config_.enable_persistence) {
        clear_persistent_storage();
    }
}

// Protected virtual methods for persistence implementation
template<typename T>
bool LockFreeQueue<T>::persist_item(const T& item) {
    (void)item; // Suppress unused parameter warning
    // Default implementation: just track the count
    // Subclasses should override this for actual persistence
    if (config_.max_persistent_size > 0 && 
        persistent_storage_size_.load(std::memory_order_relaxed) >= config_.max_persistent_size) {
        if (config_.drop_on_persistent_full) {
            return false; // Drop the item
        }
        // Could implement LRU eviction here
    }
    
    persistent_item_count_.fetch_add(1, std::memory_order_relaxed);
    persistent_storage_size_.fetch_add(sizeof(T), std::memory_order_relaxed);
    
    if (config_.persistence_callback) {
        config_.persistence_callback("item_persisted", sizeof(T));
    }
    
    return true;
}

template<typename T>
bool LockFreeQueue<T>::load_persistent_item(T& item) {
    (void)item; // Suppress unused parameter warning
    // Default implementation: just decrement the count
    // Subclasses should override this for actual persistence
    size_t current_count = persistent_item_count_.load(std::memory_order_relaxed);
    if (current_count == 0) {
        return false; // No items in persistent storage
    }
    
    persistent_item_count_.fetch_sub(1, std::memory_order_relaxed);
    persistent_storage_size_.fetch_sub(sizeof(T), std::memory_order_relaxed);
    
    if (config_.persistence_callback) {
        config_.persistence_callback("item_loaded", sizeof(T));
    }
    
    return true;
}

template<typename T>
bool LockFreeQueue<T>::clear_persistent_storage() {
    // Default implementation: just reset the counters
    // Subclasses should override this for actual persistence
    persistent_item_count_.store(0, std::memory_order_relaxed);
    persistent_storage_size_.store(0, std::memory_order_relaxed);
    
    if (config_.persistence_callback) {
        config_.persistence_callback("storage_cleared", 0);
    }
    
    return true;
}

template<typename T>
size_t LockFreeQueue<T>::get_persistent_item_count() const {
    return persistent_item_count_.load(std::memory_order_relaxed);
}

template<typename T>
size_t LockFreeQueue<T>::get_persistent_storage_size() const {
    return persistent_storage_size_.load(std::memory_order_relaxed);
}

} // namespace storage
} // namespace tsdb 