#pragma once

#include "tsdb/prometheus/promql/value.h"
#include "tsdb/prometheus/model/types.h"
#include "tsdb/prometheus/storage/adapter.h"
#include "tsdb/storage/storage.h"
#include "tsdb/core/aggregation.h"
#include <memory>

namespace tsdb {
namespace prometheus {
namespace storage {

/**
 * @brief Adapter implementation for the main TSDB storage
 */
class TSDBAdapter : public StorageAdapter {
public:
    explicit TSDBAdapter(std::shared_ptr<tsdb::storage::Storage> storage);

    promql::Matrix SelectSeries(
        const std::vector<model::LabelMatcher>& matchers,
        int64_t start,
        int64_t end) override;

    promql::Matrix SelectAggregateSeries(
        const std::vector<model::LabelMatcher>& matchers,
        int64_t start,
        int64_t end,
        const core::AggregationRequest& aggregation) override;

    std::vector<std::string> LabelNames() override;

    std::vector<std::string> LabelValues(const std::string& label_name) override;

private:
    std::shared_ptr<tsdb::storage::Storage> storage_;
};

} // namespace storage
} // namespace prometheus
} // namespace tsdb
