#pragma once

#include "authenticator.h"
#include <map>
#include <string>

namespace tsdb {
namespace prometheus {
namespace auth {

/**
 * @brief HTTP Basic Authentication
 * 
 * Authenticates requests using HTTP Basic Auth (RFC 7617).
 * Passwords are stored as SHA-256 hashes.
 */
class BasicAuthenticator : public Authenticator {
public:
    /**
     * @brief Add a user with hashed password
     * @param username Username
     * @param password_hash SHA-256 hash of password
     * @param tenant_id Optional tenant ID for multi-tenancy
     */
    void AddUser(const std::string& username, 
                 const std::string& password_hash,
                 const std::string& tenant_id = "");
    
    /**
     * @brief Add a user with plain password (will be hashed)
     * @param username Username
     * @param password Plain text password (will be hashed)
     * @param tenant_id Optional tenant ID for multi-tenancy
     */
    void AddUserWithPassword(const std::string& username,
                            const std::string& password,
                            const std::string& tenant_id = "");
    
    AuthResult Authenticate(const Request& req) override;
    
private:
    struct UserInfo {
        std::string password_hash;
        std::string tenant_id;
    };
    
    std::map<std::string, UserInfo> users_;
    
    /**
     * @brief Hash a password using SHA-256
     */
    std::string HashPassword(const std::string& password);
    
    /**
     * @brief Verify a password against a hash
     */
    bool VerifyPassword(const std::string& password, 
                       const std::string& hash);
    
    /**
     * @brief Parse Basic Auth header
     * @return pair of (username, password)
     */
    std::pair<std::string, std::string> ParseBasicAuth(
        const std::string& auth_header);
    
    /**
     * @brief Base64 decode
     */
    std::string Base64Decode(const std::string& encoded);
};

} // namespace auth
} // namespace prometheus
} // namespace tsdb
