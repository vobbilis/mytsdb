#include "tsdb/arrow/flight_server.h"

#include <arrow/api.h>
#include <arrow/flight/api.h>
#include <arrow/io/api.h>
#include <arrow/ipc/api.h>
#include <spdlog/spdlog.h>

#include "tsdb/core/types.h"

namespace tsdb {
namespace arrow {

MetricsFlightServer::MetricsFlightServer(std::shared_ptr<storage::Storage> storage)
    : storage_(std::move(storage)) {}

::arrow::Status MetricsFlightServer::Init(int port) {
    // Arrow 22.0+ API: ForGrpcTcp returns Result<Location>
    ARROW_ASSIGN_OR_RAISE(auto location,
        ::arrow::flight::Location::ForGrpcTcp("0.0.0.0", port));
    
    ::arrow::flight::FlightServerOptions options(location);
    ARROW_RETURN_NOT_OK(FlightServerBase::Init(options));
    
    spdlog::info("Arrow Flight server listening on port {}", port);
    return ::arrow::Status::OK();
}

// Expected schema: timestamp (int64), value (float64), tags (Map<String, String>)
::arrow::Status MetricsFlightServer::DoPut(
    const ::arrow::flight::ServerCallContext& context,
    std::unique_ptr<::arrow::flight::FlightMessageReader> reader,
    std::unique_ptr<::arrow::flight::FlightMetadataWriter> writer) {
    
    while (true) {
        ::arrow::flight::FlightStreamChunk chunk;
        ARROW_ASSIGN_OR_RAISE(chunk, reader->Next());
        if (!chunk.data) break; // End of stream
        
        auto batch = chunk.data;
        int64_t num_rows = batch->num_rows();
        if (num_rows == 0) continue;
        
        auto schema = batch->schema();
        // Validate basic schema: at least timestamp and value
        if (schema->num_fields() < 2) {
            errors_++;
            return ::arrow::Status::Invalid("Schema must have at least timestamp and value columns");
        }
        
        auto ts_array = std::dynamic_pointer_cast<::arrow::Int64Array>(batch->column(0));
        auto val_array = std::dynamic_pointer_cast<::arrow::DoubleArray>(batch->column(1));
        
        if (!ts_array || !val_array) {
            errors_++;
            return ::arrow::Status::Invalid("First two columns must be timestamp(int64) and value(float64)");
        }
        
        // Look for "tags" column (Map) or fallback to string columns
        std::shared_ptr<::arrow::MapArray> map_array;
        std::vector<std::pair<std::string, std::shared_ptr<::arrow::StringArray>>> string_cols;
        
        int tags_col_idx = batch->schema()->GetFieldIndex("tags");
        if (tags_col_idx != -1) {
            map_array = std::dynamic_pointer_cast<::arrow::MapArray>(batch->column(tags_col_idx));
        } else {
            // Fallback: look for remaining string columns
            for (int i = 2; i < schema->num_fields(); ++i) {
                 if (schema->field(i)->type()->id() == ::arrow::Type::STRING) {
                    string_cols.emplace_back(
                        schema->field(i)->name(), 
                        std::dynamic_pointer_cast<::arrow::StringArray>(batch->column(i))
                    );
                 }
            }
        }

        // Process per row

        // Batching optimization: accumulate samples for the same series
        std::optional<core::TimeSeries> current_series;
        
        // Pre-extract map array components if available
        const int32_t* raw_offsets = nullptr;
        std::shared_ptr<::arrow::StringArray> key_array;
        std::shared_ptr<::arrow::StringArray> item_array;

        if (map_array) {
            raw_offsets = map_array->raw_value_offsets();
            key_array = std::dynamic_pointer_cast<::arrow::StringArray>(map_array->keys());
            item_array = std::dynamic_pointer_cast<::arrow::StringArray>(map_array->items());
        }

        // Determine the __name__ fallback from FlightDescriptor path
        std::string flight_descriptor_path_name;
        if (reader->descriptor().type == ::arrow::flight::FlightDescriptor::PATH && 
            !reader->descriptor().path.empty()) {
            flight_descriptor_path_name = reader->descriptor().path[0];
        } else {
            flight_descriptor_path_name = "unknown";
        }

        // Helper to get tags for a row
        auto get_tags = [&](int row_idx) -> std::map<std::string, std::string> {
            std::map<std::string, std::string> tags;
            
            if (map_array && !map_array->IsNull(row_idx) && key_array && item_array) {
                int32_t start_offset = raw_offsets[row_idx];
                int32_t end_offset = raw_offsets[row_idx + 1];
                for (int32_t j = start_offset; j < end_offset; ++j) {
                    if (!key_array->IsNull(j) && !item_array->IsNull(j)) {
                        tags[key_array->GetString(j)] = item_array->GetString(j);
                    }
                }
            } else {
                // Fallback to string columns
                for (const auto& col : string_cols) {
                    if (!col.second->IsNull(row_idx)) {
                        tags[col.first] = col.second->GetString(row_idx);
                    }
                }
            }
            
            // Ensure __name__ exists
            if (tags.find("__name__") == tags.end()) {
                tags["__name__"] = flight_descriptor_path_name;
            }
            return tags;
        };

        std::map<std::string, std::string> last_tags;

        for (int i = 0; i < num_rows; ++i) {
            if (ts_array->IsNull(i) || val_array->IsNull(i)) continue;

            auto current_row_tags = get_tags(i);
            
            if (current_series && current_row_tags == last_tags) {
                 // Same series, accumulate
                 current_series->add_sample(core::Sample(ts_array->Value(i), val_array->Value(i)));
            } else {
                // Different series or first series in batch, flush previous if any
                if (current_series) {
                    auto result = storage_->write(*current_series);
                    if (!result.ok()) {
                         spdlog::warn("Write error for series");
                         errors_++;
                    } else {
                        samples_ingested_ += current_series->samples().size();
                    }
                }
                
                // Start new series
                last_tags = std::move(current_row_tags);
                current_series.emplace(core::Labels(last_tags));
                current_series->add_sample(core::Sample(ts_array->Value(i), val_array->Value(i)));
            }
        }
        
        // Flush the final series after the loop
        if (current_series) {
            auto result = storage_->write(*current_series);
            if (!result.ok()) {
                spdlog::warn("Write error for final series");
                errors_++;
            } else {
                samples_ingested_ += current_series->samples().size();
            }
        }
        batches_processed_++;
    }
    return ::arrow::Status::OK();
}

MetricsFlightServer::Stats MetricsFlightServer::GetStats() const {
    return Stats{
        samples_ingested_.load(),
        batches_processed_.load(),
        errors_.load()
    };
}

std::unique_ptr<MetricsFlightServer> CreateMetricsFlightServer(
    std::shared_ptr<storage::Storage> storage,
    int port) {
    auto server = std::make_unique<MetricsFlightServer>(std::move(storage));
    auto status = server->Init(port);
    if (!status.ok()) {
        spdlog::error("Failed to initialize Arrow Flight server: {}", status.ToString());
        return nullptr;
    }
    return server;
}

}  // namespace arrow
}  // namespace tsdb
