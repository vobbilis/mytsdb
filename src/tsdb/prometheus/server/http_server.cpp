#include "tsdb/prometheus/server/http_server.h"
#include "tsdb/prometheus/server/request.h"
#include <httplib.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <mutex>
#include <unordered_map>
#include <atomic>

namespace tsdb {
namespace prometheus {

class HttpServer::Impl {
public:
    explicit Impl(const ServerConfig& config)
        : config_(config), server_(std::make_unique<httplib::Server>()),
          request_count_(0), active_connections_(0) {
        
        // Set up server options
        server_->set_keep_alive_max_count(config.max_connections);
        server_->set_read_timeout(config.timeout_seconds);
        server_->set_write_timeout(config.timeout_seconds);
        
        // Set up SSL if configured
        if (!config.cert_file.empty() && !config.key_file.empty()) {
            // SSL configuration should be done before starting the server
            // in the listen() call using httplib::SSLServer instead
            throw ServerError("SSL support not available in this version");
        }
        
        // Set up default handlers
        server_->Get("/health", [](const httplib::Request&, httplib::Response& res) {
            res.set_content("{\"status\":\"up\"}", "application/json");
        });
        
        server_->Get("/metrics", [this](const httplib::Request&, httplib::Response& res) {
            res.set_content(GetMetricsJson(), "application/json");
        });
        
        // Set up request counting
        server_->set_pre_routing_handler([this](const httplib::Request& /*req*/, httplib::Response& res) {
            // Check max connections atomically
            uint64_t current = active_connections_.fetch_add(1);
            
            if (current >= config_.max_connections) {
                res.status = 503;
                res.set_content("Too Many Requests", "text/plain");
                return httplib::Server::HandlerResponse::Handled;
            }
            
            return httplib::Server::HandlerResponse::Unhandled;
        });
        
        server_->set_post_routing_handler([this](const httplib::Request& /*req*/, const httplib::Response& /*res*/) {
            active_connections_--;
            request_count_++;
            return httplib::Server::HandlerResponse::Unhandled;
        });
    }
    
    void Start() {
        if (server_thread_.joinable()) {
            throw ServerError("Server is already running");
        }
        
        server_thread_ = std::thread([this]() {
            if (!server_->listen(config_.listen_address.c_str(), config_.port)) {
                throw ServerError("Failed to start server");
            }
        });
    }
    
    void Stop() {
        if (server_thread_.joinable()) {
            server_->stop();
            server_thread_.join();
        }
    }
    
    void RegisterHandler(const std::string& path, RequestHandler handler) {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        handlers_[path] = handler;
        
        auto handle_req = [this, path](const httplib::Request& req, httplib::Response& res) {
            try {
                Request prom_req;
                prom_req.method = req.method;
                prom_req.path = req.path;
                prom_req.body = req.body;
                
                // Copy params
                for (const auto& [k, v] : req.params) {
                    prom_req.params.insert({k, v});
                }

                // Copy path params
                for (const auto& [k, v] : req.path_params) {
                    prom_req.path_params[k] = v;
                }
                
                // Copy headers
                for (const auto& [k, v] : req.headers) {
                    prom_req.headers[k] = v;
                }
                
                std::string response;
                handlers_[path](prom_req, response);
                res.set_content(response, "application/json");
            } catch (const std::exception& e) {
                res.status = 500;
                res.set_content(CreateErrorJson(e.what()), "application/json");
            }
        };

        server_->Get(path.c_str(), handle_req);
        server_->Post(path.c_str(), handle_req);
    }
    
    void RegisterQueryHandler(const std::string& path, QueryHandlerFunc handler) {
        std::lock_guard<std::mutex> lock(query_handlers_mutex_);
        query_handlers_[path] = handler;
        
        server_->Get(path.c_str(), [this, path](const httplib::Request& req, 
                                               httplib::Response& res) {
            try {
                // Use req.target which contains the original path + query string
                std::string path_with_query = req.target;
                std::string response = query_handlers_[path](path_with_query);
                res.set_content(response, "application/json");
            } catch (const std::exception& e) {
                res.status = 500;
                res.set_content(CreateErrorJson(e.what()), "application/json");
            }
        });
    }
    
    std::string GetMetricsJson() const {
        rapidjson::Document doc;
        doc.SetObject();
        auto& allocator = doc.GetAllocator();
        
        // Add server metrics using our own counters
        doc.AddMember("active_connections", 
                     active_connections_.load(),
                     allocator);
        doc.AddMember("total_requests", 
                     request_count_.load(),
                     allocator);
        
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);
        
        return buffer.GetString();
    }

private:
    ServerConfig config_;
    std::unique_ptr<httplib::Server> server_;
    std::thread server_thread_;
    std::unordered_map<std::string, RequestHandler> handlers_;
    mutable std::mutex handlers_mutex_;
    std::unordered_map<std::string, QueryHandlerFunc> query_handlers_;
    mutable std::mutex query_handlers_mutex_;
    std::atomic<uint64_t> request_count_;
    std::atomic<uint64_t> active_connections_;
    
    std::string CreateErrorJson(const std::string& message) const {
        rapidjson::Document doc;
        doc.SetObject();
        auto& allocator = doc.GetAllocator();
        
        doc.AddMember("error", 
                     rapidjson::Value(message.c_str(), allocator).Move(),
                     allocator);
        
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);
        
        return buffer.GetString();
    }
};

HttpServer::HttpServer(const ServerConfig& config)
    : impl_(std::make_unique<Impl>(config)) {}

HttpServer::~HttpServer() {
    Stop();
}

void HttpServer::Start() {
    if (running_) {
        throw ServerError("Server is already running");
    }
    impl_->Start();
    running_ = true;
}

void HttpServer::Stop() {
    if (running_) {
        impl_->Stop();
        running_ = false;
    }
}

bool HttpServer::IsRunning() const {
    return running_;
}

void HttpServer::RegisterHandler(const std::string& path, RequestHandler handler) {
    impl_->RegisterHandler(path, handler);
}

void HttpServer::RegisterQueryHandler(const std::string& path, QueryHandlerFunc handler) {
    impl_->RegisterQueryHandler(path, handler);
}

std::string HttpServer::GetMetrics() const {
    return impl_->GetMetricsJson();
}

} // namespace prometheus
} // namespace tsdb 