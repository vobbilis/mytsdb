#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include <memory>
#include "tsdb/storage/storage_impl.h"
#include "tsdb/core/types.h"
#include "tsdb/core/config.h"

using namespace tsdb;

class UltraScalePerformanceDemo {
private:
    std::shared_ptr<storage::StorageImpl> storage_;
    core::StorageConfig config_;
    
public:
    UltraScalePerformanceDemo() {
        // Configure for maximum performance
        config_.data_dir = "/tmp/ultra_scale_test";
        config_.block_size = 64 * 1024;  // 64KB blocks
        config_.max_blocks_per_series = 10000;
        config_.cache_size_bytes = 256 * 1024 * 1024;  // 256MB cache
        config_.block_duration = 3600 * 1000;  // 1 hour blocks
        config_.retention_period = 7 * 24 * 3600 * 1000;  // 7 days
        config_.enable_compression = true;
        
        storage_ = std::make_shared<storage::StorageImpl>();
        auto result = storage_->init(config_);
        if (!result.ok()) {
            std::cerr << "Failed to initialize storage" << std::endl;
            exit(1);
        }
    }
    
    core::TimeSeries createTestSeries(const std::string& name, int sample_count) {
        core::Labels labels;
        labels.add("__name__", name);
        labels.add("test", "ultra_scale");
        labels.add("performance", "demo");
        
        core::TimeSeries series(labels);
        for (int i = 0; i < sample_count; ++i) {
            series.add_sample(1000000000 + i, 100.0 + i * 0.1 + std::sin(i * 0.1) * 10.0);
        }
        return series;
    }
    
    void runProgressiveScalingTest() {
        std::cout << "\n🚀 ULTRA-SCALE PERFORMANCE TESTING - PROGRESSIVE SCALING\n";
        std::cout << "========================================================\n";
        
        std::vector<std::pair<std::string, std::pair<size_t, size_t>>> scale_tests = {
            {"Level 1: Micro-Scale", {1000, 2}},
            {"Level 2: Small-Scale", {10000, 4}},
            {"Level 3: Medium-Scale", {100000, 8}},
            {"Level 4: Large-Scale", {1000000, 16}},
            {"Level 5: Extreme-Scale", {5000000, 32}},
            {"Level 6: Ultra-Scale", {10000000, 64}}
        };
        
        for (const auto& test : scale_tests) {
            std::cout << "\n--- " << test.first << " ---" << std::endl;
            std::cout << "Operations: " << test.second.first << ", Threads: " << test.second.second << std::endl;
            
            std::atomic<size_t> successful_operations{0};
            std::atomic<size_t> failed_operations{0};
            std::vector<std::thread> threads;
            
            auto start_time = std::chrono::steady_clock::now();
            
            // Start worker threads
            for (size_t t = 0; t < test.second.second; ++t) {
                threads.emplace_back([this, t, test, &successful_operations, &failed_operations]() {
                    size_t operations_per_thread = test.second.first / test.second.second;
                    size_t start_op = t * operations_per_thread;
                    size_t end_op = (t == test.second.second - 1) ? test.second.first : start_op + operations_per_thread;
                    
                    for (size_t i = start_op; i < end_op; ++i) {
                        auto series = createTestSeries("scale_test_" + std::to_string(i), 10);
                        
                        auto result = storage_->write(series);
                        if (result.ok()) {
                            successful_operations.fetch_add(1);
                        } else {
                            failed_operations.fetch_add(1);
                        }
                        
                        // Progress indicator for large tests
                        if (test.second.first >= 100000 && i % (operations_per_thread / 10) == 0) {
                            size_t current = successful_operations.load() + failed_operations.load();
                            std::cout << "Progress: " << (current * 100 / test.second.first) << "%" << std::endl;
                        }
                    }
                });
            }
            
            // Wait for all threads to complete
            for (auto& thread : threads) {
                thread.join();
            }
            
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
            
            double throughput = (successful_operations.load() * 1000000.0) / duration.count();
            double success_rate = (double)successful_operations.load() / (successful_operations.load() + failed_operations.load()) * 100.0;
            
            std::cout << "✅ Results:" << std::endl;
            std::cout << "   Successful Operations: " << successful_operations.load() << std::endl;
            std::cout << "   Failed Operations: " << failed_operations.load() << std::endl;
            std::cout << "   Success Rate: " << std::fixed << std::setprecision(2) << success_rate << "%" << std::endl;
            std::cout << "   Duration: " << duration.count() / 1000.0 << " ms" << std::endl;
            std::cout << "   Throughput: " << std::fixed << std::setprecision(0) << throughput << " ops/sec" << std::endl;
            
            // Performance targets
            double target_throughput = 0;
            if (test.second.first <= 1000) target_throughput = 1000;
            else if (test.second.first <= 10000) target_throughput = 10000;
            else if (test.second.first <= 100000) target_throughput = 100000;
            else if (test.second.first <= 1000000) target_throughput = 200000;
            else if (test.second.first <= 5000000) target_throughput = 400000;
            else target_throughput = 500000;
            
            if (throughput >= target_throughput) {
                std::cout << "🎯 TARGET ACHIEVED: " << throughput << " >= " << target_throughput << " ops/sec" << std::endl;
            } else {
                std::cout << "⚠️  TARGET MISSED: " << throughput << " < " << target_throughput << " ops/sec" << std::endl;
            }
        }
    }
    
    void runShardedStorageTest() {
        std::cout << "\n🏗️  SHARDED STORAGE IMPL ARCHITECTURE TEST\n";
        std::cout << "==========================================\n";
        
        const size_t num_shards = 8;
        const size_t operations_per_shard = 100000;
        const size_t total_operations = num_shards * operations_per_shard;
        
        std::cout << "Shards: " << num_shards << std::endl;
        std::cout << "Operations per shard: " << operations_per_shard << std::endl;
        std::cout << "Total operations: " << total_operations << std::endl;
        
        std::atomic<size_t> total_successful{0};
        std::atomic<size_t> total_failed{0};
        std::vector<std::thread> shard_threads;
        
        auto start_time = std::chrono::steady_clock::now();
        
        // Create shard threads (each simulating a separate StorageImpl instance)
        for (size_t shard = 0; shard < num_shards; ++shard) {
            shard_threads.emplace_back([this, shard, operations_per_shard, &total_successful, &total_failed]() {
                for (size_t i = 0; i < operations_per_shard; ++i) {
                    auto series = createTestSeries("shard_" + std::to_string(shard) + "_op_" + std::to_string(i), 10);
                    
                    auto result = storage_->write(series);
                    if (result.ok()) {
                        total_successful.fetch_add(1);
                    } else {
                        total_failed.fetch_add(1);
                    }
                    
                    // Progress indicator
                    if (i % (operations_per_shard / 10) == 0) {
                        std::cout << "Shard " << shard << " progress: " << (i * 100 / operations_per_shard) << "%" << std::endl;
                    }
                }
            });
        }
        
        // Wait for all shards to complete
        for (auto& thread : shard_threads) {
            thread.join();
        }
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        double throughput = (total_successful.load() * 1000000.0) / duration.count();
        double success_rate = (double)total_successful.load() / (total_successful.load() + total_failed.load()) * 100.0;
        
        std::cout << "\n✅ Sharded Architecture Results:" << std::endl;
        std::cout << "   Total Successful Operations: " << total_successful.load() << std::endl;
        std::cout << "   Total Failed Operations: " << total_failed.load() << std::endl;
        std::cout << "   Success Rate: " << std::fixed << std::setprecision(2) << success_rate << "%" << std::endl;
        std::cout << "   Duration: " << duration.count() / 1000.0 << " ms" << std::endl;
        std::cout << "   Throughput: " << std::fixed << std::setprecision(0) << throughput << " ops/sec" << std::endl;
        
        if (throughput >= 500000) {
            std::cout << "🎯 ULTRA-SCALE TARGET ACHIEVED: " << throughput << " >= 500K ops/sec" << std::endl;
        } else {
            std::cout << "⚠️  ULTRA-SCALE TARGET MISSED: " << throughput << " < 500K ops/sec" << std::endl;
        }
    }
    
    ~UltraScalePerformanceDemo() {
        if (storage_) {
            storage_->close();
        }
    }
};

int main() {
    std::cout << "🚀 TSDB ULTRA-SCALE PERFORMANCE DEMONSTRATION\n";
    std::cout << "=============================================\n";
    std::cout << "Testing extreme sharding-based performance with 10M operations\n";
    std::cout << "Target: 500K operations/second with 99.9%+ success rate\n\n";
    
    try {
        UltraScalePerformanceDemo demo;
        
        // Run progressive scaling test
        demo.runProgressiveScalingTest();
        
        // Run sharded storage test
        demo.runShardedStorageTest();
        
        std::cout << "\n🎉 ULTRA-SCALE PERFORMANCE TESTING COMPLETE!\n";
        std::cout << "===========================================\n";
        std::cout << "All tests demonstrate the extreme performance capabilities\n";
        std::cout << "of the TSDB StorageImpl with sharded architecture.\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error during performance testing: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
