#include "tsdb/prometheus/remote/read_handler.h"
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

ReadHandler::ReadHandler(std::shared_ptr<tsdb::storage::Storage> storage_ptr,
                        std::shared_ptr<tsdb::prometheus::auth::Authenticator> authenticator)
    : storage_(std::move(storage_ptr))
    , authenticator_(authenticator ? authenticator : std::make_shared<auth::NoAuthenticator>()) {}

void ReadHandler::Handle(const Request& req, std::string& res) {
    try {
        // Check authentication first
        std::string auth_error;
        if (!CheckAuth(req, auth_error)) {
            res = FormatAuthError(auth_error);
            return;
        }
        
        // Check method
        if (req.method != "POST") {
            res = FormatErrorResponse("Method not allowed", 405);
            return;
        }
        
        // Decompress body if needed
        std::string decompressed;
#ifdef HAVE_SNAPPY
        auto encoding_it = req.headers.find("Content-Encoding");
        if (encoding_it != req.headers.end() && 
            encoding_it->second == "snappy") {
            decompressed = DecompressSnappy(req.body);
        } else {
            decompressed = req.body;
        }
#else
        decompressed = req.body;
#endif
        
        // Parse protobuf
        ::prometheus::ReadRequest read_req;
        if (!read_req.ParseFromString(decompressed)) {
            res = FormatErrorResponse("Failed to parse protobuf");
            return;
        }
        
        // Process queries
        ::prometheus::ReadResponse read_resp;
        
        for (const auto& query : read_req.queries()) {
            // Convert matchers
            std::vector<core::LabelMatcher> matchers;
            for (const auto& proto_matcher : query.matchers()) {
                matchers.push_back(Converter::FromProtoMatcher(proto_matcher));
            }
            
            // Query storage
            auto result = storage_->query(
                matchers,
                query.start_timestamp_ms(),
                query.end_timestamp_ms()
            );
            
            if (!result.ok()) {
                res = FormatErrorResponse("Query failed: " + result.error());
                return;
            }
            
            // Convert results
            auto* query_result = read_resp.add_results();
            for (const auto& series : result.value()) {
                auto* proto_ts = query_result->add_timeseries();
                
                // Convert labels
                for (const auto& [name, value] : series.labels().map()) {
                    auto* proto_label = proto_ts->add_labels();
                    proto_label->set_name(name);
                    proto_label->set_value(value);
                }
                
                // Convert samples
                for (const auto& sample : series.samples()) {
                    auto* proto_sample = proto_ts->add_samples();
                    proto_sample->set_timestamp(sample.timestamp());
                    proto_sample->set_value(sample.value());
                }
            }
        }
        
        // Serialize response
        std::string serialized;
        if (!read_resp.SerializeToString(&serialized)) {
            res = FormatErrorResponse("Failed to serialize response");
            return;
        }
        
        // Compress if requested
#ifdef HAVE_SNAPPY
        auto accept_it = req.headers.find("Accept-Encoding");
        if (accept_it != req.headers.end() && 
            accept_it->second.find("snappy") != std::string::npos) {
            res = CompressSnappy(serialized);
        } else {
            res = serialized;
        }
#else
        res = serialized;
#endif
        
    } catch (const std::exception& e) {
        res = FormatErrorResponse(std::string("Exception: ") + e.what());
    }
}

bool ReadHandler::CheckAuth(const Request& req, std::string& error) {
    auto result = authenticator_->Authenticate(req);
    if (!result.authenticated) {
        error = result.error;
        return false;
    }
    return true;
}

std::string ReadHandler::DecompressSnappy(const std::string& compressed) {
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

std::string ReadHandler::CompressSnappy(const std::string& data) {
#ifdef HAVE_SNAPPY
    std::string compressed;
    snappy::Compress(data.data(), data.size(), &compressed);
    return compressed;
#else
    return data;
#endif
}

std::string ReadHandler::FormatErrorResponse(const std::string& error, int /*status_code*/) {
    return "{\"error\":\"" + error + "\"}";
}

std::string ReadHandler::FormatAuthError(const std::string& error) {
    return "{\"error\":\"Authentication failed: " + error + "\",\"status\":401}";
}

} // namespace remote
} // namespace prometheus
} // namespace tsdb
