#pragma once

#include "authenticator.h"
#include <map>
#include <string>
#include <chrono>

namespace tsdb {
namespace prometheus {
namespace auth {

/**
 * @brief Bearer Token Authentication
 * 
 * Authenticates requests using Bearer tokens in the Authorization header.
 * Supports token rotation and expiration.
 */
class BearerAuthenticator : public Authenticator {
public:
    /**
     * @brief Add a valid token
     * @param token Bearer token
     * @param tenant_id Optional tenant ID for multi-tenancy
     */
    void AddToken(const std::string& token,
                  const std::string& tenant_id = "");
    
    /**
     * @brief Remove a token (for rotation)
     * @param token Token to revoke
     */
    void RevokeToken(const std::string& token);
    
    /**
     * @brief Check if a token exists
     */
    bool HasToken(const std::string& token) const;
    
    AuthResult Authenticate(const Request& req) override;
    
private:
    struct TokenInfo {
        std::string tenant_id;
        int64_t created_at;
    };
    
    std::map<std::string, TokenInfo> tokens_;
    
    /**
     * @brief Extract Bearer token from Authorization header
     */
    std::string ExtractBearerToken(const std::string& auth_header);
};

} // namespace auth
} // namespace prometheus
} // namespace tsdb
