#pragma once

#include <memory>
#include <string>
#include <chrono>
#include "tsdb/prometheus/promql/value.h"
#include "tsdb/prometheus/promql/parser.h"
#include "tsdb/prometheus/storage/adapter.h"

namespace tsdb {
namespace prometheus {
namespace promql {

/**
 * @brief Configuration options for the Query Engine
 */
struct EngineOptions {
    std::chrono::milliseconds timeout{10000}; // 10 seconds default
    int max_samples{50000000};                // 50 million samples default
    std::chrono::milliseconds lookback_delta{300000}; // 5 minutes default
    bool enable_at_modifier{true};
    bool enable_negative_offset{true};
    tsdb::prometheus::storage::StorageAdapter* storage_adapter{nullptr};
};

/**
 * @brief Result of a query execution
 */
struct QueryResult {
    Value value;
    std::vector<std::string> warnings;
    std::string error;

    bool hasError() const { return !error.empty(); }
};

/**
 * @brief The PromQL Query Engine
 */
class Engine {
public:
    explicit Engine(EngineOptions options = EngineOptions{});
    ~Engine();

    /**
     * @brief Execute an instant query at a specific timestamp
     * 
     * @param query The PromQL query string
     * @param time The evaluation timestamp
     * @return QueryResult containing the result or error
     */
    QueryResult ExecuteInstant(const std::string& query, int64_t time);

    /**
     * @brief Execute a range query over a time range
     * 
     * @param query The PromQL query string
     * @param start Start timestamp
     * @param end End timestamp
     * @param step Step duration in milliseconds
     * @return QueryResult containing the result (usually a Matrix) or error
     */
    QueryResult ExecuteRange(const std::string& query, int64_t start, int64_t end, int64_t step);

    /**
     * @brief Get values for a specific label
     */
    std::vector<std::string> LabelValues(const std::string& label_name);

private:
    EngineOptions options_;
};

} // namespace promql
} // namespace prometheus
} // namespace tsdb
