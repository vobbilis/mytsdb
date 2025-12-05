#ifndef TSDB_STORAGE_FILTERING_STORAGE_H_
#define TSDB_STORAGE_FILTERING_STORAGE_H_

#include "tsdb/storage/storage.h"
#include "tsdb/storage/rule_manager.h"
#include <memory>

namespace tsdb {
namespace storage {

/**
 * @brief Decorator for Storage that applies filtering and mapping rules.
 * 
 * This class wraps an underlying Storage implementation and intercepts
 * write operations to apply rules defined in RuleManager.
 */
class FilteringStorage : public Storage {
public:
    FilteringStorage(std::shared_ptr<Storage> underlying, std::shared_ptr<RuleManager> rule_manager);
    
    // --- Storage Interface Implementation ---
    
    // Intercepted method: applies rules
    core::Result<void> write(const core::TimeSeries& series) override;
    
    // Pass-through methods
    core::Result<void> init(const core::StorageConfig& config) override;
    core::Result<void> close() override;
    
    core::Result<core::TimeSeries> read(
        const core::Labels& labels,
        int64_t start_time,
        int64_t end_time) override;
        
    core::Result<std::vector<core::TimeSeries>> query(
        const std::vector<core::LabelMatcher>& matchers,
        int64_t start_time,
        int64_t end_time) override;
        
    core::Result<std::vector<core::TimeSeries>> query_aggregate(
        const std::vector<core::LabelMatcher>& matchers,
        int64_t start_time,
        int64_t end_time,
        const core::AggregationRequest& aggregation) override;
        
    core::Result<std::vector<std::string>> label_names() override;
    core::Result<std::vector<std::string>> label_values(const std::string& label_name) override;
    
    core::Result<void> delete_series(const std::vector<core::LabelMatcher>& matchers) override;
    
    // Missing methods
    core::Result<void> compact() override;
    core::Result<void> flush() override;
    std::string stats() const override;

private:
    std::shared_ptr<Storage> underlying_;
    std::shared_ptr<RuleManager> rule_manager_;
};

} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_FILTERING_STORAGE_H_
