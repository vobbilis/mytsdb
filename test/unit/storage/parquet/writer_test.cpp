#include <gtest/gtest.h>
#include <arrow/api.h>
#include <arrow/io/file.h>
#include <parquet/arrow/reader.h>
#include <filesystem>
#include "tsdb/storage/parquet/writer.hpp"
#include "tsdb/storage/parquet/schema_mapper.hpp"
#include "tsdb/core/types.h"

using namespace tsdb::storage::parquet;
using namespace tsdb;

class ParquetWriterTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = std::filesystem::temp_directory_path() / "parquet_writer_test";
        std::filesystem::create_directories(test_dir_);
        test_file_ = test_dir_ / "test.parquet";
    }

    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
    }

    std::filesystem::path test_dir_;
    std::filesystem::path test_file_;
};

TEST_F(ParquetWriterTest, TestWriteAndRead) {
    // 1. Create data
    std::vector<tsdb::core::Sample> samples = {
        tsdb::core::Sample(1000, 1.1),
        tsdb::core::Sample(2000, 2.2),
        tsdb::core::Sample(3000, 3.3)
    };
    std::map<std::string, std::string> tags = {
        {"metric", "cpu"},
        {"host", "localhost"}
    };

    // 2. Convert to RecordBatch
    auto schema = SchemaMapper::GetArrowSchema();
    auto batch = SchemaMapper::ToRecordBatch(samples, tags);
    ASSERT_NE(batch, nullptr);

    // 3. Write to Parquet
    ParquetWriter writer;
    auto result = writer.Open(test_file_.string(), schema);
    ASSERT_TRUE(result.ok()) << result.error();

    result = writer.WriteBatch(batch);
    ASSERT_TRUE(result.ok()) << result.error();

    result = writer.Close();
    ASSERT_TRUE(result.ok()) << result.error();

    // 4. Verify file exists and has content
    ASSERT_TRUE(std::filesystem::exists(test_file_));
    ASSERT_GT(std::filesystem::file_size(test_file_), 0);

    // 5. Read back using Arrow Parquet Reader to verify content
    arrow::MemoryPool* pool = arrow::default_memory_pool();
    std::shared_ptr<arrow::io::RandomAccessFile> input;
    auto open_res = arrow::io::ReadableFile::Open(test_file_.string());
    ASSERT_TRUE(open_res.ok());
    input = *open_res;

    std::unique_ptr<::parquet::arrow::FileReader> reader;
    auto open_status = ::parquet::arrow::FileReader::Make(
        pool,
        ::parquet::ParquetFileReader::Open(input),
        &reader
    );
    ASSERT_TRUE(open_status.ok());

    std::shared_ptr<arrow::Table> table;
    auto read_status = reader->ReadTable(&table);
    ASSERT_TRUE(read_status.ok());

    ASSERT_EQ(table->num_rows(), 3);
    
    // Check Timestamp column (index 0)
    auto ts_col = std::static_pointer_cast<arrow::Int64Array>(table->column(0)->chunk(0));
    EXPECT_EQ(ts_col->Value(0), 1000);
    EXPECT_EQ(ts_col->Value(1), 2000);
    EXPECT_EQ(ts_col->Value(2), 3000);

    // Check Value column (index 1)
    auto val_col = std::static_pointer_cast<arrow::DoubleArray>(table->column(1)->chunk(0));
    EXPECT_EQ(val_col->Value(0), 1.1);
    EXPECT_EQ(val_col->Value(1), 2.2);
    EXPECT_EQ(val_col->Value(2), 3.3);
}
