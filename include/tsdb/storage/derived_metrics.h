#ifndef TSDB_STORAGE_DERIVED_METRICS_H_
#define TSDB_STORAGE_DERIVED_METRICS_H_

#include "tsdb/storage/storage.h"
#include "tsdb/storage/background_processor.h"
#include "tsdb/prometheus/promql/engine.h"
#include "tsdb/prometheus/storage/tsdb_adapter.h"
#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>

namespace tsdb {
namespace storage {

// Rule evaluation type
enum class RuleEvaluationType {
    INSTANT,    // Execute at single point in time (default)
    RANGE       // Execute over a time range (for backfill)
};

struct DerivedMetricRule {
    std::string name;           // Name of the new metric
    std::string query;          // PromQL query to execute
    int64_t interval_ms;        // Execution interval
    int64_t last_execution_time{0}; // Timestamp of last execution
    
    // Error backoff fields
    int consecutive_failures{0};    // Number of consecutive failures
    int64_t backoff_until{0};       // Don't execute until this timestamp (ms)
    int max_backoff_seconds{300};   // Maximum backoff duration (default 5 min)
    
    // Label transformation fields
    std::vector<std::string> keep_labels;   // If non-empty, only keep these labels
    std::vector<std::string> drop_labels;   // If non-empty, drop these labels
    // Note: keep_labels takes precedence over drop_labels
    
    // Staleness handling fields
    int64_t staleness_threshold_ms{300000}; // 5 minutes default - samples older than this are stale
    bool skip_if_stale{false};              // If true, don't write stale samples (opt-in)
    
    // Range query fields
    RuleEvaluationType evaluation_type{RuleEvaluationType::INSTANT};
    int64_t range_duration_ms{0};   // For RANGE type: how far back to query
    int64_t range_step_ms{0};       // For RANGE type: step between samples
};

/**
 * @brief A group of related rules with shared interval.
 * 
 * Rules within a group are evaluated sequentially (in order),
 * allowing later rules to depend on earlier rules' output.
 */
struct RuleGroup {
    std::string name;                       // Group name
    int64_t interval_ms{60000};             // Group evaluation interval
    std::vector<DerivedMetricRule> rules;   // Rules in this group
    int64_t last_execution_time{0};         // Track last execution
};

/**
 * @brief Manages derived metrics generation.
 * 
 * Periodically executes PromQL queries and writes the results back to storage
 * as new metrics.
 */
class DerivedMetricManager {
public:
    DerivedMetricManager(
        std::shared_ptr<Storage> storage,
        std::shared_ptr<BackgroundProcessor> background_processor);
        
    ~DerivedMetricManager();
    
    // Start the scheduler thread
    void start();
    
    // Stop the scheduler thread
    void stop();
    
    // Add a derived metric rule (basic)
    void add_rule(const std::string& name, const std::string& query, int64_t interval_ms = 60000);
    
    // Add a derived metric rule with label filtering
    void add_rule(const std::string& name, 
                  const std::string& query, 
                  int64_t interval_ms,
                  const std::vector<std::string>& keep_labels,
                  const std::vector<std::string>& drop_labels = {});
    
    // Clear all rules
    void clear_rules();
    
    // Rule group management
    void add_group(const std::string& name, int64_t interval_ms = 60000);
    void add_rule_to_group(const std::string& group_name,
                           const std::string& rule_name,
                           const std::string& query);
    void clear_groups();
protected:
    // Helper to execute a single rule (non-const ref to update backoff state)
    core::Result<void> execute_rule(DerivedMetricRule& rule);

private:
    std::shared_ptr<Storage> storage_;
    std::shared_ptr<BackgroundProcessor> background_processor_;
    
    // Reusable adapter and engine (created once, used for all rule executions)
    std::unique_ptr<prometheus::storage::TSDBAdapter> adapter_;
    std::unique_ptr<prometheus::promql::Engine> engine_;
    
    std::vector<DerivedMetricRule> rules_;
    std::vector<RuleGroup> groups_;  // Rule groups for sequential execution
    std::mutex rules_mutex_;
    
    std::thread scheduler_thread_;
    std::atomic<bool> running_{false};
    
    // Scheduler loop
    void scheduler_loop();
};

} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_DERIVED_METRICS_H_
