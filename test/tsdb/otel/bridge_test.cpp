#include <gtest/gtest.h>
#include "tsdb/otel/bridge.h"
#include "tsdb/storage/storage.h"
#include <filesystem>
#include <limits>
#include <thread>
#include <future>

namespace tsdb {
namespace otel {
namespace {

class OTELBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for test data
        test_dir_ = std::filesystem::temp_directory_path() / "tsdb_otel_test";
        std::filesystem::create_directories(test_dir_);
        
        storage::StorageOptions storage_options;
        storage_options.data_dir = test_dir_.string();
        storage_ = storage::CreateStorage(storage_options);
        
        OTELMetricsBridgeOptions bridge_options;
        bridge_ = CreateOTELMetricsBridge(storage_, bridge_options);
    }
    
    void TearDown() override {
        bridge_.reset();
        storage_.reset();
        std::filesystem::remove_all(test_dir_);
    }
    
    std::filesystem::path test_dir_;
    std::shared_ptr<storage::Storage> storage_;
    std::unique_ptr<OTELMetricsBridge> bridge_;
};

TEST_F(OTELBridgeTest, ConvertGaugeMetric) {
    opentelemetry::proto::metrics::v1::MetricsData metrics_data;
    
    // Create a resource
    auto* resource_metrics = metrics_data.add_resource_metrics();
    auto* resource = resource_metrics->mutable_resource();
    auto* resource_attr = resource->mutable_attributes()->add_values();
    resource_attr->set_key("service.name");
    resource_attr->mutable_value()->set_string_value("test_service");
    
    // Create a scope
    auto* scope_metrics = resource_metrics->add_scope_metrics();
    auto* scope = scope_metrics->mutable_scope();
    auto* scope_attr = scope->mutable_attributes()->add_values();
    scope_attr->set_key("library.name");
    scope_attr->mutable_value()->set_string_value("test_library");
    
    // Create a gauge metric
    auto* metric = scope_metrics->add_metrics();
    metric->set_name("test_gauge");
    auto* gauge = metric->mutable_gauge();
    auto* datapoint = gauge->add_data_points();
    datapoint->set_time_unix_nano(1000000000);  // 1 second
    datapoint->set_as_double(42.0);
    
    // Convert metrics
    auto result = bridge_->ConvertMetrics(metrics_data);
    ASSERT_TRUE(result.ok());
    
    // Verify conversion
    EXPECT_EQ(bridge_->ProcessedMetrics(), 1);
    EXPECT_EQ(bridge_->DroppedMetrics(), 0);
    
    // Query the storage to verify the data
    core::Labels query_labels = {{"__name__", "test_gauge"}};
    auto series = storage_->Query(query_labels, 0, 2000);
    ASSERT_TRUE(series.ok());
    ASSERT_EQ(series.value().size(), 1);
    
    auto samples = storage_->Read(series.value()[0], 0, 2000);
    ASSERT_TRUE(samples.ok());
    ASSERT_EQ(samples.value().size(), 1);
    EXPECT_EQ(samples.value()[0].timestamp, 1000);
    EXPECT_DOUBLE_EQ(samples.value()[0].value, 42.0);
}

TEST_F(OTELBridgeTest, ConvertHistogramMetric) {
    opentelemetry::proto::metrics::v1::MetricsData metrics_data;
    
    // Create a resource metrics
    auto* resource_metrics = metrics_data.add_resource_metrics();
    auto* scope_metrics = resource_metrics->add_scope_metrics();
    
    // Create a histogram metric
    auto* metric = scope_metrics->add_metrics();
    metric->set_name("test_histogram");
    auto* histogram = metric->mutable_histogram();
    auto* datapoint = histogram->add_data_points();
    
    // Set histogram data
    datapoint->set_time_unix_nano(1000000000);
    datapoint->set_count(100);
    datapoint->set_sum(1000.0);
    
    // Add bucket counts
    std::vector<uint64_t> bucket_counts = {10, 20, 30, 40};
    for (uint64_t count : bucket_counts) {
        datapoint->add_bucket_counts(count);
    }
    
    std::vector<double> bounds = {1.0, 5.0, 10.0};
    for (double bound : bounds) {
        datapoint->add_explicit_bounds(bound);
    }
    
    // Convert metrics
    auto result = bridge_->ConvertMetrics(metrics_data);
    ASSERT_TRUE(result.ok());
    
    // Verify conversion
    EXPECT_EQ(bridge_->ProcessedMetrics(), 1);
    EXPECT_EQ(bridge_->DroppedMetrics(), 0);
    
    // Query the storage to verify the data
    core::Labels query_labels = {{"__name__", "test_histogram"}};
    auto series = storage_->Query(query_labels, 0, 2000);
    ASSERT_TRUE(series.ok());
    ASSERT_EQ(series.value().size(), 1);
    
    auto samples = storage_->Read(series.value()[0], 0, 2000);
    ASSERT_TRUE(samples.ok());
    
    // Verify histogram data points
    // We expect count, sum, and bucket counts
    ASSERT_EQ(samples.value().size(), 6);  // count + sum + 4 buckets
    
    size_t idx = 0;
    EXPECT_EQ(samples.value()[idx].timestamp, 1000);
    EXPECT_DOUBLE_EQ(samples.value()[idx].value, 100);  // count
    
    idx++;
    EXPECT_EQ(samples.value()[idx].timestamp, 1001);
    EXPECT_DOUBLE_EQ(samples.value()[idx].value, 1000.0);  // sum
    
    // Verify bucket counts
    for (size_t i = 0; i < bucket_counts.size(); i++) {
        idx++;
        EXPECT_EQ(samples.value()[idx].timestamp, 1002 + i);
        EXPECT_DOUBLE_EQ(samples.value()[idx].value, bucket_counts[i]);
    }
}

TEST_F(OTELBridgeTest, ConvertMultipleMetrics) {
    opentelemetry::proto::metrics::v1::MetricsData metrics_data;
    
    // Create multiple metrics of different types
    auto* resource_metrics = metrics_data.add_resource_metrics();
    auto* scope_metrics = resource_metrics->add_scope_metrics();
    
    // Add a gauge
    {
        auto* metric = scope_metrics->add_metrics();
        metric->set_name("test_gauge");
        auto* gauge = metric->mutable_gauge();
        auto* datapoint = gauge->add_data_points();
        datapoint->set_time_unix_nano(1000000000);
        datapoint->set_as_double(42.0);
    }
    
    // Add a sum
    {
        auto* metric = scope_metrics->add_metrics();
        metric->set_name("test_sum");
        auto* sum = metric->mutable_sum();
        auto* datapoint = sum->add_data_points();
        datapoint->set_time_unix_nano(1000000000);
        datapoint->set_as_double(100.0);
    }
    
    // Add a histogram
    {
        auto* metric = scope_metrics->add_metrics();
        metric->set_name("test_histogram");
        auto* histogram = metric->mutable_histogram();
        auto* datapoint = histogram->add_data_points();
        datapoint->set_time_unix_nano(1000000000);
        datapoint->set_count(10);
        datapoint->set_sum(50.0);
        datapoint->add_bucket_counts(5);
        datapoint->add_bucket_counts(5);
        datapoint->add_explicit_bounds(10.0);
    }
    
    // Convert metrics
    auto result = bridge_->ConvertMetrics(metrics_data);
    ASSERT_TRUE(result.ok());
    
    // Verify conversion
    EXPECT_EQ(bridge_->ProcessedMetrics(), 3);
    EXPECT_EQ(bridge_->DroppedMetrics(), 0);
    
    // Verify each metric type
    std::vector<std::string> metric_names = {
        "test_gauge", "test_sum", "test_histogram"};
    
    for (const auto& name : metric_names) {
        core::Labels query_labels = {{"__name__", name}};
        auto series = storage_->Query(query_labels, 0, 2000);
        ASSERT_TRUE(series.ok());
        ASSERT_EQ(series.value().size(), 1);
        
        auto samples = storage_->Read(series.value()[0], 0, 2000);
        ASSERT_TRUE(samples.ok());
        ASSERT_FALSE(samples.value().empty());
    }
}

TEST_F(OTELBridgeTest, HandleInvalidMetrics) {
    opentelemetry::proto::metrics::v1::MetricsData metrics_data;
    
    // Create a metric without any data (invalid)
    auto* resource_metrics = metrics_data.add_resource_metrics();
    auto* scope_metrics = resource_metrics->add_scope_metrics();
    auto* metric = scope_metrics->add_metrics();
    metric->set_name("invalid_metric");
    
    // Convert metrics
    auto result = bridge_->ConvertMetrics(metrics_data);
    ASSERT_TRUE(result.ok());  // Overall conversion should succeed
    
    // Verify that the metric was dropped
    EXPECT_EQ(bridge_->ProcessedMetrics(), 0);
    EXPECT_EQ(bridge_->DroppedMetrics(), 1);
}

// Test attribute conversion edge cases
TEST_F(OTELBridgeTest, AttributeConversion) {
    opentelemetry::proto::metrics::v1::MetricsData metrics_data;
    auto* resource_metrics = metrics_data.add_resource_metrics();
    auto* resource = resource_metrics->mutable_resource();
    auto* scope_metrics = resource_metrics->add_scope_metrics();
    auto* metric = scope_metrics->add_metrics();
    metric->set_name("test_metric");
    auto* gauge = metric->mutable_gauge();
    auto* datapoint = gauge->add_data_points();
    datapoint->set_time_unix_nano(1000000000);
    datapoint->set_as_double(42.0);
    
    // Test empty attributes
    auto result = bridge_->ConvertMetrics(metrics_data);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(bridge_->ProcessedMetrics(), 1);
    
    // Test all attribute value types
    auto* attrs = resource->mutable_attributes();
    
    // String value
    auto* attr = attrs->add_values();
    attr->set_key("string_attr");
    attr->mutable_value()->set_string_value("test");
    
    // Bool value
    attr = attrs->add_values();
    attr->set_key("bool_attr");
    attr->mutable_value()->set_bool_value(true);
    
    // Int value
    attr = attrs->add_values();
    attr->set_key("int_attr");
    attr->mutable_value()->set_int_value(123);
    
    // Double value
    attr = attrs->add_values();
    attr->set_key("double_attr");
    attr->mutable_value()->set_double_value(3.14);
    
    // Array value (should be skipped)
    attr = attrs->add_values();
    attr->set_key("array_attr");
    attr->mutable_value()->mutable_array_value();
    
    // Empty key (should be skipped)
    attr = attrs->add_values();
    attr->set_key("");
    attr->mutable_value()->set_string_value("value");
    
    result = bridge_->ConvertMetrics(metrics_data);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(bridge_->ProcessedMetrics(), 2);
    
    // Verify labels in storage
    core::Labels query_labels = {{"__name__", "test_metric"}};
    auto series = storage_->Query(query_labels, 0, 2000);
    ASSERT_TRUE(series.ok());
    ASSERT_EQ(series.value().size(), 1);
    
    const auto& labels = storage_->GetSeries(series.value()[0])->Labels();
    EXPECT_EQ(labels.size(), 5);  // name + 4 valid attributes
}

// Test timestamp conversion edge cases
TEST_F(OTELBridgeTest, TimestampConversion) {
    opentelemetry::proto::metrics::v1::MetricsData metrics_data;
    auto* resource_metrics = metrics_data.add_resource_metrics();
    auto* scope_metrics = resource_metrics->add_scope_metrics();
    auto* metric = scope_metrics->add_metrics();
    metric->set_name("test_metric");
    auto* gauge = metric->mutable_gauge();
    
    // Test minimum timestamp
    auto* datapoint = gauge->add_data_points();
    datapoint->set_time_unix_nano(0);
    datapoint->set_as_double(1.0);
    
    // Test maximum timestamp
    datapoint = gauge->add_data_points();
    datapoint->set_time_unix_nano(std::numeric_limits<uint64_t>::max());
    datapoint->set_as_double(2.0);
    
    // Test current timestamp
    datapoint = gauge->add_data_points();
    datapoint->set_time_unix_nano(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count());
    datapoint->set_as_double(3.0);
    
    auto result = bridge_->ConvertMetrics(metrics_data);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(bridge_->ProcessedMetrics(), 1);
    
    // Verify timestamps in storage
    core::Labels query_labels = {{"__name__", "test_metric"}};
    auto series = storage_->Query(query_labels, 0, std::numeric_limits<core::Timestamp>::max());
    ASSERT_TRUE(series.ok());
    ASSERT_EQ(series.value().size(), 1);
    
    auto samples = storage_->Read(series.value()[0], 0, std::numeric_limits<core::Timestamp>::max());
    ASSERT_TRUE(samples.ok());
    ASSERT_EQ(samples.value().size(), 3);
    
    EXPECT_EQ(samples.value()[0].timestamp, 0);
    EXPECT_GT(samples.value()[2].timestamp, samples.value()[1].timestamp);
}

// Test value conversion edge cases
TEST_F(OTELBridgeTest, ValueConversion) {
    opentelemetry::proto::metrics::v1::MetricsData metrics_data;
    auto* resource_metrics = metrics_data.add_resource_metrics();
    auto* scope_metrics = resource_metrics->add_scope_metrics();
    auto* metric = scope_metrics->add_metrics();
    metric->set_name("test_metric");
    
    // Test gauge with special values
    {
        auto* gauge = metric->mutable_gauge();
        
        // Infinity
        auto* datapoint = gauge->add_data_points();
        datapoint->set_time_unix_nano(1000000000);
        datapoint->set_as_double(std::numeric_limits<double>::infinity());
        
        // -Infinity
        datapoint = gauge->add_data_points();
        datapoint->set_time_unix_nano(2000000000);
        datapoint->set_as_double(-std::numeric_limits<double>::infinity());
        
        // NaN
        datapoint = gauge->add_data_points();
        datapoint->set_time_unix_nano(3000000000);
        datapoint->set_as_double(std::numeric_limits<double>::quiet_NaN());
        
        // Min/Max values
        datapoint = gauge->add_data_points();
        datapoint->set_time_unix_nano(4000000000);
        datapoint->set_as_double(std::numeric_limits<double>::min());
        
        datapoint = gauge->add_data_points();
        datapoint->set_time_unix_nano(5000000000);
        datapoint->set_as_double(std::numeric_limits<double>::max());
    }
    
    auto result = bridge_->ConvertMetrics(metrics_data);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(bridge_->ProcessedMetrics(), 1);
}

// Test histogram conversion edge cases
TEST_F(OTELBridgeTest, HistogramConversion) {
    opentelemetry::proto::metrics::v1::MetricsData metrics_data;
    auto* resource_metrics = metrics_data.add_resource_metrics();
    auto* scope_metrics = resource_metrics->add_scope_metrics();
    auto* metric = scope_metrics->add_metrics();
    metric->set_name("test_histogram");
    auto* histogram = metric->mutable_histogram();
    
    // Test empty histogram
    auto* datapoint = histogram->add_data_points();
    datapoint->set_time_unix_nano(1000000000);
    datapoint->set_count(0);
    datapoint->set_sum(0.0);
    
    // Test single bucket
    datapoint = histogram->add_data_points();
    datapoint->set_time_unix_nano(2000000000);
    datapoint->set_count(1);
    datapoint->set_sum(42.0);
    datapoint->add_bucket_counts(1);
    datapoint->add_explicit_bounds(std::numeric_limits<double>::infinity());
    
    // Test many buckets
    datapoint = histogram->add_data_points();
    datapoint->set_time_unix_nano(3000000000);
    datapoint->set_count(1000);
    datapoint->set_sum(50000.0);
    for (int i = 0; i < 100; i++) {
        datapoint->add_bucket_counts(10);
        if (i < 99) {
            datapoint->add_explicit_bounds(std::pow(2.0, i));
        }
    }
    
    auto result = bridge_->ConvertMetrics(metrics_data);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(bridge_->ProcessedMetrics(), 1);
    
    // Verify histogram data in storage
    core::Labels query_labels = {{"__name__", "test_histogram"}};
    auto series = storage_->Query(query_labels, 0, 4000);
    ASSERT_TRUE(series.ok());
    ASSERT_EQ(series.value().size(), 1);
    
    auto samples = storage_->Read(series.value()[0], 0, 4000);
    ASSERT_TRUE(samples.ok());
    
    // First histogram: empty
    EXPECT_DOUBLE_EQ(samples.value()[0].value, 0);  // count
    EXPECT_DOUBLE_EQ(samples.value()[1].value, 0);  // sum
    
    // Second histogram: single bucket
    size_t offset = 3;  // count + sum + 1 bucket
    EXPECT_DOUBLE_EQ(samples.value()[offset].value, 1);     // count
    EXPECT_DOUBLE_EQ(samples.value()[offset+1].value, 42);  // sum
    EXPECT_DOUBLE_EQ(samples.value()[offset+2].value, 1);   // bucket
    
    // Third histogram: many buckets
    offset += 3;  // Previous histogram size
    EXPECT_DOUBLE_EQ(samples.value()[offset].value, 1000);     // count
    EXPECT_DOUBLE_EQ(samples.value()[offset+1].value, 50000);  // sum
    for (int i = 0; i < 100; i++) {
        EXPECT_DOUBLE_EQ(samples.value()[offset+2+i].value, 10);  // bucket
    }
}

// Test concurrent metric ingestion
TEST_F(OTELBridgeTest, ConcurrentIngestion) {
    const size_t num_threads = 4;
    const size_t metrics_per_thread = 100;
    
    std::vector<std::future<void>> futures;
    std::atomic<size_t> success_count{0};
    
    for (size_t t = 0; t < num_threads; t++) {
        futures.push_back(std::async(std::launch::async,
            [this, t, metrics_per_thread, &success_count]() {
                for (size_t i = 0; i < metrics_per_thread; i++) {
                    opentelemetry::proto::metrics::v1::MetricsData metrics_data;
                    auto* resource_metrics = metrics_data.add_resource_metrics();
                    auto* scope_metrics = resource_metrics->add_scope_metrics();
                    auto* metric = scope_metrics->add_metrics();
                    
                    metric->set_name("concurrent_metric_" + 
                                   std::to_string(t) + "_" +
                                   std::to_string(i));
                    
                    auto* gauge = metric->mutable_gauge();
                    auto* datapoint = gauge->add_data_points();
                    datapoint->set_time_unix_nano(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::system_clock::now().time_since_epoch()
                        ).count());
                    datapoint->set_as_double(static_cast<double>(i));
                    
                    auto result = bridge_->ConvertMetrics(metrics_data);
                    if (result.ok()) {
                        success_count++;
                    }
                }
            }));
    }
    
    for (auto& future : futures) {
        future.get();
    }
    
    EXPECT_EQ(success_count, num_threads * metrics_per_thread);
    EXPECT_EQ(bridge_->ProcessedMetrics(), num_threads * metrics_per_thread);
    EXPECT_EQ(bridge_->DroppedMetrics(), 0);
}

} // namespace
} // namespace otel
} // namespace tsdb 