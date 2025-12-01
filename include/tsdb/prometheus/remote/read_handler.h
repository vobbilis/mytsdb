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
 * @brief Handler for Prometheus Remote Read endpoint
 */
class ReadHandler {
public:
    /**
     * @brief Construct a new Read Handler
     * @param storage Storage backend
     * @param authenticator Optional authenticator (defaults to NoAuth)
     */
    explicit ReadHandler(std::shared_ptr<tsdb::storage::Storage> storage_ptr,
                        std::shared_ptr<tsdb::prometheus::auth::Authenticator> authenticator = nullptr);
    
    /**
     * @brief Handle a remote read request
     * @param req HTTP request
     * @param res HTTP response (output)
     */
    void Handle(const Request& req, std::string& res);
    
private:
    std::shared_ptr<tsdb::storage::Storage> storage_;
    std::shared_ptr<auth::Authenticator> authenticator_;
    
    /**
     * @brief Check authentication
     */
    bool CheckAuth(const Request& req, std::string& error);
    
    /**
     * @brief Decompress Snappy-compressed data
     * @param compressed Compressed data
     * @return Decompressed data
     */
    std::string DecompressSnappy(const std::string& compressed);
    
    /**
     * @brief Compress data with Snappy
     * @param data Uncompressed data
     * @return Compressed data
     */
    std::string CompressSnappy(const std::string& data);
    
    /**
     * @brief Format error response
     * @param error Error message
     * @param status_code HTTP status code
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
