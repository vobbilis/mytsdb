#ifndef TSDB_STORAGE_DERIVED_METRICS_H_
#define TSDB_STORAGE_DERIVED_METRICS_H_

#include "tsdb/storage/storage.h"
#include "tsdb/storage/background_processor.h"
#include "tsdb/prometheus/promql/engine.h"
#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>

namespace tsdb {
namespace storage {

struct DerivedMetricRule {
    std::string name;           // Name of the new metric
    std::string query;          // PromQL query to execute
    int64_t interval_ms;        // Execution interval
    int64_t last_execution_time; // Timestamp of last execution
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
    
    // Add a derived metric rule
    void add_rule(const std::string& name, const std::string& query, int64_t interval_ms = 60000);
    
    // Clear all rules
    void clear_rules();

protected:
    // Helper to execute a single rule
    core::Result<void> execute_rule(const DerivedMetricRule& rule);

private:
    std::shared_ptr<Storage> storage_;
    std::shared_ptr<BackgroundProcessor> background_processor_;
    std::unique_ptr<prometheus::promql::Engine> engine_;
    
    std::vector<DerivedMetricRule> rules_;
    std::mutex rules_mutex_;
    
    std::thread scheduler_thread_;
    std::atomic<bool> running_{false};
    
    // Scheduler loop
    void scheduler_loop();
};

} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_DERIVED_METRICS_H_
