#include <gtest/gtest.h>
#include <filesystem>
#include <vector>
#include "tsdb/storage/parquet/writer.hpp"
#include "tsdb/storage/parquet/parquet_block.hpp"
#include "tsdb/storage/parquet/schema_mapper.hpp"
#include "tsdb/core/types.h"

namespace tsdb {
namespace storage {
namespace parquet {

class PredicatePushdownTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "test_predicate_pushdown_" + std::to_string(std::time(nullptr));
        std::filesystem::create_directory(test_dir_);
        file_path_ = test_dir_ + "/test.parquet";
    }

    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
    }

    std::string test_dir_;
    std::string file_path_;
};

TEST_F(PredicatePushdownTest, TestFiltering) {
    // 1. Create a Parquet file with 3 RowGroups (one per series)
    {
        ParquetWriter writer;
        auto schema = SchemaMapper::GetArrowSchema();
        // Set max_row_group_length to 1 to force a new row group for every row (series)
        ASSERT_TRUE(writer.Open(file_path_, schema, 1).ok());

        // Series 1: metric=test, job=a
        {
            core::Labels labels;
            labels.add("metric", "test");
            labels.add("job", "a");
            
            std::vector<core::Sample> samples;
            samples.emplace_back(1000, 1.0);
            
            auto batch = SchemaMapper::ToRecordBatch(samples, labels.map());
            ASSERT_TRUE(batch != nullptr);
            auto write_res = writer.WriteBatch(batch);
            ASSERT_TRUE(write_res.ok());
        }

        // Series 2: metric=test, job=b
        {
            core::Labels labels;
            labels.add("metric", "test");
            labels.add("job", "b");
            
            std::vector<core::Sample> samples;
            samples.emplace_back(1000, 2.0);
            
            auto batch = SchemaMapper::ToRecordBatch(samples, labels.map());
            ASSERT_TRUE(batch != nullptr);
            auto write_res = writer.WriteBatch(batch);
            ASSERT_TRUE(write_res.ok());
        }

        // Series 3: metric=test, job=c
        {
            core::Labels labels;
            labels.add("metric", "test");
            labels.add("job", "c");
            
            std::vector<core::Sample> samples;
            samples.emplace_back(1000, 3.0);
            
            auto batch = SchemaMapper::ToRecordBatch(samples, labels.map());
            ASSERT_TRUE(batch != nullptr);
            auto write_res = writer.WriteBatch(batch);
            ASSERT_TRUE(write_res.ok());
        }

        ASSERT_TRUE(writer.Close().ok());
    }

    // 2. Query for job=b
    internal::BlockHeader header;
    header.start_time = 0;
    header.end_time = 2000;
    
    ParquetBlock block(header, file_path_);
    
    std::vector<std::pair<std::string, std::string>> matchers;
    matchers.emplace_back("job", "b");
    
    auto result = block.query(matchers, 0, 2000);
    
    // 3. Verify result
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].labels().get("job").value(), "b");
    EXPECT_EQ(result[0].samples()[0].value(), 2.0);
    
    // 4. Verify implicit optimization
    // We can't easily check if row groups were skipped without instrumentation,
    // but we can ensure the result is correct.
    // If we implement logging in ParquetBlock, we could capture stdout, but that's flaky.
    // For now, correctness is the primary check.
}

} // namespace parquet
} // namespace storage
} // namespace tsdb
