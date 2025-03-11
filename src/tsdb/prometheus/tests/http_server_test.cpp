#include <gtest/gtest.h>
#include <httplib.h>
#include <thread>
#include <chrono>
#include "../server/http_server.h"

namespace tsdb {
namespace prometheus {
namespace {

class HttpServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        config.listen_address = "127.0.0.1";
        config.port = 9091;  // Use non-standard port for testing
        config.num_threads = 2;
        config.timeout_seconds = 5;
        config.max_connections = 10;
    }
    
    void TearDown() override {
        if (server && server->IsRunning()) {
            server->Stop();
        }
    }
    
    // Helper to wait for server to start
    void WaitForServer() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    ServerConfig config;
    std::unique_ptr<HttpServer> server;
    httplib::Client client{"http://127.0.0.1:9091"};
};

TEST_F(HttpServerTest, StartStop) {
    server = std::make_unique<HttpServer>(config);
    
    EXPECT_FALSE(server->IsRunning());
    
    EXPECT_NO_THROW({
        server->Start();
    });
    
    EXPECT_TRUE(server->IsRunning());
    
    EXPECT_NO_THROW({
        server->Stop();
    });
    
    EXPECT_FALSE(server->IsRunning());
}

TEST_F(HttpServerTest, DoubleStart) {
    server = std::make_unique<HttpServer>(config);
    
    EXPECT_NO_THROW({
        server->Start();
    });
    
    EXPECT_THROW({
        server->Start();
    }, ServerError);
    
    server->Stop();
}

TEST_F(HttpServerTest, DefaultEndpoints) {
    server = std::make_unique<HttpServer>(config);
    server->Start();
    WaitForServer();
    
    // Test /health endpoint
    auto health = client.Get("/health");
    ASSERT_TRUE(health);
    EXPECT_EQ(health->status, 200);
    EXPECT_EQ(health->body, "{\"status\":\"up\"}");
    
    // Test /metrics endpoint
    auto metrics = client.Get("/metrics");
    ASSERT_TRUE(metrics);
    EXPECT_EQ(metrics->status, 200);
    EXPECT_TRUE(metrics->body.find("active_connections") != std::string::npos);
    EXPECT_TRUE(metrics->body.find("total_requests") != std::string::npos);
    
    server->Stop();
}

TEST_F(HttpServerTest, CustomHandler) {
    server = std::make_unique<HttpServer>(config);
    
    // Register custom handler
    server->RegisterHandler("/test", [](const std::string& request, std::string& response) {
        response = "{\"message\":\"test\"}";
    });
    
    server->Start();
    WaitForServer();
    
    // Test GET request
    auto get = client.Get("/test");
    ASSERT_TRUE(get);
    EXPECT_EQ(get->status, 200);
    EXPECT_EQ(get->body, "{\"message\":\"test\"}");
    
    // Test POST request
    auto post = client.Post("/test", "{\"data\":\"test\"}", "application/json");
    ASSERT_TRUE(post);
    EXPECT_EQ(post->status, 200);
    EXPECT_EQ(post->body, "{\"message\":\"test\"}");
    
    server->Stop();
}

TEST_F(HttpServerTest, HandlerError) {
    server = std::make_unique<HttpServer>(config);
    
    // Register handler that throws
    server->RegisterHandler("/error", [](const std::string&, std::string&) {
        throw std::runtime_error("test error");
    });
    
    server->Start();
    WaitForServer();
    
    auto res = client.Get("/error");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 500);
    EXPECT_TRUE(res->body.find("test error") != std::string::npos);
    
    server->Stop();
}

TEST_F(HttpServerTest, ConcurrentRequests) {
    server = std::make_unique<HttpServer>(config);
    
    // Register handler with delay
    server->RegisterHandler("/slow", [](const std::string&, std::string& response) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        response = "{\"status\":\"done\"}";
    });
    
    server->Start();
    WaitForServer();
    
    // Make concurrent requests
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&]() {
            auto res = client.Get("/slow");
            if (res && res->status == 200) {
                ++success_count;
            }
        });
    }
    
    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }
    
    EXPECT_EQ(success_count, 5);
    
    server->Stop();
}

TEST_F(HttpServerTest, ServerTimeout) {
    // Set very short timeout
    config.timeout_seconds = 1;
    server = std::make_unique<HttpServer>(config);
    
    // Register slow handler
    server->RegisterHandler("/timeout", [](const std::string&, std::string& response) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        response = "{\"status\":\"done\"}";
    });
    
    server->Start();
    WaitForServer();
    
    auto res = client.Get("/timeout");
    ASSERT_FALSE(res);  // Request should fail due to timeout
    
    server->Stop();
}

TEST_F(HttpServerTest, MaxConnections) {
    // Set low max connections
    config.max_connections = 2;
    server = std::make_unique<HttpServer>(config);
    
    // Register handler with delay
    server->RegisterHandler("/connect", [](const std::string&, std::string& response) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        response = "{\"status\":\"done\"}";
    });
    
    server->Start();
    WaitForServer();
    
    // Make more requests than max_connections
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    std::atomic<int> error_count{0};
    
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&]() {
            auto res = client.Get("/connect");
            if (res && res->status == 200) {
                ++success_count;
            } else {
                ++error_count;
            }
        });
    }
    
    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }
    
    // Some requests should fail due to max connections limit
    EXPECT_GT(error_count, 0);
    
    server->Stop();
}

} // namespace
} // namespace prometheus
} // namespace tsdb 