#include "tsdb/otel/query_service.h"
#include "tsdb/storage/storage.h"
#include "../proto/gen/tsdb.pb.h"
#include "../proto/gen/tsdb.grpc.pb.h"
#include <chrono>
#include <memory>
#include <vector>
#include <spdlog/spdlog.h>

namespace tsdb {
namespace otel {

QueryService::QueryService(std::shared_ptr<storage::Storage> storage)
    : storage_(std::move(storage)) {
    spdlog::info("[QueryService] Initialized");
}

grpc::Status QueryService::GetSeries(
    grpc::ServerContext* /*context*/,
    const tsdb::proto::QueryParams* request,
    tsdb::proto::SeriesResponse* response) {
    try {
        spdlog::info("[QueryService] GetSeries called: matchers={}, has_time_range={}, "
                     "time_range=({}, {})",
                     request->matchers_size(),
                     request->has_time_range(),
                     request->has_time_range() ? request->time_range().start_time() : 0,
                     request->has_time_range() ? request->time_range().end_time() : 0);

        // Convert QueryParams to storage query format
        std::vector<std::pair<std::string, std::string>> matchers;
        
        // Convert label matchers from request
        for (int i = 0; i < request->matchers_size(); ++i) {
            const auto& matcher = request->matchers(i);
            if (matcher.type() == tsdb::proto::LabelMatcher_Type_EQ) {
                matchers.push_back({matcher.name(), matcher.value()});
            }
            // TODO: Support other matcher types (NEQ, RE, NRE)
        }
        spdlog::debug("[QueryService] Built {} matchers for query", matchers.size());
        
        // Get time range
        int64_t start_time = 0;
        int64_t end_time = 0;
        if (request->has_time_range()) {
            start_time = request->time_range().start_time();
            end_time = request->time_range().end_time();
        } else {
            // Default to last hour if no time range specified
            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            start_time = now - 3600000;  // 1 hour ago
            end_time = now;
        }
        
        // Query storage
        auto query_result = storage_->query(matchers, start_time, end_time);
        if (!query_result.ok()) {
            spdlog::error("[QueryService] storage_->query failed: {}", query_result.error());
            return grpc::Status(grpc::StatusCode::INTERNAL, 
                              "Query failed: " + query_result.error());
        }
        
        // Convert results to protobuf response
        const auto& results = query_result.value();
        spdlog::info("[QueryService] storage_->query returned {} series", results.size());
        for (const auto& series : results) {
            auto* proto_series = response->add_series();
            
            // Convert labels
            for (const auto& [key, value] : series.labels().map()) {
                auto* label = proto_series->add_labels();
                label->set_name(key);
                label->set_value(value);
            }
            
            // Convert samples
            for (const auto& sample : series.samples()) {
                auto* proto_sample = proto_series->add_samples();
                proto_sample->set_timestamp(sample.timestamp());
                proto_sample->set_value(sample.value());
            }
        }
        
        spdlog::info("[QueryService] GetSeries completed, response has {} series",
                     response->series_size());
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        spdlog::error("[QueryService] Exception in GetSeries: {}", e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL,
                            "Error in GetSeries: " + std::string(e.what()));
    }
}

grpc::Status QueryService::GetLabelNames(
    grpc::ServerContext* /*context*/,
    const tsdb::proto::QueryParams* /*request*/,
    tsdb::proto::LabelNamesResponse* response) {
    try {
        spdlog::info("[QueryService] GetLabelNames called");
        auto result = storage_->label_names();
        if (!result.ok()) {
            spdlog::error("[QueryService] label_names failed: {}", result.error());
            return grpc::Status(grpc::StatusCode::INTERNAL, 
                              "GetLabelNames failed: " + result.error());
        }
        
        for (const auto& name : result.value()) {
            response->add_names(name);
        }
        
        spdlog::info("[QueryService] GetLabelNames returning {} names",
                     response->names_size());
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        spdlog::error("[QueryService] Exception in GetLabelNames: {}", e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL,
                            "Error in GetLabelNames: " + std::string(e.what()));
    }
}

grpc::Status QueryService::GetLabelValues(
    grpc::ServerContext* /*context*/,
    const tsdb::proto::LabelValuesRequest* request,
    tsdb::proto::LabelValuesResponse* response) {
    try {
        spdlog::info("[QueryService] GetLabelValues called for label '{}'",
                     request->label_name());
        auto result = storage_->label_values(request->label_name());
        if (!result.ok()) {
            spdlog::error("[QueryService] label_values failed: {}", result.error());
            return grpc::Status(grpc::StatusCode::INTERNAL, 
                              "GetLabelValues failed: " + result.error());
        }
        
        for (const auto& value : result.value()) {
            response->add_values(value);
        }
        
        spdlog::info("[QueryService] GetLabelValues returning {} values",
                     response->values_size());
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        spdlog::error("[QueryService] Exception in GetLabelValues: {}", e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL,
                            "Error in GetLabelValues: " + std::string(e.what()));
    }
}

} // namespace otel
} // namespace tsdb
