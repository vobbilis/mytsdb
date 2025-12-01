#include "tsdb/prometheus/auth/bearer_auth.h"

namespace tsdb {
namespace prometheus {
namespace auth {

void BearerAuthenticator::AddToken(const std::string& token,
                                    const std::string& tenant_id) {
    TokenInfo info;
    info.tenant_id = tenant_id;
    info.created_at = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    tokens_[token] = info;
}

void BearerAuthenticator::RevokeToken(const std::string& token) {
    tokens_.erase(token);
}

bool BearerAuthenticator::HasToken(const std::string& token) const {
    return tokens_.find(token) != tokens_.end();
}

AuthResult BearerAuthenticator::Authenticate(const Request& req) {
    // Get Authorization header
    auto it = req.headers.find("Authorization");
    if (it == req.headers.end()) {
        return AuthResult::Failure("Missing Authorization header");
    }
    
    const std::string& auth_header = it->second;
    
    // Extract Bearer token
    std::string token = ExtractBearerToken(auth_header);
    if (token.empty()) {
        return AuthResult::Failure("Invalid Bearer token format");
    }
    
    // Check if token is valid
    auto token_it = tokens_.find(token);
    if (token_it == tokens_.end()) {
        return AuthResult::Failure("Invalid or revoked token");
    }
    
    // Success
    return AuthResult::Success(token_it->second.tenant_id);
}

std::string BearerAuthenticator::ExtractBearerToken(const std::string& auth_header) {
    // Check if it starts with "Bearer "
    if (auth_header.substr(0, 7) != "Bearer ") {
        return "";
    }
    
    // Extract token (everything after "Bearer ")
    return auth_header.substr(7);
}

} // namespace auth
} // namespace prometheus
} // namespace tsdb
