/**
 * @file secondary_index_test.cpp
 * @brief Unit tests for Parquet Secondary Index (B+ Tree)
 * 
 * Phase A: B+ Tree Secondary Index for O(log n) series lookup
 */

#include <gtest/gtest.h>
#include "tsdb/storage/parquet/secondary_index.h"
#include "tsdb/storage/read_performance_instrumentation.h"
#include <random>
#include <chrono>
#include "test_util/temp_dir.h"
#include <arrow/api.h>
#include <arrow/io/file.h>
#include <parquet/arrow/writer.h>

namespace tsdb {
namespace storage {
namespace parquet {
namespace test {

class SecondaryIndexTest : public ::testing::Test {
protected:
    void SetUp() override {
        index_ = std::make_unique<SecondaryIndex>();
    }
    
    void TearDown() override {
        index_.reset();
    }
    
    std::unique_ptr<SecondaryIndex> index_;
};

// Basic functionality tests
TEST_F(SecondaryIndexTest, EmptyIndexReturnsNoResults) {
    EXPECT_TRUE(index_->Empty());
    EXPECT_EQ(index_->Size(), 0);
    
    auto locations = index_->Lookup(12345);
    EXPECT_TRUE(locations.empty());
}

TEST_F(SecondaryIndexTest, InsertAndLookup) {
    core::SeriesID series_id = 100;
    RowLocation loc(0, 0, 1000, 2000);
    
    index_->Insert(series_id, loc);
    
    EXPECT_FALSE(index_->Empty());
    EXPECT_EQ(index_->Size(), 1);
    EXPECT_TRUE(index_->Contains(series_id));
    
    auto locations = index_->Lookup(series_id);
    ASSERT_EQ(locations.size(), 1);
    EXPECT_EQ(locations[0].row_group_id, 0);
    EXPECT_EQ(locations[0].min_timestamp, 1000);
    EXPECT_EQ(locations[0].max_timestamp, 2000);
}

TEST_F(SecondaryIndexTest, MultipleSeries) {
    for (int i = 0; i < 100; ++i) {
        core::SeriesID series_id = 1000 + i;
        RowLocation loc(i % 10, i * 100, i * 1000, (i + 1) * 1000);
        index_->Insert(series_id, loc);
    }
    
    EXPECT_EQ(index_->Size(), 100);
    
    // Verify each series
    for (int i = 0; i < 100; ++i) {
        core::SeriesID series_id = 1000 + i;
        EXPECT_TRUE(index_->Contains(series_id));
        
        auto locations = index_->Lookup(series_id);
        ASSERT_EQ(locations.size(), 1);
        EXPECT_EQ(locations[0].row_group_id, i % 10);
    }
}

TEST_F(SecondaryIndexTest, SeriesSpanningMultipleRowGroups) {
    core::SeriesID series_id = 42;
    
    // Insert same series across multiple row groups
    index_->Insert(series_id, RowLocation(0, 0, 1000, 2000));
    index_->Insert(series_id, RowLocation(1, 0, 2000, 3000));
    index_->Insert(series_id, RowLocation(2, 0, 3000, 4000));
    
    EXPECT_EQ(index_->Size(), 1);  // Still one series
    EXPECT_EQ(index_->TotalLocations(), 3);  // But 3 locations
    
    auto locations = index_->Lookup(series_id);
    ASSERT_EQ(locations.size(), 3);
}

TEST_F(SecondaryIndexTest, TimeRangeFiltering) {
    core::SeriesID series_id = 42;
    
    // Insert same series across multiple row groups with different time ranges
    index_->Insert(series_id, RowLocation(0, 0, 1000, 2000));
    index_->Insert(series_id, RowLocation(1, 0, 2000, 3000));
    index_->Insert(series_id, RowLocation(2, 0, 3000, 4000));
    
    // Query that spans all row groups
    auto all = index_->LookupInTimeRange(series_id, 1000, 4000);
    EXPECT_EQ(all.size(), 3);
    
    // Query that only hits first row group
    auto first = index_->LookupInTimeRange(series_id, 1000, 1500);
    EXPECT_EQ(first.size(), 1);
    EXPECT_EQ(first[0].row_group_id, 0);
    
    // Query that hits middle row group
    auto middle = index_->LookupInTimeRange(series_id, 2500, 2500);
    EXPECT_EQ(middle.size(), 1);
    EXPECT_EQ(middle[0].row_group_id, 1);
    
    // Query that hits last two row groups
    auto last_two = index_->LookupInTimeRange(series_id, 2500, 4000);
    EXPECT_EQ(last_two.size(), 2);
    
    // Query outside range
    auto none = index_->LookupInTimeRange(series_id, 5000, 6000);
    EXPECT_TRUE(none.empty());
}

TEST_F(SecondaryIndexTest, Clear) {
    for (int i = 0; i < 10; ++i) {
        index_->Insert(i, RowLocation(0, 0, 0, 100));
    }
    EXPECT_EQ(index_->Size(), 10);
    
    index_->Clear();
    EXPECT_TRUE(index_->Empty());
    EXPECT_EQ(index_->Size(), 0);
}

TEST_F(SecondaryIndexTest, GetAllSeriesIDs) {
    std::vector<core::SeriesID> expected;
    for (int i = 0; i < 5; ++i) {
        core::SeriesID id = 100 + i;
        expected.push_back(id);
        index_->Insert(id, RowLocation(0, 0, 0, 100));
    }
    
    auto result = index_->GetAllSeriesIDs();
    EXPECT_EQ(result.size(), expected.size());
    
    // Order is not guaranteed (hash map); compare as sets.
    std::sort(result.begin(), result.end());
    std::sort(expected.begin(), expected.end());
    EXPECT_EQ(result, expected);
}

// Performance tests
TEST_F(SecondaryIndexTest, LookupPerformance) {
    // Insert 10,000 series
    const int num_series = 10000;
    std::vector<core::SeriesID> series_ids;
    
    for (int i = 0; i < num_series; ++i) {
        core::SeriesID id = static_cast<core::SeriesID>(i);
        series_ids.push_back(id);
        index_->Insert(id, RowLocation(i % 100, i, i * 1000, (i + 1) * 1000));
    }
    
    // Measure lookup time for 1000 random lookups
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, num_series - 1);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 1000; ++i) {
        auto id = series_ids[dist(gen)];
        auto locations = index_->Lookup(id);
        EXPECT_FALSE(locations.empty());  // Should always find
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    
    // Average lookup should be < 10us for O(log n) performance
    double avg_lookup_us = static_cast<double>(duration_us) / 1000;
    std::cout << "Average lookup time: " << avg_lookup_us << " us" << std::endl;
    EXPECT_LT(avg_lookup_us, 100.0);  // Should be well under 100us
}

TEST_F(SecondaryIndexTest, IndexStats) {
    for (int i = 0; i < 100; ++i) {
        index_->Insert(i, RowLocation(i % 10, i, i * 1000, (i + 1) * 1000));
    }
    
    auto stats = index_->GetStats();
    EXPECT_EQ(stats.num_series, 100);
    EXPECT_GE(stats.memory_bytes, 0);  // Should track memory
}

// Persistence tests
TEST_F(SecondaryIndexTest, SaveAndLoad) {
    // Insert test data
    for (int i = 0; i < 50; ++i) {
        index_->Insert(i * 100, RowLocation(i, i * 10, i * 1000, (i + 1) * 1000));
    }
    
    std::string test_file = "/tmp/secondary_index_test.idx";
    
    // Save to file
    ASSERT_TRUE(index_->SaveToFile(test_file));
    
    // Create new index and load
    SecondaryIndex loaded_index;
    ASSERT_TRUE(loaded_index.LoadFromFile(test_file));
    
    // Verify loaded data matches
    EXPECT_EQ(loaded_index.Size(), index_->Size());
    
    for (int i = 0; i < 50; ++i) {
        core::SeriesID id = i * 100;
        auto orig = index_->Lookup(id);
        auto loaded = loaded_index.Lookup(id);
        
        ASSERT_EQ(orig.size(), loaded.size());
        EXPECT_EQ(orig[0].row_group_id, loaded[0].row_group_id);
        EXPECT_EQ(orig[0].min_timestamp, loaded[0].min_timestamp);
        EXPECT_EQ(orig[0].max_timestamp, loaded[0].max_timestamp);
    }
    
    // Cleanup
    std::remove(test_file.c_str());
}

TEST_F(SecondaryIndexTest, LoadNonexistentFile) {
    EXPECT_FALSE(index_->LoadFromFile("/tmp/nonexistent_index.idx"));
}

TEST_F(SecondaryIndexTest, BuildFromParquetUsesRowGroupSpecificTimeBounds) {
    // This test is designed to fail under the old behavior where each RowLocation
    // gets a series-wide min/max instead of row-group-specific bounds.

    auto dir = tsdb::testutil::MakeUniqueTestDir("secondary_index_rg_bounds");
    std::filesystem::create_directories(dir);
    auto file_path = (dir / "rg_bounds.parquet").string();

    // Build a single series that spans two row groups with disjoint timestamp ranges.
    arrow::Int64Builder ts_builder;
    arrow::DoubleBuilder val_builder;
    auto pool = arrow::default_memory_pool();
    arrow::MapBuilder tags_builder(pool,
        std::make_shared<arrow::StringBuilder>(pool),
        std::make_shared<arrow::StringBuilder>(pool));

    auto append_row = [&](int64_t ts, double val) {
        ASSERT_TRUE(ts_builder.Append(ts).ok());
        ASSERT_TRUE(val_builder.Append(val).ok());

        ASSERT_TRUE(tags_builder.Append().ok());
        auto key_builder = dynamic_cast<arrow::StringBuilder*>(tags_builder.key_builder());
        auto item_builder = dynamic_cast<arrow::StringBuilder*>(tags_builder.item_builder());
        ASSERT_NE(key_builder, nullptr);
        ASSERT_NE(item_builder, nullptr);

        ASSERT_TRUE(key_builder->Append("__name__").ok());
        ASSERT_TRUE(item_builder->Append("rg_metric").ok());
        ASSERT_TRUE(key_builder->Append("instance").ok());
        ASSERT_TRUE(item_builder->Append("host1").ok());
    };

    // Force 2 row groups by using chunk_size == first_rg_rows and writing > chunk_size rows.
    const int64_t first_rg_rows = 1024;
    for (int64_t i = 0; i < first_rg_rows; ++i) {
        append_row(1'000'000 + i, static_cast<double>(i));
    }
    for (int64_t i = 0; i < first_rg_rows; ++i) {
        append_row(5'000'000 + i, static_cast<double>(i));
    }

    std::shared_ptr<arrow::Array> ts_array, val_array, tags_array;
    ASSERT_TRUE(ts_builder.Finish(&ts_array).ok());
    ASSERT_TRUE(val_builder.Finish(&val_array).ok());
    ASSERT_TRUE(tags_builder.Finish(&tags_array).ok());

    auto schema = arrow::schema({
        arrow::field("timestamp", arrow::int64(), false),
        arrow::field("value", arrow::float64(), false),
        arrow::field("tags", arrow::map(arrow::utf8(), arrow::utf8()), true),
    });

    auto table = arrow::Table::Make(schema, {ts_array, val_array, tags_array});
    auto outfile_result = arrow::io::FileOutputStream::Open(file_path);
    ASSERT_TRUE(outfile_result.ok());
    auto outfile = *outfile_result;
    ASSERT_TRUE(::parquet::arrow::WriteTable(*table, arrow::default_memory_pool(), outfile, /*chunk_size=*/first_rg_rows).ok());

    SecondaryIndex idx;
    ASSERT_TRUE(idx.BuildFromParquetFile(file_path));

    // Compute the series id exactly like BuildFromParquetFile does: sort labels then hash "k=v,k=v".
    std::vector<std::pair<std::string, std::string>> pairs = {
        {"__name__", "rg_metric"},
        {"instance", "host1"},
    };
    std::sort(pairs.begin(), pairs.end());
    std::string labels_str;
    for (const auto& [k, v] : pairs) {
        if (!labels_str.empty()) labels_str += ",";
        labels_str += k + "=" + v;
    }
    core::SeriesID series_id = std::hash<std::string>{}(labels_str);

    auto early = idx.LookupInTimeRange(series_id, 1'000'000, 1'000'100);
    ASSERT_EQ(early.size(), 1);

    auto late = idx.LookupInTimeRange(series_id, 5'000'000, 5'000'100);
    ASSERT_EQ(late.size(), 1);

    // The old (series-wide) behavior would return both row groups for both queries.
    EXPECT_NE(early[0].row_group_id, late[0].row_group_id);
}

// Cache tests
TEST(SecondaryIndexCacheTest, SingletonAccess) {
    auto& cache1 = SecondaryIndexCache::Instance();
    auto& cache2 = SecondaryIndexCache::Instance();
    EXPECT_EQ(&cache1, &cache2);
}

TEST(SecondaryIndexCacheTest, CacheStats) {
    auto& cache = SecondaryIndexCache::Instance();
    cache.ClearAll();
    
    auto stats = cache.GetStats();
    EXPECT_EQ(stats.num_cached_indices, 0);
}

// ============================================================================
// Self-Monitoring Metrics Tests
// ============================================================================

class SecondaryIndexMetricsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset instrumentation stats before each test
        storage::ReadPerformanceInstrumentation::instance().reset_stats();
    }
};

TEST_F(SecondaryIndexMetricsTest, ReadMetricsHasSecondaryIndexFields) {
    storage::ReadPerformanceInstrumentation::ReadMetrics m;
    
    // Verify default values
    EXPECT_FALSE(m.secondary_index_used);
    EXPECT_EQ(m.secondary_index_lookup_us, 0.0);
    EXPECT_EQ(m.secondary_index_build_us, 0.0);
    EXPECT_EQ(m.secondary_index_hits, 0);
    EXPECT_EQ(m.secondary_index_row_groups_selected, 0);
    
    // Set values
    m.secondary_index_used = true;
    m.secondary_index_lookup_us = 100.5;
    m.secondary_index_build_us = 500.0;
    m.secondary_index_hits = 5;
    m.secondary_index_row_groups_selected = 3;
    
    // Verify values set correctly
    EXPECT_TRUE(m.secondary_index_used);
    EXPECT_DOUBLE_EQ(m.secondary_index_lookup_us, 100.5);
    EXPECT_DOUBLE_EQ(m.secondary_index_build_us, 500.0);
    EXPECT_EQ(m.secondary_index_hits, 5);
    EXPECT_EQ(m.secondary_index_row_groups_selected, 3);
    
    // Test reset
    m.reset();
    EXPECT_FALSE(m.secondary_index_used);
    EXPECT_EQ(m.secondary_index_lookup_us, 0.0);
    EXPECT_EQ(m.secondary_index_hits, 0);
}

TEST_F(SecondaryIndexMetricsTest, MetricsToStringIncludesSecondaryIndex) {
    storage::ReadPerformanceInstrumentation::ReadMetrics m;
    m.secondary_index_used = true;
    m.secondary_index_lookup_us = 50.0;
    m.secondary_index_hits = 1;
    m.secondary_index_row_groups_selected = 2;
    
    std::string str = m.to_string();
    
    // Verify secondary index info is in the string
    EXPECT_NE(str.find("SecondaryIdx: Yes"), std::string::npos);
    EXPECT_NE(str.find("lookup:"), std::string::npos);
    EXPECT_NE(str.find("hits: 1"), std::string::npos);
    EXPECT_NE(str.find("rg_selected: 2"), std::string::npos);
}

TEST_F(SecondaryIndexMetricsTest, MetricsToStringShowsNoWhenNotUsed) {
    storage::ReadPerformanceInstrumentation::ReadMetrics m;
    m.secondary_index_used = false;
    
    std::string str = m.to_string();
    
    // Verify it shows "No" when not used
    EXPECT_NE(str.find("SecondaryIdx: No"), std::string::npos);
}

TEST_F(SecondaryIndexMetricsTest, RecordReadAggregatesSecondaryIndexMetrics) {
    auto& instr = storage::ReadPerformanceInstrumentation::instance();
    
    // Record a read that used the secondary index
    {
        storage::ReadPerformanceInstrumentation::ReadMetrics m;
        m.secondary_index_used = true;
        m.secondary_index_lookup_us = 100.0;
        m.secondary_index_hits = 1;
        m.secondary_index_row_groups_selected = 5;
        m.row_groups_total = 100;
        instr.record_read(m);
    }
    
    // Record another read that used the secondary index
    {
        storage::ReadPerformanceInstrumentation::ReadMetrics m;
        m.secondary_index_used = true;
        m.secondary_index_lookup_us = 200.0;
        m.secondary_index_hits = 2;
        m.secondary_index_row_groups_selected = 3;
        m.row_groups_total = 100;
        instr.record_read(m);
    }
    
    // Record a read that did NOT use the secondary index
    {
        storage::ReadPerformanceInstrumentation::ReadMetrics m;
        m.secondary_index_used = false;
        m.row_groups_total = 50;  // Had row groups but didn't use index
        instr.record_read(m);
    }
    
    // Get aggregate stats
    auto stats = instr.get_stats();
    
    // Verify aggregation
    EXPECT_EQ(stats.secondary_index_lookups, 2);  // Two reads used the index
    EXPECT_EQ(stats.secondary_index_hits, 3);     // 1 + 2
    EXPECT_EQ(stats.secondary_index_misses, 1);   // One read didn't use it
    EXPECT_DOUBLE_EQ(stats.secondary_index_lookup_time_us, 300.0);  // 100 + 200
    EXPECT_EQ(stats.secondary_index_row_groups_selected, 8);  // 5 + 3
}

TEST_F(SecondaryIndexMetricsTest, AggregateStatsHasAllSecondaryIndexFields) {
    auto& instr = storage::ReadPerformanceInstrumentation::instance();
    auto stats = instr.get_stats();
    
    // Just verify the fields exist and are accessible
    // After reset, they should all be 0
    EXPECT_EQ(stats.secondary_index_lookups, 0);
    EXPECT_EQ(stats.secondary_index_hits, 0);
    EXPECT_EQ(stats.secondary_index_misses, 0);
    EXPECT_DOUBLE_EQ(stats.secondary_index_lookup_time_us, 0.0);
    EXPECT_DOUBLE_EQ(stats.secondary_index_build_time_us, 0.0);
    EXPECT_EQ(stats.secondary_index_row_groups_selected, 0);
}

TEST_F(SecondaryIndexMetricsTest, ResetStatsResetsSecondaryIndexMetrics) {
    auto& instr = storage::ReadPerformanceInstrumentation::instance();
    
    // Record some metrics
    storage::ReadPerformanceInstrumentation::ReadMetrics m;
    m.secondary_index_used = true;
    m.secondary_index_lookup_us = 100.0;
    m.secondary_index_hits = 5;
    m.row_groups_total = 100;
    instr.record_read(m);
    
    // Verify we have data
    auto before = instr.get_stats();
    EXPECT_GT(before.secondary_index_lookups, 0);
    
    // Reset
    instr.reset_stats();
    
    // Verify reset worked
    auto after = instr.get_stats();
    EXPECT_EQ(after.secondary_index_lookups, 0);
    EXPECT_EQ(after.secondary_index_hits, 0);
    EXPECT_EQ(after.secondary_index_misses, 0);
    EXPECT_DOUBLE_EQ(after.secondary_index_lookup_time_us, 0.0);
}

}  // namespace test
}  // namespace parquet
}  // namespace storage
}  // namespace tsdb
