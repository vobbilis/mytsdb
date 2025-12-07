# Prometheus Alert Manager Integration Research

## Overview

This directory contains comprehensive research on integrating Prometheus-compatible Alert Manager functionality into our TSDB system. The integration leverages our existing background processing system to provide alerting capabilities without impacting the main read and write paths.

## Research Documents

### 1. [Alert Rule Format](./01_alert_rule_format.md)
- Prometheus alert rule YAML format specification
- Alert rule structure and components
- Alert states (inactive, pending, firing, resolved)
- Templating support for labels and annotations
- Storage and evaluation requirements
- Compatibility considerations

### 2. [Alertmanager Architecture](./02_alertmanager_architecture.md)
- Alertmanager component architecture
- Processing pipeline (deduplication, grouping, inhibition, silencing, routing)
- Configuration format and structure
- Route configuration and matching
- Inhibition rules
- Receiver integrations (email, Slack, PagerDuty, webhooks, etc.)
- API endpoints for alert and silence management
- Alert state lifecycle and persistence
- Key features to implement

### 3. [Integration Design](./03_integration_design.md)
- Architecture for integrating with background processing system
- Component design (Rule Manager, Evaluation Engine, State Store, Router, Notification Manager, Silence Manager)
- Background task integration strategy
- API endpoint design
- Configuration structure
- Performance impact analysis (zero impact on read/write paths)
- Implementation phases
- Testing strategy
- Monitoring and metrics

## Key Findings

### Design Principles

1. **Zero Impact on Read/Write Paths**: All alert evaluation and processing happens in background threads
2. **Leverage Existing Infrastructure**: Uses the existing `BackgroundProcessor` for task scheduling
3. **Prometheus Compatibility**: 100% compatible with Prometheus alert rule format and Alertmanager API
4. **Scalability**: Designed for high-volume alert processing (10,000+ rules, 100,000+ concurrent alerts)
5. **Reliability**: Ensures alerts are not lost during failures
6. **✨ Feature Integration**: Native integration with Filter Rules, Aggregation Pushdown, and Derived Metrics

### Integration with Existing Features

The alerting system will leverage three core features already implemented:

#### 1. Filter Rules Integration
- Alert rules can reference filtered metric streams
- Drop/keep rules apply before alert evaluation
- Reduces noise by filtering at ingestion, not alerting

#### 2. Aggregation Pushdown Integration  
- Alert PromQL queries automatically benefit from ~785x speedup
- Complex aggregations (`sum`, `avg`, `min`, `max`, `count`) execute at storage layer
- Reduces alert evaluation latency and CPU usage

#### 3. Derived Metrics (Recording Rules) Integration
- Alerts can query pre-computed derived metrics
- Recording rules and alert rules share the same scheduler infrastructure
- Alert expressions can use recording rule outputs for efficiency

```
┌─────────────────────────────────────────────────────────────┐
│                    Unified Rule Processing                   │
│  ┌─────────────────┐  ┌─────────────────┐  ┌──────────────┐│
│  │  Filter Rules   │  │ Recording Rules │  │ Alert Rules  ││
│  │  (Drop/Keep)    │  │ (DerivedMetrics)│  │  (Alerting)  ││
│  └────────┬────────┘  └────────┬────────┘  └───────┬──────┘│
│           │                    │                   │        │
│           ▼                    ▼                   ▼        │
│  ┌─────────────────────────────────────────────────────────┐│
│  │              Aggregation Pushdown Engine                ││
│  │              (~785x Query Speedup)                      ││
│  └─────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────┘
```

### Architecture Highlights

```
Prometheus Server (Alert Rules) 
    ↓
Alert Evaluation Engine (Background Task)
    ↓
Alert Processing (Deduplication, Grouping, Inhibition, Silencing)
    ↓
Routing Engine
    ↓
Notification Manager
    ↓
Integrations (Email, Slack, Webhook, etc.)
```

### Background Processing Integration

The design adds four new background task types:

1. **ALERT_EVALUATION**: Periodic evaluation of alert rules using PromQL
2. **ALERT_PROCESSING**: Process and route alerts through the pipeline
3. **ALERT_NOTIFICATION**: Send notifications to configured receivers
4. **ALERT_CLEANUP**: Clean up old alerts and expired silences

All tasks run with appropriate priorities to avoid impacting core TSDB operations.

### Resource Requirements

| Component | Memory | CPU | Disk I/O |
|-----------|--------|-----|----------|
| Rule Storage | ~1MB per 1000 rules | Minimal | Config reload only |
| Alert State | ~1KB per active alert | Minimal | Optional persistence |
| Evaluation | Query-dependent | Periodic spikes | None |
| Notification | ~10KB per pending notification | Minimal | Logging only |

### API Compatibility

The design provides full Prometheus Alertmanager v2 API compatibility:

- `GET/POST /api/v2/alerts` - Alert management
- `GET/POST/DELETE /api/v2/silences` - Silence management
- `GET /-/healthy` - Health checks
- `POST /-/reload` - Configuration reload

## Implementation Phases

### Phase 1: Core Infrastructure (Weeks 1-2)
- Alert rule manager and YAML parsing
- Alert state store
- Basic evaluation engine
- Integration with background processor

### Phase 2: Alertmanager Features (Weeks 3-4)
- Routing engine
- Grouping and deduplication
- Inhibition rules
- Silence management

### Phase 3: Notifications (Week 5)
- Notification manager
- Webhook integration
- Email integration
- Retry logic

### Phase 4: API and Management (Week 6)
- REST API endpoints
- Configuration hot-reload
- Metrics and monitoring
- Documentation

### Phase 5: Advanced Features (Weeks 7-8)
- Additional integrations (Slack, PagerDuty, etc.)
- High availability support
- Alert history and analytics
- Performance optimization

## Dependencies

### Required Components

1. **PromQL Engine**: Required for alert expression evaluation (see `docs/planning/promql_research/`)
2. **Background Processor**: Already implemented in `src/tsdb/storage/background_processor.cpp`
3. **HTTP Server**: For REST API endpoints (may need to add if not present)
4. **YAML Parser**: For configuration and rule parsing

### Optional Components

1. **Persistent Storage**: For alert history and state recovery
2. **Template Engine**: For notification formatting (Go template syntax)
3. **Integration Libraries**: For various notification channels

### Integrated Components (Already Implemented)

These features are already implemented and will be leveraged by alerting:

| Component | Status | Integration ||
|-----------|--------|-------------|
| **Filter Rules** | ✅ Implemented | Drop/keep rules apply before alert evaluation |
| **Aggregation Pushdown** | ✅ Implemented | Alert queries auto-benefit from ~785x speedup |
| **Derived Metrics** | ✅ Implemented | Recording rules share scheduler infrastructure |
| **PromQL Engine** | ✅ Implemented | 100% function coverage, O(1) cache |
| **BackgroundProcessor** | ✅ Implemented | Task scheduling infrastructure |

#### Documentation
- [Filter Rules Guide](../examples/FILTER_RULES_GUIDE.md)
- [Aggregation Pushdown Guide](../examples/AGGREGATION_PUSHDOWN_GUIDE.md)
- [Derived Metrics Guide](../examples/DERIVED_METRICS_GUIDE.md)

## Performance Impact

### Read Path
- **Impact**: None
- **Reason**: Alert evaluation uses read-only queries through PromQL engine in background threads

### Write Path
- **Impact**: None
- **Reason**: Alert processing is completely decoupled from write path with no shared locks

### Background Processing
- **Impact**: Low priority tasks that don't block core operations
- **Resource Usage**: Configurable worker threads and queue sizes

## Compatibility

### Prometheus Alert Rules
- 100% compatible with Prometheus alert rule YAML format
- Supports all alert rule fields (`alert`, `expr`, `for`, `keep_firing_for`, `labels`, `annotations`)
- Full template support for labels and annotations

### Alertmanager Configuration
- Compatible with Alertmanager configuration format
- Supports routing trees with label matching
- Supports inhibition rules
- Supports all standard receiver types

### API
- Implements Alertmanager v2 API specification
- Compatible with existing Prometheus/Alertmanager tooling
- Supports standard alert and silence management operations

## Next Steps

1. **Review Research**: Review all research documents for completeness
2. **Design Approval**: Get approval on integration design approach
3. **PromQL Dependency**: Ensure PromQL engine implementation is complete
4. **Implementation Planning**: Create detailed implementation plan for Phase 1
5. **Prototype**: Build proof-of-concept for core evaluation engine
6. **Testing**: Design comprehensive test strategy

## References

- [Prometheus Alerting Rules Documentation](https://prometheus.io/docs/prometheus/latest/configuration/alerting_rules/)
- [Alertmanager Configuration Documentation](https://prometheus.io/docs/alerting/latest/configuration/)
- [Alertmanager API Documentation](https://prometheus.io/docs/alerting/latest/management_api/)
- [Prometheus GitHub Repository](https://github.com/prometheus/prometheus)
- [Alertmanager GitHub Repository](https://github.com/prometheus/alertmanager)

## Questions for Review

1. Should we implement alert persistence for state recovery after restarts?
2. What notification integrations should be prioritized (webhook, email, Slack)?
3. Should we support high availability/clustering in the initial implementation?
4. What alert retention period is appropriate (default: 7 days)?
5. Should we implement alert history and analytics features?
