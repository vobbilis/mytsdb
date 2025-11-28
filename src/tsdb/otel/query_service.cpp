#include "tsdb/otel/query_service.h"
#include "tsdb/storage/storage.h"
#include "tsdb/core/matcher.h"
#include "../proto/gen/tsdb.pb.h"
#include "../proto/gen/tsdb.grpc.pb.h"
#include <chrono>
#include <memory>
#include <vector>

namespace tsdb {
namespace otel {

QueryService::QueryService(std::shared_ptr<storage::Storage> storage)
    : storage_(std::move(storage)) {
}

grpc::Status QueryService::GetSeries(
    grpc::ServerContext* /*context*/,
    const tsdb::proto::QueryParams* request,
    tsdb::proto::SeriesResponse* response) {
    try {
        // Convert QueryParams to storage query format
        std::vector<core::LabelMatcher> matchers;
        
        // Convert label matchers from request
        for (int i = 0; i < request->matchers_size(); ++i) {
            const auto& matcher = request->matchers(i);
            core::MatcherType type = core::MatcherType::Equal;
            
            if (matcher.type() == tsdb::proto::LabelMatcher_Type_EQ) {
                type = core::MatcherType::Equal;
            } else if (matcher.type() == tsdb::proto::LabelMatcher_Type_NEQ) {
                type = core::MatcherType::NotEqual;
            } else if (matcher.type() == tsdb::proto::LabelMatcher_Type_RE) {
                type = core::MatcherType::RegexMatch;
            } else if (matcher.type() == tsdb::proto::LabelMatcher_Type_NRE) {
                type = core::MatcherType::RegexNoMatch;
            }
            
            matchers.emplace_back(type, matcher.name(), matcher.value());
        }
        
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
            return grpc::Status(grpc::StatusCode::INTERNAL, 
                              "Query failed: " + query_result.error());
        }
        
        // Convert results to protobuf response
        const auto& results = query_result.value();
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
        
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL,
                            "Error in GetSeries: " + std::string(e.what()));
    }
}

grpc::Status QueryService::GetLabelNames(
    grpc::ServerContext* /*context*/,
    const tsdb::proto::QueryParams* /*request*/,
    tsdb::proto::LabelNamesResponse* response) {
    try {
        auto result = storage_->label_names();
        if (!result.ok()) {
            return grpc::Status(grpc::StatusCode::INTERNAL, 
                              "GetLabelNames failed: " + result.error());
        }
        
        for (const auto& name : result.value()) {
            response->add_names(name);
        }
        
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL,
                            "Error in GetLabelNames: " + std::string(e.what()));
    }
}

grpc::Status QueryService::GetLabelValues(
    grpc::ServerContext* /*context*/,
    const tsdb::proto::LabelValuesRequest* request,
    tsdb::proto::LabelValuesResponse* response) {
    try {
        auto result = storage_->label_values(request->label_name());
        if (!result.ok()) {
            return grpc::Status(grpc::StatusCode::INTERNAL, 
                              "GetLabelValues failed: " + result.error());
        }
        
        for (const auto& value : result.value()) {
            response->add_values(value);
        }
        
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL,
                            "Error in GetLabelValues: " + std::string(e.what()));
    }
}

} // namespace otel
} // namespace tsdb
