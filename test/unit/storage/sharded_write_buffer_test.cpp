#include <gtest/gtest.h>
#include "tsdb/storage/sharded_write_buffer.h"
#include "tsdb/storage/storage_impl.h"
#include "tsdb/core/result.h"
#include "tsdb/core/types.h"
#include <thread>
#include <vector>
#include <chrono>
#include <random>
#include <future>
#include <atomic>

using namespace tsdb::storage;
using namespace tsdb::core;

class MockStorage : public Storage {
public:
    MockStorage() : write_count_(0), error_on_write_(false) {}
    
    tsdb::core::Result<void> init(const tsdb::core::StorageConfig& config) override {
        return tsdb::core::Result<void>();
    }
    
    tsdb::core::Result<void> write(const tsdb::core::TimeSeries& series) override {
        if (error_on_write_) {
            return tsdb::core::Result<void>::error("Mock storage error");
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        write_count_++;
        written_series_.push_back(series);
        
        return tsdb::core::Result<void>();
    }
    
    tsdb::core::Result<tsdb::core::TimeSeries> read(const tsdb::core::Labels& labels,
                                       int64_t start_time,
                                       int64_t end_time) override {
        return tsdb::core::Result<tsdb::core::TimeSeries>::error("Not implemented in mock");
    }
    
    tsdb::core::Result<std::vector<tsdb::core::TimeSeries>> query(
        const std::vector<std::pair<std::string, std::string>>& matchers,
        int64_t start_time,
        int64_t end_time) override {
        return tsdb::core::Result<std::vector<tsdb::core::TimeSeries>>::error("Not implemented in mock");
    }
    
    tsdb::core::Result<std::vector<std::string>> label_names() override {
        return tsdb::core::Result<std::vector<std::string>>::error("Not implemented in mock");
    }
    
    tsdb::core::Result<std::vector<std::string>> label_values(const std::string& label_name) override {
        return tsdb::core::Result<std::vector<std::string>>::error("Not implemented in mock");
    }
    
    tsdb::core::Result<void> delete_series(
        const std::vector<std::pair<std::string, std::string>>& matchers) override {
        return tsdb::core::Result<void>::error("Not implemented in mock");
    }
    
    tsdb::core::Result<void> compact() override {
        return tsdb::core::Result<void>();
    }
    
    tsdb::core::Result<void> flush() override {
        return tsdb::core::Result<void>();
    }
    
    tsdb::core::Result<void> close() override {
        return tsdb::core::Result<void>();
    }
    
    std::string stats() const override {
        return "Mock storage stats";
    }
    
    // Mock-specific methods
    void setErrorOnWrite(bool error) { error_on_write_ = error; }
    
    uint64_t getWriteCount() const { 
        std::lock_guard<std::mutex> lock(mutex_);
        return write_count_; 
    }
    
    std::vector<tsdb::core::TimeSeries> getWrittenSeries() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return written_series_;
    }
    
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        write_count_ = 0;
        written_series_.clear();
    }

private:
    mutable std::mutex mutex_;
    uint64_t write_count_;
    bool error_on_write_;
    std::vector<tsdb::core::TimeSeries> written_series_;
};

class ShardedWriteBufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_storage_ = std::make_shared<MockStorage>();
        
        ShardedWriteBufferConfig config;
        config.num_shards = 4;  // Small number for testing
        config.buffer_size_per_shard = 1000;  // Increased to handle 1000 writes
        config.flush_interval_ms = 100;
        config.max_flush_workers = 2;
        config.retry_attempts = 2;
        config.retry_delay = std::chrono::milliseconds(10);
        
        buffer_ = std::make_unique<ShardedWriteBuffer>(config);
        buffer_->initialize(mock_storage_);
    }
    
    void TearDown() override {
        if (buffer_) {
            buffer_->shutdown();
        }
        buffer_.reset();
        mock_storage_.reset();
    }
    
    tsdb::core::TimeSeries createTestSeries(const std::string& name, const std::vector<double>& values) {
        tsdb::core::Labels labels;
        labels.add("__name__", name);
        labels.add("instance", "test");
        
        tsdb::core::TimeSeries series(labels);
        for (size_t i = 0; i < values.size(); ++i) {
            series.add_sample(tsdb::core::Sample(1000 + i * 1000, values[i]));
        }
        return series;
    }
    
    std::shared_ptr<MockStorage> mock_storage_;
    std::unique_ptr<ShardedWriteBuffer> buffer_;
};

TEST_F(ShardedWriteBufferTest, BasicWriteAndFlush) {
    // Create a test time series
    auto series = createTestSeries("test_metric", {1.0, 2.0, 3.0});
    
    // Write the series
    auto result = buffer_->write(series);
    EXPECT_TRUE(result.ok());
    
    // Flush and verify
    result = buffer_->flush(true);
    EXPECT_TRUE(result.ok());
    
    // Wait a bit for background processing
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    EXPECT_EQ(mock_storage_->getWriteCount(), 1);
    auto written_series = mock_storage_->getWrittenSeries();
    EXPECT_EQ(written_series.size(), 1);
    EXPECT_EQ(written_series[0].labels().get("__name__"), "test_metric");
}

TEST_F(ShardedWriteBufferTest, MultipleWrites) {
    // Write to multiple series
    for (int i = 0; i < 10; ++i) {
        std::string series_name = "series" + std::to_string(i);
        auto series = createTestSeries(series_name, {static_cast<double>(i)});
        
        auto result = buffer_->write(series);
        EXPECT_TRUE(result.ok());
    }
    
    // Flush and verify
    auto result = buffer_->flush(true);
    EXPECT_TRUE(result.ok());
    
    // Wait for background processing
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    EXPECT_EQ(mock_storage_->getWriteCount(), 10);
    auto written_series = mock_storage_->getWrittenSeries();
    EXPECT_EQ(written_series.size(), 10);
}

TEST_F(ShardedWriteBufferTest, ShardDistribution) {
    // Write to different series to test shard distribution
    std::vector<std::string> series_names = {
        "cpu_usage", "memory_usage", "disk_io", "network_traffic",
        "temperature", "humidity", "pressure", "voltage"
    };
    
    for (const auto& name : series_names) {
        auto series = createTestSeries(name, {1.0});
        auto result = buffer_->write(series);
        EXPECT_TRUE(result.ok());
    }
    
    // Flush and check distribution
    buffer_->flush(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Get stats to verify distribution
    auto stats = buffer_->getStats();
    EXPECT_EQ(stats.total_writes, 8);
    EXPECT_EQ(stats.total_shards, 4);
}

TEST_F(ShardedWriteBufferTest, BufferFullHandling) {
    // Create a buffer with very small capacity
    ShardedWriteBufferConfig config;
    config.num_shards = 1;
    config.buffer_size_per_shard = 2;  // Very small buffer
    config.flush_interval_ms = 1000;   // Long flush interval
    
    auto small_buffer = std::make_unique<ShardedWriteBuffer>(config);
    small_buffer->initialize(mock_storage_);
    
    // Write more than buffer capacity
    for (int i = 0; i < 5; ++i) {
        std::string series_name = "series" + std::to_string(i);
        auto series = createTestSeries(series_name, {static_cast<double>(i)});
        
        auto result = small_buffer->write(series);
        
        if (i < 2) {
            EXPECT_TRUE(result.ok());
        } else {
            EXPECT_FALSE(result.ok());  // Buffer should be full
        }
    }
    
    // Check stats
    auto stats = small_buffer->getStats();
    EXPECT_EQ(stats.total_writes, 2);
    EXPECT_EQ(stats.dropped_writes, 3);
    
    small_buffer->shutdown();
}

TEST_F(ShardedWriteBufferTest, BackgroundFlushing) {
    // Write data without explicit flush
    for (int i = 0; i < 5; ++i) {
        std::string series_name = "series" + std::to_string(i);
        auto series = createTestSeries(series_name, {static_cast<double>(i)});
        
        auto result = buffer_->write(series);
        EXPECT_TRUE(result.ok());
    }
    
    // Wait for background flush
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    // Verify data was flushed
    EXPECT_EQ(mock_storage_->getWriteCount(), 5);
}

TEST_F(ShardedWriteBufferTest, ConcurrentWrites) {
    const int num_threads = 4;
    const int writes_per_thread = 10;
    std::atomic<int> successful_writes{0};
    std::atomic<int> failed_writes{0};
    
    auto write_worker = [&](int thread_id) {
        for (int i = 0; i < writes_per_thread; ++i) {
            std::string series_name = "thread" + std::to_string(thread_id) + "_series" + std::to_string(i);
            auto series = createTestSeries(series_name, {static_cast<double>(i)});
            
            auto result = buffer_->write(series);
            if (result.ok()) {
                successful_writes++;
            } else {
                failed_writes++;
            }
        }
    };
    
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(write_worker, i);
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Flush and verify
    buffer_->flush(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    EXPECT_EQ(successful_writes.load(), num_threads * writes_per_thread);
    EXPECT_EQ(mock_storage_->getWriteCount(), num_threads * writes_per_thread);
}

TEST_F(ShardedWriteBufferTest, Statistics) {
    // Write some data
    for (int i = 0; i < 10; ++i) {
        std::string series_name = "series" + std::to_string(i);
        auto series = createTestSeries(series_name, {static_cast<double>(i)});
        
        buffer_->write(series);
    }
    
    // Get statistics
    auto stats = buffer_->getStats();
    EXPECT_EQ(stats.total_writes, 10);
    EXPECT_EQ(stats.total_shards, 4);
    EXPECT_GT(stats.write_throughput, 0.0);
    
    // Check individual shard stats
    for (uint32_t i = 0; i < 4; ++i) {
        auto shard_stats = buffer_->getShardStats(i);
        EXPECT_EQ(shard_stats.shard_id, i);
        EXPECT_GE(shard_stats.utilization, 0.0);
        EXPECT_LE(shard_stats.utilization, 100.0);
    }
}

TEST_F(ShardedWriteBufferTest, LoadBalancing) {
    // Write data to test load balancing
    for (int i = 0; i < 20; ++i) {
        std::string series_name = "series" + std::to_string(i);
        auto series = createTestSeries(series_name, {static_cast<double>(i)});
        
        buffer_->write(series);
    }
    
    // Get load balance information
    auto load_info = buffer_->getLoadBalanceInfo();
    EXPECT_GE(load_info.imbalance_ratio, 0.0);
    EXPECT_LE(load_info.imbalance_ratio, 1.0);
    EXPECT_GE(load_info.std_deviation, 0.0);
    EXPECT_LT(load_info.most_loaded_shard, 4);
    EXPECT_LT(load_info.least_loaded_shard, 4);
}

TEST_F(ShardedWriteBufferTest, HealthCheck) {
    // Buffer should be healthy initially
    EXPECT_TRUE(buffer_->isHealthy());
    
    // Write some data
    for (int i = 0; i < 5; ++i) {
        std::string series_name = "series" + std::to_string(i);
        auto series = createTestSeries(series_name, {static_cast<double>(i)});
        
        buffer_->write(series);
    }
    
    // Should still be healthy
    EXPECT_TRUE(buffer_->isHealthy());
    
    // Shutdown and check health
    buffer_->shutdown();
    EXPECT_FALSE(buffer_->isHealthy());
}

TEST_F(ShardedWriteBufferTest, ErrorHandling) {
    // Test write with empty series
    tsdb::core::Labels empty_labels;
    tsdb::core::TimeSeries empty_series(empty_labels);
    
    auto result = buffer_->write(empty_series);
    EXPECT_FALSE(result.ok());
    
    // Test flush with invalid shard
    result = buffer_->flushShard(999, true);
    EXPECT_FALSE(result.ok());
}

TEST_F(ShardedWriteBufferTest, RetryLogic) {
    // Set storage to error on first write
    mock_storage_->setErrorOnWrite(true);
    
    // Write data
    auto series = createTestSeries("test_metric", {1.0, 2.0, 3.0});
    
    auto result = buffer_->write(series);
    EXPECT_TRUE(result.ok());  // Write to buffer should succeed
    
    // Flush should succeed (background flushing)
    result = buffer_->flush(false);
    EXPECT_TRUE(result.ok());  // Flush operation itself succeeds
    
    // Wait for retry attempts
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Check that no writes succeeded
    EXPECT_EQ(mock_storage_->getWriteCount(), 0);
    
    // Reset storage and try again
    mock_storage_->setErrorOnWrite(false);
    buffer_->flush(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Now should succeed
    EXPECT_EQ(mock_storage_->getWriteCount(), 1);
}

TEST_F(ShardedWriteBufferTest, CallbackHandling) {
    std::atomic<bool> callback_called{false};
    std::atomic<bool> callback_success{false};
    
    auto callback = [&](tsdb::core::Result<void> result) {
        callback_called = true;
        callback_success = result.ok();
    };
    
    // Write with callback
    auto series = createTestSeries("test_metric", {1.0});
    
    auto result = buffer_->write(series, callback);
    EXPECT_TRUE(result.ok());
    
    // Flush to trigger callback
    buffer_->flush(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    EXPECT_TRUE(callback_called.load());
    EXPECT_TRUE(callback_success.load());
}

TEST_F(ShardedWriteBufferTest, FactoryMethods) {
    // Test default factory
    auto buffer1 = ShardedWriteBufferFactory::create();
    EXPECT_NE(buffer1, nullptr);
    
    // Test custom config factory
    ShardedWriteBufferConfig config;
    config.num_shards = 8;
    config.buffer_size_per_shard = 2048;
    
    auto buffer2 = ShardedWriteBufferFactory::create(config);
    EXPECT_NE(buffer2, nullptr);
    EXPECT_EQ(buffer2->getConfig().num_shards, 8);
    
    // Test optimized factory
    auto buffer3 = ShardedWriteBufferFactory::createOptimized(10000, 10, 1024 * 1024 * 100);
    EXPECT_NE(buffer3, nullptr);
    
    auto optimized_config = buffer3->getConfig();
    EXPECT_GT(optimized_config.num_shards, 0);
    EXPECT_GT(optimized_config.buffer_size_per_shard, 0);
}

TEST_F(ShardedWriteBufferTest, ConfigurationUpdate) {
    // Create a separate buffer for testing configuration updates
    ShardedWriteBufferConfig config;
    config.num_shards = 4;
    config.buffer_size_per_shard = 100;
    
    auto test_buffer = std::make_unique<ShardedWriteBuffer>(config);
    
    // Test updating configuration before initialization
    ShardedWriteBufferConfig new_config;
    new_config.num_shards = 8;
    new_config.buffer_size_per_shard = 2048;
    
    auto result = test_buffer->updateConfig(new_config);
    EXPECT_TRUE(result.ok());
    EXPECT_EQ(test_buffer->getConfig().num_shards, 8);
    
    // Test that configuration was updated correctly
    EXPECT_EQ(test_buffer->getConfig().num_shards, 8);
    EXPECT_EQ(test_buffer->getConfig().buffer_size_per_shard, 2048);
}

TEST_F(ShardedWriteBufferTest, GracefulShutdown) {
    // Write some data
    for (int i = 0; i < 10; ++i) {
        std::string series_name = "series" + std::to_string(i);
        auto series = createTestSeries(series_name, {static_cast<double>(i)});
        
        buffer_->write(series);
    }
    
    // Shutdown
    auto result = buffer_->shutdown();
    EXPECT_TRUE(result.ok());
    
    // Wait a bit and verify all data was flushed
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(mock_storage_->getWriteCount(), 10);
    
    // Try to write after shutdown (should fail)
    auto series = createTestSeries("after_shutdown", {1.0});
    result = buffer_->write(series);
    EXPECT_FALSE(result.ok());
}

TEST_F(ShardedWriteBufferTest, PerformanceBenchmark) {
    const int num_writes = 1000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_writes; ++i) {
        std::string series_name = "series" + std::to_string(i);
        auto series = createTestSeries(series_name, {static_cast<double>(i)});
        
        auto result = buffer_->write(series);
        EXPECT_TRUE(result.ok());
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Performance should be reasonable (less than 100μs per write)
    double avg_time_per_write = static_cast<double>(duration.count()) / num_writes;
    EXPECT_LT(avg_time_per_write, 100.0);  // Less than 100μs per write
    
    // Flush and verify all writes
    buffer_->flush(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(mock_storage_->getWriteCount(), num_writes);
} 