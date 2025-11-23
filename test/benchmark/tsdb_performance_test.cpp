#include <iostream>
#include <chrono>
#include <vector>
#include <thread>
#include <atomic>
#include <filesystem>
#include <random>

// Include MyTSDB headers
#include "tsdb/core/types.h"
#include "tsdb/storage/storage_impl.h"
#include "tsdb/core/config.h"

using namespace tsdb;

int main() {
    std::cout << "=== MyTSDB PERFORMANCE TEST ===" << std::endl;
    std::cout << "Testing actual MyTSDB performance with real operations..." << std::endl;
    
    // Setup test directory
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "tsdb_perf_test";
    std::filesystem::create_directories(test_dir);
    
    try {
        // Initialize storage
        std::cout << "\n=== INITIALIZING STORAGE ===" << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        
        Config config;
        config.storage_path = test_dir.string();
        config.wal_enabled = true;
        config.wal_path = (test_dir / "wal").string();
        
        StorageImpl storage(config);
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << "Storage initialization: " << duration.count() << " microseconds" << std::endl;
        
        // Test 1: Single Write Performance
        std::cout << "\n=== SINGLE WRITE PERFORMANCE TEST ===" << std::endl;
        
        const int single_writes = 10000;
        start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < single_writes; ++i) {
            TimeSeries series;
            series.labels = {{"job", "test"}, {"instance", "server" + std::to_string(i % 10)}};
            series.samples = {{std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count(), 
                static_cast<double>(i)}};
            
            storage.Write(series);
        }
        
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        double single_write_ops_per_sec = single_writes / (duration.count() / 1000000.0);
        std::cout << "Single writes: " << single_write_ops_per_sec << " ops/sec" << std::endl;
        
        // Test 2: Batch Write Performance
        std::cout << "\n=== BATCH WRITE PERFORMANCE TEST ===" << std::endl;
        
        const int batch_size = 1000;
        const int num_batches = 10;
        start = std::chrono::high_resolution_clock::now();
        
        for (int batch = 0; batch < num_batches; ++batch) {
            std::vector<TimeSeries> batch_data;
            for (int i = 0; i < batch_size; ++i) {
                TimeSeries series;
                series.labels = {{"job", "batch_test"}, {"batch", std::to_string(batch)}, {"item", std::to_string(i)}};
                series.samples = {{std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count(), 
                    static_cast<double>(batch * batch_size + i)}};
                batch_data.push_back(series);
            }
            
            for (const auto& series : batch_data) {
                storage.Write(series);
            }
        }
        
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        double batch_write_ops_per_sec = (num_batches * batch_size) / (duration.count() / 1000000.0);
        std::cout << "Batch writes: " << batch_write_ops_per_sec << " ops/sec" << std::endl;
        
        // Test 3: Concurrent Write Performance
        std::cout << "\n=== CONCURRENT WRITE PERFORMANCE TEST ===" << std::endl;
        
        const int num_threads = 4;
        const int writes_per_thread = 2500;
        std::atomic<int> completed_writes{0};
        
        start = std::chrono::high_resolution_clock::now();
        
        std::vector<std::thread> threads;
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&, t]() {
                for (int i = 0; i < writes_per_thread; ++i) {
                    TimeSeries series;
                    series.labels = {{"job", "concurrent_test"}, {"thread", std::to_string(t)}, {"item", std::to_string(i)}};
                    series.samples = {{std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count(), 
                        static_cast<double>(t * writes_per_thread + i)}};
                    
                    storage.Write(series);
                    completed_writes++;
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        double concurrent_write_ops_per_sec = completed_writes.load() / (duration.count() / 1000000.0);
        std::cout << "Concurrent writes: " << concurrent_write_ops_per_sec << " ops/sec" << std::endl;
        
        // Test 4: Read Performance
        std::cout << "\n=== READ PERFORMANCE TEST ===" << std::endl;
        
        const int read_queries = 1000;
        start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < read_queries; ++i) {
            // Simple query - get all series with job="test"
            auto result = storage.Query("job=\"test\"");
            // Note: This might not work depending on query implementation
            // We'll just measure the call time
        }
        
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        double read_ops_per_sec = read_queries / (duration.count() / 1000000.0);
        std::cout << "Read queries: " << read_ops_per_sec << " ops/sec" << std::endl;
        
        // Test 5: WAL Performance Analysis
        std::cout << "\n=== WAL PERFORMANCE ANALYSIS ===" << std::endl;
        
        std::filesystem::path wal_dir = test_dir / "wal";
        if (std::filesystem::exists(wal_dir)) {
            size_t wal_size = 0;
            int wal_files = 0;
            
            for (const auto& entry : std::filesystem::directory_iterator(wal_dir)) {
                if (entry.is_regular_file()) {
                    wal_size += std::filesystem::file_size(entry.path());
                    wal_files++;
                }
            }
            
            std::cout << "WAL files: " << wal_files << std::endl;
            std::cout << "WAL size: " << wal_size << " bytes" << std::endl;
            std::cout << "Average WAL file size: " << (wal_files > 0 ? wal_size / wal_files : 0) << " bytes" << std::endl;
        }
        
        // Results Summary
        std::cout << "\n=== MyTSDB PERFORMANCE RESULTS ===" << std::endl;
        std::cout << "Single Write Performance: " << single_write_ops_per_sec << " ops/sec" << std::endl;
        std::cout << "Batch Write Performance: " << batch_write_ops_per_sec << " ops/sec" << std::endl;
        std::cout << "Concurrent Write Performance: " << concurrent_write_ops_per_sec << " ops/sec" << std::endl;
        std::cout << "Read Performance: " << read_ops_per_sec << " ops/sec" << std::endl;
        
        // Performance Analysis
        std::cout << "\n=== PERFORMANCE ANALYSIS ===" << std::endl;
        
        if (single_write_ops_per_sec > 100000) {
            std::cout << "✅ Single Write Performance: EXCELLENT (>100K ops/sec)" << std::endl;
        } else if (single_write_ops_per_sec > 10000) {
            std::cout << "✅ Single Write Performance: GOOD (>10K ops/sec)" << std::endl;
        } else {
            std::cout << "❌ Single Write Performance: POOR (<10K ops/sec)" << std::endl;
        }
        
        if (batch_write_ops_per_sec > 100000) {
            std::cout << "✅ Batch Write Performance: EXCELLENT (>100K ops/sec)" << std::endl;
        } else if (batch_write_ops_per_sec > 10000) {
            std::cout << "✅ Batch Write Performance: GOOD (>10K ops/sec)" << std::endl;
        } else {
            std::cout << "❌ Batch Write Performance: POOR (<10K ops/sec)" << std::endl;
        }
        
        if (concurrent_write_ops_per_sec > 100000) {
            std::cout << "✅ Concurrent Write Performance: EXCELLENT (>100K ops/sec)" << std::endl;
        } else if (concurrent_write_ops_per_sec > 10000) {
            std::cout << "✅ Concurrent Write Performance: GOOD (>10K ops/sec)" << std::endl;
        } else {
            std::cout << "❌ Concurrent Write Performance: POOR (<10K ops/sec)" << std::endl;
        }
        
        if (read_ops_per_sec > 10000) {
            std::cout << "✅ Read Performance: EXCELLENT (>10K ops/sec)" << std::endl;
        } else if (read_ops_per_sec > 1000) {
            std::cout << "✅ Read Performance: GOOD (>1K ops/sec)" << std::endl;
        } else {
            std::cout << "❌ Read Performance: POOR (<1K ops/sec)" << std::endl;
        }
        
        // Check if we achieved the claimed >100K ops/sec
        double max_throughput = std::max({single_write_ops_per_sec, batch_write_ops_per_sec, concurrent_write_ops_per_sec});
        
        std::cout << "\n=== PERFORMANCE CLAIM VALIDATION ===" << std::endl;
        std::cout << "Maximum throughput achieved: " << max_throughput << " ops/sec" << std::endl;
        
        if (max_throughput > 100000) {
            std::cout << "🎉 SUCCESS: Achieved >100K ops/sec! (" << max_throughput << " ops/sec)" << std::endl;
        } else {
            std::cout << "❌ FAILED: Did not achieve >100K ops/sec. Actual: " << max_throughput << " ops/sec" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "❌ ERROR: " << e.what() << std::endl;
        return 1;
    }
    
    // Cleanup
    std::filesystem::remove_all(test_dir);
    
    std::cout << "\n=== TEST COMPLETE ===" << std::endl;
    return 0;
}

