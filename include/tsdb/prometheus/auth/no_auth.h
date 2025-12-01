#pragma once

#include "authenticator.h"

namespace tsdb {
namespace prometheus {
namespace auth {

/**
 * @brief No-op authenticator that always allows access
 * 
 * This is the default authenticator for backward compatibility.
 * All requests are authenticated successfully.
 */
class NoAuthenticator : public Authenticator {
public:
    AuthResult Authenticate(const Request& /*req*/) override {
        return AuthResult::Success();
    }
};

} // namespace auth
} // namespace prometheus
} // namespace tsdb
