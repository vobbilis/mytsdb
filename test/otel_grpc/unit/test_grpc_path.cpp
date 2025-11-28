#include <gtest/gtest.h>
#include "opentelemetry/proto/metrics/v1/metrics.pb.h"
#include "opentelemetry/proto/common/v1/common.pb.h"
#include "opentelemetry/proto/collector/metrics/v1/metrics_service.pb.h"
#include "tsdb/otel/bridge.h"
#include "tsdb/storage/storage_impl.h"
#include "tsdb/core/config.h"
#include <filesystem>
#include <memory>
#include <iostream>

using namespace opentelemetry::proto::metrics::v1;
using namespace opentelemetry::proto::common::v1;
using namespace opentelemetry::proto::collector::metrics::v1;

// Test: Simulate exact gRPC path - ExportMetricsServiceRequest -> MetricsData -> Bridge
TEST(GRPCPath, SimulateExportRequest) {
    // Create temporary storage
    auto test_data_dir = std::filesystem::temp_directory_path() / ("tsdb_grpc_path_test_" + std::to_string(std::rand()));
    std::filesystem::create_directories(test_data_dir);
    
    tsdb::core::StorageConfig config;
    config.data_dir = test_data_dir.string();
    
    auto storage = std::make_shared<tsdb::storage::StorageImpl>();
    auto init_result = storage->init(config);
    ASSERT_TRUE(init_result.ok());
    
    // Create bridge (like MetricsService does)
    tsdb::otel::OTELMetricsBridgeOptions options;
    auto bridge = tsdb::otel::CreateOTELMetricsBridge(storage, options);
    
    // Create request exactly like benchmark does
    ExportMetricsServiceRequest request;
    auto* resource_metrics = request.add_resource_metrics();
    auto* scope_metrics = resource_metrics->add_scope_metrics();
    
    // Create metric with 40 attributes (like benchmark)
    Metric metric;
    metric.set_name("grpc_test_metric");
    
    NumberDataPoint point;
    point.set_time_unix_nano(1234567890000000);
    point.set_as_double(42.0);
    
    // Add 40 attributes
    for (int i = 0; i < 40; ++i) {
        auto* attr = point.add_attributes();
        attr->set_key("attr" + std::to_string(i));
        attr->mutable_value()->set_string_value("val" + std::to_string(i));
    }
    
    auto* gauge = metric.mutable_gauge();
    *gauge->mutable_data_points()->Add() = point;
    
    *scope_metrics->add_metrics() = metric;
    
    // Simulate MetricsService::Export (exact code path)
    std::cout << "Simulating MetricsService::Export..." << std::endl;
    for (const auto& resource_metrics : request.resource_metrics()) {
        opentelemetry::proto::metrics::v1::MetricsData metrics_data;
        *metrics_data.add_resource_metrics() = resource_metrics;
        
        std::cout << "Calling bridge_->ConvertMetrics..." << std::endl;
        auto result = bridge->ConvertMetrics(metrics_data);
        ASSERT_TRUE(result.ok()) << "Bridge conversion failed: " << result.error();
    }
    
    bridge->flush();
    
#include "tsdb/core/matcher.h"
// ...

    // Verify by querying
    std::vector<tsdb::core::LabelMatcher> matchers;
    matchers.emplace_back(tsdb::core::MatcherType::Equal, "__name__", "grpc_test_metric");
    
    int64_t start_time = 1234567890000 - 1000;
    int64_t end_time = 1234567890000 + 1000;
    
    auto query_result = storage->query(matchers, start_time, end_time);
    ASSERT_TRUE(query_result.ok());
    
    const auto& results = query_result.value();
    ASSERT_GT(results.size(), 0) << "No results found";
    
    const auto& series = results[0];
    const auto& labels = series.labels();
    
    std::cout << "Series has " << labels.map().size() << " labels (expected 41: __name__ + 40 attributes)" << std::endl;
    std::cout << "Labels: " << labels.to_string() << std::endl;
    
    EXPECT_GE(labels.map().size(), 41) << "Series should have 41 labels";
    
    // Cleanup
    storage->close();
    std::filesystem::remove_all(test_data_dir);
}

