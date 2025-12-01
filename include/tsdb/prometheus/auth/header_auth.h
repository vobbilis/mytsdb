#pragma once

#include "authenticator.h"
#include <set>
#include <string>

namespace tsdb {
namespace prometheus {
namespace auth {

/**
 * @brief Header-based Authentication for Multi-tenancy
 * 
 * Authenticates requests based on custom HTTP headers (e.g., X-Scope-OrgID).
 * Commonly used for multi-tenant deployments.
 */
class HeaderAuthenticator : public Authenticator {
public:
    /**
     * @brief Set the header name to check for tenant ID
     * @param header_name Header name (default: X-Scope-OrgID)
     */
    void SetTenantHeader(const std::string& header_name);
    
    /**
     * @brief Add a valid tenant ID
     * @param tenant_id Tenant ID to allow
     */
    void AddValidTenant(const std::string& tenant_id);
    
    /**
     * @brief Enable/disable tenant validation
     * If disabled, any tenant ID in the header is accepted
     */
    void SetValidateTenants(bool validate);
    
    /**
     * @brief Require the tenant header to be present
     */
    void SetRequireHeader(bool require);
    
    AuthResult Authenticate(const Request& req) override;
    
private:
    std::string tenant_header_ = "X-Scope-OrgID";
    std::set<std::string> valid_tenants_;
    bool validate_tenants_ = false;
    bool require_header_ = true;
};

} // namespace auth
} // namespace prometheus
} // namespace tsdb
