#include <gtest/gtest.h>
#include <filesystem>
#include <vector>
#include "tsdb/storage/parquet/writer.hpp"
#include "tsdb/storage/parquet/parquet_block.hpp"
#include "tsdb/storage/parquet/schema_mapper.hpp"
#include "tsdb/storage/read_performance_instrumentation.h"
#include "tsdb/core/types.h"

namespace tsdb {
namespace storage {
namespace parquet {

class PruningTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "test_pruning_" + std::to_string(std::time(nullptr));
        std::filesystem::create_directory(test_dir_);
        file_path_ = test_dir_ + "/test.parquet";
        
        // Enable instrumentation
        ReadPerformanceInstrumentation::instance().enable();
        ReadPerformanceInstrumentation::instance().reset_stats();
    }

    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
    }

    std::string test_dir_;
    std::string file_path_;
};

TEST_F(PruningTest, TestTimeBasedPruning) {
    // 1. Create a Parquet file with 3 RowGroups with distinct time ranges
    {
        ParquetWriter writer;
        auto schema = SchemaMapper::GetArrowSchema();
        // Set max_row_group_length to 1 to force a new row group for every batch
        ASSERT_TRUE(writer.Open(file_path_, schema, 1).ok());

        core::Labels labels;
        labels.add("metric", "test");

        // RG1: Time 1000-2000
        {
            std::vector<core::Sample> samples;
            samples.emplace_back(1000, 1.0);
            samples.emplace_back(2000, 1.0);
            auto batch = SchemaMapper::ToRecordBatch(samples, labels.map());
            ASSERT_TRUE(writer.WriteBatch(batch).ok());
        }

        // RG2: Time 3000-4000
        {
            std::vector<core::Sample> samples;
            samples.emplace_back(3000, 2.0);
            samples.emplace_back(4000, 2.0);
            auto batch = SchemaMapper::ToRecordBatch(samples, labels.map());
            ASSERT_TRUE(writer.WriteBatch(batch).ok());
        }

        // RG3: Time 5000-6000
        {
            std::vector<core::Sample> samples;
            samples.emplace_back(5000, 3.0);
            samples.emplace_back(6000, 3.0);
            auto batch = SchemaMapper::ToRecordBatch(samples, labels.map());
            ASSERT_TRUE(writer.WriteBatch(batch).ok());
        }

        ASSERT_TRUE(writer.Close().ok());
    }

    // 2. Query for time range 2500-4500
    // Should skip RG1 (end 2000 < 2500)
    // Should read RG2 (overlap)
    // Should skip RG3 (start 5000 > 4500)
    
    internal::BlockHeader header;
    header.start_time = 0;
    header.end_time = 7000;
    
    ParquetBlock block(header, file_path_);
    
    std::vector<std::pair<std::string, std::string>> matchers;
    matchers.emplace_back("metric", "test");
    
    // Reset metrics before query
    ReadPerformanceInstrumentation::ReadMetrics metrics_snapshot; // To capture what was recorded
    // We need to capture the metrics recorded by the block.
    // Since ReadPerformanceInstrumentation aggregates, we can just check the aggregate stats.
    ReadPerformanceInstrumentation::instance().reset_stats();
    
    auto result = block.query(matchers, 2500, 4500);
    
    // 3. Verify result correctness
    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result[0].samples().size(), 2); // Only samples from RG2
    EXPECT_EQ(result[0].samples()[0].timestamp(), 3000);
    EXPECT_EQ(result[0].samples()[1].timestamp(), 4000);
    
    // 4. Verify instrumentation metrics
    auto stats = ReadPerformanceInstrumentation::instance().get_stats();
    
    // We expect:
    // - Total row groups: 6 (2 per batch due to max_row_group_length=1)
    // - Pruned by time: 4 (RG1,2 and RG5,6)
    // - Read: 2 (RG3,4)
    
    EXPECT_EQ(stats.row_groups_total, 6);
    EXPECT_EQ(stats.row_groups_pruned_time, 4);
    EXPECT_EQ(stats.row_groups_read, 2);
    EXPECT_GT(stats.bytes_skipped, 0);
    EXPECT_GT(stats.bytes_read, 0);
}

} // namespace parquet
} // namespace storage
} // namespace tsdb
