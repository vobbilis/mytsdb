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

class BridgeConversionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary storage
        test_data_dir_ = std::filesystem::temp_directory_path() / ("tsdb_bridge_test_" + std::to_string(std::rand()));
        std::filesystem::create_directories(test_data_dir_);
        
        tsdb::core::StorageConfig config;
        config.data_dir = test_data_dir_.string();
        
        storage_ = std::make_shared<tsdb::storage::StorageImpl>();
        auto init_result = storage_->init(config);
        ASSERT_TRUE(init_result.ok()) << "Failed to initialize storage: " << init_result.error();
        
        // Create bridge
        tsdb::otel::OTELMetricsBridgeOptions options;
        bridge_ = tsdb::otel::CreateOTELMetricsBridge(storage_, options);
    }
    
    void TearDown() override {
        if (storage_) {
            storage_->close();
        }
        if (std::filesystem::exists(test_data_dir_)) {
            std::filesystem::remove_all(test_data_dir_);
        }
    }
    
    std::filesystem::path test_data_dir_;
    std::shared_ptr<tsdb::storage::Storage> storage_;
    std::shared_ptr<tsdb::otel::Bridge> bridge_;
};

// Test: Verify bridge converts data point attributes to labels
TEST_F(BridgeConversionTest, ConvertDataPointAttributes) {
    // Create a data point with 10 attributes
    NumberDataPoint point;
    point.set_time_unix_nano(1234567890000000000);
    point.set_as_double(42.0);
    
    for (int i = 0; i < 10; ++i) {
        auto* attr = point.add_attributes();
        attr->set_key("label" + std::to_string(i));
        attr->mutable_value()->set_string_value("value" + std::to_string(i));
    }
    
    ASSERT_EQ(point.attributes_size(), 10);
    
    // Create full request structure
    ExportMetricsServiceRequest request;
    auto* resource_metrics = request.add_resource_metrics();
    auto* scope_metrics = resource_metrics->add_scope_metrics();
    
    Metric metric;
    metric.set_name("test_metric");
    auto* gauge = metric.mutable_gauge();
    *gauge->mutable_data_points()->Add() = point;
    
    *scope_metrics->add_metrics() = metric;
    
    // Convert to MetricsData
    opentelemetry::proto::metrics::v1::MetricsData metrics_data;
    *metrics_data.add_resource_metrics() = request.resource_metrics(0);
    
    // Convert via bridge
    auto result = bridge_->ConvertMetrics(metrics_data.resource_metrics());
    ASSERT_TRUE(result.ok()) << "Bridge conversion failed: " << result.error();
    
#include "tsdb/core/matcher.h"
// ...

    // Verify data was written by querying
    std::vector<tsdb::core::LabelMatcher> matchers;
    matchers.emplace_back(tsdb::core::MatcherType::Equal, "__name__", "test_metric");
    matchers.emplace_back(tsdb::core::MatcherType::Equal, "label0", "value0");
    
    int64_t start_time = 1234567890000 - 1000;  // 1 second before
    int64_t end_time = 1234567890000 + 1000;    // 1 second after
    
    auto query_result = storage_->query(matchers, start_time, end_time);
    ASSERT_TRUE(query_result.ok()) << "Query failed: " << query_result.error();
    
    const auto& results = query_result.value();
    ASSERT_GT(results.size(), 0) << "No results found - data was not written";
    
    // Check that the series has all labels
    const auto& series = results[0];
    const auto& labels = series.labels();
    
    // Should have __name__ + 10 data point attributes = 11 labels
    std::cout << "Series has " << labels.map().size() << " labels" << std::endl;
    std::cout << "Labels: " << labels.to_string() << std::endl;
    
    EXPECT_GE(labels.map().size(), 11) << "Series should have at least 11 labels (__name__ + 10 attributes)";
    EXPECT_TRUE(labels.has("__name__")) << "Should have __name__ label";
    EXPECT_TRUE(labels.has("label0")) << "Should have label0";
    EXPECT_TRUE(labels.has("label9")) << "Should have label9";
    
    if (labels.has("label0")) {
        EXPECT_EQ(labels.get("label0").value(), "value0");
    }
}

// Test: Verify bridge handles 40 attributes (like benchmark)
TEST_F(BridgeConversionTest, ConvertManyAttributes) {
    // Create a data point with 40 attributes (like benchmark)
    NumberDataPoint point;
    point.set_time_unix_nano(1234567890000000000);
    point.set_as_double(42.0);
    
    for (int i = 0; i < 40; ++i) {
        auto* attr = point.add_attributes();
        attr->set_key("attr" + std::to_string(i));
        attr->mutable_value()->set_string_value("val" + std::to_string(i));
    }
    
    ASSERT_EQ(point.attributes_size(), 40);
    
    // Create full request
    ExportMetricsServiceRequest request;
    auto* resource_metrics = request.add_resource_metrics();
    auto* scope_metrics = resource_metrics->add_scope_metrics();
    
    Metric metric;
    metric.set_name("many_attrs_metric");
    auto* gauge = metric.mutable_gauge();
    *gauge->mutable_data_points()->Add() = point;
    
    *scope_metrics->add_metrics() = metric;
    
    // Convert
    opentelemetry::proto::metrics::v1::MetricsData metrics_data;
    *metrics_data.add_resource_metrics() = request.resource_metrics(0);
    
    auto result = bridge_->ConvertMetrics(metrics_data.resource_metrics());
    ASSERT_TRUE(result.ok());
    
    // Query and verify
    std::vector<tsdb::core::LabelMatcher> matchers;
    matchers.emplace_back(tsdb::core::MatcherType::Equal, "__name__", "many_attrs_metric");
    matchers.emplace_back(tsdb::core::MatcherType::Equal, "attr0", "val0");
    
    int64_t start_time = 1234567890000 - 1000;
    int64_t end_time = 1234567890000 + 1000;
    
    auto query_result = storage_->query(matchers, start_time, end_time);
    ASSERT_TRUE(query_result.ok());
    
    const auto& results = query_result.value();
    ASSERT_GT(results.size(), 0) << "No results found";
    
    const auto& series = results[0];
    const auto& labels = series.labels();
    
    std::cout << "Series has " << labels.map().size() << " labels (expected 41: __name__ + 40 attributes)" << std::endl;
    
    EXPECT_GE(labels.map().size(), 41) << "Series should have 41 labels (__name__ + 40 attributes)";
    EXPECT_TRUE(labels.has("__name__"));
    EXPECT_TRUE(labels.has("attr0"));
    EXPECT_TRUE(labels.has("attr39"));
}

