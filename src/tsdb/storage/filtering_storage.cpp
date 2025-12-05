#include "tsdb/storage/filtering_storage.h"
#include "tsdb/storage/atomic_metrics.h"
#include "tsdb/common/logger.h"

namespace tsdb {
namespace storage {

FilteringStorage::FilteringStorage(std::shared_ptr<Storage> underlying, std::shared_ptr<RuleManager> rule_manager)
    : underlying_(std::move(underlying)), rule_manager_(std::move(rule_manager)) {}

core::Result<void> FilteringStorage::init(const core::StorageConfig& config) {
    return underlying_->init(config);
}

core::Result<void> FilteringStorage::close() {
    return underlying_->close();
}

core::Result<void> FilteringStorage::write(const core::TimeSeries& series) {
    // 1. Get current rules (Atomic Load - Lock Free)
    auto rules = rule_manager_->get_current_rules();
    
    // 2. Check Drop Rules
    auto start = std::chrono::high_resolution_clock::now();
    bool drop = rules->should_drop(series);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    TSDB_METRICS_RULE_CHECK(duration);

    if (drop) {
        // Drop the metric silently (success)
        TSDB_METRICS_DROPPED_SAMPLE();
        return core::Result<void>();
    }
    
    // 3. Apply Mapping Rules (Placeholder for future implementation)
    // auto mapped_series = rules->apply_mapping(series);
    // return underlying_->write(mapped_series);
    
    return underlying_->write(series);
}

core::Result<core::TimeSeries> FilteringStorage::read(
    const core::Labels& labels,
    int64_t start_time,
    int64_t end_time) {
    return underlying_->read(labels, start_time, end_time);
}

core::Result<std::vector<core::TimeSeries>> FilteringStorage::query(
    const std::vector<core::LabelMatcher>& matchers,
    int64_t start_time,
    int64_t end_time) {
    return underlying_->query(matchers, start_time, end_time);
}

core::Result<std::vector<core::TimeSeries>> FilteringStorage::query_aggregate(
    const std::vector<core::LabelMatcher>& matchers,
    int64_t start_time,
    int64_t end_time,
    const core::AggregationRequest& aggregation) {
    return underlying_->query_aggregate(matchers, start_time, end_time, aggregation);
}

core::Result<std::vector<std::string>> FilteringStorage::label_names() {
    return underlying_->label_names();
}

core::Result<std::vector<std::string>> FilteringStorage::label_values(const std::string& label_name) {
    return underlying_->label_values(label_name);
}

core::Result<void> FilteringStorage::delete_series(const std::vector<core::LabelMatcher>& matchers) {
    return underlying_->delete_series(matchers);
}

core::Result<void> FilteringStorage::compact() {
    return underlying_->compact();
}

core::Result<void> FilteringStorage::flush() {
    return underlying_->flush();
}

std::string FilteringStorage::stats() const {
    return underlying_->stats();
}

} // namespace storage
} // namespace tsdb
