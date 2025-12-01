#include "tsdb/prometheus/remote/write_handler.h"
#include "tsdb/prometheus/remote/converter.h"
#include "tsdb/prometheus/auth/no_auth.h"
#include "tsdb/common/logger.h"
#include "remote.pb.h"

#ifdef HAVE_SNAPPY
#include <snappy.h>
#endif

#include <iostream>
#include <chrono>
#include <atomic>

namespace tsdb {
namespace prometheus {
namespace remote {

WriteHandler::WriteHandler(std::shared_ptr<tsdb::storage::Storage> storage_ptr,
                          std::shared_ptr<tsdb::prometheus::auth::Authenticator> authenticator)
    : storage_(std::move(storage_ptr))
    , authenticator_(authenticator ? authenticator : std::make_shared<auth::NoAuthenticator>()) {}

void WriteHandler::Handle(const Request& req, std::string& res) {
    auto start_time = std::chrono::steady_clock::now();
    
    // Generate request ID for tracking
    static std::atomic<uint64_t> request_counter{0};
    uint64_t request_id = ++request_counter;
    
    TSDB_INFO("[REQ:{}] Remote Write request received", request_id);
    TSDB_DEBUG("[REQ:{}] Method: {}, Content-Length: {}", 
               request_id, req.method, req.body.size());
    
    try {
        // Check authentication first
        std::string auth_error;
        if (!CheckAuth(req, auth_error)) {
            TSDB_WARN("[REQ:{}] Authentication failed: {}", request_id, auth_error);
            res = FormatAuthError(auth_error);
            
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time).count();
            TSDB_INFO("[REQ:{}] Completed with auth error in {}ms", request_id, duration);
            return;
        }
        
        TSDB_DEBUG("[REQ:{}] Authentication successful", request_id);
        
        // Check method
        if (req.method != "POST") {
            TSDB_WARN("[REQ:{}] Invalid method: {}", request_id, req.method);
            res = FormatErrorResponse("Method not allowed", 405);
            return;
        }
        
        // Decompress body if needed
        std::string decompressed;
#ifdef HAVE_SNAPPY
        auto encoding_it = req.headers.find("Content-Encoding");
        if (encoding_it != req.headers.end() && 
            encoding_it->second == "snappy") {
            TSDB_DEBUG("[REQ:{}] Decompressing Snappy payload ({} bytes)", 
                       request_id, req.body.size());
            decompressed = DecompressSnappy(req.body);
            TSDB_DEBUG("[REQ:{}] Decompressed to {} bytes", 
                       request_id, decompressed.size());
        } else {
            decompressed = req.body;
        }
#else
        decompressed = req.body;
#endif
        
        // Parse protobuf
        ::prometheus::WriteRequest write_req;
        if (!write_req.ParseFromString(decompressed)) {
            TSDB_ERROR("[REQ:{}] Failed to parse protobuf", request_id);
            res = FormatErrorResponse("Failed to parse protobuf");
            return;
        }
        
        TSDB_INFO("[REQ:{}] Parsed {} time series", 
                  request_id, write_req.timeseries_size());
        
        // Convert to internal format
        auto series_list = Converter::FromWriteRequest(write_req);
        TSDB_DEBUG("[REQ:{}] Converted to internal format", request_id);
        
        // Write to storage
        int series_count = 0;
        int sample_count = 0;
        for (const auto& series : series_list) {
            auto result = storage_->write(series);
            if (!result.ok()) {
                TSDB_ERROR("[REQ:{}] Write failed for series {}: {}", 
                           request_id, series_count, result.error());
                res = FormatErrorResponse("Write failed: " + result.error());
                return;
            }
            series_count++;
            sample_count += series.samples().size();
        }
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time).count();
        
        TSDB_INFO("[REQ:{}] Successfully wrote {} series, {} samples in {}ms", 
                  request_id, series_count, sample_count, duration);
        
        res = FormatSuccessResponse();
    } catch (const std::exception& e) {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time).count();
        TSDB_ERROR("[REQ:{}] Exception after {}ms: {}", request_id, duration, e.what());
        res = FormatErrorResponse(std::string("Exception: ") + e.what());
    }
}

bool WriteHandler::CheckAuth(const Request& req, std::string& error) {
    auto result = authenticator_->Authenticate(req);
    if (!result.authenticated) {
        error = result.error;
        return false;
    }
    return true;
}

std::string WriteHandler::DecompressSnappy(const std::string& compressed) {
#ifdef HAVE_SNAPPY
    std::string decompressed;
    if (!snappy::Uncompress(compressed.data(), compressed.size(), &decompressed)) {
        throw std::runtime_error("Snappy decompression failed");
    }
    return decompressed;
#else
    throw std::runtime_error("Snappy support not compiled");
#endif
}

std::string WriteHandler::FormatSuccessResponse() {
    return "{}";  // Empty JSON object indicates success
}

std::string WriteHandler::FormatErrorResponse(const std::string& error, int /*status_code*/) {
    return "{\"error\":\"" + error + "\"}";
}

std::string WriteHandler::FormatAuthError(const std::string& error) {
    return "{\"error\":\"Authentication failed: " + error + "\",\"status\":401}";
}

} // namespace remote
} // namespace prometheus
} // namespace tsdb
