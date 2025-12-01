#pragma once

#include "authenticator.h"
#include <vector>
#include <memory>

namespace tsdb {
namespace prometheus {
namespace auth {

/**
 * @brief Composite Authenticator for Multiple Auth Methods
 * 
 * Combines multiple authenticators with AND/OR logic.
 * Useful for supporting multiple authentication methods simultaneously.
 */
class CompositeAuthenticator : public Authenticator {
public:
    enum class Mode {
        ANY,  // Any authenticator succeeds (OR logic)
        ALL   // All authenticators must succeed (AND logic)
    };
    
    /**
     * @brief Add an authenticator to the composite
     */
    void AddAuthenticator(std::shared_ptr<Authenticator> auth);
    
    /**
     * @brief Set the combination mode (ANY or ALL)
     */
    void SetMode(Mode mode) { mode_ = mode; }
    
    /**
     * @brief Get current mode
     */
    Mode GetMode() const { return mode_; }
    
    AuthResult Authenticate(const Request& req) override;
    
private:
    std::vector<std::shared_ptr<Authenticator>> authenticators_;
    Mode mode_ = Mode::ANY;
};

} // namespace auth
} // namespace prometheus
} // namespace tsdb
