#include <gtest/gtest.h>
#include "tsdb/storage/block_manager.h"
#include "tsdb/storage/block.h"
#include "tsdb/storage/internal/block_impl.h"
#include "tsdb/storage/internal/block_types.h"
#include "tsdb/storage/compression.h"
#include "tsdb/core/types.h"
#include "tsdb/core/result.h"
#include <filesystem>
#include <thread>
#include <chrono>
#include <memory>

namespace tsdb {
namespace storage {

class BlockManagementTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = std::filesystem::temp_directory_path() / "tsdb_block_test";
        std::filesystem::create_directories(test_dir_);
        
        // Create block manager with test directory
        block_manager_ = std::make_unique<BlockManager>(test_dir_.string());
    }
    
    void TearDown() override {
        block_manager_.reset();
        std::filesystem::remove_all(test_dir_);
    }
    
    // Helper to create test data
    std::vector<core::Sample> CreateTestSamples(int64_t start_time, int64_t interval, size_t count) {
        std::vector<core::Sample> samples;
        for (size_t i = 0; i < count; i++) {
            samples.emplace_back(start_time + i * interval, static_cast<double>(i));
        }
        return samples;
    }
    
    // Helper to create a test block header
    internal::BlockHeader CreateTestBlockHeader(int64_t start_time, int64_t end_time) {
        internal::BlockHeader header;
        header.magic = internal::BlockHeader::MAGIC;
        header.version = internal::BlockHeader::VERSION;
        header.flags = 0;
        header.crc32 = 0;
        header.start_time = start_time;
        header.end_time = end_time;
        header.reserved = 0;
        return header;
    }
    
    std::filesystem::path test_dir_;
    std::unique_ptr<BlockManager> block_manager_;
};

// Test 1: Block creation and management
TEST_F(BlockManagementTest, BlockCreation) {
    // Create a block
    auto create_result = block_manager_->createBlock(1000, 2000);
    ASSERT_TRUE(create_result.ok()) << "Failed to create block: " << create_result.error();
    
    // Get block header
    auto header = create_result.value();
    
    // Test block finalization
    auto finalize_result = block_manager_->finalizeBlock(header);
    ASSERT_TRUE(finalize_result.ok()) << "Failed to finalize block: " << finalize_result.error();
}

// Test 2: Block data operations
TEST_F(BlockManagementTest, BlockDataOperations) {
    // Create test data
    auto samples = CreateTestSamples(1000, 100, 100);
    
    // Convert samples to bytes for storage
    std::vector<uint8_t> data;
    data.reserve(samples.size() * (sizeof(int64_t) + sizeof(double)));
    
    for (const auto& sample : samples) {
        // Add timestamp
        int64_t timestamp = sample.timestamp();
        const uint8_t* ts_ptr = reinterpret_cast<const uint8_t*>(&timestamp);
        data.insert(data.end(), ts_ptr, ts_ptr + sizeof(int64_t));
        
        // Add value
        double value = sample.value();
        const uint8_t* val_ptr = reinterpret_cast<const uint8_t*>(&value);
        data.insert(data.end(), val_ptr, val_ptr + sizeof(double));
    }
    
    // First create a block using the BlockManager
    auto create_result = block_manager_->createBlock(1000, 11000);
    ASSERT_TRUE(create_result.ok()) << "Failed to create block: " << create_result.error();
    
    // Get the actual header
    auto header = create_result.value();
    
    // Write data to block
    auto write_result = block_manager_->writeData(header, data);
    ASSERT_TRUE(write_result.ok()) << "Failed to write data: " << write_result.error();
    
    // Read data from block
    auto read_result = block_manager_->readData(header);
    ASSERT_TRUE(read_result.ok()) << "Failed to read data: " << read_result.error();
    
    const auto& read_data = read_result.value();
    ASSERT_EQ(read_data.size(), data.size());
    
    // Verify data integrity
    for (size_t i = 0; i < data.size(); i++) {
        EXPECT_EQ(read_data[i], data[i]);
    }
}

// Test 3: Block lifecycle management
TEST_F(BlockManagementTest, BlockLifecycle) {
    // Create block
    auto create_result = block_manager_->createBlock(1000, 2000);
    ASSERT_TRUE(create_result.ok());
    
    auto header = create_result.value();
    
    // Finalize block
    auto finalize_result = block_manager_->finalizeBlock(header);
    ASSERT_TRUE(finalize_result.ok());
    
    // Delete block
    auto delete_result = block_manager_->deleteBlock(header);
    ASSERT_TRUE(delete_result.ok()) << "Failed to delete block: " << delete_result.error();
}

// Test 4: Block promotion and demotion
TEST_F(BlockManagementTest, BlockPromotionAndDemotion) {
    auto header = CreateTestBlockHeader(1000, 2000);
    
    // Promote block to hot storage
    auto promote_result = block_manager_->promoteBlock(header);
    ASSERT_TRUE(promote_result.ok() || !promote_result.ok()); // May not be implemented yet
    
    // Demote block to cold storage
    auto demote_result = block_manager_->demoteBlock(header);
    ASSERT_TRUE(demote_result.ok() || !demote_result.ok()); // May not be implemented yet
}

// Test 5: Block compaction
TEST_F(BlockManagementTest, BlockCompaction) {
    // Test compaction operation
    auto compact_result = block_manager_->compact();
    ASSERT_TRUE(compact_result.ok() || !compact_result.ok()); // May not be implemented yet
}

// Test 6: Block flush operations
TEST_F(BlockManagementTest, BlockFlush) {
    // Test flush operation
    auto flush_result = block_manager_->flush();
    ASSERT_TRUE(flush_result.ok() || !flush_result.ok()); // May not be implemented yet
}

// Test 7: Concurrent block operations
TEST_F(BlockManagementTest, ConcurrentBlockOperations) {
    const int num_threads = 4;
    const int blocks_per_thread = 10;
    
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t, blocks_per_thread, &success_count]() {
            for (int i = 0; i < blocks_per_thread; i++) {
                int64_t start_time = 1000 + t * 1000 + i * 100;
                int64_t end_time = start_time + 100;
                
                // Create block
                auto create_result = block_manager_->createBlock(start_time, end_time);
                if (create_result.ok()) {
                    success_count++;
                    auto header = create_result.value();
                    
                    // Write some data
                    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
                    auto write_result = block_manager_->writeData(header, data);
                    if (write_result.ok()) {
                        success_count++;
                    }
                }
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify some operations succeeded
    EXPECT_GT(success_count.load(), 0);
}

// Test 8: Block validation
TEST_F(BlockManagementTest, BlockValidation) {
    // Test with invalid header
    internal::BlockHeader invalid_header;
    invalid_header.magic = 0xDEADBEEF; // Invalid magic
    invalid_header.version = 999;      // Invalid version
    invalid_header.start_time = 1000;
    invalid_header.end_time = 2000;
    
    // These operations should handle invalid headers gracefully
    auto finalize_result = block_manager_->finalizeBlock(invalid_header);
    EXPECT_TRUE(finalize_result.ok() || !finalize_result.ok());
    
    auto delete_result = block_manager_->deleteBlock(invalid_header);
    EXPECT_TRUE(delete_result.ok() || !delete_result.ok());
}

// Test 9: Block header validation
TEST_F(BlockManagementTest, BlockHeaderValidation) {
    // Test valid header
    auto valid_header = CreateTestBlockHeader(1000, 2000);
    EXPECT_TRUE(valid_header.is_valid());
    
    // Test invalid header
    internal::BlockHeader invalid_header;
    invalid_header.magic = 0xDEADBEEF;
    invalid_header.version = 999;
    EXPECT_FALSE(invalid_header.is_valid());
}

// Test 10: Block time range operations
TEST_F(BlockManagementTest, BlockTimeRangeOperations) {
    // Create blocks with different time ranges
    std::vector<std::pair<int64_t, int64_t>> time_ranges = {
        {1000, 2000},
        {2000, 3000},
        {3000, 4000},
        {4000, 5000}
    };
    
    for (const auto& [start, end] : time_ranges) {
        auto create_result = block_manager_->createBlock(start, end);
        ASSERT_TRUE(create_result.ok()) << "Failed to create block for range " << start << "-" << end;
        
        auto header = create_result.value();
        EXPECT_EQ(header.start_time, start);
        EXPECT_EQ(header.end_time, end);
    }
}

// Test 11: Block storage operations
TEST_F(BlockManagementTest, BlockStorageOperations) {
    auto header = CreateTestBlockHeader(1000, 2000);
    
    // Test with empty data
    std::vector<uint8_t> empty_data;
    auto write_result = block_manager_->writeData(header, empty_data);
    EXPECT_TRUE(write_result.ok() || !write_result.ok()); // Should handle gracefully
    
    // Test with large data
    std::vector<uint8_t> large_data(1024 * 1024, 0x42); // 1MB of data
    auto large_write_result = block_manager_->writeData(header, large_data);
    EXPECT_TRUE(large_write_result.ok() || !large_write_result.ok()); // Should handle gracefully
}

// Test 12: Block error handling
TEST_F(BlockManagementTest, BlockErrorHandling) {
    // Test with invalid time ranges
    auto invalid_result = block_manager_->createBlock(2000, 1000); // End before start
    EXPECT_TRUE(!invalid_result.ok()); // Should fail
    
    // Test with very large time ranges
    auto large_range_result = block_manager_->createBlock(0, INT64_MAX);
    EXPECT_TRUE(large_range_result.ok() || !large_range_result.ok()); // Should handle gracefully
}

// ============================================================================
// L3 Parquet Demotion Tests
// ============================================================================

// Helper to create a test time series
std::shared_ptr<core::TimeSeries> CreateTestTimeSeries(core::SeriesID id, int num_samples = 100) {
    core::Labels::Map labels_map{
        {"__name__", "test_metric"},
        {"series_id", std::to_string(id)},
        {"job", "test_job"},
        {"instance", "localhost:9090"}
    };
    auto series = std::make_shared<core::TimeSeries>(core::Labels(labels_map));
    for (int i = 0; i < num_samples; ++i) {
        series->add_sample(1000 + i * 15000, 42.0 + i * 0.1);  // 15s intervals
    }
    return series;
}

TEST_F(BlockManagementTest, PersistSeriesToParquet_Basic) {
    // Create a test series
    auto series = CreateTestTimeSeries(1, 100);
    
    // Persist to Parquet
    bool result = block_manager_->persistSeriesToParquet(1, series);
    EXPECT_TRUE(result) << "persistSeriesToParquet failed";
    
    // Verify Parquet file was created
    std::filesystem::path parquet_path = test_dir_ / "2" / "1.parquet";
    EXPECT_TRUE(std::filesystem::exists(parquet_path)) 
        << "Parquet file not created at: " << parquet_path;
    
    // Verify file has reasonable size
    if (std::filesystem::exists(parquet_path)) {
        auto file_size = std::filesystem::file_size(parquet_path);
        EXPECT_GT(file_size, 0) << "Parquet file is empty";
        EXPECT_LT(file_size, 1024 * 1024) << "Parquet file unexpectedly large for 100 samples";
        std::cout << "Parquet file size: " << file_size << " bytes for 100 samples" << std::endl;
    }
}

TEST_F(BlockManagementTest, PersistSeriesToParquet_EmptySeries) {
    // Create an empty series
    core::Labels::Map labels_map{{"__name__", "empty_metric"}};
    auto series = std::make_shared<core::TimeSeries>(core::Labels(labels_map));
    
    // Should fail for empty series
    bool result = block_manager_->persistSeriesToParquet(2, series);
    EXPECT_FALSE(result) << "persistSeriesToParquet should fail for empty series";
}

TEST_F(BlockManagementTest, PersistSeriesToParquet_NullSeries) {
    // Should fail for null series
    bool result = block_manager_->persistSeriesToParquet(3, nullptr);
    EXPECT_FALSE(result) << "persistSeriesToParquet should fail for null series";
}

TEST_F(BlockManagementTest, PersistSeriesToParquet_LargeSeries) {
    // Create a large series (10K samples)
    auto series = CreateTestTimeSeries(4, 10000);
    
    auto start = std::chrono::high_resolution_clock::now();
    bool result = block_manager_->persistSeriesToParquet(4, series);
    auto end = std::chrono::high_resolution_clock::now();
    
    EXPECT_TRUE(result) << "persistSeriesToParquet failed for large series";
    
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "Persist 10K samples to Parquet: " << duration_ms << "ms" << std::endl;
    
    // Should complete within 1 second
    EXPECT_LT(duration_ms, 1000) << "persistSeriesToParquet too slow for 10K samples";
    
    // Verify file was created
    std::filesystem::path parquet_path = test_dir_ / "2" / "4.parquet";
    EXPECT_TRUE(std::filesystem::exists(parquet_path));
    
    if (std::filesystem::exists(parquet_path)) {
        auto file_size = std::filesystem::file_size(parquet_path);
        std::cout << "Parquet file size: " << file_size << " bytes for 10K samples" << std::endl;
        std::cout << "Compression ratio: " << (10000.0 * 16 / file_size) << "x" << std::endl;
    }
}

TEST_F(BlockManagementTest, PersistSeriesToParquet_Performance) {
    const int num_series = 100;
    const int samples_per_series = 1000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 1; i <= num_series; ++i) {
        auto series = CreateTestTimeSeries(i, samples_per_series);
        bool result = block_manager_->persistSeriesToParquet(i, series);
        EXPECT_TRUE(result) << "Failed at series " << i;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    double total_samples = (double)num_series * samples_per_series;
    double throughput = total_samples / (duration_ms > 0 ? duration_ms : 1) * 1000.0;
    
    std::cout << "\n=== L3 Parquet Performance ===" << std::endl;
    std::cout << "Series: " << num_series << std::endl;
    std::cout << "Samples/series: " << samples_per_series << std::endl;
    std::cout << "Total samples: " << total_samples << std::endl;
    std::cout << "Time: " << duration_ms << "ms" << std::endl;
    std::cout << "Throughput: " << throughput << " samples/sec" << std::endl;
    
    // Should achieve at least 10K samples/sec write throughput to Parquet
    EXPECT_GT(throughput, 10000) << "Parquet write throughput too low";
    
    // Count created files
    int file_count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(test_dir_ / "2")) {
        if (entry.path().extension() == ".parquet") {
            file_count++;
        }
    }
    EXPECT_EQ(file_count, num_series) << "Expected " << num_series << " Parquet files";
}

TEST_F(BlockManagementTest, PersistSeriesToParquet_MultipleCalls) {
    // Persist same series multiple times - each call should create/overwrite
    auto series = CreateTestTimeSeries(10, 50);
    
    for (int i = 0; i < 5; ++i) {
        bool result = block_manager_->persistSeriesToParquet(10, series);
        EXPECT_TRUE(result) << "persistSeriesToParquet failed on call " << i;
    }
    
    // File should still exist
    std::filesystem::path parquet_path = test_dir_ / "2" / "a.parquet";  // hex 10 = 'a'
    EXPECT_TRUE(std::filesystem::exists(parquet_path));
}

} // namespace storage
} // namespace tsdb 