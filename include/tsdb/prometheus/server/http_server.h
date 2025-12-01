#pragma once

#include <string>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <stdexcept>
#include "tsdb/prometheus/server/request.h"

namespace tsdb {
namespace prometheus {

/**
 * @brief Configuration for the Prometheus HTTP server
 */
struct ServerConfig {
    std::string listen_address = "0.0.0.0";  // Listen address
    uint16_t port = 9090;                    // Listen port
    size_t num_threads = 4;                  // Number of worker threads
    int timeout_seconds = 30;                // Request timeout
    size_t max_connections = 1000;           // Maximum concurrent connections
    bool enable_compression = true;          // Enable response compression
    std::string cert_file;                   // TLS certificate file (optional)
    std::string key_file;                    // TLS key file (optional)
};

/**
 * @brief Handler function type for HTTP endpoints
 */
/**
 * @brief Handler function type for HTTP endpoints
 */
using RequestHandler = std::function<void(const Request& request, std::string& response)>;

/**
 * @brief Query handler function type that receives full request path with query parameters
 */
using QueryHandlerFunc = std::function<std::string(const std::string& path_with_query)>;

/**
 * @brief HTTP server for Prometheus API endpoints
 */
class HttpServer {
public:
    explicit HttpServer(const ServerConfig& config);
    ~HttpServer();

    // Non-copyable
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    /**
     * @brief Start the HTTP server
     * @throws ServerError if server fails to start
     */
    void Start();

    /**
     * @brief Stop the HTTP server
     */
    void Stop();

    /**
     * @brief Check if server is running
     */
    bool IsRunning() const;

    /**
     * @brief Register a handler for a specific endpoint
     * @param path The endpoint path (e.g., "/api/v1/query")
     * @param handler The handler function
     */
    void RegisterHandler(const std::string& path, RequestHandler handler);
    
    /**
     * @brief Register a query handler that receives full request path with query parameters
     * @param path The endpoint path (e.g., "/api/v1/query")
     * @param handler The query handler function that returns JSON response
     */
    void RegisterQueryHandler(const std::string& path, QueryHandlerFunc handler);

    /**
     * @brief Get server metrics
     * @return JSON string with server metrics
     */
    std::string GetMetrics() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    std::atomic<bool> running_{false};
};

// Exception classes
class ServerError : public std::runtime_error {
public:
    explicit ServerError(const std::string& message) 
        : std::runtime_error(message) {}
};

class HandlerError : public std::runtime_error {
public:
    explicit HandlerError(const std::string& message) 
        : std::runtime_error(message) {}
};

} // namespace prometheus
} // namespace tsdb 