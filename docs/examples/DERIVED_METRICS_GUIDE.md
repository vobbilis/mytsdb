# Derived Metrics (Recording Rules) - Comprehensive Admin Guide

## Overview

**Derived Metrics** (also called Recording Rules) allow you to pre-compute expensive PromQL queries and save the results as new time series. This eliminates repeated computation, reduces query latency, and enables building metric hierarchies.

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                  DerivedMetricManager                        │
│  ┌─────────────────────────────────────────────────────────┐│
│  │              Scheduler Loop (Background Thread)         ││
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐     ││
│  │  │ Check Rule  │──│ Check Group │──│   Execute   │     ││
│  │  │  Intervals  │  │  Intervals  │  │   PromQL    │     ││
│  │  └─────────────┘  └─────────────┘  └─────────────┘     ││
│  └─────────────────────────────────────────────────────────┘│
│                              │                               │
│              ┌───────────────┼───────────────┐               │
│              ▼               ▼               ▼               │
│  ┌──────────────────────────────────────────────────────┐   │
│  │  Error Backoff │ Label Filter │ Staleness Check      │   │
│  └──────────────────────────────────────────────────────┘   │
│                              │                               │
│                              ▼                               │
│  ┌──────────────────────────────────────────────────────┐   │
│  │                   Write to Storage                    │   │
│  └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

---

## Data Structures

### RuleEvaluationType Enum

```cpp
// include/tsdb/storage/derived_metrics.h

enum class RuleEvaluationType {
    INSTANT,    // Execute at single point in time (default)
    RANGE       // Execute over a time range (for backfill)
};
```

### DerivedMetricRule Struct

```cpp
// include/tsdb/storage/derived_metrics.h

struct DerivedMetricRule {
    // === Core Fields ===
    std::string name;               // Output metric name
    std::string query;              // PromQL query to execute
    int64_t interval_ms;            // Execution interval (default 60s)
    int64_t last_execution_time{0}; // Timestamp of last execution (internal)
    
    // === Error Backoff Fields ===
    int consecutive_failures{0};    // Number of consecutive failures
    int64_t backoff_until{0};       // Don't execute until this timestamp (ms)
    int max_backoff_seconds{300};   // Maximum backoff duration (default 5 min)
    // Backoff formula: min(2^consecutive_failures, max_backoff_seconds)
    
    // === Label Transformation Fields ===
    std::vector<std::string> keep_labels;   // Whitelist: only keep these labels
    std::vector<std::string> drop_labels;   // Blacklist: drop these labels
    // Note: keep_labels takes precedence over drop_labels
    
    // === Staleness Handling Fields ===
    int64_t staleness_threshold_ms{300000}; // 5 minutes default
    bool skip_if_stale{false};              // Opt-in: don't write stale samples
    
    // === Range Query Fields ===
    RuleEvaluationType evaluation_type{RuleEvaluationType::INSTANT};
    int64_t range_duration_ms{0};   // For RANGE: how far back to query
    int64_t range_step_ms{0};       // For RANGE: step between samples
};
```

### RuleGroup Struct

```cpp
// include/tsdb/storage/derived_metrics.h

/**
 * A group of related rules with shared interval.
 * Rules within a group are evaluated sequentially (in order),
 * allowing later rules to depend on earlier rules' output.
 */
struct RuleGroup {
    std::string name;                       // Group name
    int64_t interval_ms{60000};             // Group evaluation interval
    std::vector<DerivedMetricRule> rules;   // Rules in execution order
    int64_t last_execution_time{0};         // Track last execution
};
```

### DerivedMetricManager Class

```cpp
// include/tsdb/storage/derived_metrics.h

class DerivedMetricManager {
public:
    DerivedMetricManager(
        std::shared_ptr<Storage> storage,
        std::shared_ptr<BackgroundProcessor> background_processor);
    ~DerivedMetricManager();
    
    // === Lifecycle ===
    void start();  // Start the scheduler thread
    void stop();   // Stop the scheduler thread
    
    // === Rule Management (Basic) ===
    void add_rule(
        const std::string& name, 
        const std::string& query, 
        int64_t interval_ms = 60000
    );
    
    // === Rule Management (With Label Filtering) ===
    void add_rule(
        const std::string& name, 
        const std::string& query, 
        int64_t interval_ms,
        const std::vector<std::string>& keep_labels,
        const std::vector<std::string>& drop_labels = {}
    );
    
    void clear_rules();  // Remove all rules
    
    // === Rule Group Management ===
    void add_group(const std::string& name, int64_t interval_ms = 60000);
    void add_rule_to_group(
        const std::string& group_name,
        const std::string& rule_name,
        const std::string& query
    );
    void clear_groups();  // Remove all groups

protected:
    // Execute a single rule (updates backoff state)
    core::Result<void> execute_rule(DerivedMetricRule& rule);

private:
    std::shared_ptr<Storage> storage_;
    std::shared_ptr<BackgroundProcessor> background_processor_;
    
    // Reusable adapter and engine (created once)
    std::unique_ptr<prometheus::storage::TSDBAdapter> adapter_;
    std::unique_ptr<prometheus::promql::Engine> engine_;
    
    std::vector<DerivedMetricRule> rules_;
    std::vector<RuleGroup> groups_;
    std::mutex rules_mutex_;
    
    std::thread scheduler_thread_;
    std::atomic<bool> running_{false};
    
    void scheduler_loop();  // Main scheduling logic
};
```

---

## Usage Examples

### Basic Setup

```cpp
#include "tsdb/storage/derived_metrics.h"
#include "tsdb/storage/storage_impl.h"
#include "tsdb/storage/background_processor.h"

// 1. Create storage and background processor
auto storage = std::make_shared<StorageImpl>(config);
storage->init(config);

auto bg_processor = std::make_shared<BackgroundProcessor>();

// 2. Create derived metric manager
auto derived_manager = std::make_shared<DerivedMetricManager>(
    storage, 
    bg_processor
);

// 3. Add rules
derived_manager->add_rule(
    "http_request_rate_5m",              // output metric name
    "rate(http_requests_total[5m])",     // PromQL expression
    60000                                 // evaluate every 60 seconds
);

// 4. Start the scheduler
derived_manager->start();

// ... application runs ...

// 5. Cleanup
derived_manager->stop();
```

### Rule with Label Filtering

```cpp
// Keep only specific labels in the output
derived_manager->add_rule(
    "cpu_usage_by_host",
    "avg by (host, datacenter) (cpu_usage)",
    30000,                   // 30 second interval
    {"host", "datacenter"},  // keep_labels: only these appear in output
    {}                       // drop_labels: not used when keep_labels is set
);

// Drop specific labels from output
derived_manager->add_rule(
    "request_rate_no_instance",
    "sum by (service, method, status) (rate(requests[5m]))",
    60000,
    {},                      // keep_labels: empty = keep all
    {"instance", "pod"}      // drop_labels: remove these
);
```

### Rule Groups (Sequential Execution)

```cpp
// Rule groups ensure sequential execution - later rules can use earlier outputs

// Create a group with 60s evaluation interval
derived_manager->add_group("error_rate_hierarchy", 60000);

// Rule 1: Compute error requests per service
derived_manager->add_rule_to_group(
    "error_rate_hierarchy",
    "service_errors",
    "sum by (service) (rate(http_requests{status=~\"5..\"}[5m]))"
);

// Rule 2: Compute total requests per service
derived_manager->add_rule_to_group(
    "error_rate_hierarchy",
    "service_total", 
    "sum by (service) (rate(http_requests[5m]))"
);

// Rule 3: Compute error rate using the derived metrics from Rules 1 & 2
// This DEPENDS on Rule 1 and 2 being executed first!
derived_manager->add_rule_to_group(
    "error_rate_hierarchy",
    "service_error_rate",
    "service_errors / service_total"
);
```

### Range Query (Backfill)

```cpp
// For backfilling historical data, use RANGE evaluation
DerivedMetricRule backfill_rule;
backfill_rule.name = "hourly_request_count";
backfill_rule.query = "sum(increase(requests[1h]))";
backfill_rule.interval_ms = 3600000;  // 1 hour

// Configure for range query
backfill_rule.evaluation_type = RuleEvaluationType::RANGE;
backfill_rule.range_duration_ms = 3600000;  // 1 hour of data
backfill_rule.range_step_ms = 60000;        // 1 sample per minute

// Add using internal method (advanced usage)
// Note: Typically use add_rule() for simpler cases
```

### Staleness Configuration

```cpp
// Skip writing samples when source data is too old
DerivedMetricRule stale_aware_rule;
stale_aware_rule.name = "real_time_cpu_avg";
stale_aware_rule.query = "avg(cpu_usage)";
stale_aware_rule.interval_ms = 10000;  // 10 seconds

// Staleness settings
stale_aware_rule.staleness_threshold_ms = 60000;  // 1 minute
stale_aware_rule.skip_if_stale = true;            // Skip if older than 1 min
```

### Custom Backoff Configuration

```cpp
// Configure error backoff behavior
DerivedMetricRule resilient_rule;
resilient_rule.name = "complex_aggregation";
resilient_rule.query = "histogram_quantile(0.99, rate(latency_bucket[5m]))";
resilient_rule.interval_ms = 60000;

// Backoff settings
resilient_rule.max_backoff_seconds = 600;  // Max 10 minutes between retries
// Backoff progression: 2s, 4s, 8s, 16s, 32s, 64s, 128s, 256s, 512s, 600s (max)
```

---

## Production Configuration Example

```cpp
void configure_production_derived_metrics(
    std::shared_ptr<DerivedMetricManager> manager
) {
    // === Request Rate Recording ===
    manager->add_rule(
        "request_rate_5m",
        "sum by (service, method, status) (rate(http_requests_total[5m]))",
        60000,
        {"service", "method", "status"},
        {}
    );
    
    // === Error Rate Recording ===
    manager->add_rule(
        "error_rate_5m",
        "sum by (service) (rate(http_requests_total{status=~\"5..\"}[5m])) / "
        "sum by (service) (rate(http_requests_total[5m]))",
        60000,
        {"service"},
        {}
    );
    
    // === Latency Percentiles ===
    manager->add_group("latency_percentiles", 60000);
    
    manager->add_rule_to_group(
        "latency_percentiles",
        "request_latency_p50",
        "histogram_quantile(0.50, rate(request_duration_bucket[5m]))"
    );
    
    manager->add_rule_to_group(
        "latency_percentiles",
        "request_latency_p90",
        "histogram_quantile(0.90, rate(request_duration_bucket[5m]))"
    );
    
    manager->add_rule_to_group(
        "latency_percentiles",
        "request_latency_p99",
        "histogram_quantile(0.99, rate(request_duration_bucket[5m]))"
    );
    
    // === Resource Usage ===
    manager->add_group("resource_metrics", 30000);
    
    manager->add_rule_to_group(
        "resource_metrics",
        "cpu_usage_avg",
        "avg by (host) (cpu_usage_percent)"
    );
    
    manager->add_rule_to_group(
        "resource_metrics",
        "memory_usage_avg",
        "avg by (host) (memory_usage_bytes / memory_total_bytes)"
    );
}
```

---

## Scheduler Logic

```
┌─────────────────────────────────────────────────────────────┐
│                   scheduler_loop()                           │
│                                                              │
│  while (running_) {                                          │
│    now = current_time_ms()                                   │
│                                                              │
│    // === Individual Rules ===                               │
│    for each rule in rules_ {                                 │
│      if (now < rule.backoff_until)                          │
│        continue  // Skip: in backoff period                  │
│                                                              │
│      if (now - rule.last_execution_time >= rule.interval_ms)│
│        execute_rule(rule)                                    │
│    }                                                         │
│                                                              │
│    // === Rule Groups ===                                    │
│    for each group in groups_ {                               │
│      if (now - group.last_execution_time >= group.interval)  │
│        for each rule in group.rules {  // Sequential!        │
│          if (now < rule.backoff_until)                      │
│            continue  // Individual backoff still applies     │
│          execute_rule(rule)                                  │
│        }                                                     │
│        group.last_execution_time = now                       │
│    }                                                         │
│                                                              │
│    sleep(100ms)  // Scheduler tick rate                      │
│  }                                                           │
└─────────────────────────────────────────────────────────────┘
```

---

## Error Backoff Behavior

| Consecutive Failures | Backoff Duration | Next Retry |
|---------------------|------------------|------------|
| 1 | 2s | 2 seconds after failure |
| 2 | 4s | 4 seconds after failure |
| 3 | 8s | 8 seconds after failure |
| 4 | 16s | 16 seconds after failure |
| 5 | 32s | 32 seconds after failure |
| 6 | 64s | 64 seconds after failure |
| 7 | 128s | 128 seconds after failure |
| 8 | 256s | 256 seconds after failure |
| 9+ | 300s (max) | 5 minutes after failure |

**On Success**: `consecutive_failures` resets to 0, `backoff_until` clears.

---

## Label Transformation Logic

```
┌─────────────────────────────────────────────────────────────┐
│                 Label Filtering Logic                        │
│                                                              │
│  if (!keep_labels.empty()) {                                 │
│    // Whitelist mode: ONLY keep specified labels             │
│    for each label in source.labels {                         │
│      if label.name in keep_labels                            │
│        output.add(label)                                     │
│    }                                                         │
│  } else if (!drop_labels.empty()) {                          │
│    // Blacklist mode: keep all EXCEPT specified labels       │
│    for each label in source.labels {                         │
│      if label.name NOT in drop_labels                        │
│        output.add(label)                                     │
│    }                                                         │
│  } else {                                                    │
│    // No filtering: copy all labels                          │
│    output = source.labels                                    │
│  }                                                           │
│                                                              │
│  // Always set output metric name                            │
│  output.set("__name__", rule.name)                           │
└─────────────────────────────────────────────────────────────┘
```

---

## Monitoring & Metrics

```prometheus
# Rule execution count
mytsdb_derived_rule_executions_total{rule="...",status="success|failure"}

# Rule execution duration
mytsdb_derived_rule_duration_seconds{rule="..."}

# Current backoff status
mytsdb_derived_rule_backoff_active{rule="..."}

# Samples written by derived metrics
mytsdb_derived_samples_written_total{rule="..."}

# Stale samples skipped
mytsdb_derived_stale_samples_skipped_total{rule="..."}
```

---

## Best Practices

### 1. Use Rule Groups for Dependencies

```cpp
// When Rule B needs Rule A's output, use a group
manager->add_group("chained_rules", 60000);
manager->add_rule_to_group("chained_rules", "base_metric", "...");
manager->add_rule_to_group("chained_rules", "derived_from_base", "...");
```

### 2. Match Recording Interval to Dashboard Refresh

```cpp
// If dashboard refreshes every 30s, record at 30s or faster
manager->add_rule("dashboard_metric", query, 30000);
```

### 3. Use Label Filtering to Reduce Cardinality

```cpp
// Pre-aggregate to fewer dimensions
manager->add_rule(
    "aggregated_requests",
    "sum by (service) (rate(requests[5m]))",
    60000,
    {"service"},  // Output only has 'service' label
    {}
);
```

### 4. Enable Staleness for Real-Time Metrics

```cpp
// For real-time dashboards, skip stale data
rule.skip_if_stale = true;
rule.staleness_threshold_ms = 60000;  // 1 minute
```

### 5. Configure Appropriate Backoff for Complex Queries

```cpp
// Complex histograms may fail more often, allow longer backoff
rule.max_backoff_seconds = 600;  // 10 minutes max
```
