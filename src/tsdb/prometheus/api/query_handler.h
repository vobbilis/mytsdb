#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include "tsdb/storage/storage.h"

namespace tsdb {
namespace prometheus {

using Storage = storage::Storage;  // Alias for convenience

/**
 * @brief Parameters for query requests
 */
struct QueryParams {
    std::vector<std::string> matchers;  // Label matchers (e.g., '{__name__="metric_name"}')
    std::optional<int64_t> start_time;  // Start time in milliseconds
    std::optional<int64_t> end_time;    // End time in milliseconds
};

/**
 * @brief Handler for query API endpoints
 */
class QueryHandler {
public:
    explicit QueryHandler(const std::shared_ptr<Storage>& storage);
    
    /**
     * @brief Query series matching the specified matchers
     * @param params Query parameters
     * @return JSON response with series data (labels + samples)
     */
    std::string QuerySeries(const QueryParams& params);
    
    /**
     * @brief Create an error JSON response (public for testing)
     */
    std::string CreateErrorResponse(const std::string& error_type,
                                   const std::string& error);

private:
    std::shared_ptr<Storage> storage_;
    
    // Helper to convert matcher string to storage format
    std::vector<std::pair<std::string, std::string>> ParseMatchers(
        const std::vector<std::string>& matchers);
    
    // Helper to create JSON response
    std::string CreateJSONResponse(
        const std::vector<core::TimeSeries>& series);
    
};

} // namespace prometheus
} // namespace tsdb

