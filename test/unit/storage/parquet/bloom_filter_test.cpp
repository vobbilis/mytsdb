/**
 * @file bloom_filter_test.cpp
 * @brief Unit tests for Bloom Filter Manager
 * 
 * Tests:
 * 1. Bloom filter creation and basic operations
 * 2. False positive rate verification
 * 3. Serialization/deserialization to disk
 * 4. Integration with SeriesID computation
 * 5. Metrics recording for self-monitoring
 */

#include <gtest/gtest.h>
#include <filesystem>
#include <random>
#include "tsdb/storage/parquet/bloom_filter_manager.h"
#include "tsdb/storage/read_performance_instrumentation.h"

namespace tsdb {
namespace storage {
namespace parquet {
namespace test {

class BloomFilterTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = std::filesystem::temp_directory_path() / "tsdb_bloom_filter_test";
        std::filesystem::create_directories(test_dir_);
        
        // Clear the cache
        BloomFilterCache::Instance().Clear();
    }
    
    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
        BloomFilterCache::Instance().Clear();
    }
    
    std::filesystem::path test_dir_;
};

// =============================================================================
// Basic Functionality Tests
// =============================================================================

TEST_F(BloomFilterTest, CreateFilterAndAddEntries) {
    BloomFilterManager manager;
    
    // Initially not valid
    EXPECT_FALSE(manager.IsValid());
    
    // Create filter
    manager.CreateFilter(1000, 0.01);  // 1000 entries, 1% FPP
    
    EXPECT_TRUE(manager.IsValid());
    EXPECT_EQ(manager.GetEntriesAdded(), 0);
    EXPECT_GT(manager.GetFilterSizeBytes(), 0);
}

TEST_F(BloomFilterTest, AddAndQuerySeriesId) {
    BloomFilterManager manager;
    manager.CreateFilter(1000, 0.01);
    
    // Add some series IDs
    core::SeriesID id1 = 12345;
    core::SeriesID id2 = 67890;
    core::SeriesID id3 = 11111;
    
    manager.AddSeriesId(id1);
    manager.AddSeriesId(id2);
    
    EXPECT_EQ(manager.GetEntriesAdded(), 2);
    
    // Query - should find added IDs
    EXPECT_TRUE(manager.MightContain(id1));
    EXPECT_TRUE(manager.MightContain(id2));
    
    // id3 was not added - might return false (no false negatives)
    // Note: Bloom filters can have false positives but never false negatives
}

TEST_F(BloomFilterTest, AddByLabelsString) {
    BloomFilterManager manager;
    manager.CreateFilter(1000, 0.01);
    
    std::string labels1 = "__name__=cpu_usage,pod=pod-1";
    std::string labels2 = "__name__=memory_usage,pod=pod-2";
    
    manager.AddSeriesByLabels(labels1);
    manager.AddSeriesByLabels(labels2);
    
    EXPECT_EQ(manager.GetEntriesAdded(), 2);
    
    // Query using same labels
    EXPECT_TRUE(manager.MightContainLabels(labels1));
    EXPECT_TRUE(manager.MightContainLabels(labels2));
}

TEST_F(BloomFilterTest, SeriesIdComputation) {
    // Verify that ComputeSeriesId is deterministic
    std::string labels = "__name__=test,pod=pod-1";
    
    core::SeriesID id1 = BloomFilterManager::ComputeSeriesId(labels);
    core::SeriesID id2 = BloomFilterManager::ComputeSeriesId(labels);
    
    EXPECT_EQ(id1, id2);
    
    // Different labels should produce different IDs (with high probability)
    std::string labels2 = "__name__=test,pod=pod-2";
    core::SeriesID id3 = BloomFilterManager::ComputeSeriesId(labels2);
    
    EXPECT_NE(id1, id3);
}

// =============================================================================
// Serialization Tests
// =============================================================================

TEST_F(BloomFilterTest, SaveAndLoadFilter) {
    std::string parquet_path = (test_dir_ / "test.parquet").string();
    std::string bloom_path = BloomFilterManager::GetBloomPath(parquet_path);
    
    // Create and populate filter
    BloomFilterManager writer_manager;
    writer_manager.CreateFilter(1000, 0.01);
    
    core::SeriesID id1 = 12345;
    core::SeriesID id2 = 67890;
    writer_manager.AddSeriesId(id1);
    writer_manager.AddSeriesId(id2);
    
    // Save to disk
    EXPECT_TRUE(writer_manager.SaveFilter(parquet_path));
    EXPECT_TRUE(std::filesystem::exists(bloom_path));
    
    // Load in a new manager
    BloomFilterManager reader_manager;
    EXPECT_TRUE(reader_manager.LoadFilter(parquet_path));
    EXPECT_TRUE(reader_manager.IsValid());
    
    // Verify queries work
    EXPECT_TRUE(reader_manager.MightContain(id1));
    EXPECT_TRUE(reader_manager.MightContain(id2));
}

TEST_F(BloomFilterTest, GetBloomPath) {
    EXPECT_EQ(BloomFilterManager::GetBloomPath("/data/file.parquet"), "/data/file.bloom");
    EXPECT_EQ(BloomFilterManager::GetBloomPath("/data/file"), "/data/file.bloom");
    EXPECT_EQ(BloomFilterManager::GetBloomPath("test.parquet"), "test.bloom");
}

TEST_F(BloomFilterTest, LoadNonExistentFilter) {
    BloomFilterManager manager;
    
    // Should return false for non-existent file
    EXPECT_FALSE(manager.LoadFilter("/nonexistent/path.parquet"));
    EXPECT_FALSE(manager.IsValid());
}

// =============================================================================
// False Positive Rate Tests
// =============================================================================

TEST_F(BloomFilterTest, FalsePositiveRate) {
    const int NUM_ENTRIES = 10000;
    const double TARGET_FPP = 0.01;  // 1%
    
    BloomFilterManager manager;
    manager.CreateFilter(NUM_ENTRIES, TARGET_FPP);
    
    // Add entries
    std::vector<core::SeriesID> added_ids;
    for (int i = 0; i < NUM_ENTRIES; ++i) {
        core::SeriesID id = 1000000 + i;
        manager.AddSeriesId(id);
        added_ids.push_back(id);
    }
    
    // Verify no false negatives - all added IDs must be found
    for (const auto& id : added_ids) {
        EXPECT_TRUE(manager.MightContain(id)) << "False negative for ID: " << id;
    }
    
    // Count false positives for IDs that were NOT added
    int false_positives = 0;
    const int NUM_QUERIES = 100000;
    std::mt19937_64 rng(42);  // Fixed seed for reproducibility
    
    for (int i = 0; i < NUM_QUERIES; ++i) {
        // Generate ID that was definitely not added
        core::SeriesID id = 9000000 + i;
        if (manager.MightContain(id)) {
            false_positives++;
        }
    }
    
    double actual_fpp = static_cast<double>(false_positives) / NUM_QUERIES;
    
    // Allow some margin (2x the target FPP)
    EXPECT_LT(actual_fpp, TARGET_FPP * 2.0) 
        << "False positive rate too high: " << actual_fpp 
        << " (target: " << TARGET_FPP << ")";
    
    std::cout << "Bloom Filter FPP Test: " << false_positives << "/" << NUM_QUERIES 
              << " = " << (actual_fpp * 100) << "% (target: " << (TARGET_FPP * 100) << "%)" << std::endl;
}

// =============================================================================
// Cache Tests
// =============================================================================

TEST_F(BloomFilterTest, CacheGetOrLoad) {
    std::string parquet_path = (test_dir_ / "cached.parquet").string();
    
    // Create and save a filter
    BloomFilterManager writer_manager;
    writer_manager.CreateFilter(100, 0.01);
    writer_manager.AddSeriesId(12345);
    EXPECT_TRUE(writer_manager.SaveFilter(parquet_path));
    
    // First access should load from disk
    auto filter1 = BloomFilterCache::Instance().GetOrLoad(parquet_path);
    ASSERT_NE(filter1, nullptr);
    EXPECT_TRUE(filter1->IsValid());
    EXPECT_TRUE(filter1->MightContain(12345));
    
    // Second access should return cached version
    auto filter2 = BloomFilterCache::Instance().GetOrLoad(parquet_path);
    EXPECT_EQ(filter1.get(), filter2.get());  // Same pointer
    
    EXPECT_EQ(BloomFilterCache::Instance().Size(), 1);
}

TEST_F(BloomFilterTest, CacheInvalidate) {
    std::string parquet_path = (test_dir_ / "invalidate.parquet").string();
    
    // Create and save a filter
    BloomFilterManager writer_manager;
    writer_manager.CreateFilter(100, 0.01);
    writer_manager.AddSeriesId(12345);
    EXPECT_TRUE(writer_manager.SaveFilter(parquet_path));
    
    // Load into cache
    auto filter1 = BloomFilterCache::Instance().GetOrLoad(parquet_path);
    ASSERT_NE(filter1, nullptr);
    
    // Invalidate
    BloomFilterCache::Instance().Invalidate(parquet_path);
    EXPECT_EQ(BloomFilterCache::Instance().Size(), 0);
    
    // Next access should reload
    auto filter2 = BloomFilterCache::Instance().GetOrLoad(parquet_path);
    EXPECT_NE(filter1.get(), filter2.get());  // Different pointer
}

TEST_F(BloomFilterTest, CacheNonExistentReturnsNull) {
    auto filter = BloomFilterCache::Instance().GetOrLoad("/nonexistent.parquet");
    EXPECT_EQ(filter, nullptr);
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_F(BloomFilterTest, EmptyFilter) {
    BloomFilterManager manager;
    manager.CreateFilter(100, 0.01);
    
    // Don't add anything
    EXPECT_EQ(manager.GetEntriesAdded(), 0);
    
    // Should still be valid
    EXPECT_TRUE(manager.IsValid());
}

TEST_F(BloomFilterTest, LargeFilter) {
    BloomFilterManager manager;
    manager.CreateFilter(100000, 0.01);  // 100K entries
    
    EXPECT_TRUE(manager.IsValid());
    EXPECT_GT(manager.GetFilterSizeBytes(), 0);
    
    // Add many entries
    for (int i = 0; i < 10000; ++i) {
        manager.AddSeriesId(i);
    }
    
    EXPECT_EQ(manager.GetEntriesAdded(), 10000);
}

// =============================================================================
// Test: Bloom filter metrics fields exist in instrumentation
// =============================================================================

TEST_F(BloomFilterTest, MetricsFieldsExist) {
    // This test verifies that the ReadPerformanceInstrumentation
    // has the Bloom filter metrics fields we need.
    
    // Get reference to instrumentation singleton
    auto& instr = storage::ReadPerformanceInstrumentation::instance();
    
    // Reset stats
    instr.reset_stats();
    
    // Record some Bloom filter usage
    instr.recordBloomFilterUsage(true, 10.5);   // skip
    instr.recordBloomFilterUsage(false, 5.2);   // pass
    instr.recordBloomFilterUsage(true, 8.3);    // skip
    
    // Get stats and verify
    auto stats = instr.get_stats();
    
    EXPECT_EQ(stats.bloom_filter_checks, 3) 
        << "Should have 3 Bloom filter checks";
    EXPECT_EQ(stats.bloom_filter_skips, 2) 
        << "Should have 2 skips (series definitely not present)";
    EXPECT_EQ(stats.bloom_filter_passes, 1) 
        << "Should have 1 pass (series might be present)";
    EXPECT_NEAR(stats.bloom_filter_lookup_time_us, 24.0, 0.1) 
        << "Total lookup time should be ~24.0 µs";
    
    std::cout << "Bloom filter metrics test:" << std::endl;
    std::cout << "  checks: " << stats.bloom_filter_checks << std::endl;
    std::cout << "  skips: " << stats.bloom_filter_skips << std::endl;
    std::cout << "  passes: " << stats.bloom_filter_passes << std::endl;
    std::cout << "  lookup_time_us: " << stats.bloom_filter_lookup_time_us << std::endl;
}

TEST_F(BloomFilterTest, MetricsResetCorrectly) {
    auto& instr = storage::ReadPerformanceInstrumentation::instance();
    
    // Add some metrics
    instr.recordBloomFilterUsage(true, 100.0);
    instr.recordBloomFilterUsage(false, 50.0);
    
    auto stats_before = instr.get_stats();
    EXPECT_GT(stats_before.bloom_filter_checks, 0);
    
    // Reset
    instr.reset_stats();
    
    // Verify reset
    auto stats_after = instr.get_stats();
    EXPECT_EQ(stats_after.bloom_filter_checks, 0);
    EXPECT_EQ(stats_after.bloom_filter_skips, 0);
    EXPECT_EQ(stats_after.bloom_filter_passes, 0);
    EXPECT_EQ(stats_after.bloom_filter_lookup_time_us, 0.0);
}

}  // namespace test
}  // namespace parquet
}  // namespace storage
}  // namespace tsdb
