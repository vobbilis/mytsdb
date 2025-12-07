#include "tsdb/storage/derived_metrics.h"
#include "tsdb/storage/atomic_metrics.h"
#include "tsdb/prometheus/storage/tsdb_adapter.h"
#include "tsdb/common/logger.h"
#include <chrono>
#include <algorithm>

namespace tsdb {
namespace storage {

DerivedMetricManager::DerivedMetricManager(
    std::shared_ptr<Storage> storage,
    std::shared_ptr<BackgroundProcessor> background_processor)
    : storage_(std::move(storage)), 
      background_processor_(std::move(background_processor)) {
      
    // Create persistent adapter and engine for reuse across all rule executions
    // This avoids the overhead of recreating these objects on every evaluation
    adapter_ = std::make_unique<prometheus::storage::TSDBAdapter>(storage_);
    
    prometheus::promql::EngineOptions options;
    options.storage_adapter = adapter_.get();
    engine_ = std::make_unique<prometheus::promql::Engine>(options);
    
    TSDB_DEBUG("DerivedMetricManager: Created persistent adapter and engine");
}

DerivedMetricManager::~DerivedMetricManager() {
    stop();
}

void DerivedMetricManager::start() {
    if (running_) return;
    running_ = true;
    scheduler_thread_ = std::thread(&DerivedMetricManager::scheduler_loop, this);
    TSDB_INFO("DerivedMetricManager started");
}

void DerivedMetricManager::stop() {
    if (!running_) return;
    running_ = false;
    if (scheduler_thread_.joinable()) {
        scheduler_thread_.join();
    }
    TSDB_INFO("DerivedMetricManager stopped");
}

void DerivedMetricManager::add_rule(const std::string& name, const std::string& query, int64_t interval_ms) {
    // Call the extended version with empty label filters
    add_rule(name, query, interval_ms, {}, {});
}

void DerivedMetricManager::add_rule(const std::string& name, 
                                     const std::string& query, 
                                     int64_t interval_ms,
                                     const std::vector<std::string>& keep_labels,
                                     const std::vector<std::string>& drop_labels) {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    DerivedMetricRule rule;
    rule.name = name;
    rule.query = query;
    rule.interval_ms = interval_ms;
    rule.last_execution_time = 0;
    rule.consecutive_failures = 0;
    rule.backoff_until = 0;
    rule.max_backoff_seconds = 300;  // 5 minute max backoff
    rule.keep_labels = keep_labels;
    rule.drop_labels = drop_labels;
    rules_.push_back(rule);
    TSDB_INFO("Added derived metric rule: {} = {}", name, query);
}

void DerivedMetricManager::clear_rules() {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    rules_.clear();
}

void DerivedMetricManager::add_group(const std::string& name, int64_t interval_ms) {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    RuleGroup group;
    group.name = name;
    group.interval_ms = interval_ms;
    group.last_execution_time = 0;
    groups_.push_back(group);
    TSDB_INFO("Added rule group: {} (interval={}ms)", name, interval_ms);
}

void DerivedMetricManager::add_rule_to_group(const std::string& group_name,
                                              const std::string& rule_name,
                                              const std::string& query) {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    for (auto& group : groups_) {
        if (group.name == group_name) {
            DerivedMetricRule rule;
            rule.name = rule_name;
            rule.query = query;
            rule.interval_ms = group.interval_ms;  // Use group's interval
            group.rules.push_back(rule);
            TSDB_INFO("Added rule {} to group {}", rule_name, group_name);
            return;
        }
    }
    TSDB_WARN("Rule group {} not found", group_name);
}

void DerivedMetricManager::clear_groups() {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    groups_.clear();
}

void DerivedMetricManager::scheduler_loop() {
    while (running_) {
        auto now = std::chrono::system_clock::now();
        int64_t current_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
            
        {
            std::lock_guard<std::mutex> lock(rules_mutex_);
            for (auto& rule : rules_) {
                // Check if rule is in backoff period
                if (current_time_ms < rule.backoff_until) {
                    continue;  // Skip - still in backoff period
                }
                
                if (current_time_ms - rule.last_execution_time >= rule.interval_ms) {
                    // Execute rule directly (not via background processor for now)
                    // This allows us to update the rule's backoff state
                    auto result = execute_rule(rule);
                    
                    // Update last execution time regardless of success/failure
                    rule.last_execution_time = current_time_ms;
                }
            }
            
            // Execute rule groups (rules within each group run sequentially)
            for (auto& group : groups_) {
                if (current_time_ms - group.last_execution_time >= group.interval_ms) {
                    TSDB_INFO("Executing rule group: {}", group.name);
                    
                    // Execute all rules in the group sequentially
                    for (auto& rule : group.rules) {
                        // Check backoff for individual rules
                        if (current_time_ms < rule.backoff_until) {
                            continue;
                        }
                        
                        auto result = execute_rule(rule);
                        rule.last_execution_time = current_time_ms;
                        
                        // If a rule fails, we still continue with other rules in the group
                        if (!result.ok()) {
                            TSDB_WARN("Rule {} in group {} failed: {}", 
                                      rule.name, group.name, result.error());
                        }
                    }
                    
                    group.last_execution_time = current_time_ms;
                }
            }
        }
        
        // Sleep for a bit (e.g. 1 second)
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

core::Result<void> DerivedMetricManager::execute_rule(DerivedMetricRule& rule) {
    int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    try {
        // Use the persistent engine (created in constructor)
        // This avoids the overhead of recreating adapter+engine on every evaluation
        
        prometheus::promql::QueryResult result;
        
        if (rule.evaluation_type == RuleEvaluationType::RANGE) {
            // Range query: execute over a time range
            int64_t start_ms = now_ms - rule.range_duration_ms;
            int64_t step_ms = rule.range_step_ms > 0 ? rule.range_step_ms : 60000; // Default 60s step
            TSDB_INFO("Executing range query for rule: {} ({}ms - {}ms, step {}ms)", 
                      rule.name, start_ms, now_ms, step_ms);
            result = engine_->ExecuteRange(rule.query, start_ms, now_ms, step_ms);
        } else {
            // Instant query (default)
            TSDB_INFO("Executing query for rule: {}", rule.name);
            result = engine_->ExecuteInstant(rule.query, now_ms);
        }
        
        if (result.hasError()) {
            // Increment failure count and set exponential backoff
            rule.consecutive_failures++;
            int backoff_seconds = std::min(
                (1 << rule.consecutive_failures),  // 2^n seconds
                rule.max_backoff_seconds
            );
            rule.backoff_until = now_ms + (backoff_seconds * 1000);
            TSDB_WARN("Rule {} failed (attempt {}), backing off for {}s. Error: {}", 
                      rule.name, rule.consecutive_failures, backoff_seconds, result.error);
            return core::Result<void>("Execution error: " + result.error, core::Result<void>::ErrorTag{});
        }
        
        // Success - reset backoff state
        rule.consecutive_failures = 0;
        rule.backoff_until = 0;
        TSDB_INFO("Query executed successfully");
        
        // Write Results
        if (result.value.isVector()) {
            const auto& vector = result.value.getVector();
            for (const auto& sample : vector) {
                // Create a new TimeSeries
                core::Labels labels;
                // Copy labels from result with filtering
                for (const auto& [k, v] : sample.metric.labels()) {
                    if (k == "__name__") continue; // Skip old name, will set new one
                    
                    // Apply label filtering
                    if (!rule.keep_labels.empty()) {
                        // If keep_labels specified, only keep those
                        if (std::find(rule.keep_labels.begin(), rule.keep_labels.end(), k) 
                            == rule.keep_labels.end()) {
                            continue;  // Label not in keep list, skip it
                        }
                    } else if (!rule.drop_labels.empty()) {
                        // If drop_labels specified, drop those
                        if (std::find(rule.drop_labels.begin(), rule.drop_labels.end(), k) 
                            != rule.drop_labels.end()) {
                            continue;  // Label in drop list, skip it
                        }
                    }
                    
                    labels.add(k, v);
                }
                // Set new name
                labels.add("__name__", rule.name);
                
                // Check staleness
                if (rule.skip_if_stale) {
                    int64_t sample_age = now_ms - sample.timestamp;
                    if (sample_age > rule.staleness_threshold_ms) {
                        TSDB_DEBUG("Skipping stale sample for {}: age={}ms > threshold={}ms", 
                                   rule.name, sample_age, rule.staleness_threshold_ms);
                        continue;  // Skip this stale sample
                    }
                }
                
                core::TimeSeries series(labels);
                series.add_sample(core::Sample(sample.timestamp, sample.value));
                // Write back to storage
                auto write_result = storage_->write(series);
                if (!write_result.ok()) {
                    TSDB_WARN("Failed to write derived metric: {}", write_result.error());
                } else {
                    TSDB_METRICS_DERIVED_SAMPLE();
                }
            }
        } else if (result.value.isScalar()) {
            // Handle scalar result (single series with just name?)
            core::Labels labels;
            labels.add("__name__", rule.name);
            core::TimeSeries series(labels);
            series.add_sample(core::Sample(result.value.getScalar().timestamp, result.value.getScalar().value));
            storage_->write(series);
        } else if (result.value.isMatrix()) {
            // Handle matrix result (from range queries)
            const auto& matrix = result.value.getMatrix();
            for (const auto& rangeSeries : matrix) {
                // Build labels for this series
                core::Labels labels;
                for (const auto& [k, v] : rangeSeries.metric.labels()) {
                    if (k == "__name__") continue;
                    
                    // Apply label filtering
                    if (!rule.keep_labels.empty()) {
                        if (std::find(rule.keep_labels.begin(), rule.keep_labels.end(), k) 
                            == rule.keep_labels.end()) {
                            continue;
                        }
                    } else if (!rule.drop_labels.empty()) {
                        if (std::find(rule.drop_labels.begin(), rule.drop_labels.end(), k) 
                            != rule.drop_labels.end()) {
                            continue;
                        }
                    }
                    
                    labels.add(k, v);
                }
                labels.add("__name__", rule.name);
                
                // Write each sample in the series
                for (const auto& sample : rangeSeries.samples) {
                    // Check staleness
                    if (rule.skip_if_stale) {
                        int64_t sample_age = now_ms - sample.timestamp();
                        if (sample_age > rule.staleness_threshold_ms) {
                            continue;
                        }
                    }
                    
                    core::TimeSeries ts(labels);
                    ts.add_sample(core::Sample(sample.timestamp(), sample.value()));
                    auto write_result = storage_->write(ts);
                    if (!write_result.ok()) {
                        TSDB_WARN("Failed to write derived metric: {}", write_result.error());
                    } else {
                        TSDB_METRICS_DERIVED_SAMPLE();
                    }
                }
            }
        }
        
        return core::Result<void>();
    } catch (const std::exception& e) {
        // Exception - also apply backoff
        rule.consecutive_failures++;
        int backoff_seconds = std::min(
            (1 << rule.consecutive_failures),
            rule.max_backoff_seconds
        );
        rule.backoff_until = now_ms + (backoff_seconds * 1000);
        TSDB_ERROR("Exception in execute_rule for {}: {} (backing off {}s)", 
                   rule.name, e.what(), backoff_seconds);
        return core::Result<void>("Exception: " + std::string(e.what()), core::Result<void>::ErrorTag{});
    } catch (...) {
        rule.consecutive_failures++;
        int backoff_seconds = std::min(
            (1 << rule.consecutive_failures),
            rule.max_backoff_seconds
        );
        rule.backoff_until = now_ms + (backoff_seconds * 1000);
        TSDB_ERROR("Unknown exception in execute_rule for {} (backing off {}s)", 
                   rule.name, backoff_seconds);
        return core::Result<void>("Unknown exception", core::Result<void>::ErrorTag{});
    }
}

} // namespace storage
} // namespace tsdb
