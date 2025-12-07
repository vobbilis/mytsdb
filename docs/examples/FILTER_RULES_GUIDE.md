# Filter Rules - Comprehensive Admin Guide

## Overview

MyTSDB's **FilteringStorage** provides a powerful filtering layer that intercepts write operations to drop, keep, or transform metrics based on configurable rules. This enables cost reduction, data governance, and cardinality management.

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                   Incoming Write Request                    │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    FilteringStorage                          │
│  ┌─────────────────────────────────────────────────────────┐│
│  │                    RuleManager                          ││
│  │  ┌─────────────────┐  ┌─────────────────┐               ││
│  │  │  RuleSet (RCU)  │──│  Drop Rules     │               ││
│  │  │  Lock-Free Read │  │  Mapping Rules  │               ││
│  │  └─────────────────┘  └─────────────────┘               ││
│  └─────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────┘
                              │
              ┌───────────────┴───────────────┐
              ▼                               ▼
      ┌──────────────┐                ┌──────────────┐
      │ DROP (Silent)│                │ WRITE Storage│
      └──────────────┘                └──────────────┘
```

---

## Data Structures

### RuleAction Enum

```cpp
// include/tsdb/storage/rule_manager.h

enum class RuleAction {
    DROP,   // Drop the metric silently
    KEEP,   // Keep the metric (default)
    MAP     // Apply label mapping transformation
};
```

### RuleSet (Immutable, RCU Pattern)

```cpp
class RuleSet {
public:
    // === Drop Rules: Multiple match types for performance ===
    
    // 1. Exact match on metric name (O(1) lookup)
    std::unordered_map<std::string, bool> drop_exact_names;
    
    // 2. Prefix match using Trie (O(n) where n = prefix length)
    struct TrieNode {
        std::unordered_map<char, std::unique_ptr<TrieNode>> children;
        bool is_leaf = false;  // If true, drop any metric with this prefix
    };
    std::unique_ptr<TrieNode> drop_prefix_names;
    
    // 3. Regex match (most flexible, slowest)
    std::vector<std::regex> drop_regex_names;
    
    // 4. Label-based rules
    struct LabelRules {
        std::unordered_map<std::string, bool> exact_values;  // exact match
        std::vector<std::regex> regex_values;                 // regex match
    };
    std::unordered_map<std::string, LabelRules> drop_label_rules;
    
    // === Mapping Rules ===
    struct MappingRule {
        std::string label_name;
        std::string old_value;
        std::string new_value;
    };
    std::vector<MappingRule> mapping_rules;
    
    // Core methods
    bool should_drop(const core::TimeSeries& series) const;
    core::TimeSeries apply_mapping(const core::TimeSeries& series) const;
};
```

### RuleManager (Thread-Safe Configuration)

```cpp
class RuleManager {
public:
    // Hot path: Lock-free read via RCU
    std::shared_ptr<RuleSet> get_current_rules() const;
    
    // Configuration (slow path, mutex protected)
    void add_drop_rule(const std::string& selector);
    void clear_rules();
    
private:
    std::shared_ptr<RuleSet> current_rules_;  // Atomic load/store
    std::mutex update_mutex_;
};
```

---

## Usage Examples

### Basic Setup

```cpp
#include "tsdb/storage/filtering_storage.h"
#include "tsdb/storage/rule_manager.h"
#include "tsdb/storage/storage_impl.h"

// 1. Create base storage
auto base_storage = std::make_shared<StorageImpl>(config);
base_storage->init(config);

// 2. Create rule manager
auto rule_manager = std::make_shared<RuleManager>();

// 3. Wrap with filtering storage
auto filtering_storage = std::make_shared<FilteringStorage>(
    base_storage, 
    rule_manager
);

// 4. Use filtering_storage for all writes
auto result = filtering_storage->write(series);
```

### Drop Rules Examples

#### Drop by Exact Metric Name

```cpp
// Drop all metrics named "debug_logs"
rule_manager->add_drop_rule("{__name__=\"debug_logs\"}");
```

#### Drop by Metric Name Prefix

```cpp
// Drop all metrics starting with "debug_" (uses Trie for efficiency)
rule_manager->add_drop_rule("{__name__=~\"debug_.*\"}");
```

#### Drop by Regex Pattern

```cpp
// Drop metrics matching complex patterns
rule_manager->add_drop_rule("{__name__=~\".*_test_.*\"}");
rule_manager->add_drop_rule("{__name__=~\"(temp|tmp|cache)_.*\"}");
```

#### Drop by Label Values

```cpp
// Drop all metrics from development environment
rule_manager->add_drop_rule("{env=\"dev\"}");

// Drop all metrics from specific instances
rule_manager->add_drop_rule("{instance=~\"test-.*\"}");

// Combine metric name and label matching
rule_manager->add_drop_rule("{__name__=\"http_requests_total\",status=\"500\",env=\"production\"}");
```

### Production Configuration Example

```cpp
// Common production filter configuration
void configure_production_filters(std::shared_ptr<RuleManager> rm) {
    // 1. Drop debug/test metrics
    rm->add_drop_rule("{__name__=~\"debug_.*\"}");
    rm->add_drop_rule("{__name__=~\"test_.*\"}");
    rm->add_drop_rule("{__name__=~\"_temp$\"}");
    
    // 2. Drop development environment data
    rm->add_drop_rule("{env=\"dev\"}");
    rm->add_drop_rule("{env=\"staging\"}");
    
    // 3. Drop high-cardinality problematic labels
    rm->add_drop_rule("{user_id=~\".+\"}");  // Drop user-specific metrics
    
    // 4. Drop verbose histogram buckets (keep summary)
    rm->add_drop_rule("{__name__=~\".*_bucket\",le=~\"0\\.[0-9]+\"}");
}
```

### Dynamic Rule Updates

```cpp
// Rules can be updated at runtime without restart
void update_filters_from_config(
    std::shared_ptr<RuleManager> rm, 
    const std::vector<std::string>& selectors
) {
    // Clear existing rules (mutex protected)
    rm->clear_rules();
    
    // Add new rules
    for (const auto& selector : selectors) {
        rm->add_drop_rule(selector);
    }
    
    // Changes take effect immediately on next write
    // Existing in-flight writes complete with old rules (RCU)
}
```

---

## Selector Syntax

The selector syntax follows Prometheus label matching:

| Syntax | Description | Example |
|--------|-------------|---------|
| `{label="value"}` | Exact match | `{env="prod"}` |
| `{label!="value"}` | Not equal | `{env!="dev"}` |
| `{label=~"regex"}` | Regex match | `{__name__=~"http_.*"}` |
| `{label!~"regex"}` | Negative regex | `{status!~"5.."}` |

### Supported Operators

| Operator | Type | Performance |
|----------|------|-------------|
| `=` | Exact match | O(1) hash lookup |
| `!=` | Not equal | O(1) hash lookup + negate |
| `=~` | Regex match | O(n) regex execution |
| `!~` | Negative regex | O(n) regex execution |

---

## Performance Characteristics

| Match Type | Time Complexity | Recommended Use |
|------------|-----------------|-----------------|
| Exact name | O(1) | High-volume drops |
| Prefix match | O(prefix length) | Drop metric families |
| Regex match | O(input length) | Complex patterns |
| Label exact | O(labels count) | Specific label values |
| Label regex | O(labels × pattern) | Flexible matching |

### Best Practices

1. **Use exact matches first** - Order rules from most specific to least
2. **Prefer prefix over regex** - `debug_.*` as prefix is faster than regex
3. **Batch rule updates** - Use `clear_rules()` then add all, not incremental
4. **Monitor drop rates** - Track `Dropped` counter in metrics

---

## Metrics Exposed

```prometheus
# Total samples dropped by filter rules
tsdb_filter_dropped_samples_total

# Rule check latency (nanoseconds)
tsdb_filter_rule_check_duration_ns

# Current active rules count
tsdb_filter_active_rules_count
```

---

## Integration with Prometheus Remote Write

```yaml
# prometheus.yml - Use FilteringStorage on receive path
remote_write:
  - url: "http://mytsdb:9090/api/v1/write"

# MyTSDB configuration
storage:
  filtering:
    enabled: true
    rules:
      - "{__name__=~\"debug_.*\"}"
      - "{env=\"dev\"}"
```
