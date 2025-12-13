#include "tsdb/prometheus/api/labels.h"
#include "tsdb/core/matcher.h"
#include "tsdb/storage/storage.h"
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <regex>
#include <limits>
#include <iostream>
#include <set>

namespace tsdb {
namespace prometheus {
namespace api {

namespace {
    const std::regex kLabelNameRegex("[a-zA-Z_][a-zA-Z0-9_.]*");
    const std::regex kMatcherRegex(R"(\{([^{}]+)\})");
    const int64_t kMaxTimeRange = 86400000;  // 24 hours in milliseconds
}

bool LabelQueryParams::Validate() const {
    // Validate time range
    if (start_time && end_time) {
        if (*start_time > *end_time) {
            return false;
        }
        if (*end_time - *start_time > kMaxTimeRange) {
            return false;
        }
    }
    
    // Validate matchers - allow both metric names (e.g., "container_cpu_usage_seconds_total")
    // and full matchers (e.g., "{__name__=\"container_cpu_usage_seconds_total\"}")
    for (const auto& matcher : matchers) {
        // Allow metric names (simple identifiers)
        if (std::regex_match(matcher, std::regex("[a-zA-Z_][a-zA-Z0-9_.]*"))) {
            continue;
        }
        // Allow full matchers (e.g., "{__name__=\"metric\"}")
        if (std::regex_match(matcher, kMatcherRegex)) {
            continue;
        }
        // If neither format matches, it's invalid
        return false;
    }
    
    return true;
}

std::string LabelQueryResult::ToJSON() const {
    rapidjson::Document doc;
    doc.SetObject();
    auto& allocator = doc.GetAllocator();
    
    // Add status
    doc.AddMember("status", 
                 rapidjson::Value(status.c_str(), allocator).Move(),
                 allocator);
    
    if (status == "success") {
        // Add data array
        rapidjson::Value data(rapidjson::kArrayType);
        for (const auto& value : values) {
            data.PushBack(rapidjson::Value(value.c_str(), allocator).Move(),
                         allocator);
        }
        doc.AddMember("data", data, allocator);
    } else {
        // Add error information
        doc.AddMember("errorType",
                     rapidjson::Value(error_type.c_str(), allocator).Move(),
                     allocator);
        doc.AddMember("error",
                     rapidjson::Value(error.c_str(), allocator).Move(),
                     allocator);
    }
    
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    
    return buffer.GetString();
}

LabelsHandler::LabelsHandler(const std::shared_ptr<Storage>& storage)
    : storage_(storage) {
    if (!storage_) {
        std::cerr << "LabelsHandler initialized with null storage!" << std::endl;
    }
}

LabelQueryResult LabelsHandler::GetLabels(const LabelQueryParams& params) {
    // Validate parameters
    if (!params.Validate()) {
        return CreateErrorResponse("invalid_parameters",
                                 "Invalid query parameters");
    }
    
    if (!storage_) {
        return CreateErrorResponse("internal_error", "Storage not initialized");
    }

    try {
        auto result = storage_->label_names();
        if (!result.ok()) {
            return CreateErrorResponse("internal_error", result.error());
        }
        return CreateSuccessResponse(result.take_value());
    } catch (const std::exception& e) {
        return CreateErrorResponse("internal_error", e.what());
    }
}

LabelQueryResult LabelsHandler::GetLabelValues(const std::string& label_name,
                                             const LabelQueryParams& params) {
    // Validate label name
    if (!ValidateLabelName(label_name)) {
        return CreateErrorResponse("invalid_parameter",
                                 "Invalid label name: " + label_name);
    }
    
    // Validate parameters
    if (!params.Validate()) {
        return CreateErrorResponse("invalid_parameters",
                                 "Invalid query parameters");
    }
    
    if (!storage_) {
        return CreateErrorResponse("internal_error", "Storage not initialized");
    }

    try {
        // If matchers are provided, filter label values by querying matching series
        if (!params.matchers.empty()) {
            // Convert matchers to core::LabelMatcher format
            std::vector<core::LabelMatcher> storage_matchers;
            for (const auto& matcher_str : params.matchers) {
                // Check if it's a metric name (simple identifier)
                if (std::regex_match(matcher_str, std::regex("[a-zA-Z_][a-zA-Z0-9_.]*"))) {
                    // Convert metric name to __name__ matcher
                    storage_matchers.emplace_back(core::MatcherType::Equal, "__name__", matcher_str);
                } else {
                    // Parse full matcher format: {key="value"}
                    std::regex matcher_regex(R"xxx(\{([^=]+)="([^"]+)"\})xxx");
                    std::smatch match;
                    if (std::regex_match(matcher_str, match, matcher_regex)) {
                        storage_matchers.emplace_back(core::MatcherType::Equal, match[1].str(), match[2].str());
                    }
                }
            }
            
            // Query matching series and extract label values
            int64_t start = params.start_time.value_or(0);
            int64_t end = params.end_time.value_or(std::numeric_limits<int64_t>::max());
            auto query_result = storage_->query(storage_matchers, start, end);
            if (!query_result.ok()) {
                return CreateErrorResponse("internal_error", query_result.error());
            }
            
            // Extract unique label values from matching series
            std::set<std::string> values_set;
            for (const auto& series : query_result.value()) {
                const auto& labels = series.labels();
                auto value_it = labels.map().find(label_name);
                if (value_it != labels.map().end()) {
                    values_set.insert(value_it->second);
                }
            }
            
            std::vector<std::string> result(values_set.begin(), values_set.end());
            return CreateSuccessResponse(std::move(result));
        } else {
            // No matchers - return all label values
            auto result = storage_->label_values(label_name);
            if (!result.ok()) {
                return CreateErrorResponse("internal_error", result.error());
            }
            return CreateSuccessResponse(result.take_value());
        }
    } catch (const std::exception& e) {
        return CreateErrorResponse("internal_error", e.what());
    }
}

LabelQueryResult LabelsHandler::GetSeries(const std::vector<std::string>& matchers,
                                        const LabelQueryParams& params) {
    // Validate matchers
    if (!ValidateMatchers(matchers)) {
        return CreateErrorResponse("invalid_parameter",
                                 "Invalid label matchers");
    }
    
    // Validate parameters
    if (!params.Validate()) {
        return CreateErrorResponse("invalid_parameters",
                                 "Invalid query parameters");
    }
    
    if (!storage_) {
        return CreateErrorResponse("internal_error", "Storage not initialized");
    }

    try {
#include "tsdb/core/matcher.h"
// ...

        // Convert matchers to the format expected by storage
        std::vector<core::LabelMatcher> storage_matchers;
        for (const auto& matcher : matchers) {
            // Simple parser for {key="value"} format
            std::regex matcher_regex(R"xxx(\{([^=]+)="([^"]+)"\})xxx");
            std::smatch match;
            if (std::regex_match(matcher, match, matcher_regex)) {
                storage_matchers.emplace_back(core::MatcherType::Equal, match[1].str(), match[2].str());
            }
        }
        
        auto result = storage_->query(storage_matchers, 
                                    params.start_time.value_or(0),
                                    params.end_time.value_or(std::numeric_limits<int64_t>::max()));
        if (!result.ok()) {
            return CreateErrorResponse("internal_error", result.error());
        }
        
        std::vector<std::string> series_strings;
        for (const auto& s : result.value()) {
            series_strings.push_back(s.labels().to_string());
        }
        return CreateSuccessResponse(std::move(series_strings));
    } catch (const std::exception& e) {
        return CreateErrorResponse("internal_error", e.what());
    }
}

bool LabelsHandler::ValidateLabelName(const std::string& name) const {
    return std::regex_match(name, kLabelNameRegex);
}

bool LabelsHandler::ValidateTimeRange(const LabelQueryParams& params) const {
    if (params.start_time && params.end_time) {
        return *params.start_time <= *params.end_time &&
               *params.end_time - *params.start_time <= kMaxTimeRange;
    }
    return true;
}

bool LabelsHandler::ValidateMatchers(const std::vector<std::string>& matchers) const {
    for (const auto& matcher : matchers) {
        if (!std::regex_match(matcher, kMatcherRegex)) {
            return false;
        }
    }
    return true;
}

LabelQueryResult LabelsHandler::CreateErrorResponse(const std::string& error_type,
                                                  const std::string& error) const {
    LabelQueryResult result;
    result.status = "error";
    result.error_type = error_type;
    result.error = error;
    return result;
}

LabelQueryResult LabelsHandler::CreateSuccessResponse(std::vector<std::string> values) const {
    LabelQueryResult result;
    result.values = std::move(values);
    return result;
}

} // namespace api
} // namespace prometheus
} // namespace tsdb 