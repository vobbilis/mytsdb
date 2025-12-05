#include <gtest/gtest.h>
#include <arrow/api.h>
#include <filesystem>
#include "tsdb/storage/parquet/reader.hpp"
#include "tsdb/storage/parquet/writer.hpp"
#include "tsdb/storage/parquet/schema_mapper.hpp"
#include "tsdb/core/types.h"

using namespace tsdb::storage::parquet;
using namespace tsdb;

class ParquetReaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = std::filesystem::temp_directory_path() / "parquet_reader_test";
        std::filesystem::create_directories(test_dir_);
        test_file_ = test_dir_ / "test_read.parquet";
    }

    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
    }

    std::filesystem::path test_dir_;
    std::filesystem::path test_file_;
};

TEST_F(ParquetReaderTest, TestReadBatch) {
    // 1. Create and Write Data
    std::vector<tsdb::core::Sample> samples = {
        tsdb::core::Sample(1000, 10.0),
        tsdb::core::Sample(2000, 20.0),
        tsdb::core::Sample(3000, 30.0)
    };
    std::map<std::string, std::string> tags = {
        {"metric", "memory"},
        {"host", "localhost"}
    };

    auto schema = SchemaMapper::GetArrowSchema();
    auto batch = SchemaMapper::ToRecordBatch(samples, tags);
    ASSERT_NE(batch, nullptr);

    ParquetWriter writer;
    ASSERT_TRUE(writer.Open(test_file_.string(), schema).ok());
    ASSERT_TRUE(writer.WriteBatch(batch).ok());
    ASSERT_TRUE(writer.Close().ok());

    // 2. Read Data using ParquetReader
    ParquetReader reader;
    auto open_result = reader.Open(test_file_.string());
    ASSERT_TRUE(open_result.ok()) << open_result.error();

    std::shared_ptr<arrow::RecordBatch> read_batch;
    auto read_result = reader.ReadBatch(&read_batch);
    if (!read_result.ok()) {
        FAIL() << "ReadBatch failed: " << read_result.error();
    }
    
    ASSERT_NE(read_batch, nullptr);
    ASSERT_EQ(read_batch->num_rows(), 3);

    // Verify content
    auto ts_col = std::static_pointer_cast<arrow::Int64Array>(read_batch->column(0));
    EXPECT_EQ(ts_col->Value(0), 1000);
    EXPECT_EQ(ts_col->Value(1), 2000);
    EXPECT_EQ(ts_col->Value(2), 3000);

    auto val_col = std::static_pointer_cast<arrow::DoubleArray>(read_batch->column(1));
    EXPECT_EQ(val_col->Value(0), 10.0);
    EXPECT_EQ(val_col->Value(1), 20.0);
    EXPECT_EQ(val_col->Value(2), 30.0);

    // 3. Read again (should be EOF)
    auto eof_result = reader.ReadBatch(&read_batch);
    ASSERT_TRUE(eof_result.ok());
    ASSERT_EQ(read_batch, nullptr);

    ASSERT_TRUE(reader.Close().ok());
}
