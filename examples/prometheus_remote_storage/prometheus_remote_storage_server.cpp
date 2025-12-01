/**
 * @file prometheus_remote_storage_server.cpp
 * @brief Example server demonstrating Prometheus Remote Write/Read API with Authentication
 * 
 * This example shows how to:
 * 1. Create a storage instance
 * 2. Set up HTTP server with Remote Write/Read handlers
 * 3. Configure different authentication mechanisms
 * 4. Start the server and handle Prometheus remote storage requests
 * 
 * Usage:
 *   ./prometheus_remote_storage_server [port] [auth_type]
 * 
 * Auth types:
 *   none    - No authentication (default)
 *   basic   - Basic authentication (user:pass)
 *   bearer  - Bearer token authentication
 *   header  - Header-based multi-tenancy
 *   composite - Multiple auth methods
 * 
 * Examples:
 *   ./prometheus_remote_storage_server 9090 none
 *   ./prometheus_remote_storage_server 9090 basic
 *   ./prometheus_remote_storage_server 9090 bearer
 */

#include "tsdb/storage/storage_impl.h"
#include "tsdb/prometheus/server/http_server.h"
#include "tsdb/prometheus/remote/write_handler.h"
#include "tsdb/prometheus/remote/read_handler.h"
#include "tsdb/prometheus/storage/tsdb_adapter.h"
#include "tsdb/prometheus/promql/engine.h"
#include "tsdb/prometheus/api/query_handler.h"
#include "tsdb/prometheus/auth/no_auth.h"
#include "tsdb/prometheus/auth/basic_auth.h"
#include "tsdb/prometheus/auth/bearer_auth.h"
#include "tsdb/prometheus/auth/header_auth.h"
#include "tsdb/prometheus/auth/composite_auth.h"
#include <iostream>
#include <csignal>
#include <memory>
#include <string>

using namespace tsdb;

// Global server pointer for signal handling
std::unique_ptr<prometheus::HttpServer> g_server;

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nShutting down server..." << std::endl;
        if (g_server) {
            g_server->Stop();
        }
        exit(0);
    }
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [port] [auth_type]\n\n";
    std::cout << "Auth types:\n";
    std::cout << "  none      - No authentication (default)\n";
    std::cout << "  basic     - Basic authentication\n";
    std::cout << "  bearer    - Bearer token authentication\n";
    std::cout << "  header    - Header-based multi-tenancy\n";
    std::cout << "  composite - Multiple auth methods (Basic OR Bearer)\n";
    std::cout << "\nExamples:\n";
    std::cout << "  " << program_name << " 9090 none\n";
    std::cout << "  " << program_name << " 9090 basic\n";
    std::cout << "  " << program_name << " 9090 bearer\n";
}

std::shared_ptr<prometheus::auth::Authenticator> create_authenticator(const std::string& auth_type) {
    if (auth_type == "basic") {
        auto auth = std::make_shared<prometheus::auth::BasicAuthenticator>();
        auth->AddUserWithPassword("prometheus", "secret", "default");
        auth->AddUserWithPassword("grafana", "password", "grafana-tenant");
        std::cout << "\n📝 Basic Auth Credentials:\n";
        std::cout << "   Username: prometheus, Password: secret\n";
        std::cout << "   Username: grafana, Password: password\n";
        return auth;
        
    } else if (auth_type == "bearer") {
        auto auth = std::make_shared<prometheus::auth::BearerAuthenticator>();
        auth->AddToken("secret-token-123", "tenant1");
        auth->AddToken("secret-token-456", "tenant2");
        std::cout << "\n🔑 Bearer Tokens:\n";
        std::cout << "   Token: secret-token-123 (tenant1)\n";
        std::cout << "   Token: secret-token-456 (tenant2)\n";
        return auth;
        
    } else if (auth_type == "header") {
        auto auth = std::make_shared<prometheus::auth::HeaderAuthenticator>();
        auth->SetTenantHeader("X-Scope-OrgID");
        auth->SetValidateTenants(false);  // Accept any tenant
        std::cout << "\n🏢 Header-Based Multi-tenancy:\n";
        std::cout << "   Header: X-Scope-OrgID\n";
        std::cout << "   Any tenant ID accepted\n";
        return auth;
        
    } else if (auth_type == "composite") {
        auto basic = std::make_shared<prometheus::auth::BasicAuthenticator>();
        basic->AddUserWithPassword("user", "pass");
        
        auto bearer = std::make_shared<prometheus::auth::BearerAuthenticator>();
        bearer->AddToken("token123");
        
        auto composite = std::make_shared<prometheus::auth::CompositeAuthenticator>();
        composite->SetMode(prometheus::auth::CompositeAuthenticator::Mode::ANY);
        composite->AddAuthenticator(basic);
        composite->AddAuthenticator(bearer);
        
        std::cout << "\n🔀 Composite Auth (ANY mode):\n";
        std::cout << "   Basic: user:pass\n";
        std::cout << "   Bearer: token123\n";
        std::cout << "   Either method accepted\n";
        return composite;
        
    } else {
        std::cout << "\n🔓 No Authentication\n";
        return std::make_shared<prometheus::auth::NoAuthenticator>();
    }
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    uint16_t port = 9090;
    std::string auth_type = "none";
    
    if (argc > 1) {
        if (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
            print_usage(argv[0]);
            return 0;
        }
        port = static_cast<uint16_t>(std::atoi(argv[1]));
    }
    
    if (argc > 2) {
        auth_type = argv[2];
    }
    
    std::cout << "=== Prometheus Remote Storage Server ===\n";
    std::cout << "Starting server on port " << port << std::endl;
    std::cout << "Authentication: " << auth_type << std::endl;
    
    try {
        // 1. Create storage instance
        core::StorageConfig storage_config = core::StorageConfig::Default();
        storage_config.data_dir = "./prometheus_remote_data";
        
        auto storage = std::make_shared<storage::StorageImpl>(storage_config);
        
        // Initialize storage
        auto init_result = storage->init(storage_config);
        if (!init_result.ok()) {
            std::cerr << "Failed to initialize storage: " << init_result.error() << std::endl;
            return 1;
        }
        
        std::cout << "✓ Storage initialized at " << storage_config.data_dir << std::endl;
        
        // 2. Create authenticator
        auto authenticator = create_authenticator(auth_type);
        
        // 3. Create HTTP server
        prometheus::ServerConfig server_config;
        server_config.listen_address = "0.0.0.0";
        server_config.port = port;
        server_config.num_threads = 4;
        server_config.enable_compression = true;
        
        g_server = std::make_unique<prometheus::HttpServer>(server_config);
        
        // 4. Create and register Remote Write handler with auth
        auto write_handler = std::make_shared<prometheus::remote::WriteHandler>(storage, authenticator);
        g_server->RegisterHandler("/api/v1/write", 
            [write_handler](const prometheus::Request& req, std::string& res) {
                write_handler->Handle(req, res);
            });
        
        std::cout << "✓ Registered /api/v1/write endpoint" << std::endl;
        
        // 5. Create and register Remote Read handler with auth
        auto read_handler = std::make_shared<prometheus::remote::ReadHandler>(storage, authenticator);
        g_server->RegisterHandler("/api/v1/read",
            [read_handler](const prometheus::Request& req, std::string& res) {
                read_handler->Handle(req, res);
            });
        
        std::cout << "✓ Registered /api/v1/read endpoint" << std::endl;

        // 6. Create and register PromQL Query handler
        auto storage_adapter = std::make_shared<prometheus::storage::TSDBAdapter>(storage);
        
        prometheus::promql::EngineOptions engine_opts;
        engine_opts.storage_adapter = storage_adapter.get();
        auto engine = std::make_shared<prometheus::promql::Engine>(engine_opts);
        
        auto query_handler = std::make_shared<prometheus::api::QueryHandler>(engine);

        g_server->RegisterHandler("/api/v1/query",
            [query_handler](const prometheus::Request& req, std::string& res) {
                query_handler->HandleInstantQuery(req, res);
            });

        g_server->RegisterHandler("/api/v1/query_range",
            [query_handler](const prometheus::Request& req, std::string& res) {
                query_handler->HandleRangeQuery(req, res);
            });

        g_server->RegisterHandler("/api/v1/label/:name/values",
            [query_handler](const prometheus::Request& req, std::string& res) {
                query_handler->HandleLabelValues(req, res);
            });

        std::cout << "✓ Registered /api/v1/query endpoint" << std::endl;
        std::cout << "✓ Registered /api/v1/query_range endpoint" << std::endl;
        
        // 7. Register health check endpoint
        g_server->RegisterHandler("/health",
            [](const prometheus::Request& /*req*/, std::string& res) {
                res = "{\"status\":\"ok\"}";
            });
        
        std::cout << "✓ Registered /health endpoint" << std::endl;
        
        // 8. Set up signal handlers
        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);
        
        // 9. Start server
        std::cout << "\n=== Server Ready ===\n";
        std::cout << "Listening on http://" << server_config.listen_address 
                  << ":" << server_config.port << std::endl;
        std::cout << "\nEndpoints:\n";
        std::cout << "  POST /api/v1/write  - Prometheus Remote Write\n";
        std::cout << "  POST /api/v1/read   - Prometheus Remote Read\n";
        std::cout << "  GET  /api/v1/query  - PromQL Instant Query\n";
        std::cout << "  GET  /api/v1/query_range - PromQL Range Query\n";
        std::cout << "  GET  /health        - Health check\n";
        
        if (auth_type != "none") {
            std::cout << "\n📖 See AUTHENTICATION.md for configuration examples\n";
        }
        
        std::cout << "\nPress Ctrl+C to stop\n" << std::endl;
        
        g_server->Start();
        
        // Keep server running
        while (g_server->IsRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        // Cleanup
        storage->close();
        std::cout << "Server stopped" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
