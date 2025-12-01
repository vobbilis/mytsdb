# Authentication Guide

This guide covers all authentication mechanisms supported by MyTSDB's Prometheus Remote Storage API.

## Table of Contents

1. [Overview](#overview)
2. [No Authentication (Default)](#no-authentication-default)
3. [Basic Authentication](#basic-authentication)
4. [Bearer Token Authentication](#bearer-token-authentication)
5. [Header-Based Authentication (Multi-tenancy)](#header-based-authentication-multi-tenancy)
6. [Composite Authentication](#composite-authentication)
7. [Configuration Examples](#configuration-examples)
8. [Security Best Practices](#security-best-practices)

---

## Overview

MyTSDB supports multiple authentication mechanisms for securing Prometheus Remote Storage endpoints:

- **No Authentication** - Default, backward compatible
- **Basic Authentication** - Username/password (HTTP Basic Auth)
- **Bearer Token** - API tokens/JWT
- **Header-Based** - Custom headers for multi-tenancy
- **Composite** - Combine multiple methods (AND/OR logic)

**Default Behavior:** If no authenticator is provided, the system uses `NoAuthenticator` which allows all requests (backward compatible).

---

## No Authentication (Default)

By default, no authentication is required. This maintains backward compatibility with existing deployments.

### C++ Example

```cpp
#include "tsdb/prometheus/remote/write_handler.h"
#include "tsdb/storage/storage_impl.h"

// Create handlers without authentication
auto storage = std::make_shared<storage::StorageImpl>(config);
auto write_handler = std::make_shared<WriteHandler>(storage);
auto read_handler = std::make_shared<ReadHandler>(storage);

// All requests will be accepted
```

### Prometheus Configuration

```yaml
remote_write:
  - url: "http://localhost:9090/api/v1/write"

remote_read:
  - url: "http://localhost:9090/api/v1/read"
```

---

## Basic Authentication

HTTP Basic Authentication using username and password with SHA-256 hashing.

### C++ Example

```cpp
#include "tsdb/prometheus/auth/basic_auth.h"

// Create Basic Auth authenticator
auto auth = std::make_shared<BasicAuthenticator>();

// Add users (password will be hashed automatically)
auth->AddUserWithPassword("prometheus", "secret_password", "tenant1");
auth->AddUserWithPassword("grafana", "another_password", "tenant2");

// Create handlers with authentication
auto write_handler = std::make_shared<WriteHandler>(storage, auth);
auto read_handler = std::make_shared<ReadHandler>(storage, auth);
```

### Prometheus Configuration

```yaml
remote_write:
  - url: "http://localhost:9090/api/v1/write"
    basic_auth:
      username: prometheus
      password: secret_password

remote_read:
  - url: "http://localhost:9090/api/v1/read"
    basic_auth:
      username: prometheus
      password: secret_password
```

### OTEL Collector Configuration

```yaml
exporters:
  prometheusremotewrite:
    endpoint: "http://localhost:9090/api/v1/write"
    auth:
      authenticator: basicauth/prometheus
      
extensions:
  basicauth/prometheus:
    client_auth:
      username: prometheus
      password: secret_password
```

### Testing with curl

```bash
# Write request
curl -X POST http://localhost:9090/api/v1/write \
  -u prometheus:secret_password \
  -H "Content-Type: application/x-protobuf" \
  --data-binary @write_request.pb

# Read request
curl -X POST http://localhost:9090/api/v1/read \
  -u prometheus:secret_password \
  -H "Content-Type: application/x-protobuf" \
  --data-binary @read_request.pb
```

---

## Bearer Token Authentication

API token-based authentication with support for token rotation.

### C++ Example

```cpp
#include "tsdb/prometheus/auth/bearer_auth.h"

// Create Bearer Auth authenticator
auto auth = std::make_shared<BearerAuthenticator>();

// Add tokens
auth->AddToken("secret-token-123", "tenant1");
auth->AddToken("secret-token-456", "tenant2");

// Create handlers with authentication
auto write_handler = std::make_shared<WriteHandler>(storage, auth);
auto read_handler = std::make_shared<ReadHandler>(storage, auth);

// Token rotation example
auth->RevokeToken("secret-token-123");
auth->AddToken("new-token-789", "tenant1");
```

### Prometheus Configuration

```yaml
remote_write:
  - url: "http://localhost:9090/api/v1/write"
    headers:
      Authorization: "Bearer secret-token-123"

remote_read:
  - url: "http://localhost:9090/api/v1/read"
    headers:
      Authorization: "Bearer secret-token-123"
```

### OTEL Collector Configuration

```yaml
exporters:
  prometheusremotewrite:
    endpoint: "http://localhost:9090/api/v1/write"
    headers:
      Authorization: "Bearer secret-token-123"
```

### Testing with curl

```bash
# Write request
curl -X POST http://localhost:9090/api/v1/write \
  -H "Authorization: Bearer secret-token-123" \
  -H "Content-Type: application/x-protobuf" \
  --data-binary @write_request.pb
```

---

## Header-Based Authentication (Multi-tenancy)

Custom header-based authentication for multi-tenant deployments. Commonly used with `X-Scope-OrgID` header.

### C++ Example

```cpp
#include "tsdb/prometheus/auth/header_auth.h"

// Create Header Auth authenticator
auto auth = std::make_shared<HeaderAuthenticator>();

// Configure tenant header (default is "X-Scope-OrgID")
auth->SetTenantHeader("X-Scope-OrgID");

// Option 1: Validate specific tenants
auth->AddValidTenant("tenant1");
auth->AddValidTenant("tenant2");
auth->AddValidTenant("tenant3");

// Option 2: Accept any tenant ID (no validation)
auth->SetValidateTenants(false);

// Option 3: Make header optional
auth->SetRequireHeader(false);

// Create handlers with authentication
auto write_handler = std::make_shared<WriteHandler>(storage, auth);
auto read_handler = std::make_shared<ReadHandler>(storage, auth);
```

### Prometheus Configuration

```yaml
remote_write:
  - url: "http://localhost:9090/api/v1/write"
    headers:
      X-Scope-OrgID: "tenant1"

remote_read:
  - url: "http://localhost:9090/api/v1/read"
    headers:
      X-Scope-OrgID: "tenant1"
```

### OTEL Collector Configuration

```yaml
exporters:
  prometheusremotewrite:
    endpoint: "http://localhost:9090/api/v1/write"
    headers:
      X-Scope-OrgID: "tenant1"
```

### Testing with curl

```bash
# Write request
curl -X POST http://localhost:9090/api/v1/write \
  -H "X-Scope-OrgID: tenant1" \
  -H "Content-Type: application/x-protobuf" \
  --data-binary @write_request.pb
```

---

## Composite Authentication

Combine multiple authentication methods with AND/OR logic.

### C++ Example - OR Mode (Any Method Succeeds)

```cpp
#include "tsdb/prometheus/auth/composite_auth.h"
#include "tsdb/prometheus/auth/basic_auth.h"
#include "tsdb/prometheus/auth/bearer_auth.h"

// Create individual authenticators
auto basic = std::make_shared<BasicAuthenticator>();
basic->AddUserWithPassword("user1", "pass1");

auto bearer = std::make_shared<BearerAuthenticator>();
bearer->AddToken("token123");

// Create composite authenticator (OR mode)
auto auth = std::make_shared<CompositeAuthenticator>();
auth->SetMode(CompositeAuthenticator::Mode::ANY);
auth->AddAuthenticator(basic);
auth->AddAuthenticator(bearer);

// Create handlers - accepts EITHER Basic OR Bearer auth
auto write_handler = std::make_shared<WriteHandler>(storage, auth);
```

### C++ Example - AND Mode (All Methods Must Succeed)

```cpp
// Create authenticators
auto header = std::make_shared<HeaderAuthenticator>();
header->SetValidateTenants(false);

auto bearer = std::make_shared<BearerAuthenticator>();
bearer->AddToken("token123");

// Create composite authenticator (AND mode)
auto auth = std::make_shared<CompositeAuthenticator>();
auth->SetMode(CompositeAuthenticator::Mode::ALL);
auth->AddAuthenticator(header);
auth->AddAuthenticator(bearer);

// Create handlers - requires BOTH header AND bearer token
auto write_handler = std::make_shared<WriteHandler>(storage, auth);
```

### Prometheus Configuration (OR Mode)

```yaml
# Can use either Basic Auth OR Bearer Token
remote_write:
  - url: "http://localhost:9090/api/v1/write"
    basic_auth:
      username: user1
      password: pass1
    # OR
    headers:
      Authorization: "Bearer token123"
```

---

## Configuration Examples

### Example 1: Production Setup with Basic Auth

```cpp
// Production server with Basic Auth
auto storage = std::make_shared<storage::StorageImpl>(config);

auto auth = std::make_shared<BasicAuthenticator>();
auth->AddUserWithPassword("prometheus", "strong_password_here");
auth->AddUserWithPassword("grafana", "another_strong_password");

auto write_handler = std::make_shared<WriteHandler>(storage, auth);
auto read_handler = std::make_shared<ReadHandler>(storage, auth);

server.RegisterHandler("/api/v1/write", 
    [write_handler](const auto& req, auto& res) {
        write_handler->Handle(req, res);
    });
```

### Example 2: Multi-tenant SaaS with Header Auth

```cpp
// Multi-tenant setup
auto auth = std::make_shared<HeaderAuthenticator>();
auth->SetTenantHeader("X-Scope-OrgID");
auth->SetValidateTenants(false);  // Accept any tenant

auto write_handler = std::make_shared<WriteHandler>(storage, auth);
auto read_handler = std::make_shared<ReadHandler>(storage, auth);
```

### Example 3: Secure Setup with Bearer + Tenant

```cpp
// Require both Bearer token AND tenant header
auto bearer = std::make_shared<BearerAuthenticator>();
bearer->AddToken("api-key-12345", "tenant1");

auto header = std::make_shared<HeaderAuthenticator>();
header->SetValidateTenants(false);

auto composite = std::make_shared<CompositeAuthenticator>();
composite->SetMode(CompositeAuthenticator::Mode::ALL);
composite->AddAuthenticator(bearer);
composite->AddAuthenticator(header);

auto write_handler = std::make_shared<WriteHandler>(storage, composite);
```

---

## Security Best Practices

### Password Security

1. **Use Strong Passwords**
   - Minimum 12 characters
   - Mix of uppercase, lowercase, numbers, symbols

2. **Password Storage**
   - Passwords are hashed with SHA-256
   - Never store plain text passwords
   - Consider using bcrypt or Argon2 for production

3. **Password Rotation**
   ```cpp
   // Rotate passwords periodically
   auth->AddUserWithPassword("user", "new_password");
   ```

### Token Security

1. **Generate Secure Tokens**
   ```bash
   # Generate cryptographically secure token
   openssl rand -hex 32
   ```

2. **Token Rotation**
   ```cpp
   // Rotate tokens regularly
   auth->RevokeToken("old-token");
   auth->AddToken("new-token", "tenant1");
   ```

3. **Token Expiration** (Future Enhancement)
   - Currently tokens don't expire
   - Implement manual rotation
   - Future: Add automatic expiration

### Network Security

1. **Use TLS/HTTPS**
   - Always use TLS in production
   - Prevents credential interception
   - See TLS configuration (Phase 7.3)

2. **Rate Limiting**
   - Implement rate limiting per IP/token
   - Prevent brute force attacks
   - Future enhancement

3. **Audit Logging**
   ```cpp
   // Log all authentication attempts
   LOG_INFO("Auth attempt: user={}, success={}, ip={}", 
            username, success, remote_ip);
   ```

### Multi-tenancy Security

1. **Tenant Isolation**
   - Ensure data isolation per tenant
   - Validate tenant IDs
   - Use tenant ID in storage queries

2. **Tenant Validation**
   ```cpp
   auth->AddValidTenant("tenant1");
   auth->AddValidTenant("tenant2");
   // Only these tenants allowed
   ```

---

## Error Responses

### Authentication Failure (401)

```json
{
  "error": "Authentication failed: Invalid password",
  "status": 401
}
```

### Common Error Messages

- `"Missing Authorization header"` - No auth header provided
- `"Invalid password"` - Wrong password for Basic Auth
- `"Unknown user"` - User doesn't exist
- `"Invalid or revoked token"` - Bearer token invalid
- `"Missing tenant header: X-Scope-OrgID"` - Required header missing
- `"Invalid tenant ID: unknown"` - Tenant not in allowed list

---

## Complete Example Server

See [`examples/prometheus_remote_storage/prometheus_remote_storage_server.cpp`](examples/prometheus_remote_storage/prometheus_remote_storage_server.cpp) for a complete working example demonstrating all authentication types.

---

## Testing Authentication

### Unit Tests

Run authentication unit tests:

```bash
./build/test/unit/tsdb_prometheus_unit_tests --gtest_filter="*Authenticator*"
```

### Integration Tests

Test with real Prometheus:

```bash
# Start server with auth
./build/examples/prometheus_remote_storage/prometheus_remote_storage_server 9090

# Configure Prometheus with auth
# Edit prometheus.yml with basic_auth or headers

# Verify metrics are being written
curl http://localhost:9090/health
```

---

## Troubleshooting

### Authentication Always Fails

1. Check credentials are correct
2. Verify header format (e.g., "Bearer " prefix)
3. Check server logs for auth errors
4. Test with curl first

### Backward Compatibility Issues

1. Ensure NoAuthenticator is used by default
2. Verify existing tests still pass
3. Check handler constructor calls

### Multi-tenancy Not Working

1. Verify header name matches (case-sensitive)
2. Check tenant validation settings
3. Ensure header is being sent by client

---

## Next Steps

- **TLS/mTLS**: See TLS configuration guide (coming in Phase 7.3)
- **Advanced Auth**: JWT validation, OAuth2 (future)
- **Audit Logging**: Track all authentication attempts
- **Rate Limiting**: Prevent brute force attacks

For questions or issues, please refer to the main [README.md](README.md) or [GETTING_STARTED.md](GETTING_STARTED.md).
