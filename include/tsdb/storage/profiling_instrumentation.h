#pragma once

#include <chrono>
#include <atomic>
#include <string>
#include <unordered_map>
#include <mutex>
#include <iostream>

namespace tsdb {
namespace storage {

/**
 * @brief macOS-optimized performance instrumentation for StorageImpl
 * 
 * This class provides lightweight, high-performance instrumentation
 * specifically designed for macOS profiling and analysis.
 */
class ProfilingInstrumentation {
public:
    struct TimingData {
        std::atomic<uint64_t> call_count{0};
        std::atomic<uint64_t> total_nanoseconds{0};
        std::atomic<uint64_t> min_nanoseconds{UINT64_MAX};
        std::atomic<uint64_t> max_nanoseconds{0};
    };

    struct MemoryData {
        std::atomic<uint64_t> allocation_count{0};
        std::atomic<uint64_t> total_bytes{0};
        std::atomic<uint64_t> peak_bytes{0};
    };

    // Singleton access
    static ProfilingInstrumentation& instance() {
        static ProfilingInstrumentation instance;
        return instance;
    }

    // Timing instrumentation
    class ScopedTimer {
    public:
        ScopedTimer(const std::string& operation_name) 
            : operation_name_(operation_name), start_time_(std::chrono::high_resolution_clock::now()) {}
        
        ~ScopedTimer() {
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time_).count();
            ProfilingInstrumentation::instance().record_timing(operation_name_, duration);
        }
        
    private:
        std::string operation_name_;
        std::chrono::high_resolution_clock::time_point start_time_;
    };

    // Memory instrumentation
    void record_allocation(const std::string& operation, size_t bytes) {
        if (!enabled_) return;
        
        auto& data = memory_data_[operation];
        data.allocation_count.fetch_add(1);
        data.total_bytes.fetch_add(bytes);
        
        uint64_t current_total = data.total_bytes.load();
        uint64_t current_peak = data.peak_bytes.load();
        while (current_total > current_peak && 
               !data.peak_bytes.compare_exchange_weak(current_peak, current_total)) {
            current_peak = data.peak_bytes.load();
        }
    }

    void record_deallocation(const std::string& operation, size_t bytes) {
        if (!enabled_) return;
        
        auto& data = memory_data_[operation];
        data.total_bytes.fetch_sub(bytes);
    }

    // Timing recording
    void record_timing(const std::string& operation, uint64_t nanoseconds) {
        if (!enabled_) return;
        
        auto& data = timing_data_[operation];
        data.call_count.fetch_add(1);
        data.total_nanoseconds.fetch_add(nanoseconds);
        
        // Update min
        uint64_t current_min = data.min_nanoseconds.load();
        while (nanoseconds < current_min && 
               !data.min_nanoseconds.compare_exchange_weak(current_min, nanoseconds)) {
            current_min = data.min_nanoseconds.load();
        }
        
        // Update max
        uint64_t current_max = data.max_nanoseconds.load();
        while (nanoseconds > current_max && 
               !data.max_nanoseconds.compare_exchange_weak(current_max, nanoseconds)) {
            current_max = data.max_nanoseconds.load();
        }
    }

    // Control
    void enable() { enabled_ = true; }
    void disable() { enabled_ = false; }
    bool is_enabled() const { return enabled_; }

    // Reporting
    void print_timing_report() const {
        if (!enabled_) return;
        
        std::cout << "\n🔍 STORAGEIMPL TIMING ANALYSIS\n";
        std::cout << "==============================\n";
        
        for (const auto& [operation, data] : timing_data_) {
            uint64_t calls = data.call_count.load();
            uint64_t total_ns = data.total_nanoseconds.load();
            uint64_t min_ns = data.min_nanoseconds.load();
            uint64_t max_ns = data.max_nanoseconds.load();
            
            if (calls > 0) {
                double avg_ms = (total_ns / calls) / 1e6;
                double min_ms = min_ns / 1e6;
                double max_ms = max_ns / 1e6;
                double total_ms = total_ns / 1e6;
                
                std::cout << operation << ":\n";
                std::cout << "  Calls: " << calls << "\n";
                std::cout << "  Total: " << total_ms << " ms\n";
                std::cout << "  Average: " << avg_ms << " ms\n";
                std::cout << "  Min: " << min_ms << " ms\n";
                std::cout << "  Max: " << max_ms << " ms\n";
                std::cout << "  % of total: " << (total_ms / get_total_time_ms() * 100) << "%\n\n";
            }
        }
    }

    void print_memory_report() const {
        if (!enabled_) return;
        
        std::cout << "\n🧠 STORAGEIMPL MEMORY ANALYSIS\n";
        std::cout << "==============================\n";
        
        for (const auto& [operation, data] : memory_data_) {
            uint64_t allocations = data.allocation_count.load();
            uint64_t total_bytes = data.total_bytes.load();
            uint64_t peak_bytes = data.peak_bytes.load();
            
            if (allocations > 0) {
                double total_mb = total_bytes / (1024.0 * 1024.0);
                double peak_mb = peak_bytes / (1024.0 * 1024.0);
                double avg_bytes = total_bytes / allocations;
                
                std::cout << operation << ":\n";
                std::cout << "  Allocations: " << allocations << "\n";
                std::cout << "  Total: " << total_mb << " MB\n";
                std::cout << "  Peak: " << peak_mb << " MB\n";
                std::cout << "  Average: " << avg_bytes << " bytes\n\n";
            }
        }
    }

    void reset() {
        timing_data_.clear();
        memory_data_.clear();
    }

private:
    ProfilingInstrumentation() = default;
    
    std::atomic<bool> enabled_{false};
    std::unordered_map<std::string, TimingData> timing_data_;
    std::unordered_map<std::string, MemoryData> memory_data_;
    mutable std::mutex data_mutex_;
    
    double get_total_time_ms() const {
        uint64_t total_ns = 0;
        for (const auto& [_, data] : timing_data_) {
            total_ns += data.total_nanoseconds.load();
        }
        return total_ns / 1e6;
    }
};

// Convenience macros for easy instrumentation
#define PROFILE_TIMING(operation) \
    ProfilingInstrumentation::ScopedTimer _timer(operation)

#define PROFILE_MEMORY_ALLOC(operation, bytes) \
    ProfilingInstrumentation::instance().record_allocation(operation, bytes)

#define PROFILE_MEMORY_DEALLOC(operation, bytes) \
    ProfilingInstrumentation::instance().record_deallocation(operation, bytes)

// Conditional compilation for release builds
#ifdef ENABLE_PROFILING
    #define PROFILE_ENABLED 1
#else
    #define PROFILE_ENABLED 0
#endif

#if PROFILE_ENABLED
    #define PROFILE_FUNCTION() PROFILE_TIMING(__FUNCTION__)
    #define PROFILE_SCOPE(name) PROFILE_TIMING(name)
#else
    #define PROFILE_FUNCTION() do {} while(0)
    #define PROFILE_SCOPE(name) do {} while(0)
#endif

} // namespace storage
} // namespace tsdb







