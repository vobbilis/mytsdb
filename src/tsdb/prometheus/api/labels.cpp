#include "tsdb/prometheus/api/labels.h"
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <regex>
#include <limits>
#include <iostream>

namespace tsdb {
namespace prometheus {
namespace api {

namespace {
    const std::regex kLabelNameRegex("[a-zA-Z_][a-zA-Z0-9_]*");
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
    
    // Validate matchers
    for (const auto& matcher : matchers) {
        if (!std::regex_match(matcher, kMatcherRegex)) {
            return false;
        }
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
    : storage_(storage) {}

LabelQueryResult LabelsHandler::GetLabels(const LabelQueryParams& params) {
    std::cout << "LabelsHandler::GetLabels called" << std::endl;
    // Validate parameters
    std::cout << "About to call params.Validate()" << std::endl;
    bool is_valid = params.Validate();
    std::cout << "params.Validate() returned: " << is_valid << std::endl;
    if (!is_valid) {
        std::cout << "Validation failed" << std::endl;
        return CreateErrorResponse("invalid_parameters",
                                 "Invalid query parameters");
    }
    std::cout << "Validation passed, calling storage->label_names()" << std::endl;
    
    try {
        auto result = storage_->label_names();
        std::cout << "storage->label_names() returned" << std::endl;
        std::cout << "About to call result.ok()" << std::endl;
        bool ok = result.ok();
        std::cout << "result.ok() returned: " << ok << std::endl;
        if (!ok) {
            return CreateErrorResponse("internal_error", result.error());
        }
        std::cout << "Calling CreateSuccessResponse" << std::endl;
        auto response = CreateSuccessResponse(std::move(result.value()));
        std::cout << "CreateSuccessResponse returned" << std::endl;
        return response;
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return CreateErrorResponse("internal_error", e.what());
    }
}

LabelQueryResult LabelsHandler::GetLabelValues(const std::string& label_name,
                                             const LabelQueryParams& params) {
    std::cout << "LabelsHandler::GetLabelValues called for " << label_name << std::endl;
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
    
    try {
        std::cout << "Calling storage_->label_values(" << label_name << ")" << std::endl;
        auto result = storage_->label_values(label_name);
        std::cout << "storage_->label_values returned" << std::endl;
        if (!result.ok()) {
            return CreateErrorResponse("internal_error", result.error());
        }
        return CreateSuccessResponse(std::move(result.value()));
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
    std::cout << "Inside CreateSuccessResponse" << std::endl;
    LabelQueryResult result;
    result.values = std::move(values);
    std::cout << "CreateSuccessResponse done" << std::endl;
    return result;
}

} // namespace api
} // namespace prometheus
} // namespace tsdb 