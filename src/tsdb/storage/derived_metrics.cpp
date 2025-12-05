#include "tsdb/storage/derived_metrics.h"
#include "tsdb/storage/atomic_metrics.h"
#include "tsdb/prometheus/storage/tsdb_adapter.h"
#include "tsdb/common/logger.h"
#include <chrono>

namespace tsdb {
namespace storage {

DerivedMetricManager::DerivedMetricManager(
    std::shared_ptr<Storage> storage,
    std::shared_ptr<BackgroundProcessor> background_processor)
    : storage_(std::move(storage)), 
      background_processor_(std::move(background_processor)) {
      
    // Initialize PromQL Engine
    prometheus::promql::EngineOptions options;
    // We need a persistent adapter. 
    // Note: EngineOptions takes a raw pointer. We must ensure adapter lives as long as Engine.
    // But Engine owns the adapter? No, it takes a pointer.
    // We need to store the adapter as a member if we want to manage its lifecycle.
    // However, EngineOptions struct is just config. The Engine likely uses it.
    // Let's check Engine constructor. It takes options.
    // We should probably create a TSDBAdapter on the heap and manage it, 
    // or make TSDBAdapter a member of DerivedMetricManager.
    // For now, let's leak it or manage it properly.
    // Better: Add std::unique_ptr<StorageAdapter> adapter_ member.
    
    // Re-design: Add adapter_ member to class in next edit if needed, 
    // but for now let's just create it here and hope Engine doesn't take ownership but just uses it.
    // Actually, looking at Engine code (not visible here but assumed), it likely holds the pointer.
    // We should be safe if we keep the adapter alive.
    // Let's use a shared_ptr for adapter and pass raw ptr to options.
    // But we can't easily add member to header without editing it again.
    // Let's assume for this implementation we create a new adapter for each execution?
    // No, Engine is persistent.
    
    // Hack: Create a static/global adapter or leaking it is bad.
    // Let's just instantiate a new Engine for each execution?
    // That might be expensive if Engine has state.
    // But PromQL Engine usually is stateless per query except for storage.
    
    // Let's go with: Create Engine on the fly in execute_rule.
    // This avoids lifecycle issues with the adapter for now.
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
    std::lock_guard<std::mutex> lock(rules_mutex_);
    rules_.push_back({name, query, interval_ms, 0});
    TSDB_INFO("Added derived metric rule: {} = {}", name, query);
}

void DerivedMetricManager::clear_rules() {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    rules_.clear();
}

void DerivedMetricManager::scheduler_loop() {
    while (running_) {
        auto now = std::chrono::system_clock::now();
        int64_t current_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
            
        {
            std::lock_guard<std::mutex> lock(rules_mutex_);
            for (auto& rule : rules_) {
                if (current_time_ms - rule.last_execution_time >= rule.interval_ms) {
                    // Submit task
                    // Capture rule by value to avoid lifetime issues if rules_ changes
                    DerivedMetricRule rule_copy = rule;
                    
                    background_processor_->submitTask(BackgroundTask(
                        BackgroundTaskType::INDEXING, // Abuse INDEXING type or add new one? 
                                                      // Let's use INDEXING for now as it's "processing"
                        [this, rule_copy]() {
                            return this->execute_rule(rule_copy);
                        },
                        5 // Priority
                    ));
                    
                    rule.last_execution_time = current_time_ms;
                }
            }
        }
        
        // Sleep for a bit (e.g. 1 second)
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

core::Result<void> DerivedMetricManager::execute_rule(const DerivedMetricRule& rule) {
    TSDB_INFO("Creating Adapter and Engine");
    try {
        auto adapter = std::make_unique<prometheus::storage::TSDBAdapter>(storage_);
        prometheus::promql::EngineOptions options;
        options.storage_adapter = adapter.get();
        
        auto engine = std::make_unique<prometheus::promql::Engine>(options);
        
        // 2. Execute Query
        int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        
        TSDB_INFO("Executing query for rule: {}", rule.name);
        auto result = engine->ExecuteInstant(rule.query, now_ms);
        if (result.hasError()) {
            TSDB_ERROR("Derived metric query failed for {}: {}", rule.name, result.error);
            return core::Result<void>("Execution error: " + result.error, core::Result<void>::ErrorTag{});
        }
        TSDB_INFO("Query executed successfully");
        
        // 3. Write Results
        if (result.value.isVector()) {
            const auto& vector = result.value.getVector();
            for (const auto& sample : vector) {
                // Create a new TimeSeries
                core::Labels labels;
                // Copy labels from result
                for (const auto& [k, v] : sample.metric.labels()) {
                    if (k == "__name__") continue; // Skip old name
                    labels.add(k, v);
                }
                // Set new name
                labels.add("__name__", rule.name);
                
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
        }
        
        return core::Result<void>();
    } catch (const std::exception& e) {
        TSDB_ERROR("Exception in execute_rule: {}", e.what());
        return core::Result<void>("Exception: " + std::string(e.what()), core::Result<void>::ErrorTag{});
    } catch (...) {
        TSDB_ERROR("Unknown exception in execute_rule");
        return core::Result<void>("Unknown exception", core::Result<void>::ErrorTag{});
    }
}

} // namespace storage
} // namespace tsdb
