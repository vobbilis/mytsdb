#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <filesystem>
#include "tsdb/storage/storage_impl.h"
#include "tsdb/core/config.h"
#include "tsdb/prometheus/promql/engine.h"
#include "tsdb/prometheus/promql/query_metrics.h"
#include "tsdb/prometheus/storage/tsdb_adapter.h"
#include "tsdb/server/self_monitor.h"

using namespace tsdb;

/**
 * End-to-end test for self-monitoring functionality
 * 
 * This test:
 * 1. Creates a storage instance
 * 2. Writes some test metrics
 * 3. Executes queries to generate query metrics
 * 4. Starts self-monitor
 * 5. Waits for self-monitor to write internal metrics
 * 6. Queries for internal metrics to verify they were written
 */
int main() {
    std::cout << "=== Self-Monitoring End-to-End Test ===" << std::endl;
    
    // Create temporary directory for test data
    auto test_dir = std::filesystem::temp_directory_path() / "tsdb_self_monitor_e2e_test";
    std::filesystem::remove_all(test_dir);
    std::filesystem::create_directories(test_dir);
    
    std::cout << "[Test] Using test directory: " << test_dir << std::endl;
    
    // Initialize storage
    core::StorageConfig config;
    config.data_dir = test_dir.string();
    config.block_size = 4096;
    config.cache_size_bytes = 1024 * 1024;
    config.enable_compression = true;
    
    auto storage = std::make_shared<storage::StorageImpl>();
    auto init_result = storage->init(config);
    if (!init_result.ok()) {
        std::cerr << "[Test] FAILED: Storage initialization failed: " << init_result.error() << std::endl;
        return 1;
    }
    std::cout << "[Test] ✓ Storage initialized" << std::endl;
    
    // Write some test data
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    for (int i = 0; i < 5; ++i) {
        core::Labels labels;
        labels.add("__name__", "test_metric");
        labels.add("instance", "test_" + std::to_string(i));
        labels.add("job", "test_job");
        
        core::TimeSeries ts(labels);
        for (int j = 0; j < 10; ++j) {
            ts.add_sample(core::Sample(now - (10 - j) * 1000, static_cast<double>(i * 10 + j)));
        }
        
        auto write_result = storage->write(ts);
        if (!write_result.ok()) {
            std::cerr << "[Test] WARNING: Failed to write test data: " << write_result.error() << std::endl;
        }
    }
    std::cout << "[Test] ✓ Wrote 5 test time series" << std::endl;
    
    // Initialize Prometheus components
    auto adapter = std::make_shared<prometheus::storage::TSDBAdapter>(storage);
    
    prometheus::promql::EngineOptions engine_opts;
    engine_opts.storage_adapter = adapter.get();
    auto engine = std::make_shared<prometheus::promql::Engine>(engine_opts);
    
    std::cout << "[Test] ✓ Prometheus engine initialized" << std::endl;
    
    // Execute some queries to generate metrics
    std::cout << "[Test] Executing 10 test queries..." << std::endl;
    for (int i = 0; i < 10; ++i) {
        auto result = engine->ExecuteInstant("test_metric", now);
        if (!result.error.empty()) {
            std::cout << "[Test] Query " << i << " error: " << result.error << std::endl;
        }
    }
    
    // Check query metrics
    auto query_snapshot = prometheus::promql::QueryMetrics::GetInstance().GetSnapshot();
    std::cout << "[Test] ✓ Query metrics collected:" << std::endl;
    std::cout << "  - Query count: " << query_snapshot.query_count << std::endl;
    std::cout << "  - Total query time: " << (query_snapshot.total_query_time_ns / 1e6) << " ms" << std::endl;
    
    if (query_snapshot.query_count == 0) {
        std::cerr << "[Test] FAILED: No queries were recorded!" << std::endl;
        return 1;
    }
    
    // Start self-monitor
    std::cout << "[Test] Starting self-monitor..." << std::endl;
    auto bg_processor = storage->GetBackgroundProcessor();
    if (!bg_processor) {
        std::cerr << "[Test] FAILED: Could not get background processor!" << std::endl;
        return 1;
    }
    std::cout << "[Test] ✓ Background processor obtained" << std::endl;
    
    server::SelfMonitor monitor(storage, bg_processor);
    monitor.Start();
    std::cout << "[Test] ✓ Self-monitor started" << std::endl;
    
    // Wait for self-monitor to run (it runs every 15 seconds)
    std::cout << "[Test] Waiting 25 seconds for self-monitor to write metrics..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(25));
    
    // Query for internal metrics
    std::cout << "[Test] Querying for internal metrics..." << std::endl;
    
    core::LabelMatcher matcher;
    matcher.name = "__name__";
    matcher.value = "mytsdb_query_count_total";
    
    std::vector<core::LabelMatcher> matchers = {matcher};
    auto query_result = storage->query(matchers, now - 60000, now + 60000);
    
    if (!query_result.ok()) {
        std::cerr << "[Test] FAILED: Query for internal metrics failed: " << query_result.error() << std::endl;
        monitor.Stop();
        return 1;
    }
    
    const auto& series_list = query_result.value();
    if (series_list.empty()) {
        std::cerr << "[Test] FAILED: No internal metrics found!" << std::endl;
        std::cerr << "[Test] Expected to find 'mytsdb_query_count_total' series" << std::endl;
        monitor.Stop();
        return 1;
    }
    
    std::cout << "[Test] ✓ Found " << series_list.size() << " internal metric series" << std::endl;
    
    // Verify the metric value
    for (const auto& series : series_list) {
        std::cout << "[Test] Series: " << series.labels().to_string() << std::endl;
        std::cout << "[Test]   Samples: " << series.samples().size() << std::endl;
        if (!series.samples().empty()) {
            std::cout << "[Test]   Latest value: " << series.samples().back().value() << std::endl;
            
            if (series.samples().back().value() >= query_snapshot.query_count) {
                std::cout << "[Test] ✓ Metric value matches query count!" << std::endl;
            } else {
                std::cerr << "[Test] WARNING: Metric value (" << series.samples().back().value() 
                          << ") is less than query count (" << query_snapshot.query_count << ")" << std::endl;
            }
        }
    }
    
    // Stop self-monitor
    monitor.Stop();
    std::cout << "[Test] ✓ Self-monitor stopped" << std::endl;
    
    // Cleanup
    storage->close();
    std::filesystem::remove_all(test_dir);
    
    std::cout << "\n=== TEST PASSED ===" << std::endl;
    std::cout << "Self-monitoring is working correctly!" << std::endl;
    
    return 0;
}
