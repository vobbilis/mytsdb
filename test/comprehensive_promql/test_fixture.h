#pragma once

#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <string>
#include <iostream>
#include <filesystem>
#include <chrono>
#include <random>

#include "tsdb/storage/storage_impl.h"
#include "tsdb/core/config.h"
#include "tsdb/prometheus/promql/engine.h"
#include "tsdb/prometheus/promql/evaluator.h"
#include "tsdb/prometheus/storage/tsdb_adapter.h"

namespace tsdb {
namespace comprehensive {

class ComprehensivePromQLTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        // Initialize storage once for all tests
        config_.data_dir = "data/comprehensive_test_data";
        config_.block_size = 4096;
        config_.max_blocks_per_series = 1000;
        config_.cache_size_bytes = 1024 * 1024 * 1024; // 1GB cache
        config_.block_duration = 3600 * 1000;
        config_.retention_period = 30 * 24 * 3600 * 1000LL; // 30 days
        config_.enable_compression = true;

        // Clean up old test data
        std::filesystem::remove_all(config_.data_dir);
        std::filesystem::create_directories(config_.data_dir);

        storage_ = std::make_shared<tsdb::storage::StorageImpl>();
        auto result = storage_->init(config_);
        if (!result.ok()) {
            std::cerr << "Failed to initialize storage: " << result.error() << std::endl;
            exit(1);
        }
        
        // Generate test data for PromQL tests
        GenerateTestData();
    }

    static void TearDownTestSuite() {
        if (storage_) {
            storage_->close();
            storage_.reset();
        }
        // Clean up test data
        std::filesystem::remove_all(config_.data_dir);
    }

    void SetUp() override {
    }

    // Generate synthetic test data for PromQL queries
    static void GenerateTestData() {
        std::cout << "[TestFixture] Generating test data..." << std::endl;
        
        // Services, methods, status codes for realistic cardinality
        const std::vector<std::string> services = {"frontend", "backend", "db", "cache", "auth"};
        const std::vector<std::string> methods = {"GET", "POST", "PUT", "DELETE"};
        const std::vector<std::string> statuses = {"200", "201", "400", "404", "500"};
        
        int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        std::random_device rd;
        std::mt19937 gen(42); // Fixed seed for reproducibility
        std::uniform_real_distribution<> value_dist(100.0, 10000.0);
        
        int series_count = 0;
        
        // Generate http_requests_total series
        // Target: ~200 series with good label cardinality
        for (int pod = 0; pod < 40; ++pod) {
            for (const auto& service : services) {
                // Each pod has one service
                std::string method = methods[pod % methods.size()];
                std::string status = statuses[pod % statuses.size()];
                
                tsdb::core::Labels::Map labels_map = {
                    {"__name__", "http_requests_total"},
                    {"pod", "pod-" + std::to_string(pod)},
                    {"service", service},
                    {"method", method},
                    {"status", status}
                };
                tsdb::core::Labels labels(labels_map);
                tsdb::core::TimeSeries series(labels);
                
                // Generate 60 samples over the past hour (1 per minute)
                double base_value = value_dist(gen);
                for (int i = 0; i < 60; ++i) {
                    int64_t ts = now - (60 - i) * 60000;  // Past hour, 1 sample/min
                    double value = base_value + i * 10.0;  // Increasing counter
                    series.add_sample(ts, value);
                }
                
                auto result = storage_->write(series);
                if (!result.ok()) {
                    std::cerr << "Failed to write http_requests_total: " << result.error() << std::endl;
                }
                series_count++;
            }
        }
        
        // Generate node_cpu_usage_ratio for 50 nodes
        for (int node = 0; node < 50; ++node) {
            std::string zone = "zone-" + std::to_string(node % 3);
            
            tsdb::core::Labels::Map labels_map = {
                {"__name__", "node_cpu_usage_ratio"},
                {"node", "node-" + std::to_string(node)},
                {"zone", zone}
            };
            tsdb::core::Labels labels(labels_map);
            tsdb::core::TimeSeries series(labels);
            
            double base_value = 0.3 + (node % 10) * 0.05;  // 0.3 to 0.75
            for (int i = 0; i < 60; ++i) {
                int64_t ts = now - (60 - i) * 60000;
                double value = base_value + (i % 10) * 0.01;
                series.add_sample(ts, value);
            }
            
            storage_->write(series);
            series_count++;
        }
        
        // Generate 'up' metric for availability
        for (int pod = 0; pod < 100; ++pod) {
            std::string service = services[pod % services.size()];
            std::string ns = "namespace-" + std::to_string(pod % 5);
            
            tsdb::core::Labels::Map labels_map = {
                {"__name__", "up"},
                {"pod", "pod-" + std::to_string(pod)},
                {"service", service},
                {"namespace", ns}
            };
            tsdb::core::Labels labels(labels_map);
            tsdb::core::TimeSeries series(labels);
            
            // All pods are up (value = 1)
            for (int i = 0; i < 60; ++i) {
                int64_t ts = now - (60 - i) * 60000;
                series.add_sample(ts, 1.0);
            }
            
            storage_->write(series);
            series_count++;
        }
        
        // Generate http_request_duration_seconds_* for services
        for (const auto& service : services) {
            for (const auto& method : methods) {
                // _sum
                {
                    tsdb::core::Labels::Map labels_map = {
                        {"__name__", "http_request_duration_seconds_sum"},
                        {"service", service},
                        {"method", method}
                    };
                    tsdb::core::Labels labels(labels_map);
                    tsdb::core::TimeSeries series(labels);
                    
                    double base_val = 10.0;
                    for (int i = 0; i < 60; ++i) {
                        int64_t ts = now - (60 - i) * 60000;
                        series.add_sample(ts, base_val + i * 0.5);
                    }
                    storage_->write(series);
                    series_count++;
                }
                // _count
                {
                    tsdb::core::Labels::Map labels_map = {
                        {"__name__", "http_request_duration_seconds_count"},
                        {"service", service},
                        {"method", method}
                    };
                    tsdb::core::Labels labels(labels_map);
                    tsdb::core::TimeSeries series(labels);
                    
                    double base_val = 100.0;
                    for (int i = 0; i < 60; ++i) {
                        int64_t ts = now - (60 - i) * 60000;
                        series.add_sample(ts, base_val + i * 5.0);
                    }
                    storage_->write(series);
                    series_count++;
                }
            }
        }
        
        std::cout << "[TestFixture] Generated " << series_count << " test series" << std::endl;
    }

    tsdb::prometheus::promql::Value ExecuteQuery(const std::string& query, int64_t timestamp) {
        std::cout << "[PromQL] Executing: " << query << std::endl;
        // Create adapter
        auto adapter = std::make_unique<tsdb::prometheus::storage::TSDBAdapter>(storage_);
        
        tsdb::prometheus::promql::EngineOptions options;
        options.storage_adapter = adapter.get();
        
        tsdb::prometheus::promql::Engine engine(options);
        tsdb::prometheus::promql::QueryResult result;
        try {
            result = engine.ExecuteInstant(query, timestamp);
        } catch (const std::exception& e) {
            std::cerr << "Query exception: " << e.what() << std::endl;
            return tsdb::prometheus::promql::Value();
        }
        
        if (result.hasError()) {
            std::cerr << "Query error: " << result.error << std::endl;
            return tsdb::prometheus::promql::Value(); // Return empty/none
        }
        return result.value;
    }

    // Helper to get current query time (now)
    int64_t GetQueryTime() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    static tsdb::core::StorageConfig config_;
    static std::shared_ptr<tsdb::storage::StorageImpl> storage_;
};

} // namespace comprehensive
} // namespace tsdb
