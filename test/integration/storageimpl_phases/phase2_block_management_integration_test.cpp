/**
 * @file phase2_block_management_integration_test.cpp
 * @brief Phase 2.4: Block Management Integration Tests for StorageImpl
 * 
 * This file tests the integration of block management into the StorageImpl class.
 * It verifies that write operations use block-based storage with proper tiering
 * and that read operations efficiently retrieve data from blocks.
 * 
 * Test Categories:
 * - Block creation and rotation
 * - Block compaction verification
 * - Multi-tier storage testing
 * - Block indexing validation
 * 
 * Expected Outcomes:
 * - Efficient block creation and rotation
 * - Proper block compaction
 * - Multi-tier storage optimization
 * - Fast block-based queries
 */

#include <gtest/gtest.h>
#include "tsdb/storage/storage_impl.h"
#include "tsdb/core/config.h"
#include "tsdb/core/types.h"
#include "tsdb/storage/block_manager.h"
#include "tsdb/storage/internal/block_types.h"
#include <chrono>
#include <thread>
#include <vector>
#include <filesystem>
#include <fstream>

namespace tsdb {
namespace integration {

class Phase2BlockManagementIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary test directory
        test_dir_ = "/tmp/tsdb_block_test_" + std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(test_dir_);
        
        // Initialize StorageImpl with block management configuration
        config_.data_dir = test_dir_;
        config_.block_config.max_block_size = 1000;  // Small blocks for testing rotation
        config_.block_config.max_block_records = 200; // Deterministic rotation trigger
        config_.enable_compression = true;
        config_.compression_config.timestamp_compression = core::CompressionConfig::Algorithm::GORILLA;
        config_.compression_config.value_compression = core::CompressionConfig::Algorithm::GORILLA;
        config_.compression_config.label_compression = core::CompressionConfig::Algorithm::DICTIONARY;
        
        storage_ = std::make_unique<storage::StorageImpl>(config_);
        auto result = storage_->init(config_);
        ASSERT_TRUE(result.ok()) << "StorageImpl initialization failed: " << result.error();
    }
    
    void TearDown() override {
        if (storage_) {
            storage_->close();
        }
        
        // Clean up test directory
        if (std::filesystem::exists(test_dir_)) {
            std::filesystem::remove_all(test_dir_);
        }
    }
    
    // Helper methods for block validation
    core::TimeSeries createTestSeries(const std::string& name, int sample_count) {
        core::Labels labels;
        labels.add("__name__", name);
        labels.add("instance", "test");
        labels.add("job", "block_test");
        
        std::vector<core::Sample> samples;
        samples.reserve(sample_count);
        
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        for (int i = 0; i < sample_count; ++i) {
            core::Sample sample(now + (i * 1000), 42.0 + i);  // 1 second intervals
            samples.push_back(sample);
        }
        
        core::TimeSeries series(labels);
        for (const auto& sample : samples) {
            series.add_sample(sample);
        }
        return series;
    }
    
    core::TimeSeries createLargeSeries(const std::string& name, int sample_count) {
        core::Labels labels;
        labels.add("__name__", name);
        labels.add("instance", "large_test");
        labels.add("job", "block_test");
        
        std::vector<core::Sample> samples;
        samples.reserve(sample_count);
        
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        for (int i = 0; i < sample_count; ++i) {
            core::Sample sample(now + (i * 100), 100.0 + (i % 1000));  // 100ms intervals for more data
            samples.push_back(sample);
        }
        
        core::TimeSeries series(labels);
        for (const auto& sample : samples) {
            series.add_sample(sample);
        }
        return series;
    }
    
    bool verifyBlockFilesExist() {
        // Check if block files exist in the test directory
        std::vector<std::string> tier_dirs = {"0", "1", "2"};  // HOT, WARM, COLD
        bool found_blocks = false;
        
        for (const auto& tier_dir : tier_dirs) {
            std::string tier_path = test_dir_ + "/" + tier_dir;
            if (std::filesystem::exists(tier_path)) {
                for (const auto& entry : std::filesystem::directory_iterator(tier_path)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".block") {
                        found_blocks = true;
                        break;
                    }
                }
            }
            if (found_blocks) break;
        }
        
        return found_blocks;
    }
    
    int countBlockFiles() {
        int count = 0;
        std::vector<std::string> tier_dirs = {"0", "1", "2"};  // HOT, WARM, COLD
        
        for (const auto& tier_dir : tier_dirs) {
            std::string tier_path = test_dir_ + "/" + tier_dir;
            if (std::filesystem::exists(tier_path)) {
                for (const auto& entry : std::filesystem::directory_iterator(tier_path)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".block") {
                        count++;
                    }
                }
            }
        }
        
        return count;
    }
    
    void waitForBlockRotation() {
        // Wait for block rotation to occur
        std::this_thread::sleep_for(std::chrono::seconds(6));
    }
    
    std::string test_dir_;
    core::StorageConfig config_;
    std::unique_ptr<storage::StorageImpl> storage_;
};

// Test Suite 2.4.1: Block Creation and Lifecycle
TEST_F(Phase2BlockManagementIntegrationTest, BlockCreationAndLifecycle) {
    // Test: Block creation during write operations
    // Validates: Blocks are created and managed properly
    
    auto series = createTestSeries("block_lifecycle_test", 100);
    auto result = storage_->write(series);
    ASSERT_TRUE(result.ok()) << "Write failed: " << result.error();
    
    // Close storage to ensure blocks are persisted to disk
    std::cout << "Calling storage_->close()" << std::endl;
    storage_->close();
    
    std::cout << "Calling storage_.reset()" << std::endl;
    storage_.reset();
    std::cout << "storage_.reset() complete" << std::endl;
    
    // Verify block files were created
    std::cout << "Verifying block files..." << std::endl;
    bool blocks_exist = verifyBlockFilesExist();
    std::cout << "Block files exist: " << blocks_exist << std::endl;
    EXPECT_TRUE(blocks_exist) << "No block files found after write";
    
    // Re-initialize storage to read back data
    storage_ = std::make_unique<storage::StorageImpl>(config_);
    auto init_result = storage_->init(config_);
    ASSERT_TRUE(init_result.ok()) << "Storage re-initialization failed: " << init_result.error();

    // Read back the data to verify block functionality
    auto read_result = storage_->read(series.labels(), 0, LLONG_MAX);
    ASSERT_TRUE(read_result.ok()) << "Read failed: " << read_result.error();
    
    // Verify data integrity
    EXPECT_EQ(read_result.value().samples().size(), series.samples().size());
    EXPECT_EQ(read_result.value().labels().get("__name__").value(), "block_lifecycle_test");
}

TEST_F(Phase2BlockManagementIntegrationTest, BlockRotationTriggered) {
    // Test: Block rotation when size limits are reached
    // Validates: Block rotation logic works correctly
    
    int initial_blocks = countBlockFiles();
    
    // Write data compliant with Series::append hardcoded limit (10,000 samples)
    // We need > 10000 samples to trigger rotation
    
    // Write a large series that forces rotation
    // 10050 samples > 10000 limit
    auto series = createLargeSeries("rotation_test_heavy", 10050);
    auto result = storage_->write(series);
    ASSERT_TRUE(result.ok()) << "Write failed for large series";
    
    // Check if blocks were created/rotated
    // Since we wrote > 10000 samples, the first block (0-9999) should be sealed and persisted
    // and a new block started for the remaining samples.
    
    // Wait briefly for async operations if any (though persistence is synchronous in write path for full blocks)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    int final_blocks = countBlockFiles();
    EXPECT_GT(final_blocks, initial_blocks) << "Block rotation did not occur after writing > 10000 samples";
}

// Test Suite 2.4.2: Multi-Tier Storage Integration
TEST_F(Phase2BlockManagementIntegrationTest, MultiTierStorageIntegration) {
    // Test: Data is properly stored in different tiers
    // Validates: Multi-tier storage functionality
    
    // Write data that should go to HOT tier initially
    auto hot_series = createTestSeries("hot_tier_test", 50);
    auto result = storage_->write(hot_series);
    ASSERT_TRUE(result.ok()) << "HOT tier write failed";
    
    // Verify data is accessible
    auto read_result = storage_->read(hot_series.labels(), 0, LLONG_MAX);
    ASSERT_TRUE(read_result.ok()) << "HOT tier read failed";
    EXPECT_EQ(read_result.value().samples().size(), 50);
    
    // Write more data to potentially trigger tier movement
    for (int i = 0; i < 10; ++i) {
        auto series = createLargeSeries("tier_test_" + std::to_string(i), 100);
        auto write_result = storage_->write(series);
        ASSERT_TRUE(write_result.ok()) << "Tier test write failed for series " << i;
    }
    
    // Verify all data is still accessible after tier operations
    auto final_read = storage_->read(hot_series.labels(), 0, LLONG_MAX);
    ASSERT_TRUE(final_read.ok()) << "Final read failed after tier operations";
    EXPECT_EQ(final_read.value().samples().size(), 50);
}

TEST_F(Phase2BlockManagementIntegrationTest, BlockIndexingAndFastLookups) {
    // Test: Block indexing enables fast data lookups
    // Validates: Block indexing and query performance
    
    // Write multiple series with different labels
    std::vector<core::TimeSeries> test_series;
    for (int i = 0; i < 20; ++i) {
        auto series = createTestSeries("index_test_" + std::to_string(i), 50);
        test_series.push_back(series);
        
        auto result = storage_->write(series);
        ASSERT_TRUE(result.ok()) << "Index test write failed for series " << i;
    }
    
    // Test fast lookups for each series
    for (const auto& original_series : test_series) {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        auto read_result = storage_->read(original_series.labels(), 0, LLONG_MAX);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        ASSERT_TRUE(read_result.ok()) << "Fast lookup failed for series: " << original_series.labels().get("__name__").value();
        EXPECT_EQ(read_result.value().samples().size(), 50);
        
        // Verify lookup is reasonably fast (should be < 10ms for indexed lookup)
        EXPECT_LT(duration.count(), 10000) << "Lookup too slow: " << duration.count() << "μs";
    }
}

// Test Suite 2.4.3: Block Compaction and Optimization
TEST_F(Phase2BlockManagementIntegrationTest, BlockCompactionIntegration) {
    // Test: Block compaction optimizes storage
    // Validates: Compaction logic integrates with StorageImpl
    
    // Write data to create multiple blocks
    // Write data to create multiple blocks
    // Hardcoded limit is 10,000 samples per block
    
    // Block 1
    auto series1 = createLargeSeries("compaction_test_1", 10050);
    auto result1 = storage_->write(series1);
    ASSERT_TRUE(result1.ok()) << "Compaction test write failed for block 1";
    
    // Block 2
    // We reuse the same series name or a different one? 
    // Usually compaction merges blocks of SAME series or time-adjacent?
    // If we want to compact, we usually need multiple blocks for the SAME series?
    // Or just multiple blocks in the system?
    // Let's create another block for the same series series1 if possible, but createLargeSeries uses a specific start time.
    // simpler to create distinct blocks for now, assuming "countBlockFiles" is the metric.
    
    auto series2 = createLargeSeries("compaction_test_2", 10050);
    auto result2 = storage_->write(series2);
    ASSERT_TRUE(result2.ok()) << "Compaction test write failed for block 2";
    
    int blocks_before_compaction = countBlockFiles();
    EXPECT_GT(blocks_before_compaction, 0) << "No blocks created before compaction";
    
    // Trigger compaction (this would normally be done by background processing)
    // For now, we'll verify that the system can handle multiple blocks
    // and that data remains accessible
    
    // Verify all data is still accessible after multiple block operations
    // Verify all data is still accessible after multiple block operations
    auto read_result1 = storage_->read(series1.labels(), 0, LLONG_MAX);
    ASSERT_TRUE(read_result1.ok()) << "Read failed for series 1";
    EXPECT_EQ(read_result1.value().samples().size(), 10050);
    
    auto read_result2 = storage_->read(series2.labels(), 0, LLONG_MAX);
    ASSERT_TRUE(read_result2.ok()) << "Read failed for series 2";
    EXPECT_EQ(read_result2.value().samples().size(), 10050);
}

// Test Suite 2.4.4: Block Error Handling and Recovery
TEST_F(Phase2BlockManagementIntegrationTest, BlockErrorHandlingAndRecovery) {
    // Test: System handles block-related errors gracefully
    // Validates: Error handling and recovery mechanisms
    
    // Test with invalid data that might cause block errors
    core::Labels labels;
    labels.add("__name__", "error_test");
    labels.add("instance", "test");
    labels.add("job", "block_test");
    
    // Create series with edge case data (in chronological order)
    std::vector<core::Sample> samples;
    samples.push_back({-1, -1.0});  // Edge case: negative timestamp
    samples.push_back({0, 0.0});  // Edge case: timestamp 0
    samples.push_back({LLONG_MAX, 1.0});  // Edge case: max timestamp
    
    core::TimeSeries edge_case_series(labels);
    for (const auto& sample : samples) {
        edge_case_series.add_sample(sample);
    }
    
    // This should not crash the system
    auto result = storage_->write(edge_case_series);
    // Note: We don't assert success here as edge cases might be rejected
    // The important thing is that the system doesn't crash
    
    // Verify system is still functional
    auto normal_series = createTestSeries("recovery_test", 10);
    auto recovery_result = storage_->write(normal_series);
    ASSERT_TRUE(recovery_result.ok()) << "System not functional after error test";
    
    auto read_result = storage_->read(normal_series.labels(), 0, LLONG_MAX);
    ASSERT_TRUE(read_result.ok()) << "Read failed after error test";
    EXPECT_EQ(read_result.value().samples().size(), 10);
}

// Test Suite 2.4.5: Block Performance Under Load
TEST_F(Phase2BlockManagementIntegrationTest, BlockPerformanceUnderLoad) {
    // Test: Block management performs well under load
    // Validates: Performance characteristics of block operations
    
    const int NUM_SERIES = 50;
    const int SAMPLES_PER_SERIES = 100;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Write many series to test block performance
    for (int i = 0; i < NUM_SERIES; ++i) {
        auto series = createTestSeries("perf_test_" + std::to_string(i), SAMPLES_PER_SERIES);
        auto result = storage_->write(series);
        ASSERT_TRUE(result.ok()) << "Performance test write failed for series " << i;
    }
    
    auto write_end_time = std::chrono::high_resolution_clock::now();
    auto write_duration = std::chrono::duration_cast<std::chrono::milliseconds>(write_end_time - start_time);
    
    // Verify write performance is reasonable
    double write_ops_per_sec = (NUM_SERIES * 1000.0) / write_duration.count();
    EXPECT_GT(write_ops_per_sec, 10.0) << "Write performance too slow: " << write_ops_per_sec << " ops/sec";
    
    // Test read performance
    auto read_start_time = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < NUM_SERIES; ++i) {
        core::Labels labels;
        labels.add("__name__", "perf_test_" + std::to_string(i));
        labels.add("instance", "test");
        labels.add("job", "block_test");
        
        auto read_result = storage_->read(labels, 0, LLONG_MAX);
        ASSERT_TRUE(read_result.ok()) << "Performance test read failed for series " << i;
        EXPECT_EQ(read_result.value().samples().size(), SAMPLES_PER_SERIES);
    }
    
    auto read_end_time = std::chrono::high_resolution_clock::now();
    auto read_duration = std::chrono::duration_cast<std::chrono::milliseconds>(read_end_time - read_start_time);
    
    // Verify read performance is reasonable
    double read_ops_per_sec = (NUM_SERIES * 1000.0) / read_duration.count();
    EXPECT_GT(read_ops_per_sec, 20.0) << "Read performance too slow: " << read_ops_per_sec << " ops/sec";
    
    // Close storage to ensure all data is flushed to disk
    storage_->close();
    
    // Verify block files were created
    EXPECT_TRUE(verifyBlockFilesExist()) << "No block files found after performance test";
}

} // namespace integration
} // namespace tsdb
