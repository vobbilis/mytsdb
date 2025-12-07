# Integration Design: Alertmanager with Background Processing System

## Overview

This document outlines how to integrate Prometheus-compatible Alertmanager functionality into our TSDB system using the existing background processing infrastructure without impacting the main read and write paths.

## Design Principles

1. **Zero Impact on Read/Write Paths**: All alert evaluation and processing happens in background threads
2. **Leverage Existing Infrastructure**: Use the existing `BackgroundProcessor` for task scheduling
3. **Prometheus Compatibility**: 100% compatible with Prometheus alert rule format and Alertmanager API
4. **Scalability**: Design for high-volume alert processing
5. **Reliability**: Ensure alerts are not lost during failures
6. **Feature Integration**: Native integration with Filter Rules, Aggregation Pushdown, and Derived Metrics

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                        StorageImpl                               │
│  ┌────────────┐  ┌────────────┐  ┌────────────────────────┐    │
│  │ Write Path │  │ Read Path  │  │  Query Engine (PromQL) │    │
│  └────────────┘  └────────────┘  └────────────────────────┘    │
│                                            │                     │
│                                            │ Query for alerts    │
└────────────────────────────────────────────┼─────────────────────┘
                                             ▼
┌─────────────────────────────────────────────────────────────────┐
│                   Background Processor                           │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │              Alert Evaluation Worker                      │  │
│  │  • Periodic rule evaluation (default: 1m interval)       │  │
│  │  • Execute PromQL queries for alert expressions          │  │
│  │  • Manage alert state transitions                        │  │
│  │  • Priority: LOW (does not block other tasks)            │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                  │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │           Alert Processing Worker                         │  │
│  │  • Process incoming alerts (from evaluation)             │  │
│  │  • Deduplication and grouping                            │  │
│  │  • Apply inhibition and silencing rules                  │  │
│  │  • Route alerts to receivers                             │  │
│  │  • Priority: MEDIUM                                       │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                  │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │          Notification Dispatch Worker                     │  │
│  │  • Send notifications to integrations                     │  │
│  │  • Handle retries and backoff                            │  │
│  │  • Track notification status                             │  │
│  │  • Priority: HIGH (time-sensitive)                        │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                  │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │              Cleanup Worker                               │  │
│  │  • Clean up resolved alerts                              │  │
│  │  • Remove expired silences                               │  │
│  │  • Archive old notification logs                         │  │
│  │  • Priority: LOW                                          │  │
│  └──────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│                    Alert Manager Components                      │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐  │
│  │ Rule Manager │  │ Alert Store  │  │  Notification Queue  │  │
│  └──────────────┘  └──────────────┘  └──────────────────────┘  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐  │
│  │Silence Store │  │ Router       │  │  Integration Manager │  │
│  └──────────────┘  └──────────────┘  └──────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

## Component Design

### 1. Alert Rule Manager

**Responsibility**: Manage alert rule lifecycle

```cpp
class AlertRuleManager {
public:
    // Load rules from YAML configuration
    Result<void> loadRules(const std::string& config_path);
    
    // Reload rules (hot reload)
    Result<void> reloadRules();
    
    // Get all rule groups
    std::vector<RuleGroup> getRuleGroups() const;
    
    // Get specific rule
    std::optional<AlertRule> getRule(const std::string& name) const;
    
private:
    std::vector<RuleGroup> rule_groups_;
    std::mutex rules_mutex_;
    std::string config_path_;
};

struct RuleGroup {
    std::string name;
    std::map<std::string, std::string> labels;
    std::chrono::seconds interval{60};  // Evaluation interval
    std::vector<AlertRule> rules;
};

struct AlertRule {
    std::string alert_name;
    std::string expr;  // PromQL expression
    std::chrono::seconds for_duration{0};
    std::chrono::seconds keep_firing_for{0};
    std::map<std::string, std::string> labels;
    std::map<std::string, std::string> annotations;
};
```

### 2. Alert Evaluation Engine

**Responsibility**: Evaluate alert rules and manage state transitions

```cpp
class AlertEvaluationEngine {
public:
    AlertEvaluationEngine(
        std::shared_ptr<PromQLEngine> promql_engine,
        std::shared_ptr<AlertStateStore> state_store
    );
    
    // Evaluate all rules in a group
    Result<void> evaluateRuleGroup(const RuleGroup& group);
    
    // Evaluate single rule
    Result<std::vector<Alert>> evaluateRule(const AlertRule& rule);
    
private:
    std::shared_ptr<PromQLEngine> promql_engine_;
    std::shared_ptr<AlertStateStore> state_store_;
    
    // Check if alert should transition to firing
    bool shouldFire(const Alert& alert, const AlertRule& rule);
    
    // Check if alert should be resolved
    bool shouldResolve(const Alert& alert);
};

enum class AlertState {
    INACTIVE,
    PENDING,
    FIRING,
    RESOLVED
};

struct Alert {
    std::string alert_name;
    std::map<std::string, std::string> labels;
    std::map<std::string, std::string> annotations;
    AlertState state;
    std::chrono::system_clock::time_point starts_at;
    std::chrono::system_clock::time_point ends_at;
    std::chrono::system_clock::time_point last_sent_at;
    double value;  // Expression result value
    std::string generator_url;
};
```

### 3. Alert State Store

**Responsibility**: Persist and manage alert state

```cpp
class AlertStateStore {
public:
    // Store alert state
    Result<void> storeAlert(const Alert& alert);
    
    // Get alert by fingerprint
    std::optional<Alert> getAlert(const std::string& fingerprint);
    
    // Get all active alerts
    std::vector<Alert> getActiveAlerts();
    
    // Get alerts by state
    std::vector<Alert> getAlertsByState(AlertState state);
    
    // Update alert state
    Result<void> updateAlertState(
        const std::string& fingerprint,
        AlertState new_state
    );
    
    // Remove resolved alerts older than retention period
    Result<void> cleanupOldAlerts(std::chrono::seconds retention);
    
private:
    // In-memory cache for fast access
    tbb::concurrent_hash_map<std::string, Alert> active_alerts_;
    
    // Persistent storage (optional)
    std::unique_ptr<AlertPersistence> persistence_;
    
    // Generate fingerprint from alert labels
    std::string generateFingerprint(const Alert& alert);
};
```

### 4. Alert Router

**Responsibility**: Route alerts to appropriate receivers

```cpp
class AlertRouter {
public:
    AlertRouter(const RouteConfig& config);
    
    // Route alert to receivers
    std::vector<std::string> routeAlert(const Alert& alert);
    
    // Reload routing configuration
    Result<void> reloadConfig(const RouteConfig& config);
    
private:
    RouteConfig config_;
    std::mutex config_mutex_;
    
    // Traverse routing tree
    std::vector<std::string> traverseRoutes(
        const Alert& alert,
        const Route& route
    );
    
    // Check if alert matches route
    bool matchesRoute(const Alert& alert, const Route& route);
};

struct Route {
    std::string receiver;
    std::vector<std::string> group_by;
    std::chrono::seconds group_wait{10};
    std::chrono::seconds group_interval{10};
    std::chrono::seconds repeat_interval{3600 * 12};
    std::map<std::string, std::string> match;
    std::map<std::string, std::string> match_re;
    bool continue_matching{false};
    std::vector<Route> routes;  // Child routes
};
```

### 5. Notification Manager

**Responsibility**: Send notifications to integrations

```cpp
class NotificationManager {
public:
    // Send notification to receiver
    Result<void> sendNotification(
        const std::string& receiver_name,
        const std::vector<Alert>& alerts
    );
    
    // Register integration
    void registerIntegration(
        const std::string& type,
        std::unique_ptr<NotificationIntegration> integration
    );
    
private:
    std::map<std::string, ReceiverConfig> receivers_;
    std::map<std::string, std::unique_ptr<NotificationIntegration>> integrations_;
    
    // Notification queue for retry logic
    std::queue<NotificationTask> notification_queue_;
    std::mutex queue_mutex_;
};

// Base class for notification integrations
class NotificationIntegration {
public:
    virtual ~NotificationIntegration() = default;
    
    virtual Result<void> send(
        const std::vector<Alert>& alerts,
        const std::map<std::string, std::string>& config
    ) = 0;
};

// Example integrations
class EmailIntegration : public NotificationIntegration { /* ... */ };
class SlackIntegration : public NotificationIntegration { /* ... */ };
class WebhookIntegration : public NotificationIntegration { /* ... */ };
```

### 6. Silence Manager

**Responsibility**: Manage alert silences

```cpp
class SilenceManager {
public:
    // Create silence
    Result<std::string> createSilence(const Silence& silence);
    
    // Delete silence
    Result<void> deleteSilence(const std::string& silence_id);
    
    // Get silence
    std::optional<Silence> getSilence(const std::string& silence_id);
    
    // List all silences
    std::vector<Silence> listSilences();
    
    // Check if alert is silenced
    bool isSilenced(const Alert& alert);
    
    // Cleanup expired silences
    Result<void> cleanupExpiredSilences();
    
private:
    tbb::concurrent_hash_map<std::string, Silence> silences_;
};

struct Silence {
    std::string id;
    std::vector<Matcher> matchers;
    std::chrono::system_clock::time_point starts_at;
    std::chrono::system_clock::time_point ends_at;
    std::string created_by;
    std::string comment;
};

struct Matcher {
    std::string name;
    std::string value;
    bool is_regex;
};
```

## Background Task Integration

### Task Types

Add new task types to `BackgroundTaskType`:

```cpp
enum class BackgroundTaskType {
    COMPRESSION,
    INDEXING,
    FLUSH,
    CLEANUP,
    // New alert-related tasks
    ALERT_EVALUATION,      // Evaluate alert rules
    ALERT_PROCESSING,      // Process and route alerts
    ALERT_NOTIFICATION,    // Send notifications
    ALERT_CLEANUP          // Cleanup old alerts/silences
};
```

### Task Scheduling

```cpp
// In StorageImpl initialization
void StorageImpl::initializeAlertManager() {
    if (!config_.alert_config.enabled) {
        return;
    }
    
    alert_rule_manager_ = std::make_unique<AlertRuleManager>();
    alert_evaluation_engine_ = std::make_unique<AlertEvaluationEngine>(
        promql_engine_, alert_state_store_
    );
    alert_router_ = std::make_unique<AlertRouter>(config_.alert_config.route);
    notification_manager_ = std::make_unique<NotificationManager>();
    silence_manager_ = std::make_unique<SilenceManager>();
    
    // Load alert rules
    auto result = alert_rule_manager_->loadRules(config_.alert_config.rules_path);
    if (!result.ok()) {
        TSDB_ERROR("Failed to load alert rules: {}", result.error());
        return;
    }
    
    // Schedule periodic alert evaluation
    scheduleAlertEvaluation();
}

void StorageImpl::scheduleAlertEvaluation() {
    auto eval_task = [this]() -> core::Result<void> {
        auto groups = alert_rule_manager_->getRuleGroups();
        for (const auto& group : groups) {
            auto result = alert_evaluation_engine_->evaluateRuleGroup(group);
            if (!result.ok()) {
                TSDB_ERROR("Alert evaluation failed: {}", result.error());
            }
        }
        
        // Reschedule for next interval
        scheduleAlertEvaluation();
        return core::Result<void>::ok();
    };
    
    background_processor_->submitTask(
        BackgroundTask(BackgroundTaskType::ALERT_EVALUATION, eval_task, 5)
    );
}
```

## API Endpoints

### REST API Design

```cpp
// Alert Management API
class AlertManagerAPI {
public:
    // GET /api/v2/alerts
    std::string listAlerts(const HttpRequest& req);
    
    // POST /api/v2/alerts
    std::string createAlerts(const HttpRequest& req);
    
    // GET /api/v2/alerts/groups
    std::string getAlertGroups(const HttpRequest& req);
    
    // GET /api/v2/silences
    std::string listSilences(const HttpRequest& req);
    
    // POST /api/v2/silences
    std::string createSilence(const HttpRequest& req);
    
    // GET /api/v2/silence/{id}
    std::string getSilence(const HttpRequest& req);
    
    // DELETE /api/v2/silence/{id}
    std::string deleteSilence(const HttpRequest& req);
    
    // GET /-/healthy
    std::string healthCheck(const HttpRequest& req);
    
    // POST /-/reload
    std::string reloadConfig(const HttpRequest& req);
    
private:
    std::shared_ptr<AlertStateStore> alert_store_;
    std::shared_ptr<SilenceManager> silence_manager_;
    std::shared_ptr<AlertRuleManager> rule_manager_;
};
```

## Configuration

### Alert Manager Configuration

```cpp
struct AlertManagerConfig {
    bool enabled{false};
    std::string rules_path;  // Path to alert rules YAML
    std::string config_path;  // Path to alertmanager config YAML
    
    // Evaluation settings
    std::chrono::seconds evaluation_interval{60};
    std::chrono::seconds evaluation_timeout{30};
    
    // Storage settings
    std::chrono::seconds alert_retention{7 * 24 * 3600};  // 7 days
    std::chrono::seconds silence_retention{30 * 24 * 3600};  // 30 days
    
    // Notification settings
    uint32_t notification_workers{2};
    uint32_t max_notification_retries{3};
    std::chrono::seconds notification_timeout{10};
    
    // Routing configuration
    RouteConfig route;
    std::vector<ReceiverConfig> receivers;
    std::vector<InhibitRule> inhibit_rules;
};
```

## Performance Impact Analysis

### Read Path Impact
- **None**: Alert evaluation uses read-only queries through PromQL engine
- Queries are executed in background threads with low priority

### Write Path Impact
- **None**: Alert processing is completely decoupled from write path
- No locks shared between write path and alert processing

### Resource Usage

| Component | Memory | CPU | Disk I/O |
|-----------|--------|-----|----------|
| Rule Storage | ~1MB per 1000 rules | Minimal | Config reload only |
| Alert State | ~1KB per active alert | Minimal | Optional persistence |
| Evaluation | Query-dependent | Periodic spikes | None |
| Notification | ~10KB per pending notification | Minimal | Logging only |

### Scalability

- **Alert Rules**: Supports 10,000+ rules
- **Active Alerts**: Supports 100,000+ concurrent alerts
- **Evaluation**: Parallelizable across rule groups
- **Notifications**: Queue-based with backpressure

## Integration with Existing Features

The alerting system is designed to leverage three core features that are already implemented:

### 1. Filter Rules Integration

Filter rules apply at ingestion time, before alert evaluation:

```cpp
// Alert evaluation receives pre-filtered data
class AlertEvaluationEngine {
public:
    // PromQL queries automatically see filtered data
    // Drop rules reduce noise before alerting
    Result<std::vector<Alert>> evaluateRule(const AlertRule& rule) {
        // Queries execute against FilteringStorage
        // Already-dropped metrics won't trigger alerts
        auto result = promql_engine_->ExecuteInstant(rule.expr, now_ms);
        // ...
    }
};
```

**Benefits:**
- Reduce alert noise by dropping debug/test metrics at ingestion
- Lower evaluation overhead (fewer series to scan)
- Consistent filtering across recording rules and alert rules

### 2. Aggregation Pushdown Integration

Alert PromQL queries automatically benefit from aggregation pushdown:

```promql
# These alert expressions get ~785x speedup from pushdown:
alert: HighErrorRate
expr: sum(rate(http_errors[5m])) / sum(rate(http_requests[5m])) > 0.05

alert: HighMemoryUsage  
expr: avg(memory_usage_bytes) / avg(memory_limit_bytes) > 0.9

alert: TooFewInstances
expr: count(up{job="myservice"}) < 3
```

**Benefits:**
- Faster alert evaluation (10ms vs 7,850ms for complex aggregations)
- Lower CPU usage during evaluation
- More responsive alerting

### 3. Derived Metrics (Recording Rules) Integration

Alerts can query pre-computed recording rules for efficiency:

```yaml
# Recording rules (DerivedMetricManager)
groups:
  - name: recording_rules
    interval: 60s
    rules:
      - record: job:http_inprogress_requests:sum
        expr: sum by (job) (http_inprogress_requests)
      - record: job:http_request_duration:p99
        expr: histogram_quantile(0.99, rate(http_request_duration_bucket[5m]))

# Alert rules reference recording rules (efficient!)
groups:
  - name: alerting_rules
    rules:
      - alert: TooManyInProgressRequests
        expr: job:http_inprogress_requests:sum > 1000  # Uses recording rule
        for: 5m
      - alert: HighLatency
        expr: job:http_request_duration:p99 > 1.0     # Uses recording rule
        for: 10m
```

**Architecture:**

```
┌─────────────────────────────────────────────────────────────┐
│                  Unified Rule Scheduler                      │
│  ┌─────────────────────────────────────────────────────────┐│
│  │         DerivedMetricManager (Recording Rules)          ││
│  │  • Computes recording rules on schedule                 ││
│  │  • Stores results as new time series                    ││
│  └─────────────────────────────────────────────────────────┘│
│                              │                               │
│                              ▼                               │
│  ┌─────────────────────────────────────────────────────────┐│
│  │            AlertEvaluationEngine                        ││
│  │  • Queries can use recording rule outputs               ││
│  │  • Shared BackgroundProcessor infrastructure            ││
│  │  • Same error backoff patterns                          ││
│  └─────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────┘
```

**Shared Infrastructure:**

| Component | Recording Rules | Alert Rules |
|-----------|-----------------|-------------|
| Scheduler | DerivedMetricManager | AlertEvaluationEngine |
| Worker | BackgroundProcessor | BackgroundProcessor |
| Query Engine | PromQL Engine | PromQL Engine |
| Error Handling | Exponential backoff | Exponential backoff |
| State Tracking | last_execution_time | last_evaluation_time |

## Implementation Phases

### Phase 1: Core Infrastructure
1. Alert rule manager and YAML parsing
2. Alert state store
3. Basic evaluation engine
4. Integration with background processor

### Phase 2: Alertmanager Features
1. Routing engine
2. Grouping and deduplication
3. Inhibition rules
4. Silence management

### Phase 3: Notifications
1. Notification manager
2. Webhook integration
3. Email integration
4. Retry logic

### Phase 4: API and Management
1. REST API endpoints
2. Configuration hot-reload
3. Metrics and monitoring
4. Documentation

### Phase 5: Advanced Features
1. Additional integrations (Slack, PagerDuty, etc.)
2. High availability support
3. Alert history and analytics
4. Performance optimization

## Testing Strategy

1. **Unit Tests**: Test individual components
2. **Integration Tests**: Test alert evaluation and routing
3. **Performance Tests**: Verify no impact on read/write paths
4. **Compatibility Tests**: Verify Prometheus compatibility
5. **Load Tests**: Test with high alert volumes

## Monitoring and Metrics

Track the following metrics:

- Alert evaluation duration
- Number of active alerts by state
- Notification success/failure rates
- Notification latency
- Queue sizes
- Background task execution times

## Next Steps

1. Review and approve design
2. Implement Phase 1 (Core Infrastructure)
3. Add unit tests
4. Integrate with existing background processor
5. Implement API endpoints
6. Add documentation
