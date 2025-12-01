#pragma once

#include "tsdb/storage/storage.h"
#include "tsdb/prometheus/server/request.h"
#include "tsdb/prometheus/auth/authenticator.h"
#include <memory>
#include <string>

namespace tsdb {
namespace prometheus {
namespace remote {

/**
 * @brief Handler for Prometheus Remote Write endpoint
 */
class WriteHandler {
public:
    /**
     * @brief Construct a new Write Handler
     * @param storage Storage backend
     * @param authenticator Optional authenticator (defaults to NoAuth)
     */
    explicit WriteHandler(std::shared_ptr<tsdb::storage::Storage> storage_ptr,
                         std::shared_ptr<tsdb::prometheus::auth::Authenticator> authenticator = nullptr);
    
    /**
     * @brief Handle a remote write request
     * @param req HTTP request
     * @param res HTTP response (output)
     */
    void Handle(const Request& req, std::string& res);
    
private:
    std::shared_ptr<tsdb::storage::Storage> storage_;
    std::shared_ptr<auth::Authenticator> authenticator_;
    
    /**
     * @brief Check authentication
     * @return true if authenticated, false otherwise
     */
    bool CheckAuth(const Request& req, std::string& error);
    
    /**
     * @brief Decompress Snappy-compressed data
     * @param compressed Compressed data
     * @return Decompressed data
     */
    std::string DecompressSnappy(const std::string& compressed);
    
    /**
     * @brief Format success response
     * @return JSON success response
     */
    std::string FormatSuccessResponse();
    
    /**
     * @brief Format error response
     * @param error Error message
     * @param status_code HTTP status code (default 400)
     * @return JSON error response
     */
    std::string FormatErrorResponse(const std::string& error, int status_code = 400);
    
    /**
     * @brief Format authentication error (401)
     */
    std::string FormatAuthError(const std::string& error);
};

} // namespace remote
} // namespace prometheus
} // namespace tsdb
