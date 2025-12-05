#include "tsdb/storage/parquet/reader.hpp"
#include <arrow/io/file.h>
#include <arrow/table.h>
#include <arrow/record_batch.h>
#include <parquet/arrow/reader.h>
#include <parquet/file_reader.h>

namespace tsdb {
namespace storage {
namespace parquet {

ParquetReader::~ParquetReader() {
    if (infile_) {
        (void)infile_->Close();
    }
}

core::Result<void> ParquetReader::Open(const std::string& path) {
    // Open file input stream
    auto infile_result = arrow::io::ReadableFile::Open(path);
    if (!infile_result.ok()) {
        return core::Result<void>::error("Failed to open file: " + infile_result.status().ToString());
    }
    infile_ = *infile_result;

    // Create Parquet reader
    try {
        auto status = ::parquet::arrow::FileReader::Make(
            arrow::default_memory_pool(),
            ::parquet::ParquetFileReader::Open(infile_),
            &reader_
        );
        if (!status.ok()) {
            return core::Result<void>::error("Failed to create Parquet reader: " + status.ToString());
        }
    } catch (const std::exception& e) {
        return core::Result<void>::error("Exception opening Parquet reader: " + std::string(e.what()));
    }

    return core::Result<void>();
}

core::Result<void> ParquetReader::ReadBatch(std::shared_ptr<arrow::RecordBatch>* out_batch) {
    if (!reader_) {
        return core::Result<void>::error("Reader not open");
    }

    // For simplicity in this phase, we read the entire table or row groups.
    // Ideally we should iterate over row groups and batches.
    // Let's implement a simple iterator that reads one row group at a time as a batch.
    
    if (current_row_group_ >= reader_->num_row_groups()) {
        *out_batch = nullptr;
        return core::Result<void>();
    }

    std::shared_ptr<arrow::Table> table;
    std::vector<int> row_groups = {current_row_group_};
    auto status = reader_->ReadRowGroups(row_groups, &table);
    
    if (!status.ok()) {
        return core::Result<void>::error("Failed to read row group: " + status.ToString());
    }

    if (!table) {
        return core::Result<void>::error("ReadRowGroups returned null table");
    }

    current_row_group_++;

    // Convert Table to RecordBatch (might combine chunks)
    arrow::TableBatchReader batch_reader(*table);
    std::shared_ptr<arrow::RecordBatch> batch;
    auto read_status = batch_reader.ReadNext(&batch);
    
    if (!read_status.ok()) {
         return core::Result<void>::error("Failed to extract batch from table: " + read_status.ToString());
    }
    if (!batch) {
         return core::Result<void>::error("TableBatchReader returned null batch");
    }

    *out_batch = batch;
    return core::Result<void>();
}

core::Result<void> ParquetReader::Close() {
    if (infile_) {
        auto status = infile_->Close();
        if (!status.ok()) {
            return core::Result<void>::error("Failed to close file: " + status.ToString());
        }
        infile_.reset();
    }
    reader_.reset();
    return core::Result<void>();
}

int ParquetReader::GetNumRowGroups() const {
    if (!reader_) return 0;
    return reader_->num_row_groups();
}

core::Result<std::shared_ptr<arrow::RecordBatch>> ParquetReader::ReadRowGroupTags(int row_group_index) {
    if (!reader_) {
        return core::Result<std::shared_ptr<arrow::RecordBatch>>::error("Reader not open");
    }
    if (row_group_index < 0 || row_group_index >= reader_->num_row_groups()) {
        return core::Result<std::shared_ptr<arrow::RecordBatch>>::error("Invalid row group index");
    }

    // We need to read the "tags" column.
    // In Arrow schema, "tags" is a Map type.
    // Reading just the "tags" column might require reading its children.
    // Let's try to read just the tags column by index.
    
    std::shared_ptr<arrow::Schema> schema;
    auto status = reader_->GetSchema(&schema);
    int tags_idx = schema->GetFieldIndex("tags");
    if (tags_idx == -1) {
        // No tags column, return empty batch or error?
        // Return empty batch with schema
        return core::Result<std::shared_ptr<arrow::RecordBatch>>::error("Tags column not found");
    }
    
    // Calculate leaf column indices for the "tags" field.
    // We iterate through fields before "tags" to count their leaf columns.
    int start_col_idx = 0;
    for (int i = 0; i < tags_idx; ++i) {
        auto field = schema->field(i);
        // Simplified leaf counting:
        // - Map: 2 columns (key, value)
        // - Primitive: 1 column
        // - List: 1 column (if primitive) - TODO: handle complex lists if needed
        // - Struct: sum of children - TODO: handle structs if needed
        
        if (field->type()->id() == arrow::Type::MAP) {
            start_col_idx += 2;
        } else {
            // Assume 1 for now (timestamp, value are primitive)
            start_col_idx += 1;
        }
    }
    
    // Tags is a Map, so it has 2 leaf columns (key, value)
    std::vector<int> indices = {start_col_idx, start_col_idx + 1};
    
    std::shared_ptr<arrow::Table> table;
    std::vector<int> row_groups = {row_group_index};
    
    status = reader_->ReadRowGroups(row_groups, indices, &table);
    if (!status.ok()) {
        return core::Result<std::shared_ptr<arrow::RecordBatch>>::error("Failed to read tags: " + status.ToString());
    }
    
    if (!table || table->num_rows() == 0) {
        return core::Result<std::shared_ptr<arrow::RecordBatch>>::error("Empty table returned");
    }
    
    // Convert to batch
    arrow::TableBatchReader batch_reader(*table);
    std::shared_ptr<arrow::RecordBatch> batch;
    status = batch_reader.ReadNext(&batch);
    if (!status.ok()) {
        return core::Result<std::shared_ptr<arrow::RecordBatch>>::error("Failed to get batch: " + status.ToString());
    }
    
    return core::Result<std::shared_ptr<arrow::RecordBatch>>(batch);
}

core::Result<std::shared_ptr<arrow::RecordBatch>> ParquetReader::ReadRowGroup(int row_group_index) {
    if (!reader_) {
        return core::Result<std::shared_ptr<arrow::RecordBatch>>::error("Reader not open");
    }
    
    std::vector<int> row_groups = {row_group_index};
    std::shared_ptr<arrow::Table> table;
    auto status = reader_->ReadRowGroups(row_groups, &table);
    
    if (!status.ok()) {
        return core::Result<std::shared_ptr<arrow::RecordBatch>>::error("Failed to read row group: " + status.ToString());
    }
    
    if (!table) {
        return core::Result<std::shared_ptr<arrow::RecordBatch>>::error("ReadRowGroups returned null table");
    }

    arrow::TableBatchReader batch_reader(*table);
    std::shared_ptr<arrow::RecordBatch> batch;
    auto read_status = batch_reader.ReadNext(&batch);
    
    if (!read_status.ok()) {
         return core::Result<std::shared_ptr<arrow::RecordBatch>>::error("Failed to extract batch from table: " + read_status.ToString());
    }
    
    return core::Result<std::shared_ptr<arrow::RecordBatch>>(batch);
}

} // namespace parquet
} // namespace storage
} // namespace tsdb
