# Prometheus Alert Rule Format Specification

## Overview

This document details the Prometheus alert rule format and how it can be integrated into our TSDB system. Alert rules are defined in YAML format and evaluated by Prometheus to generate alerts that are then sent to Alertmanager for processing.

## Alert Rule Structure

### Basic Format

```yaml
groups:
  - name: example
    labels:
      team: myteam
    rules:
      - alert: HighRequestLatency
        expr: job:request_latency_seconds:mean5m{job="myjob"} > 0.5
        for: 10m
        keep_firing_for: 5m
        labels:
          severity: page
        annotations:
          summary: High request latency
```

### Key Components

#### 1. **Groups**
- Alert rules are organized into groups
- Each group has a `name` and optional `labels`
- Groups can have common labels applied to all rules within

#### 2. **Alert Rule Fields**

| Field | Required | Description |
|-------|----------|-------------|
| `alert` | Yes | Name of the alert (string identifier) |
| `expr` | Yes | PromQL expression that triggers the alert |
| `for` | No | Duration the condition must be true before firing |
| `keep_firing_for` | No | Duration to keep alert firing after condition stops |
| `labels` | No | Additional labels attached to the alert |
| `annotations` | No | Informational labels (description, runbook, etc.) |

#### 3. **Alert States**

Alerts transition through three states:

1. **Inactive**: Expression evaluates to false
2. **Pending**: Expression is true but `for` duration not yet met
3. **Firing**: Expression true for longer than `for` duration

#### 4. **Templating**

Alert labels and annotations support templating:

```yaml
annotations:
  summary: "Instance {{ $labels.instance }} down"
  description: "{{ $labels.instance }} of job {{ $labels.job }} has been down for more than 5 minutes."
  value: "Current value: {{ $value }}"
```

**Template Variables:**
- `$labels.<labelname>` - Access alert label values
- `$externalLabels` - Access configured external labels
- `$value` - Numeric value of the alert expression

### Example Alert Rules

#### Instance Down Alert
```yaml
- alert: InstanceDown
  expr: up == 0
  for: 5m
  labels:
    severity: page
  annotations:
    summary: "Instance {{ $labels.instance }} down"
    description: "{{ $labels.instance }} of job {{ $labels.job }} has been down for more than 5 minutes."
```

#### High Request Latency Alert
```yaml
- alert: APIHighRequestLatency
  expr: api_http_request_latencies_second{quantile="0.5"} > 1
  for: 10m
  annotations:
    summary: "High request latency on {{ $labels.instance }}"
    description: "{{ $labels.instance }} has a median request latency above 1s (current value: {{ $value }}s)"
```

## Integration Considerations

### Storage Requirements

1. **Alert Rule Storage**
   - YAML configuration files
   - In-memory parsed rule structures
   - Rule group metadata

2. **Alert State Storage**
   - Current state (inactive/pending/firing)
   - State transition timestamps
   - Active alert instances with labels

3. **Historical Data**
   - Alert firing history
   - State transition logs
   - Evaluation metrics

### Evaluation Engine Requirements

1. **PromQL Expression Evaluation**
   - Must support full PromQL syntax
   - Query time series data from storage
   - Evaluate expressions at regular intervals

2. **State Management**
   - Track alert state transitions
   - Implement `for` duration logic
   - Implement `keep_firing_for` logic
   - Handle alert resolution

3. **Label Management**
   - Merge group labels with rule labels
   - Support label templating
   - Handle label conflicts

### Performance Considerations

1. **Evaluation Frequency**
   - Configurable evaluation interval (default: 1m)
   - Batch evaluation of rules in same group
   - Avoid impacting read/write paths

2. **Resource Usage**
   - Memory for alert state tracking
   - CPU for PromQL evaluation
   - Disk for rule storage and history

## Compatibility Notes

- **Full Prometheus Compatibility**: Alert rule format must be 100% compatible with Prometheus
- **YAML Parsing**: Use standard YAML parser with validation
- **PromQL Support**: Requires complete PromQL implementation (see PromQL research)
- **Template Engine**: Implement Go template syntax for annotations

## Next Steps

1. Design alert rule storage mechanism
2. Design alert state management system
3. Integrate with PromQL evaluation engine
4. Design alert history and metrics collection
5. Define API endpoints for rule management
