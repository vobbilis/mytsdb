#include <gtest/gtest.h>
#include <arrow/api.h>
#include <arrow/io/file.h>
#include <parquet/arrow/writer.h>
#include <parquet/properties.h>
#include <filesystem>
#include <random>
#include "tsdb/storage/parquet/reader.hpp"
#include "tsdb/storage/parquet/writer.hpp"
#include "tsdb/storage/parquet/schema_mapper.hpp"
#include "tsdb/core/types.h"

using namespace tsdb::storage::parquet;
using namespace tsdb;

class ParquetAdvancedTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = std::filesystem::temp_directory_path() / "parquet_advanced_test";
        std::filesystem::create_directories(test_dir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
    }

    std::filesystem::path test_dir_;
};

TEST_F(ParquetAdvancedTest, TestMultipleBatches) {
    std::filesystem::path file_path = test_dir_ / "multi_batch.parquet";
    auto schema = SchemaMapper::GetArrowSchema();
    
    // Use direct Arrow/Parquet writer to force small row groups
    auto outfile = *arrow::io::FileOutputStream::Open(file_path.string());
    parquet::WriterProperties::Builder builder;
    builder.max_row_group_length(1); // Force new row group every row
    auto writer_props = builder.build();
    
    std::unique_ptr<parquet::arrow::FileWriter> writer;
    auto writer_result = parquet::arrow::FileWriter::Open(*schema, arrow::default_memory_pool(), outfile, writer_props, parquet::default_arrow_writer_properties());
    ASSERT_TRUE(writer_result.ok());
    writer = std::move(*writer_result);

    // Write 3 batches
    for (int i = 0; i < 3; ++i) {
        std::vector<tsdb::core::Sample> samples = {
            tsdb::core::Sample(1000 * (i + 1), 10.0 * (i + 1))
        };
        std::map<std::string, std::string> tags = {{"batch", std::to_string(i)}};
        auto batch = SchemaMapper::ToRecordBatch(samples, tags);
        ASSERT_TRUE(writer->WriteRecordBatch(*batch).ok());
    }
    ASSERT_TRUE(writer->Close().ok());
    ASSERT_TRUE(outfile->Close().ok());

    // Read back
    ParquetReader reader;
    ASSERT_TRUE(reader.Open(file_path.string()).ok());

    for (int i = 0; i < 3; ++i) {
        std::shared_ptr<arrow::RecordBatch> batch;
        ASSERT_TRUE(reader.ReadBatch(&batch).ok());
        ASSERT_NE(batch, nullptr);
        ASSERT_EQ(batch->num_rows(), 1);
        
        auto ts_col = std::static_pointer_cast<arrow::Int64Array>(batch->column(0));
        EXPECT_EQ(ts_col->Value(0), 1000 * (i + 1));
    }

    // EOF
    std::shared_ptr<arrow::RecordBatch> batch;
    ASSERT_TRUE(reader.ReadBatch(&batch).ok());
    ASSERT_EQ(batch, nullptr);
    ASSERT_TRUE(reader.Close().ok());
}

TEST_F(ParquetAdvancedTest, TestLargeVolume) {
    std::filesystem::path file_path = test_dir_ / "large_volume.parquet";
    auto schema = SchemaMapper::GetArrowSchema();
    ParquetWriter writer;
    ASSERT_TRUE(writer.Open(file_path.string(), schema).ok());

    // Write 10k samples in one batch
    int num_samples = 10000;
    std::vector<tsdb::core::Sample> samples;
    samples.reserve(num_samples);
    for (int i = 0; i < num_samples; ++i) {
        samples.emplace_back(1000 + i, static_cast<double>(i));
    }
    std::map<std::string, std::string> tags = {{"metric", "large_test"}};
    
    auto batch = SchemaMapper::ToRecordBatch(samples, tags);
    ASSERT_TRUE(writer.WriteBatch(batch).ok());
    ASSERT_TRUE(writer.Close().ok());

    // Read back
    ParquetReader reader;
    ASSERT_TRUE(reader.Open(file_path.string()).ok());
    
    std::shared_ptr<arrow::RecordBatch> read_batch;
    ASSERT_TRUE(reader.ReadBatch(&read_batch).ok());
    ASSERT_NE(read_batch, nullptr);
    ASSERT_EQ(read_batch->num_rows(), num_samples);

    auto val_col = std::static_pointer_cast<arrow::DoubleArray>(read_batch->column(1));
    EXPECT_EQ(val_col->Value(0), 0.0);
    EXPECT_EQ(val_col->Value(num_samples - 1), static_cast<double>(num_samples - 1));

    ASSERT_TRUE(reader.Close().ok());
}

TEST_F(ParquetAdvancedTest, TestVariableTags) {
    std::filesystem::path file_path = test_dir_ / "variable_tags.parquet";
    auto schema = SchemaMapper::GetArrowSchema();
    
    // Use direct Arrow/Parquet writer to force small row groups
    auto outfile = *arrow::io::FileOutputStream::Open(file_path.string());
    parquet::WriterProperties::Builder builder;
    builder.max_row_group_length(1);
    auto writer_props = builder.build();
    
    std::unique_ptr<parquet::arrow::FileWriter> writer;
    auto writer_result = parquet::arrow::FileWriter::Open(*schema, arrow::default_memory_pool(), outfile, writer_props, parquet::default_arrow_writer_properties());
    ASSERT_TRUE(writer_result.ok());
    writer = std::move(*writer_result);

    // Batch 1: No tags
    {
        std::vector<tsdb::core::Sample> samples = {tsdb::core::Sample(1000, 1.0)};
        std::map<std::string, std::string> tags = {};
        auto batch = SchemaMapper::ToRecordBatch(samples, tags);
        ASSERT_TRUE(writer->WriteRecordBatch(*batch).ok());
    }

    // Batch 2: Multiple tags
    {
        std::vector<tsdb::core::Sample> samples = {tsdb::core::Sample(2000, 2.0)};
        std::map<std::string, std::string> tags = {{"a", "1"}, {"b", "2"}};
        auto batch = SchemaMapper::ToRecordBatch(samples, tags);
        ASSERT_TRUE(writer->WriteRecordBatch(*batch).ok());
    }

    ASSERT_TRUE(writer->Close().ok());
    ASSERT_TRUE(outfile->Close().ok());

    // Read back and verify tags (this requires inspecting the map column)
    ParquetReader reader;
    ASSERT_TRUE(reader.Open(file_path.string()).ok());

    // Batch 1
    std::shared_ptr<arrow::RecordBatch> batch1;
    ASSERT_TRUE(reader.ReadBatch(&batch1).ok());
    // TODO: Deep inspection of map column if needed, for now just verifying read success
    ASSERT_EQ(batch1->num_rows(), 1);

    // Batch 2
    std::shared_ptr<arrow::RecordBatch> batch2;
    ASSERT_TRUE(reader.ReadBatch(&batch2).ok());
    ASSERT_EQ(batch2->num_rows(), 1);

    ASSERT_TRUE(reader.Close().ok());
}

TEST_F(ParquetAdvancedTest, TestSchemaValidation) {
    // Attempt to open a non-existent file
    ParquetReader reader;
    auto result = reader.Open("non_existent_file.parquet");
    ASSERT_FALSE(result.ok());
}
