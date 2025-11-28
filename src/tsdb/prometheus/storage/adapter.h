#pragma once

#include <vector>
#include <string>
#include <memory>
#include "tsdb/prometheus/promql/value.h"
#include "tsdb/prometheus/model/types.h"
#include "tsdb/core/aggregation.h"

namespace tsdb {
namespace prometheus {
namespace storage {

/**
 * @brief Interface for accessing time series data
 */
class StorageAdapter {
public:
    virtual ~StorageAdapter() = default;

    /**
     * @brief Select series matching the given label matchers within the time range
     * 
     * @param matchers List of label matchers
     * @param start Start timestamp (ms)
     * @param end End timestamp (ms)
     * @return Matrix containing the selected series
     */
    virtual promql::Matrix SelectSeries(
        const std::vector<model::LabelMatcher>& matchers,
        int64_t start,
        int64_t end) = 0;

    /**
     * @brief Select series with aggregation pushdown
     */
    virtual promql::Matrix SelectAggregateSeries(
        const std::vector<model::LabelMatcher>& matchers,
        int64_t start,
        int64_t end,
        const core::AggregationRequest& aggregation) {
        // Default implementation falls back to SelectSeries (no pushdown)
        // Or throws error if not supported
        throw std::runtime_error("Aggregation pushdown not supported by this adapter");
    }

    /**
     * @brief Get all label names
     */
    virtual std::vector<std::string> LabelNames() = 0;

    /**
     * @brief Get all values for a specific label
     */
    virtual std::vector<std::string> LabelValues(const std::string& label_name) = 0;
};

} // namespace storage
} // namespace prometheus
} // namespace tsdb
