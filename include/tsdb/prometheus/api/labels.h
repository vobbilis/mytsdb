#pragma once

#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include "tsdb/storage/storage.h"
#include "../model/types.h"

namespace tsdb {
namespace prometheus {
namespace api {

using Storage = tsdb::storage::Storage;  // Alias for convenience

/**
 * @brief Parameters for label queries
 */
struct LabelQueryParams {
    std::optional<int64_t> start_time;  // Start time in milliseconds
    std::optional<int64_t> end_time;    // End time in milliseconds
    std::vector<std::string> matchers;  // Label matchers (e.g., '{job="prometheus"}')
    
    bool Validate() const;
};

/**
 * @brief Response format for label queries
 */
struct LabelQueryResult {
    std::vector<std::string> values;     // Label names or values
    std::string status = "success";      // Response status
    std::string error_type;             // Error type if status != success
    std::string error;                  // Error message if status != success
    
    std::string ToJSON() const;
};

/**
 * @brief Handler for label-related API endpoints
 */
class LabelsHandler {
public:
    explicit LabelsHandler(const std::shared_ptr<Storage>& storage);
    
    /**
     * @brief Get all label names
     * @param params Query parameters
     * @return List of label names
     */
    LabelQueryResult GetLabels(const LabelQueryParams& params = {});
    
    /**
     * @brief Get values for a specific label
     * @param label_name Label name
     * @param params Query parameters
     * @return List of label values
     */
    LabelQueryResult GetLabelValues(const std::string& label_name,
                                  const LabelQueryParams& params = {});
    
    /**
     * @brief Get series matching the specified matchers
     * @param matchers Label matchers
     * @param params Query parameters
     * @return List of matching series
     */
    LabelQueryResult GetSeries(const std::vector<std::string>& matchers,
                             const LabelQueryParams& params = {});

private:
    std::shared_ptr<Storage> storage_;
    
    // Validation helpers
    bool ValidateLabelName(const std::string& name) const;
    bool ValidateTimeRange(const LabelQueryParams& params) const;
    bool ValidateMatchers(const std::vector<std::string>& matchers) const;
    
    // Response helpers
    LabelQueryResult CreateErrorResponse(const std::string& error_type,
                                       const std::string& error) const;
    LabelQueryResult CreateSuccessResponse(std::vector<std::string> values) const;
};

} // namespace api
} // namespace prometheus
} // namespace tsdb 