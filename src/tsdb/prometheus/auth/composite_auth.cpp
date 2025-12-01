#include "tsdb/prometheus/auth/composite_auth.h"

namespace tsdb {
namespace prometheus {
namespace auth {

void CompositeAuthenticator::AddAuthenticator(std::shared_ptr<Authenticator> auth) {
    authenticators_.push_back(auth);
}

AuthResult CompositeAuthenticator::Authenticate(const Request& req) {
    if (authenticators_.empty()) {
        return AuthResult::Failure("No authenticators configured");
    }
    
    if (mode_ == Mode::ANY) {
        // ANY mode: At least one authenticator must succeed
        std::string last_error;
        
        for (const auto& auth : authenticators_) {
            AuthResult result = auth->Authenticate(req);
            if (result.authenticated) {
                return result;  // First success wins
            }
            last_error = result.error;
        }
        
        // All failed
        return AuthResult::Failure("All authentication methods failed: " + last_error);
        
    } else {
        // ALL mode: All authenticators must succeed
        std::optional<std::string> tenant_id;
        
        for (const auto& auth : authenticators_) {
            AuthResult result = auth->Authenticate(req);
            if (!result.authenticated) {
                return result;  // First failure fails all
            }
            
            // Merge tenant IDs (use first non-empty)
            if (!tenant_id && result.tenant_id) {
                tenant_id = result.tenant_id;
            }
        }
        
        // All succeeded
        return AuthResult::Success(tenant_id.value_or(""));
    }
}

} // namespace auth
} // namespace prometheus
} // namespace tsdb
