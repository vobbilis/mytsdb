# Prometheus Alertmanager Configuration and Architecture

## Overview

This document details the Prometheus Alertmanager configuration format, architecture, and key features that need to be implemented for full compatibility.

## Alertmanager Architecture

### Core Components

```
┌─────────────────────────────────────────────────────────────┐
│                      Prometheus Server                       │
│  ┌──────────────┐        ┌─────────────────────────────┐   │
│  │ Alert Rules  │───────▶│  Alert Evaluation Engine    │   │
│  │  (YAML)      │        │  (PromQL-based)             │   │
│  └──────────────┘        └─────────────────────────────┘   │
│                                    │                         │
│                                    │ Firing Alerts           │
└────────────────────────────────────┼─────────────────────────┘
                                     ▼
┌─────────────────────────────────────────────────────────────┐
│                      Alertmanager                            │
│  ┌──────────────┐                                           │
│  │  Dispatcher  │◀──── Receives alerts from Prometheus      │
│  └──────┬───────┘                                           │
│         │                                                    │
│         ▼                                                    │
│  ┌──────────────┐        ┌─────────────────────────────┐   │
│  │ Deduplication│───────▶│      Grouping Engine        │   │
│  └──────────────┘        └─────────────────────────────┘   │
│                                    │                         │
│                                    ▼                         │
│  ┌──────────────┐        ┌─────────────────────────────┐   │
│  │  Inhibition  │───────▶│     Silencing Engine        │   │
│  │    Rules     │        │                             │   │
│  └──────────────┘        └─────────────────────────────┘   │
│                                    │                         │
│                                    ▼                         │
│  ┌──────────────┐        ┌─────────────────────────────┐   │
│  │ Routing Tree │───────▶│   Notification Engine       │   │
│  │              │        │                             │   │
│  └──────────────┘        └─────────────────────────────┘   │
│                                    │                         │
└────────────────────────────────────┼─────────────────────────┘
                                     │
                    ┌────────────────┼────────────────┐
                    ▼                ▼                ▼
              ┌─────────┐      ┌─────────┐     ┌─────────┐
              │  Email  │      │  Slack  │     │ Webhook │
              └─────────┘      └─────────┘     └─────────┘
```

### Processing Pipeline

1. **Reception**: Alerts arrive from Prometheus via HTTP POST
2. **Deduplication**: Identical alerts are merged
3. **Grouping**: Related alerts are batched together
4. **Inhibition**: Lower-priority alerts suppressed by higher-priority ones
5. **Silencing**: Temporarily muted alerts are filtered out
6. **Routing**: Alerts routed to appropriate receivers based on labels
7. **Notification**: Alerts sent to configured integrations

## Configuration Format

### Top-Level Structure

```yaml
global:
  # Global settings
  resolve_timeout: 5m
  smtp_from: 'alertmanager@example.com'
  
route:
  # Root routing tree
  receiver: 'default-receiver'
  group_by: ['alertname', 'cluster']
  group_wait: 10s
  group_interval: 10s
  repeat_interval: 12h
  routes:
    - match:
        severity: critical
      receiver: 'pager'
    - match_re:
        service: ^(foo|bar)$
      receiver: 'team-X'

inhibit_rules:
  - source_matchers:
      - severity="critical"
    target_matchers:
      - severity="warning"
    equal: ['alertname', 'cluster']

receivers:
  - name: 'default-receiver'
    email_configs:
      - to: 'team@example.com'
  - name: 'pager'
    pagerduty_configs:
      - service_key: '<key>'
```

### Route Configuration

Routes define a tree structure for alert routing:

```yaml
route:
  receiver: 'default'           # Default receiver
  group_by: ['alertname']       # Group alerts by these labels
  group_wait: 10s               # Wait before sending first notification
  group_interval: 10s           # Wait before sending batch updates
  repeat_interval: 12h          # Wait before re-sending notification
  
  routes:
    # Child routes
    - match:
        severity: critical
      receiver: 'pager'
      continue: false           # Stop matching after this route
      
    - match_re:
        service: ^(api|web)$
      receiver: 'team-backend'
      group_by: ['service', 'instance']
```

**Key Parameters:**

- `group_by`: Labels to group alerts by
- `group_wait`: Initial wait before sending grouped notification
- `group_interval`: Wait time for subsequent batches
- `repeat_interval`: How often to resend notifications
- `continue`: Whether to continue matching sibling routes

### Inhibition Rules

Inhibition suppresses notifications for certain alerts when others are firing:

```yaml
inhibit_rules:
  - source_matchers:
      - alertname="NodeDown"
    target_matchers:
      - alertname="InstanceDown"
    equal: ['node']
```

**Logic**: If `NodeDown` is firing for a node, suppress `InstanceDown` alerts for the same node.

### Receivers and Integrations

Alertmanager supports multiple notification integrations:

```yaml
receivers:
  - name: 'team-email'
    email_configs:
      - to: 'team@example.com'
        from: 'alertmanager@example.com'
        smarthost: 'smtp.example.com:587'
        
  - name: 'slack-notifications'
    slack_configs:
      - api_url: 'https://hooks.slack.com/services/...'
        channel: '#alerts'
        title: 'Alert: {{ .GroupLabels.alertname }}'
        text: '{{ range .Alerts }}{{ .Annotations.description }}{{ end }}'
        
  - name: 'pagerduty-oncall'
    pagerduty_configs:
      - service_key: '<integration-key>'
        
  - name: 'webhook-custom'
    webhook_configs:
      - url: 'http://example.com/webhook'
        send_resolved: true
```

**Supported Integrations:**
- Email
- Slack
- PagerDuty
- Webhook
- Discord
- Microsoft Teams
- OpsGenie
- VictorOps
- WeChat
- Telegram
- SNS
- And more...

## Alertmanager API Endpoints

### Health and Status

```
GET  /-/healthy          # Health check
GET  /-/ready            # Readiness check
POST /-/reload           # Reload configuration
```

### Alert Management

```
GET  /api/v2/alerts                    # List all alerts
POST /api/v2/alerts                    # Create/update alerts
GET  /api/v2/alerts/groups             # Get alert groups
```

**Alert Object Structure:**
```json
{
  "labels": {
    "alertname": "HighRequestLatency",
    "severity": "warning",
    "instance": "server1"
  },
  "annotations": {
    "summary": "High latency detected",
    "description": "Request latency is above threshold"
  },
  "startsAt": "2025-11-27T10:00:00Z",
  "endsAt": "0001-01-01T00:00:00Z",
  "generatorURL": "http://prometheus:9090/graph?..."
}
```

### Silence Management

```
GET    /api/v2/silences              # List all silences
POST   /api/v2/silences              # Create silence
GET    /api/v2/silence/{id}          # Get silence details
DELETE /api/v2/silence/{id}          # Delete silence
```

**Silence Object Structure:**
```json
{
  "matchers": [
    {
      "name": "alertname",
      "value": "HighRequestLatency",
      "isRegex": false
    }
  ],
  "startsAt": "2025-11-27T10:00:00Z",
  "endsAt": "2025-11-27T12:00:00Z",
  "createdBy": "admin",
  "comment": "Maintenance window"
}
```

### Receiver Management

```
GET /api/v2/receivers               # List configured receivers
```

## Alert State Management

### State Lifecycle

```
┌──────────┐
│ Inactive │
└────┬─────┘
     │ Condition becomes true
     ▼
┌──────────┐
│ Pending  │ ◀── Waiting for 'for' duration
└────┬─────┘
     │ 'for' duration elapsed
     ▼
┌──────────┐
│  Firing  │ ◀── Alert sent to Alertmanager
└────┬─────┘
     │ Condition becomes false
     ▼
┌──────────┐
│ Resolved │
└──────────┘
```

### State Persistence

Alertmanager maintains state for:

1. **Active Alerts**: Currently firing alerts with full metadata
2. **Alert History**: Recent alert state transitions
3. **Silences**: Active and expired silences
4. **Notification Log**: History of sent notifications

## Key Features to Implement

### 1. Deduplication
- Hash alerts by labels
- Merge identical alerts
- Update timestamps

### 2. Grouping
- Group alerts by configured labels
- Batch notifications
- Configurable timing (group_wait, group_interval)

### 3. Inhibition
- Match source and target alerts
- Check label equality
- Suppress target notifications

### 4. Silencing
- Time-based alert muting
- Label matcher-based
- CRUD operations via API

### 5. Routing
- Tree-based routing logic
- Label matching (exact and regex)
- Receiver selection

### 6. Notification
- Template-based formatting
- Multiple integration support
- Retry logic and backoff
- Resolved notifications

## Performance Considerations

### Memory Usage
- Alert state storage
- Silence storage
- Notification queue
- Historical data

### CPU Usage
- Alert deduplication
- Grouping calculations
- Inhibition rule evaluation
- Routing tree traversal

### Disk I/O
- Configuration reloading
- State persistence
- Notification logging

## Integration with Background Processing

The Alertmanager functionality should be implemented as background tasks to avoid impacting read/write paths:

1. **Alert Evaluation**: Background task evaluating alert rules
2. **Alert Processing**: Background task processing incoming alerts
3. **Notification Dispatch**: Background task sending notifications
4. **State Cleanup**: Background task cleaning up old alerts/silences

## Next Steps

1. Design alert state storage mechanism
2. Design notification queue and dispatch system
3. Design API endpoints for alert/silence management
4. Design integration with existing background processor
5. Design configuration management and hot-reload
