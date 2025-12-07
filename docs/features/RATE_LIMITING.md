# Rate Limiting

## Overview

MyTSDB provides configurable rate limiting to protect API endpoints from abuse and ensure fair resource allocation across clients.

---

## Features

| Feature | Description |
|---------|-------------|
| **Token Bucket** | Classic token bucket algorithm for smooth rate limiting |
| **Per-Client Limits** | Configure different limits per client/tenant |
| **Configurable Bursts** | Allow controlled bursts above steady-state rate |
| **Graceful Degradation** | 429 responses with Retry-After headers |

---

## Configuration

### Basic Setup

```cpp
#include "tsdb/server/rate_limiter.h"

RateLimiterConfig config;
config.requests_per_second = 100;    // Steady-state rate
config.burst_size = 200;             // Allow bursts up to 200
config.enabled = true;

RateLimiter limiter(config);
```

### Multi-Tenant Setup

```cpp
// Different limits per tenant
limiter.set_tenant_limit("premium-tenant", 1000, 2000);
limiter.set_tenant_limit("free-tenant", 10, 20);
```

---

## HTTP Response

When rate limited, clients receive:

```http
HTTP/1.1 429 Too Many Requests
Retry-After: 1
Content-Type: application/json

{
    "error": "rate_limit_exceeded",
    "message": "Too many requests, please retry after 1 seconds"
}
```

---

## Metrics

The rate limiter exposes Prometheus-compatible metrics:

| Metric | Type | Description |
|--------|------|-------------|
| `mytsdb_rate_limit_requests_total` | Counter | Total requests processed |
| `mytsdb_rate_limit_rejected_total` | Counter | Requests rejected due to rate limit |
| `mytsdb_rate_limit_current_rate` | Gauge | Current request rate per second |

---

## Best Practices

1. **Set appropriate limits** - Base on your infrastructure capacity
2. **Use bursts wisely** - Allow for legitimate traffic spikes
3. **Monitor rejection rate** - High rejection = limits too low or abuse
4. **Per-tenant limits** - Prevent noisy neighbors in multi-tenant setups
