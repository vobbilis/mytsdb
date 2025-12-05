#include "tsdb/storage/atomic_metrics.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <cmath>

namespace tsdb {
namespace storage {
namespace internal {

// Global instance and mutex
std::unique_ptr<AtomicMetrics> GlobalMetrics::global_instance_;
std::mutex GlobalMetrics::instance_mutex_;

AtomicMetrics::AtomicMetrics(const AtomicMetricsConfig& config)
    : config_(config) {
    memory_order_ = config.use_relaxed_ordering ? 
                   std::memory_order_relaxed : std::memory_order_seq_cst;
}

void AtomicMetrics::recordWrite(size_t bytes_written, uint64_t duration_ns) {
    if (!config_.enable_tracking) return;
    
    write_count_.fetch_add(1, memory_order_);
    bytes_written_.fetch_add(bytes_written, memory_order_);
    
    if (config_.enable_timing && duration_ns > 0) {
        total_write_time_.fetch_add(duration_ns, memory_order_);
    }
}

void AtomicMetrics::recordRead(size_t bytes_read, uint64_t duration_ns) {
    if (!config_.enable_tracking) return;
    
    read_count_.fetch_add(1, memory_order_);
    bytes_read_.fetch_add(bytes_read, memory_order_);
    
    if (config_.enable_timing && duration_ns > 0) {
        total_read_time_.fetch_add(duration_ns, memory_order_);
    }
}

void AtomicMetrics::recordCacheHit() {
    if (!config_.enable_tracking || !config_.enable_cache_metrics) return;
    cache_hits_.fetch_add(1, memory_order_);
}

void AtomicMetrics::recordCacheMiss() {
    if (!config_.enable_tracking || !config_.enable_cache_metrics) return;
    cache_misses_.fetch_add(1, memory_order_);
}

void AtomicMetrics::recordCompression(size_t original_size, size_t compressed_size, uint64_t duration_ns) {
    if (!config_.enable_tracking || !config_.enable_compression_metrics) return;
    
    compression_count_.fetch_add(1, memory_order_);
    bytes_compressed_.fetch_add(original_size, memory_order_);
    
    if (config_.enable_timing && duration_ns > 0) {
        total_compression_time_.fetch_add(duration_ns, memory_order_);
    }
}

void AtomicMetrics::recordDecompression(size_t compressed_size, size_t decompressed_size, uint64_t duration_ns) {
    if (!config_.enable_tracking || !config_.enable_compression_metrics) return;
    
    decompression_count_.fetch_add(1, memory_order_);
    bytes_decompressed_.fetch_add(decompressed_size, memory_order_);
    
    if (config_.enable_timing && duration_ns > 0) {
        total_decompression_time_.fetch_add(duration_ns, memory_order_);
    }
}

void AtomicMetrics::recordAllocation(size_t bytes_allocated) {
    if (!config_.enable_tracking) return;
    
    allocation_count_.fetch_add(1, memory_order_);
    bytes_allocated_.fetch_add(bytes_allocated, memory_order_);
}

void AtomicMetrics::recordDeallocation(size_t bytes_deallocated) {
    if (!config_.enable_tracking) return;
    
    deallocation_count_.fetch_add(1, memory_order_);
    bytes_deallocated_.fetch_add(bytes_deallocated, memory_order_);
}

void AtomicMetrics::recordDroppedSample() {
    if (!config_.enable_tracking) return;
    dropped_samples_.fetch_add(1, memory_order_);
}

void AtomicMetrics::recordDerivedSample() {
    if (!config_.enable_tracking) return;
    derived_samples_.fetch_add(1, memory_order_);
}

void AtomicMetrics::recordRuleCheck(uint64_t duration_ns) {
    if (!config_.enable_tracking) return;
    if (config_.enable_timing && duration_ns > 0) {
        total_rule_check_time_.fetch_add(duration_ns, memory_order_);
    }
}

AtomicMetrics::MetricsSnapshot AtomicMetrics::getSnapshot() const {
    MetricsSnapshot snapshot;
    
    // Load atomic values with the configured memory ordering
    snapshot.write_count = write_count_.load(memory_order_);
    snapshot.read_count = read_count_.load(memory_order_);
    snapshot.cache_hits = cache_hits_.load(memory_order_);
    snapshot.cache_misses = cache_misses_.load(memory_order_);
    snapshot.compression_count = compression_count_.load(memory_order_);
    snapshot.decompression_count = decompression_count_.load(memory_order_);
    snapshot.allocation_count = allocation_count_.load(memory_order_);
    snapshot.deallocation_count = deallocation_count_.load(memory_order_);
    
    snapshot.bytes_written = bytes_written_.load(memory_order_);
    snapshot.bytes_read = bytes_read_.load(memory_order_);
    snapshot.bytes_compressed = bytes_compressed_.load(memory_order_);
    snapshot.bytes_decompressed = bytes_decompressed_.load(memory_order_);
    snapshot.bytes_allocated = bytes_allocated_.load(memory_order_);
    snapshot.bytes_deallocated = bytes_deallocated_.load(memory_order_);
    
    snapshot.dropped_samples = dropped_samples_.load(memory_order_);
    snapshot.derived_samples = derived_samples_.load(memory_order_);
    snapshot.total_rule_check_time = total_rule_check_time_.load(memory_order_);
    
    snapshot.total_write_time = total_write_time_.load(memory_order_);
    snapshot.total_read_time = total_read_time_.load(memory_order_);
    snapshot.total_compression_time = total_compression_time_.load(memory_order_);
    snapshot.total_decompression_time = total_decompression_time_.load(memory_order_);
    
    // Calculate derived metrics
    calculateDerivedMetrics(snapshot);
    
    return snapshot;
}

void AtomicMetrics::reset() {
    // Reset all atomic counters
    write_count_.store(0, memory_order_);
    read_count_.store(0, memory_order_);
    cache_hits_.store(0, memory_order_);
    cache_misses_.store(0, memory_order_);
    compression_count_.store(0, memory_order_);
    decompression_count_.store(0, memory_order_);
    allocation_count_.store(0, memory_order_);
    deallocation_count_.store(0, memory_order_);
    
    bytes_written_.store(0, memory_order_);
    bytes_read_.store(0, memory_order_);
    bytes_compressed_.store(0, memory_order_);
    bytes_decompressed_.store(0, memory_order_);
    bytes_allocated_.store(0, memory_order_);
    bytes_deallocated_.store(0, memory_order_);
    
    dropped_samples_.store(0, memory_order_);
    derived_samples_.store(0, memory_order_);
    total_rule_check_time_.store(0, memory_order_);
    
    total_write_time_.store(0, memory_order_);
    total_read_time_.store(0, memory_order_);
    total_compression_time_.store(0, memory_order_);
    total_decompression_time_.store(0, memory_order_);
}

std::string AtomicMetrics::getFormattedMetrics() const {
    auto snapshot = getSnapshot();
    std::ostringstream oss;
    
    oss << "=== TSDB Storage Metrics ===\n";
    oss << "Operations:\n";
    oss << "  Writes: " << snapshot.write_count << " (" << formatBytes(snapshot.bytes_written) << ")\n";
    oss << "  Reads: " << snapshot.read_count << " (" << formatBytes(snapshot.bytes_read) << ")\n";
    oss << "  Cache Hits: " << snapshot.cache_hits << "\n";
    oss << "  Cache Misses: " << snapshot.cache_misses << "\n";
    oss << "  Cache Hit Ratio: " << std::fixed << std::setprecision(2) 
        << (snapshot.cache_hit_ratio * 100.0) << "%\n";
    
    oss << "Compression:\n";
    oss << "  Compressions: " << snapshot.compression_count << "\n";
    oss << "  Decompressions: " << snapshot.decompression_count << "\n";
    oss << "  Average Compression Ratio: " << std::fixed << std::setprecision(2) 
        << snapshot.average_compression_ratio << "x\n";
    
    oss << "Memory:\n";
    oss << "  Allocations: " << snapshot.allocation_count << "\n";
    oss << "  Deallocations: " << snapshot.deallocation_count << "\n";
    oss << "  Net Memory Usage: " << formatBytes(snapshot.net_memory_usage) << "\n";
    
    oss << "Filtering & Derived:\n";
    oss << "  Dropped Samples: " << snapshot.dropped_samples << "\n";
    oss << "  Derived Samples: " << snapshot.derived_samples << "\n";
    if (snapshot.write_count > 0) {
        oss << "  Avg Rule Check Time: " << formatDuration(snapshot.total_rule_check_time / snapshot.write_count) << "\n";
    }
    
    if (config_.enable_timing) {
        oss << "Performance:\n";
        oss << "  Avg Write Latency: " << formatDuration(snapshot.average_write_latency_ns) << "\n";
        oss << "  Avg Read Latency: " << formatDuration(snapshot.average_read_latency_ns) << "\n";
        oss << "  Write Throughput: " << std::fixed << std::setprecision(2) 
            << snapshot.write_throughput_mbps << " MB/s\n";
        oss << "  Read Throughput: " << std::fixed << std::setprecision(2) 
            << snapshot.read_throughput_mbps << " MB/s\n";
    }
    
    return oss.str();
}

std::string AtomicMetrics::getJsonMetrics() const {
    auto snapshot = getSnapshot();
    std::ostringstream oss;
    
    oss << "{\n";
    oss << "  \"operations\": {\n";
    oss << "    \"writes\": " << snapshot.write_count << ",\n";
    oss << "    \"reads\": " << snapshot.read_count << ",\n";
    oss << "    \"cache_hits\": " << snapshot.cache_hits << ",\n";
    oss << "    \"cache_misses\": " << snapshot.cache_misses << ",\n";
    oss << "    \"cache_hit_ratio\": " << std::fixed << std::setprecision(4) 
        << snapshot.cache_hit_ratio << "\n";
    oss << "  },\n";
    
    oss << "  \"data_volumes\": {\n";
    oss << "    \"bytes_written\": " << snapshot.bytes_written << ",\n";
    oss << "    \"bytes_read\": " << snapshot.bytes_read << ",\n";
    oss << "    \"bytes_compressed\": " << snapshot.bytes_compressed << ",\n";
    oss << "    \"bytes_decompressed\": " << snapshot.bytes_decompressed << "\n";
    oss << "  },\n";
    
    oss << "  \"compression\": {\n";
    oss << "    \"compression_count\": " << snapshot.compression_count << ",\n";
    oss << "    \"decompression_count\": " << snapshot.decompression_count << ",\n";
    oss << "    \"average_compression_ratio\": " << std::fixed << std::setprecision(4) 
        << snapshot.average_compression_ratio << "\n";
    oss << "  },\n";
    
    oss << "  \"memory\": {\n";
    oss << "    \"allocation_count\": " << snapshot.allocation_count << ",\n";
    oss << "    \"deallocation_count\": " << snapshot.deallocation_count << ",\n";
    oss << "    \"bytes_allocated\": " << snapshot.bytes_allocated << ",\n";
    oss << "    \"bytes_deallocated\": " << snapshot.bytes_deallocated << ",\n";
    oss << "    \"net_memory_usage\": " << snapshot.net_memory_usage << "\n";
    oss << "  },\n";
    
    oss << "  \"filtering\": {\n";
    oss << "    \"dropped_samples\": " << snapshot.dropped_samples << ",\n";
    oss << "    \"derived_samples\": " << snapshot.derived_samples << ",\n";
    oss << "    \"total_rule_check_time\": " << snapshot.total_rule_check_time << "\n";
    oss << "  }";
    
    if (config_.enable_timing) {
        oss << ",\n";
        oss << "  \"performance\": {\n";
        oss << "    \"average_write_latency_ns\": " << snapshot.average_write_latency_ns << ",\n";
        oss << "    \"average_read_latency_ns\": " << snapshot.average_read_latency_ns << ",\n";
        oss << "    \"write_throughput_mbps\": " << std::fixed << std::setprecision(2) 
            << snapshot.write_throughput_mbps << ",\n";
        oss << "    \"read_throughput_mbps\": " << std::fixed << std::setprecision(2) 
            << snapshot.read_throughput_mbps << "\n";
        oss << "  }";
    }
    
    oss << "\n}";
    return oss.str();
}

uint64_t AtomicMetrics::getCurrentTimestamp() const {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
}

void AtomicMetrics::calculateDerivedMetrics(MetricsSnapshot& snapshot) const {
    // Calculate cache hit ratio
    uint64_t total_cache_ops = snapshot.cache_hits + snapshot.cache_misses;
    snapshot.cache_hit_ratio = total_cache_ops > 0 ? 
        static_cast<double>(snapshot.cache_hits) / total_cache_ops : 0.0;
    
    // Calculate average compression ratio
    snapshot.average_compression_ratio = snapshot.bytes_compressed > 0 ? 
        static_cast<double>(snapshot.bytes_compressed) / snapshot.bytes_decompressed : 0.0;
    
    // Calculate average latencies
    snapshot.average_write_latency_ns = snapshot.write_count > 0 ? 
        static_cast<double>(snapshot.total_write_time) / snapshot.write_count : 0.0;
    
    snapshot.average_read_latency_ns = snapshot.read_count > 0 ? 
        static_cast<double>(snapshot.total_read_time) / snapshot.read_count : 0.0;
    
    snapshot.average_compression_latency_ns = snapshot.compression_count > 0 ? 
        static_cast<double>(snapshot.total_compression_time) / snapshot.compression_count : 0.0;
    
    snapshot.average_decompression_latency_ns = snapshot.decompression_count > 0 ? 
        static_cast<double>(snapshot.total_decompression_time) / snapshot.decompression_count : 0.0;
    
    // Calculate throughput (MB/s)
    const double MB_PER_BYTE = 1.0 / (1024.0 * 1024.0);
    const double NS_PER_SEC = 1e9;
    
    if (snapshot.total_write_time > 0) {
        snapshot.write_throughput_mbps = (snapshot.bytes_written * MB_PER_BYTE * NS_PER_SEC) / 
                                        snapshot.total_write_time;
    }
    
    if (snapshot.total_read_time > 0) {
        snapshot.read_throughput_mbps = (snapshot.bytes_read * MB_PER_BYTE * NS_PER_SEC) / 
                                       snapshot.total_read_time;
    }
    
    if (snapshot.total_compression_time > 0) {
        snapshot.compression_throughput_mbps = (snapshot.bytes_compressed * MB_PER_BYTE * NS_PER_SEC) / 
                                              snapshot.total_compression_time;
    }
    
    if (snapshot.total_decompression_time > 0) {
        snapshot.decompression_throughput_mbps = (snapshot.bytes_decompressed * MB_PER_BYTE * NS_PER_SEC) / 
                                                snapshot.total_decompression_time;
    }
    
    // Calculate net memory usage
    snapshot.net_memory_usage = static_cast<int64_t>(snapshot.bytes_allocated) - 
                                static_cast<int64_t>(snapshot.bytes_deallocated);
}

std::string AtomicMetrics::formatDuration(uint64_t duration_ns) const {
    if (duration_ns < 1000) {
        return std::to_string(duration_ns) + " ns";
    } else if (duration_ns < 1000000) {
        return std::to_string(duration_ns / 1000) + " μs";
    } else if (duration_ns < 1000000000) {
        return std::to_string(duration_ns / 1000000) + " ms";
    } else {
        return std::to_string(duration_ns / 1000000000) + " s";
    }
}

std::string AtomicMetrics::formatBytes(uint64_t bytes) const {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_index = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unit_index < 4) {
        size /= 1024.0;
        unit_index++;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << units[unit_index];
    return oss.str();
}

// GlobalMetrics implementation
AtomicMetrics& GlobalMetrics::getInstance() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (!global_instance_) {
        global_instance_ = std::make_unique<AtomicMetrics>();
    }
    return *global_instance_;
}

void GlobalMetrics::initialize(const AtomicMetricsConfig& config) {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    global_instance_ = std::make_unique<AtomicMetrics>(config);
}

void GlobalMetrics::reset() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (global_instance_) {
        global_instance_->reset();
    }
}

AtomicMetrics::MetricsSnapshot GlobalMetrics::getSnapshot() {
    return getInstance().getSnapshot();
}

std::string GlobalMetrics::getFormattedMetrics() {
    return getInstance().getFormattedMetrics();
}

std::string GlobalMetrics::getJsonMetrics() {
    return getInstance().getJsonMetrics();
}

// ScopedTimer implementation
ScopedTimer::ScopedTimer(AtomicMetrics& metrics, const std::string& operation)
    : metrics_(metrics), operation_(operation), stopped_(false) {
    start_time_ = std::chrono::high_resolution_clock::now();
}

ScopedTimer::~ScopedTimer() {
    if (!stopped_) {
        stop();
    }
}

void ScopedTimer::stop(size_t additional_data) {
    if (stopped_) return;
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time_);
    
    // Record the timing based on operation type
    if (operation_ == "write") {
        metrics_.recordWrite(additional_data, duration.count());
    } else if (operation_ == "read") {
        metrics_.recordRead(additional_data, duration.count());
    } else if (operation_ == "compression") {
        // For compression, additional_data should be original size
        // We'll need to calculate compressed size separately
        metrics_.recordCompression(additional_data, 0, duration.count());
    } else if (operation_ == "decompression") {
        // For decompression, additional_data should be compressed size
        // We'll need to calculate decompressed size separately
        metrics_.recordDecompression(additional_data, 0, duration.count());
    }
    
    stopped_ = true;
}

} // namespace internal
} // namespace storage
} // namespace tsdb 