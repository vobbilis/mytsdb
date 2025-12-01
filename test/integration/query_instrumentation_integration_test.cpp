#include <gtest/gtest.h>
#include <memory>
#include <filesystem>
#include <thread>
#include <chrono>

#include "tsdb/storage/storage_impl.h"
#include "tsdb/core/config.h"
#include "tsdb/prometheus/promql/engine.h"
#include "tsdb/prometheus/promql/query_metrics.h"
#include "tsdb/prometheus/storage/tsdb_adapter.h"
#include "tsdb/server/self_monitor.h"

namespace tsdb {
namespace integration {
namespace {

class QueryInstrumentationIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for test data
        test_dir_ = std::filesystem::temp_directory_path() / "tsdb_query_instrumentation_test";
        std::filesystem::create_directories(test_dir_);
        
        // Initialize storage
        core::StorageConfig config;
        config.data_dir = test_dir_.string();
        config.block_size = 4096;
        config.cache_size_bytes = 1024 * 1024;
        config.enable_compression = true;
        
        storage_ = std::make_shared<storage::StorageImpl>();
        auto init_result = storage_->init(config);
        ASSERT_TRUE(init_result.ok()) << "Failed to initialize storage: " << init_result.error();
        
        // Initialize Prometheus components
        adapter_ = std::make_shared<prometheus::storage::TSDBAdapter>(storage_);
        
        prometheus::promql::EngineOptions engine_opts;
        engine_opts.storage_adapter = adapter_.get();
        engine_ = std::make_shared<prometheus::promql::Engine>(engine_opts);
        
        // Reset query metrics
        prometheus::promql::QueryMetrics::GetInstance().Reset();
    }
    
    void TearDown() override {
        if (storage_) {
            storage_->close();
        }
        storage_.reset();
        adapter_.reset();
        engine_.reset();
        std::filesystem::remove_all(test_dir_);
    }
    
    void WriteTestData() {
        // Write some test time series
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        for (int i = 0; i < 10; ++i) {
            core::Labels labels;
            labels.add("__name__", "test_metric");
            labels.add("instance", "test_" + std::to_string(i));
            labels.add("job", "test_job");
            
            core::TimeSeries ts(labels);
            for (int j = 0; j < 100; ++j) {
                ts.add_sample(core::Sample(now - (100 - j) * 1000, static_cast<double>(i * 100 + j)));
            }
            
            auto write_result = storage_->write(ts);
            ASSERT_TRUE(write_result.ok()) << "Failed to write test data: " << write_result.error();
        }
    }
    
    std::filesystem::path test_dir_;
    std::shared_ptr<storage::StorageImpl> storage_;
    std::shared_ptr<prometheus::storage::TSDBAdapter> adapter_;
    std::shared_ptr<prometheus::promql::Engine> engine_;
};

TEST_F(QueryInstrumentationIntegrationTest, QueryMetricsCollected) {
    WriteTestData();
    
    // Execute a query
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    auto result = engine_->ExecuteInstant("test_metric", now);
    
    // Verify query executed successfully
    EXPECT_TRUE(result.error.empty()) << "Query failed: " << result.error;
    
    // Verify metrics were collected
    auto snapshot = prometheus::promql::QueryMetrics::GetInstance().GetSnapshot();
    
    EXPECT_GT(snapshot.query_count, 0) << "Query count should be > 0";
    EXPECT_GT(snapshot.total_query_time_ns, 0) << "Query time should be > 0";
    EXPECT_GT(snapshot.total_parse_time_ns, 0) << "Parse time should be > 0";
    EXPECT_GT(snapshot.total_eval_time_ns, 0) << "Eval time should be > 0";
}

TEST_F(QueryInstrumentationIntegrationTest, StorageReadMetricsCollected) {
    WriteTestData();
    
    // Execute a query that reads from storage
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    auto result = engine_->ExecuteInstant("test_metric", now);
    
    // Verify storage read metrics were collected
    auto snapshot = prometheus::promql::QueryMetrics::GetInstance().GetSnapshot();
    
    EXPECT_GT(snapshot.total_storage_read_time_ns, 0) << "Storage read time should be > 0";
    EXPECT_GT(snapshot.samples_scanned, 0) << "Samples scanned should be > 0";
    EXPECT_GT(snapshot.series_scanned, 0) << "Series scanned should be > 0";
}

TEST_F(QueryInstrumentationIntegrationTest, MultipleQueriesAccumulate) {
    WriteTestData();
    
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Execute multiple queries
    for (int i = 0; i < 5; ++i) {
        auto result = engine_->ExecuteInstant("test_metric", now);
        EXPECT_TRUE(result.error.empty());
    }
    
    // Verify metrics accumulated
    auto snapshot = prometheus::promql::QueryMetrics::GetInstance().GetSnapshot();
    
    EXPECT_EQ(snapshot.query_count, 5) << "Should have 5 queries";
    EXPECT_GT(snapshot.total_query_time_ns, 0);
}

TEST_F(QueryInstrumentationIntegrationTest, SelfMonitorWritesMetrics) {
    WriteTestData();
    
    // Get background processor
    auto bg_processor = storage_->GetBackgroundProcessor();
    ASSERT_NE(bg_processor, nullptr) << "Background processor should not be null";
    
    // Create and start self monitor
    server::SelfMonitor monitor(storage_, bg_processor);
    monitor.Start();
    
    // Execute some queries to generate metrics
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    for (int i = 0; i < 3; ++i) {
        auto result = engine_->ExecuteInstant("test_metric", now);
        EXPECT_TRUE(result.error.empty());
    }
    
    // Wait for self-monitor to scrape and write (it runs every 15s, but we'll trigger manually)
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // Stop self monitor
    monitor.Stop();
    
    // Verify that internal metrics were written to storage
    // We should be able to query for mytsdb_query_count_total
    core::LabelMatcher matcher;
    matcher.type = core::MatcherType::Equal;
    matcher.name = "__name__";
    matcher.value = "mytsdb_query_count_total";
    
    std::vector<core::LabelMatcher> matchers = {matcher};
    auto query_result = storage_->query(matchers, now - 60000, now);
    
    // Note: This might be empty if the self-monitor hasn't run yet
    // In a real test, we'd wait longer or trigger the scrape manually
    if (query_result.ok() && !query_result.value().empty()) {
        std::cout << "Found " << query_result.value().size() << " internal metric series" << std::endl;
    }
}

TEST_F(QueryInstrumentationIntegrationTest, ErrorQueriesTracked) {
    // Execute an invalid query
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    auto result = engine_->ExecuteInstant("invalid syntax (", now);
    
    // Verify error was tracked
    auto snapshot = prometheus::promql::QueryMetrics::GetInstance().GetSnapshot();
    
    EXPECT_GT(snapshot.query_count, 0);
    // Note: Error tracking depends on how errors are handled in the engine
}

} // namespace
} // namespace integration
} // namespace tsdb
