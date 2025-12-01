#include "tsdb/prometheus/auth/basic_auth.h"
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>
#include <cstring>

namespace tsdb {
namespace prometheus {
namespace auth {

void BasicAuthenticator::AddUser(const std::string& username, 
                                  const std::string& password_hash,
                                  const std::string& tenant_id) {
    UserInfo info;
    info.password_hash = password_hash;
    info.tenant_id = tenant_id;
    users_[username] = info;
}

void BasicAuthenticator::AddUserWithPassword(const std::string& username,
                                             const std::string& password,
                                             const std::string& tenant_id) {
    AddUser(username, HashPassword(password), tenant_id);
}

AuthResult BasicAuthenticator::Authenticate(const Request& req) {
    // Get Authorization header
    auto it = req.headers.find("Authorization");
    if (it == req.headers.end()) {
        return AuthResult::Failure("Missing Authorization header");
    }
    
    const std::string& auth_header = it->second;
    
    // Check if it's Basic auth
    if (auth_header.substr(0, 6) != "Basic ") {
        return AuthResult::Failure("Not Basic authentication");
    }
    
    // Parse credentials
    auto [username, password] = ParseBasicAuth(auth_header);
    if (username.empty()) {
        return AuthResult::Failure("Invalid Basic Auth format");
    }
    
    // Check if user exists
    auto user_it = users_.find(username);
    if (user_it == users_.end()) {
        return AuthResult::Failure("Unknown user");
    }
    
    // Verify password
    if (!VerifyPassword(password, user_it->second.password_hash)) {
        return AuthResult::Failure("Invalid password");
    }
    
    // Success
    return AuthResult::Success(user_it->second.tenant_id);
}

std::string BasicAuthenticator::HashPassword(const std::string& password) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(password.c_str()), 
           password.length(), hash);
    
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') 
           << static_cast<int>(hash[i]);
    }
    return ss.str();
}

bool BasicAuthenticator::VerifyPassword(const std::string& password, 
                                       const std::string& hash) {
    return HashPassword(password) == hash;
}

std::pair<std::string, std::string> BasicAuthenticator::ParseBasicAuth(
    const std::string& auth_header) {
    
    // Extract base64 part after "Basic "
    std::string encoded = auth_header.substr(6);
    std::string decoded = Base64Decode(encoded);
    
    // Split on ':'
    size_t colon_pos = decoded.find(':');
    if (colon_pos == std::string::npos) {
        return {"", ""};
    }
    
    std::string username = decoded.substr(0, colon_pos);
    std::string password = decoded.substr(colon_pos + 1);
    
    return {username, password};
}

std::string BasicAuthenticator::Base64Decode(const std::string& encoded) {
    static const std::string base64_chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    
    std::string decoded;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[base64_chars[i]] = i;
    
    int val = 0, valb = -8;
    for (unsigned char c : encoded) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            decoded.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return decoded;
}

} // namespace auth
} // namespace prometheus
} // namespace tsdb
