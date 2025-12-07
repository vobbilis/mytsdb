#include "tsdb/storage/parquet/schema_mapper.hpp"
#include <arrow/builder.h>
#include <set>

namespace tsdb {
namespace storage {
namespace parquet {

std::shared_ptr<arrow::Schema> SchemaMapper::GetArrowSchema() {
    auto timestamp_field = arrow::field("timestamp", arrow::int64(), false);
    auto value_field = arrow::field("value", arrow::float64(), false);
    
    // Map<String, String>
    auto key_field = arrow::field("key", arrow::utf8(), false);
    auto item_field = arrow::field("value", arrow::utf8(), true); // Value in map can be null? Let's say yes for flexibility
    auto tags_type = arrow::map(arrow::utf8(), arrow::utf8());
    auto tags_field = arrow::field("tags", tags_type, true); // The map column itself can be null

    return arrow::schema({timestamp_field, value_field, tags_field});
}

std::shared_ptr<arrow::RecordBatch> SchemaMapper::ToRecordBatch(
    const std::vector<tsdb::core::Sample>& samples,
    const std::map<std::string, std::string>& tags
) {
    // 1. Scan samples to find all unique field keys
    std::set<std::string> field_keys;
    for (const auto& sample : samples) {
        for (const auto& [key, _] : sample.fields()) {
            field_keys.insert(key);
        }
    }

    // 2. Build Schema
    arrow::FieldVector fields;
    fields.push_back(arrow::field("timestamp", arrow::int64(), false));
    fields.push_back(arrow::field("value", arrow::float64(), false));
    
    // Tags map
    auto key_field = arrow::field("key", arrow::utf8(), false);
    auto item_field = arrow::field("value", arrow::utf8(), true);
    auto tags_type = arrow::map(arrow::utf8(), arrow::utf8());
    fields.push_back(arrow::field("tags", tags_type, true));

    // Field columns (nullable strings)
    for (const auto& key : field_keys) {
        fields.push_back(arrow::field(key, arrow::utf8(), true));
    }
    
    auto schema = arrow::schema(fields);

    // 3. Builders
    arrow::Int64Builder timestamp_builder;
    arrow::DoubleBuilder value_builder;
    
    auto pool = arrow::default_memory_pool();
    arrow::MapBuilder tags_builder(pool, 
        std::make_shared<arrow::StringBuilder>(pool), 
        std::make_shared<arrow::StringBuilder>(pool)
    );
    arrow::StringBuilder* key_builder = static_cast<arrow::StringBuilder*>(tags_builder.key_builder());
    arrow::StringBuilder* item_builder = static_cast<arrow::StringBuilder*>(tags_builder.item_builder());

    // Field builders
    std::map<std::string, std::shared_ptr<arrow::StringBuilder>> field_builders;
    for (const auto& key : field_keys) {
        field_builders[key] = std::make_shared<arrow::StringBuilder>();
    }

    // Reserve space
    (void)timestamp_builder.Reserve(samples.size());
    (void)value_builder.Reserve(samples.size());
    (void)tags_builder.Reserve(samples.size());
    for (auto& [_, builder] : field_builders) {
        (void)builder->Reserve(samples.size());
    }

    // 4. Append Data
    for (const auto& sample : samples) {
        (void)timestamp_builder.Append(sample.timestamp());
        (void)value_builder.Append(sample.value());

        // Append tags
        (void)tags_builder.Append(); 
        for (const auto& [k, v] : tags) {
            (void)key_builder->Append(k);
            (void)item_builder->Append(v);
        }
        
        // Append fields
        for (const auto& key : field_keys) {
            auto it = sample.fields().find(key);
            if (it != sample.fields().end()) {
                (void)field_builders[key]->Append(it->second);
            } else {
                (void)field_builders[key]->AppendNull();
            }
        }
    }

    // 5. Finish Arrays
    std::vector<std::shared_ptr<arrow::Array>> arrays;
    
    std::shared_ptr<arrow::Array> timestamp_array;
    (void)timestamp_builder.Finish(&timestamp_array);
    arrays.push_back(timestamp_array);

    std::shared_ptr<arrow::Array> value_array;
    (void)value_builder.Finish(&value_array);
    arrays.push_back(value_array);

    std::shared_ptr<arrow::Array> tags_array;
    (void)tags_builder.Finish(&tags_array);
    arrays.push_back(tags_array);
    
    for (const auto& key : field_keys) {
        std::shared_ptr<arrow::Array> field_array;
        (void)field_builders[key]->Finish(&field_array);
        arrays.push_back(field_array);
    }

    return arrow::RecordBatch::Make(schema, samples.size(), arrays);
    return arrow::RecordBatch::Make(schema, samples.size(), arrays);
}

std::shared_ptr<arrow::RecordBatch> SchemaMapper::ToRecordBatch(
    const std::vector<int64_t>& timestamps,
    const std::vector<double>& values,
    const std::map<std::string, std::string>& tags
) {
    if (timestamps.size() != values.size()) {
        return nullptr;
    }

    // 1. Build Schema (Simplified, no fields support for now in this path)
    arrow::FieldVector fields;
    fields.push_back(arrow::field("timestamp", arrow::int64(), false));
    fields.push_back(arrow::field("value", arrow::float64(), false));
    
    // Tags map
    auto key_field = arrow::field("key", arrow::utf8(), false);
    auto item_field = arrow::field("value", arrow::utf8(), true);
    auto tags_type = arrow::map(arrow::utf8(), arrow::utf8());
    fields.push_back(arrow::field("tags", tags_type, true));

    // No dynamic fields in this path yet
    
    auto schema = arrow::schema(fields);

    // 2. Builders
    arrow::Int64Builder timestamp_builder;
    arrow::DoubleBuilder value_builder;
    
    auto pool = arrow::default_memory_pool();
    arrow::MapBuilder tags_builder(pool, 
        std::make_shared<arrow::StringBuilder>(pool), 
        std::make_shared<arrow::StringBuilder>(pool)
    );
    arrow::StringBuilder* key_builder = static_cast<arrow::StringBuilder*>(tags_builder.key_builder());
    arrow::StringBuilder* item_builder = static_cast<arrow::StringBuilder*>(tags_builder.item_builder());

    // Reserve space
    size_t count = timestamps.size();
    (void)timestamp_builder.Reserve(count);
    (void)value_builder.Reserve(count);
    (void)tags_builder.Reserve(count);

    // 3. Append Data
    // Optimization: Bulk append for timestamps and values?
    // arrow::Int64Builder::AppendValues exists!
    (void)timestamp_builder.AppendValues(timestamps);
    (void)value_builder.AppendValues(values);

    // Tags must be repeated for each row
    for (size_t i = 0; i < count; ++i) {
        (void)tags_builder.Append(); 
        for (const auto& [k, v] : tags) {
            (void)key_builder->Append(k);
            (void)item_builder->Append(v);
        }
    }

    // 4. Finish Arrays
    std::vector<std::shared_ptr<arrow::Array>> arrays;
    
    std::shared_ptr<arrow::Array> timestamp_array;
    (void)timestamp_builder.Finish(&timestamp_array);
    arrays.push_back(timestamp_array);

    std::shared_ptr<arrow::Array> value_array;
    (void)value_builder.Finish(&value_array);
    arrays.push_back(value_array);

    std::shared_ptr<arrow::Array> tags_array;
    (void)tags_builder.Finish(&tags_array);
    arrays.push_back(tags_array);
    
    return arrow::RecordBatch::Make(schema, count, arrays);
}


core::Result<std::vector<core::Sample>> SchemaMapper::ToSamples(std::shared_ptr<arrow::RecordBatch> batch) {
    if (!batch) {
        return core::Result<std::vector<core::Sample>>::error("Null batch");
    }

    auto schema = batch->schema();
    auto ts_idx = schema->GetFieldIndex("timestamp");
    auto val_idx = schema->GetFieldIndex("value");
    auto tags_idx = schema->GetFieldIndex("tags"); // We ignore tags here as they are returned separately

    if (ts_idx == -1 || val_idx == -1) {
        return core::Result<std::vector<core::Sample>>::error("Missing timestamp or value column");
    }

    auto ts_array = std::static_pointer_cast<arrow::Int64Array>(batch->column(ts_idx));
    auto val_array = std::static_pointer_cast<arrow::DoubleArray>(batch->column(val_idx));

    // Identify field columns
    std::vector<std::pair<std::string, std::shared_ptr<arrow::StringArray>>> field_cols;
    for (int i = 0; i < schema->num_fields(); ++i) {
        if (i != ts_idx && i != val_idx && i != tags_idx) {
            auto field_name = schema->field(i)->name();
            auto array = std::static_pointer_cast<arrow::StringArray>(batch->column(i));
            field_cols.emplace_back(field_name, array);
        }
    }

    std::vector<core::Sample> samples;
    samples.reserve(batch->num_rows());

    for (int64_t i = 0; i < batch->num_rows(); ++i) {
        if (ts_array->IsValid(i) && val_array->IsValid(i)) {
            core::Fields fields;
            for (const auto& [name, array] : field_cols) {
                if (array->IsValid(i)) {
                    fields[name] = array->GetString(i);
                }
            }
            samples.emplace_back(ts_array->Value(i), val_array->Value(i), fields);
        }
    }

    return core::Result<std::vector<core::Sample>>(std::move(samples));
}

core::Result<std::map<std::string, std::string>> SchemaMapper::ExtractTags(std::shared_ptr<arrow::RecordBatch> batch) {
    if (!batch || batch->num_rows() == 0) {
        return core::Result<std::map<std::string, std::string>>(std::map<std::string, std::string>{});
    }

    auto schema = batch->schema();
    auto tags_idx = schema->GetFieldIndex("tags");
    if (tags_idx == -1) {
        // No tags column is fine, return empty map
        return core::Result<std::map<std::string, std::string>>(std::map<std::string, std::string>{});
    }

    auto col = batch->column(tags_idx);

    std::shared_ptr<arrow::Array> keys_array;
    std::shared_ptr<arrow::Array> items_array;
    int64_t start = 0;
    int64_t end = 0;

    if (col->type()->id() == arrow::Type::MAP) {
        auto tags_array = std::static_pointer_cast<arrow::MapArray>(col);
        if (!tags_array->IsValid(0)) {
             return core::Result<std::map<std::string, std::string>>(std::map<std::string, std::string>{});
        }
        keys_array = tags_array->keys();
        items_array = tags_array->items();
        start = tags_array->value_offset(0);
        end = tags_array->value_offset(1);
    } else if (col->type()->id() == arrow::Type::LIST) {
        // Fallback for List<Struct<key, value>>
        auto list_array = std::static_pointer_cast<arrow::ListArray>(col);
        if (!list_array->IsValid(0)) {
             return core::Result<std::map<std::string, std::string>>(std::map<std::string, std::string>{});
        }
        auto struct_array = std::static_pointer_cast<arrow::StructArray>(list_array->values());
        
        if (struct_array->num_fields() < 2) {
             return core::Result<std::map<std::string, std::string>>::error("Tags struct has fewer than 2 fields");
        }
        
        keys_array = struct_array->field(0);
        items_array = struct_array->field(1);
        start = list_array->value_offset(0);
        end = list_array->value_offset(1);
    } else {
        return core::Result<std::map<std::string, std::string>>::error("Unexpected tags column type: " + col->type()->ToString());
    }

    auto keys = std::static_pointer_cast<arrow::StringArray>(keys_array);
    auto items = std::static_pointer_cast<arrow::StringArray>(items_array);
    
    std::map<std::string, std::string> tags;
    for (int64_t i = start; i < end; ++i) {
        if (keys->IsValid(i) && items->IsValid(i)) {
            tags[keys->GetString(i)] = items->GetString(i);
        }
    }

    return core::Result<std::map<std::string, std::string>>(std::move(tags));
}

} // namespace parquet
} // namespace storage
} // namespace tsdb
