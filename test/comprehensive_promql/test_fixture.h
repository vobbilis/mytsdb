#pragma once

#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <string>
#include <iostream>
#include <filesystem>
#include <chrono>

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

        storage_ = std::make_shared<tsdb::storage::StorageImpl>();
        auto result = storage_->init(config_);
        if (!result.ok()) {
            std::cerr << "Failed to initialize storage: " << result.error() << std::endl;
            exit(1);
        }
    }

    static void TearDownTestSuite() {
        if (storage_) {
            storage_->close();
            storage_.reset();
        }
    }

    void SetUp() override {
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

    // Helper to get current query time (now + 1h)
    int64_t GetQueryTime() {
        int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        return now + 3600 * 1000;
    }

    static tsdb::core::StorageConfig config_;
    static std::shared_ptr<tsdb::storage::StorageImpl> storage_;
};

} // namespace comprehensive
} // namespace tsdb
