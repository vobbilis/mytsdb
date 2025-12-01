#include "tsdb/prometheus/auth/header_auth.h"

namespace tsdb {
namespace prometheus {
namespace auth {

void HeaderAuthenticator::SetTenantHeader(const std::string& header_name) {
    tenant_header_ = header_name;
}

void HeaderAuthenticator::AddValidTenant(const std::string& tenant_id) {
    valid_tenants_.insert(tenant_id);
    validate_tenants_ = true;
}

void HeaderAuthenticator::SetValidateTenants(bool validate) {
    validate_tenants_ = validate;
}

void HeaderAuthenticator::SetRequireHeader(bool require) {
    require_header_ = require;
}

AuthResult HeaderAuthenticator::Authenticate(const Request& req) {
    // Get tenant header
    auto it = req.headers.find(tenant_header_);
    
    if (it == req.headers.end()) {
        if (require_header_) {
            return AuthResult::Failure("Missing tenant header: " + tenant_header_);
        } else {
            // Header not required, allow with no tenant
            return AuthResult::Success();
        }
    }
    
    const std::string& tenant_id = it->second;
    
    // Check if tenant ID is empty
    if (tenant_id.empty()) {
        return AuthResult::Failure("Empty tenant ID");
    }
    
    // Validate tenant if required
    if (validate_tenants_) {
        if (valid_tenants_.find(tenant_id) == valid_tenants_.end()) {
            return AuthResult::Failure("Invalid tenant ID: " + tenant_id);
        }
    }
    
    // Success with tenant ID
    return AuthResult::Success(tenant_id);
}

} // namespace auth
} // namespace prometheus
} // namespace tsdb
