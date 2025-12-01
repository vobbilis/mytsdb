#pragma once

#include "tsdb/prometheus/server/request.h"
#include <memory>
#include <string>
#include <optional>

namespace tsdb {
namespace prometheus {
namespace auth {

/**
 * @brief Result of an authentication attempt
 */
struct AuthResult {
    bool authenticated = false;
    std::optional<std::string> tenant_id;
    std::string error;
    
    /**
     * @brief Create a successful authentication result
     */
    static AuthResult Success(const std::string& tenant = "") {
        AuthResult result;
        result.authenticated = true;
        if (!tenant.empty()) {
            result.tenant_id = tenant;
        }
        return result;
    }
    
    /**
     * @brief Create a failed authentication result
     */
    static AuthResult Failure(const std::string& err) {
        AuthResult result;
        result.authenticated = false;
        result.error = err;
        return result;
    }
};

/**
 * @brief Base interface for authentication mechanisms
 */
class Authenticator {
public:
    virtual ~Authenticator() = default;
    
    /**
     * @brief Authenticate a request
     * @param req HTTP request to authenticate
     * @return Authentication result with tenant info if applicable
     */
    virtual AuthResult Authenticate(const Request& req) = 0;
};

} // namespace auth
} // namespace prometheus
} // namespace tsdb
