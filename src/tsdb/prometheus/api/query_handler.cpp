#include "query_handler.h"
#include "tsdb/core/types.h"
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <regex>
#include <limits>
#include <sstream>
#include <iostream>
#include <spdlog/spdlog.h>

namespace tsdb {
namespace prometheus {

namespace {
    const std::regex kMatcherRegex(R"xxx(\{([^=]+)="([^"]+)"\})xxx");
    // Remove time range limit for now - allow queries over any range
    // const int64_t kMaxTimeRange = 86400000;  // 24 hours in milliseconds
}

prometheus::QueryHandler::QueryHandler(const std::shared_ptr<Storage>& storage)
    : storage_(storage) {}

std::vector<std::pair<std::string, std::string>> prometheus::QueryHandler::ParseMatchers(
    const std::vector<std::string>& matchers) {
    std::vector<std::pair<std::string, std::string>> storage_matchers;
    
    // Regex patterns for different matcher formats
    static const std::regex pattern_with_braces(R"xxx(\{([^=]+)="([^"]+)"\})xxx");
    static const std::regex pattern_without_braces(R"xxx(([^=]+)="([^"]+)")xxx");
    static const std::regex pattern_no_quotes(R"xxx(([^=]+)=(.+))xxx");
    
    for (const auto& matcher : matchers) {
        std::smatch match;
        
        if (std::regex_match(matcher, match, kMatcherRegex)) {
            storage_matchers.emplace_back(match[1].str(), match[2].str());
        } else if (std::regex_match(matcher, match, pattern_without_braces)) {
            storage_matchers.emplace_back(match[1].str(), match[2].str());
        } else if (std::regex_match(matcher, match, pattern_no_quotes)) {
            storage_matchers.emplace_back(match[1].str(), match[2].str());
        }
        // Silently skip matchers that don't match any pattern
    }
    
    return storage_matchers;
}

std::string prometheus::QueryHandler::CreateJSONResponse(
    const std::vector<core::TimeSeries>& series) {
    rapidjson::Document doc;
    doc.SetObject();
    auto& allocator = doc.GetAllocator();
    
    // Add status
    doc.AddMember("status", rapidjson::Value("success", allocator), allocator);
    
    // Add data object
    rapidjson::Value data(rapidjson::kObjectType);
    
    // Add result array
    rapidjson::Value result(rapidjson::kArrayType);
    
    for (const auto& ts : series) {
        rapidjson::Value series_obj(rapidjson::kObjectType);
        
        // Add metric (labels)
        rapidjson::Value metric(rapidjson::kObjectType);
        for (const auto& [key, value] : ts.labels().map()) {
            metric.AddMember(
                rapidjson::Value(key.c_str(), allocator),
                rapidjson::Value(value.c_str(), allocator),
                allocator);
        }
        series_obj.AddMember("metric", metric, allocator);
        
        // Add values (samples)
        rapidjson::Value values(rapidjson::kArrayType);
        for (const auto& sample : ts.samples()) {
            rapidjson::Value sample_array(rapidjson::kArrayType);
            // Prometheus format: [timestamp_ms, value]
            sample_array.PushBack(
                rapidjson::Value(static_cast<int64_t>(sample.timestamp())),
                allocator);
            sample_array.PushBack(
                rapidjson::Value(sample.value()),
                allocator);
            values.PushBack(sample_array, allocator);
        }
        series_obj.AddMember("values", values, allocator);
        
        result.PushBack(series_obj, allocator);
    }
    
    data.AddMember("result", result, allocator);
    doc.AddMember("data", data, allocator);
    
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    
    return buffer.GetString();
}

std::string prometheus::QueryHandler::CreateErrorResponse(const std::string& error_type,
                                             const std::string& error) {
    rapidjson::Document doc;
    doc.SetObject();
    auto& allocator = doc.GetAllocator();
    
    doc.AddMember("status", rapidjson::Value("error", allocator), allocator);
    doc.AddMember("errorType",
                 rapidjson::Value(error_type.c_str(), allocator),
                 allocator);
    doc.AddMember("error",
                 rapidjson::Value(error.c_str(), allocator),
                 allocator);
    
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    
    return buffer.GetString();
}

std::string prometheus::QueryHandler::QuerySeries(const QueryParams& params) {
    try {
        // Validate time range
        int64_t start_time = params.start_time.value_or(0);
        int64_t end_time = params.end_time.value_or(std::numeric_limits<int64_t>::max());
        
        if (start_time > end_time) {
            return CreateErrorResponse("invalid_parameter",
                                     "start_time must be <= end_time");
        }
        
        // Parse matchers
        auto storage_matchers = ParseMatchers(params.matchers);
        
        if (storage_matchers.empty() && !params.matchers.empty()) {
            return CreateErrorResponse("invalid_parameter",
                                     "Invalid label matcher format");
        }
        
        // Query storage
        auto result = storage_->query(storage_matchers, start_time, end_time);
        if (!result.ok()) {
            return CreateErrorResponse("internal_error", result.error());
        }
        
        // Convert to JSON
        return CreateJSONResponse(result.value());
    } catch (const std::exception& e) {
        return CreateErrorResponse("internal_error", e.what());
    }
}

} // namespace prometheus
} // namespace tsdb

