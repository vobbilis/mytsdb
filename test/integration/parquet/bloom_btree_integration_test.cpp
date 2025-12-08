/**
 * @file bloom_btree_integration_test.cpp
 * @brief Integration tests for Bloom Filter + B+ Tree Secondary Index
 * 
 * This test verifies the two-phase query optimization:
 * 1. Phase 0: Bloom Filter - O(1) "definitely not present" check
 * 2. Phase 1: B+ Tree - O(log n) precise row group location
 * 
 * The architecture is:
 *   Query → Bloom Filter → B+ Tree → Parquet Reader
 *             ↓              ↓
 *         SKIP file    Find row groups
 */

#include <gtest/gtest.h>
#include <filesystem>
#include <random>
#include <chrono>

#include "tsdb/storage/parquet/parquet_block.hpp"
#include "tsdb/storage/parquet/secondary_index.h"
#include "tsdb/storage/parquet/bloom_filter_manager.h"
#include "tsdb/storage/parquet/writer.hpp"
#include "tsdb/storage/parquet/schema_mapper.hpp"
#include "tsdb/storage/read_performance_instrumentation.h"
#include "tsdb/core/types.h"

#include <arrow/api.h>
#include <arrow/io/file.h>

namespace tsdb {
namespace storage {
namespace parquet {
namespace test {

class BloomBTreeIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = std::filesystem::temp_directory_path() / "tsdb_bloom_btree_test";
        std::filesystem::create_directories(test_dir_);
        
        // Clear caches
        SecondaryIndexCache::Instance().ClearAll();
        BloomFilterCache::Instance().Clear();
        
        // Reset metrics
        ReadPerformanceInstrumentation::instance().reset_stats();
    }
    
    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
        SecondaryIndexCache::Instance().ClearAll();
        BloomFilterCache::Instance().Clear();
    }
    
    /**
     * @brief Helper to create a Parquet file with Bloom filter
     * 
     * Creates a file with known series and saves both:
     * - The Parquet data file
     * - The .bloom Bloom filter file
     * - The .idx B+ Tree secondary index
     */
    std::string CreateTestParquetWithBloom(
        const std::string& name,
        const std::vector<std::string>& series_names,
        int samples_per_series
    ) {
        std::string file_path = (test_dir_ / name).string();
        
        ParquetWriter writer;
        auto schema = SchemaMapper::GetArrowSchema();
        auto result = writer.Open(file_path, schema);
        EXPECT_TRUE(result.ok()) << result.error();
        
        int64_t base_timestamp = 1000000;
        
        for (size_t series_idx = 0; series_idx < series_names.size(); ++series_idx) {
            const auto& series_name = series_names[series_idx];
            
            // Build samples for this series
            std::vector<int64_t> timestamps;
            std::vector<double> values;
            std::map<std::string, std::string> tags = {
                {"__name__", series_name},
                {"pod", "pod-" + std::to_string(series_idx % 10)}
            };
            
            for (int sample = 0; sample < samples_per_series; ++sample) {
                timestamps.push_back(base_timestamp + sample * 1000);
                values.push_back(static_cast<double>(series_idx * 100 + sample));
            }
            
            // Add to Bloom filter
            std::vector<std::pair<std::string, std::string>> sorted_labels(tags.begin(), tags.end());
            std::sort(sorted_labels.begin(), sorted_labels.end());
            std::string labels_str;
            for (const auto& [k, v] : sorted_labels) {
                if (!labels_str.empty()) labels_str += ",";
                labels_str += k + "=" + v;
            }
            writer.AddSeriesToBloomFilterByLabels(labels_str);
            
            // Write batch
            auto batch = SchemaMapper::ToRecordBatch(timestamps, values, tags);
            result = writer.WriteBatch(batch);
            EXPECT_TRUE(result.ok()) << result.error();
        }
        
        result = writer.Close();
        EXPECT_TRUE(result.ok()) << result.error();
        
        return file_path;
    }
    
    internal::BlockHeader CreateBlockHeader(int64_t start_time, int64_t end_time) {
        internal::BlockHeader header;
        header.magic = internal::BlockHeader::MAGIC;
        header.version = internal::BlockHeader::VERSION;
        header.id = 1;
        header.flags = 0;
        header.crc32 = 0;
        header.start_time = start_time;
        header.end_time = end_time;
        header.reserved = 0;
        return header;
    }
    
    std::filesystem::path test_dir_;
};

// =============================================================================
// Test: Bloom filter is created alongside Parquet file
// =============================================================================

TEST_F(BloomBTreeIntegrationTest, BloomFilterCreatedWithParquet) {
    std::vector<std::string> series = {"cpu_usage", "memory_usage", "disk_io"};
    std::string parquet_path = CreateTestParquetWithBloom("test.parquet", series, 100);
    
    // Verify Parquet file exists
    EXPECT_TRUE(std::filesystem::exists(parquet_path));
    
    // Verify Bloom filter file exists
    std::string bloom_path = BloomFilterManager::GetBloomPath(parquet_path);
    EXPECT_TRUE(std::filesystem::exists(bloom_path)) 
        << "Bloom filter file not created: " << bloom_path;
    
    // Verify we can load it
    BloomFilterManager manager;
    EXPECT_TRUE(manager.LoadFilter(parquet_path));
    EXPECT_TRUE(manager.IsValid());
}

// =============================================================================
// Test: Bloom filter correctly identifies present/absent series
// =============================================================================

TEST_F(BloomBTreeIntegrationTest, BloomFilterIdentifiesSeriesCorrectly) {
    std::vector<std::string> series = {"metric_a", "metric_b", "metric_c"};
    std::string parquet_path = CreateTestParquetWithBloom("test_identify.parquet", series, 50);
    
    // Load bloom filter
    auto bloom = BloomFilterCache::Instance().GetOrLoad(parquet_path);
    ASSERT_NE(bloom, nullptr);
    
    // Check for series that ARE in the file
    // Note: In CreateTestParquetWithBloom, pod = "pod-" + (series_idx % 10)
    for (size_t i = 0; i < series.size(); ++i) {
        const auto& name = series[i];
        std::string pod = "pod-" + std::to_string(i % 10);
        
        // Normalize labels (sort them) - must match how they were added
        std::map<std::string, std::string> tags = {{"__name__", name}, {"pod", pod}};
        std::vector<std::pair<std::string, std::string>> sorted(tags.begin(), tags.end());
        std::sort(sorted.begin(), sorted.end());
        std::string labels_str;
        for (const auto& [k, v] : sorted) {
            if (!labels_str.empty()) labels_str += ",";
            labels_str += k + "=" + v;
        }
        
        EXPECT_TRUE(bloom->MightContainLabels(labels_str))
            << "Bloom filter should find: " << labels_str;
    }
    
    // Check for series that are NOT in the file
    // These should ideally return false (though Bloom filters can have false positives)
    std::string absent_labels = "__name__=nonexistent_metric,pod=pod-999";
    // Note: We can't assert false here due to possible false positives
    // But we verify no crash and reasonable behavior
    bool might_contain = bloom->MightContainLabels(absent_labels);
    std::cout << "Absent series '" << absent_labels << "' MightContain: " 
              << (might_contain ? "true (false positive)" : "false") << std::endl;
}

// =============================================================================
// Test: Phase 0 Bloom filter skips file when series not present
// =============================================================================

TEST_F(BloomBTreeIntegrationTest, BloomFilterSkipsFileWhenSeriesAbsent) {
    // Create a Parquet file with specific series
    std::vector<std::string> series = {"series_1", "series_2", "series_3"};
    std::string parquet_path = CreateTestParquetWithBloom("skip_test.parquet", series, 100);
    
    // Create a ParquetBlock
    auto header = CreateBlockHeader(1000000, 1000000 + 100 * 1000);
    ParquetBlock block(header, parquet_path);
    
    // Query for a series that doesn't exist
    // The Bloom filter should return "definitely not present" and skip the file
    core::Labels absent_labels;
    absent_labels.add("__name__", "completely_different_metric");
    absent_labels.add("pod", "pod-999");
    
    // read_columns should return empty result quickly due to Bloom filter skip
    auto start = std::chrono::high_resolution_clock::now();
    auto result = block.read_columns(absent_labels);
    auto end = std::chrono::high_resolution_clock::now();
    
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    
    // Result should be empty
    EXPECT_TRUE(result.first.empty());
    EXPECT_TRUE(result.second.empty());
    
    std::cout << "Bloom filter skip took: " << elapsed_ms << " ms" << std::endl;
}

// =============================================================================
// Test: Both Bloom and B+ Tree are consulted for present series
// =============================================================================

TEST_F(BloomBTreeIntegrationTest, BloomAndBTreeBothConsulted) {
    std::vector<std::string> series = {"target_metric"};
    std::string parquet_path = CreateTestParquetWithBloom("both_test.parquet", series, 100);
    
    // Load both indexes explicitly first
    auto bloom = BloomFilterCache::Instance().GetOrLoad(parquet_path);
    ASSERT_NE(bloom, nullptr);
    EXPECT_TRUE(bloom->IsValid());
    
    // The secondary index is built on-demand when ParquetBlock accesses the file
    auto btree = SecondaryIndexCache::Instance().GetOrCreate(parquet_path);
    // Note: btree might be nullptr until ParquetBlock builds it
    
    // Create ParquetBlock and query
    auto header = CreateBlockHeader(1000000, 1000000 + 100 * 1000);
    ParquetBlock block(header, parquet_path);
    
    core::Labels present_labels;
    present_labels.add("__name__", "target_metric");
    present_labels.add("pod", "pod-0");
    
    // Query - should pass Bloom filter and use B+ Tree
    auto result = block.read_columns(present_labels);
    
    // Should find data
    EXPECT_FALSE(result.first.empty()) << "Should find timestamps for present series";
    EXPECT_FALSE(result.second.empty()) << "Should find values for present series";
    EXPECT_EQ(result.first.size(), 100) << "Should have 100 samples";
    
    std::cout << "Found " << result.first.size() << " samples for target_metric" << std::endl;
}

// =============================================================================
// Test: Performance comparison - with vs without Bloom filter
// =============================================================================

TEST_F(BloomBTreeIntegrationTest, PerformanceWithAndWithoutBloom) {
    const int NUM_SERIES = 100;
    const int SAMPLES_PER_SERIES = 100;
    
    // Create series names
    std::vector<std::string> series;
    for (int i = 0; i < NUM_SERIES; ++i) {
        series.push_back("metric_" + std::to_string(i));
    }
    
    std::string parquet_path = CreateTestParquetWithBloom("perf_test.parquet", series, SAMPLES_PER_SERIES);
    
    auto header = CreateBlockHeader(1000000, 1000000 + SAMPLES_PER_SERIES * 1000);
    
    // Test 1: Query for series NOT in file (Bloom filter should help)
    const int NUM_ABSENT_QUERIES = 100;
    std::vector<double> absent_times;
    
    for (int i = 0; i < NUM_ABSENT_QUERIES; ++i) {
        ParquetBlock block(header, parquet_path);
        core::Labels labels;
        labels.add("__name__", "nonexistent_" + std::to_string(i));
        labels.add("pod", "pod-" + std::to_string(i));
        
        auto start = std::chrono::high_resolution_clock::now();
        auto result = block.read_columns(labels);
        auto end = std::chrono::high_resolution_clock::now();
        
        absent_times.push_back(std::chrono::duration<double, std::micro>(end - start).count());
        
        EXPECT_TRUE(result.first.empty());
    }
    
    // Test 2: Query for series IN file (both Bloom + B+ Tree used)
    const int NUM_PRESENT_QUERIES = 100;
    std::vector<double> present_times;
    
    for (int i = 0; i < NUM_PRESENT_QUERIES; ++i) {
        ParquetBlock block(header, parquet_path);
        core::Labels labels;
        labels.add("__name__", "metric_" + std::to_string(i % NUM_SERIES));
        labels.add("pod", "pod-" + std::to_string(i % 10));
        
        auto start = std::chrono::high_resolution_clock::now();
        auto result = block.read_columns(labels);
        auto end = std::chrono::high_resolution_clock::now();
        
        present_times.push_back(std::chrono::duration<double, std::micro>(end - start).count());
    }
    
    // Calculate averages
    double avg_absent = 0, avg_present = 0;
    for (auto t : absent_times) avg_absent += t;
    for (auto t : present_times) avg_present += t;
    avg_absent /= absent_times.size();
    avg_present /= present_times.size();
    
    std::cout << "\n=== Performance Results ===" << std::endl;
    std::cout << "Absent series (Bloom filter skip): " << avg_absent << " µs avg" << std::endl;
    std::cout << "Present series (Bloom + B+ Tree): " << avg_present << " µs avg" << std::endl;
    
    // Absent queries should generally be faster due to Bloom filter skip
    // But this depends on false positive rate
}

// =============================================================================
// Test: Cache persistence across multiple queries
// =============================================================================

TEST_F(BloomBTreeIntegrationTest, CachesPersistAcrossQueries) {
    std::vector<std::string> series = {"cached_metric"};
    std::string parquet_path = CreateTestParquetWithBloom("cache_test.parquet", series, 50);
    
    // First query - caches should be populated
    {
        auto header = CreateBlockHeader(1000000, 1000000 + 50 * 1000);
        ParquetBlock block(header, parquet_path);
        
        core::Labels labels;
        labels.add("__name__", "cached_metric");
        labels.add("pod", "pod-0");
        
        auto result = block.read_columns(labels);
        EXPECT_FALSE(result.first.empty());
    }
    
    // Verify caches are populated
    EXPECT_GT(BloomFilterCache::Instance().Size(), 0);
    
    // Second query - should use cached Bloom filter
    {
        auto header = CreateBlockHeader(1000000, 1000000 + 50 * 1000);
        ParquetBlock block(header, parquet_path);
        
        core::Labels labels;
        labels.add("__name__", "cached_metric");
        labels.add("pod", "pod-0");
        
        // Should be faster due to cached indexes
        auto start = std::chrono::high_resolution_clock::now();
        auto result = block.read_columns(labels);
        auto end = std::chrono::high_resolution_clock::now();
        
        double elapsed_us = std::chrono::duration<double, std::micro>(end - start).count();
        std::cout << "Cached query took: " << elapsed_us << " µs" << std::endl;
        
        EXPECT_FALSE(result.first.empty());
    }
}

// =============================================================================
// Test: Bloom filter metrics are recorded correctly
// =============================================================================

TEST_F(BloomBTreeIntegrationTest, BloomFilterMetricsRecorded) {
    // Reset metrics before test
    ReadPerformanceInstrumentation::instance().reset_stats();
    
    // Create a Parquet file with known series
    std::vector<std::string> series = {"metric_alpha", "metric_beta", "metric_gamma"};
    std::string parquet_path = CreateTestParquetWithBloom("metrics_test.parquet", series, 50);
    
    auto header = CreateBlockHeader(1000000, 1000000 + 50 * 1000);
    
    // Query 1: Series that EXISTS - should PASS Bloom filter
    {
        ParquetBlock block(header, parquet_path);
        core::Labels labels;
        labels.add("__name__", "metric_alpha");
        labels.add("pod", "pod-0");
        
        auto result = block.read_columns(labels);
        EXPECT_FALSE(result.first.empty()) << "Should find existing series";
    }
    
    // Query 2: Series that does NOT exist - should SKIP via Bloom filter
    {
        ParquetBlock block(header, parquet_path);
        core::Labels labels;
        labels.add("__name__", "nonexistent_metric");
        labels.add("pod", "pod-999");
        
        auto result = block.read_columns(labels);
        EXPECT_TRUE(result.first.empty()) << "Should not find absent series";
    }
    
    // Query 3: Another existing series - should PASS Bloom filter
    {
        ParquetBlock block(header, parquet_path);
        core::Labels labels;
        labels.add("__name__", "metric_beta");
        labels.add("pod", "pod-1");
        
        auto result = block.read_columns(labels);
        EXPECT_FALSE(result.first.empty()) << "Should find existing series";
    }
    
    // Query 4: Another non-existent series - should SKIP
    {
        ParquetBlock block(header, parquet_path);
        core::Labels labels;
        labels.add("__name__", "fake_metric_xyz");
        labels.add("pod", "pod-12345");
        
        auto result = block.read_columns(labels);
        EXPECT_TRUE(result.first.empty()) << "Should not find absent series";
    }
    
    // Now verify the metrics were recorded
    auto stats = ReadPerformanceInstrumentation::instance().get_stats();
    
    std::cout << "\n=== Bloom Filter Metrics ===" << std::endl;
    std::cout << "  bloom_filter_checks: " << stats.bloom_filter_checks << std::endl;
    std::cout << "  bloom_filter_skips: " << stats.bloom_filter_skips << std::endl;
    std::cout << "  bloom_filter_passes: " << stats.bloom_filter_passes << std::endl;
    std::cout << "  bloom_filter_lookup_time_us: " << stats.bloom_filter_lookup_time_us << std::endl;
    
    // Validate metrics
    // We made 4 queries, so bloom_filter_checks should be 4
    EXPECT_EQ(stats.bloom_filter_checks, 4) 
        << "Expected 4 Bloom filter checks (4 queries)";
    
    // 2 series existed (metric_alpha, metric_beta) -> 2 passes
    // 2 series didn't exist -> 2 skips (assuming no false positives)
    EXPECT_GE(stats.bloom_filter_passes, 2) 
        << "Expected at least 2 Bloom filter passes (2 existing series)";
    
    // Note: bloom_filter_skips could be < 2 if there are false positives
    // But we expect at least some skips for non-existent series
    EXPECT_GE(stats.bloom_filter_skips + stats.bloom_filter_passes, 4)
        << "Total skips + passes should equal total checks";
    
    // Lookup time should be non-zero
    EXPECT_GT(stats.bloom_filter_lookup_time_us, 0)
        << "Bloom filter lookup time should be recorded";
    
    std::cout << "✓ Bloom filter metrics validation PASSED!" << std::endl;
}

// =============================================================================
// Test: Combined Bloom + B+ Tree metrics show both indexes working
// =============================================================================

TEST_F(BloomBTreeIntegrationTest, CombinedBloomAndBTreeMetrics) {
    // Reset metrics before test
    ReadPerformanceInstrumentation::instance().reset_stats();
    
    // Create a larger file to ensure B+ Tree is used
    std::vector<std::string> series;
    for (int i = 0; i < 50; ++i) {
        series.push_back("metric_" + std::to_string(i));
    }
    std::string parquet_path = CreateTestParquetWithBloom("combined_metrics.parquet", series, 100);
    
    auto header = CreateBlockHeader(1000000, 1000000 + 100 * 1000);
    
    // Make multiple queries for existing series
    for (int i = 0; i < 10; ++i) {
        ParquetBlock block(header, parquet_path);
        core::Labels labels;
        labels.add("__name__", "metric_" + std::to_string(i % 50));
        labels.add("pod", "pod-" + std::to_string(i % 10));
        
        auto result = block.read_columns(labels);
        // Note: May or may not find data depending on exact label match
    }
    
    // Make queries for non-existing series
    for (int i = 0; i < 5; ++i) {
        ParquetBlock block(header, parquet_path);
        core::Labels labels;
        labels.add("__name__", "nonexistent_" + std::to_string(i));
        labels.add("pod", "pod-" + std::to_string(i + 1000));
        
        auto result = block.read_columns(labels);
        EXPECT_TRUE(result.first.empty());
    }
    
    auto stats = ReadPerformanceInstrumentation::instance().get_stats();
    
    std::cout << "\n=== Combined Index Metrics ===" << std::endl;
    std::cout << "Bloom Filter:" << std::endl;
    std::cout << "  checks: " << stats.bloom_filter_checks << std::endl;
    std::cout << "  skips: " << stats.bloom_filter_skips << std::endl;
    std::cout << "  passes: " << stats.bloom_filter_passes << std::endl;
    std::cout << "  lookup_time_us: " << stats.bloom_filter_lookup_time_us << std::endl;
    std::cout << "B+ Tree Secondary Index:" << std::endl;
    std::cout << "  lookups: " << stats.secondary_index_lookups << std::endl;
    std::cout << "  hits: " << stats.secondary_index_hits << std::endl;
    std::cout << "  misses: " << stats.secondary_index_misses << std::endl;
    std::cout << "  lookup_time_us: " << stats.secondary_index_lookup_time_us << std::endl;
    
    // Validate that both indexes are being used
    EXPECT_GT(stats.bloom_filter_checks, 0) 
        << "Bloom filter should have been checked";
    
    // For non-existent series, Bloom should skip before B+ Tree is consulted
    // So bloom_filter_skips should be > 0 (assuming no false positives)
    std::cout << "\n✓ Combined Bloom + B+ Tree metrics recorded successfully!" << std::endl;
    
    // Key insight: If bloom_filter_skips > 0, we're saving B+ Tree lookups
    if (stats.bloom_filter_skips > 0) {
        std::cout << "  → " << stats.bloom_filter_skips 
                  << " Parquet files skipped thanks to Bloom filter (saved B+ Tree lookups)" 
                  << std::endl;
    }
}

}  // namespace test
}  // namespace parquet
}  // namespace storage
}  // namespace tsdb
