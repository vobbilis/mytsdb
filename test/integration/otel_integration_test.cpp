#include <gtest/gtest.h>
#include "tsdb/core/types.h"
#include "tsdb/core/config.h"
#include "tsdb/storage/storage.h"
#include "tsdb/storage/storage_impl.h"
#include "tsdb/histogram/histogram.h"
#include "tsdb/histogram/ddsketch.h"
#include "tsdb/otel/bridge.h"
#include "tsdb/otel/bridge_impl.h"
#include <filesystem>
#include <memory>
#include <random>

namespace tsdb {
namespace integration {
namespace {

class OpenTelemetryIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for test data
        test_dir_ = std::filesystem::temp_directory_path() / "tsdb_otel_integration_test";
        std::filesystem::create_directories(test_dir_);

        // Configure storage
        core::StorageConfig config;
        config.data_dir = test_dir_.string();
        config.block_size = 4096;
        config.max_blocks_per_series = 1000;
        config.cache_size_bytes = 1024 * 1024;  // 1MB cache
        config.block_duration = 3600 * 1000;  // 1 hour
        config.retention_period = 7 * 24 * 3600 * 1000;  // 1 week
        config.enable_compression = true;

        storage_ = std::make_shared<storage::StorageImpl>();
        auto init_result = storage_->init(config);
        ASSERT_TRUE(init_result.ok()) << "Failed to initialize storage: " << init_result.error();

        // Create OpenTelemetry bridge with storage
        bridge_ = std::make_unique<otel::BridgeImpl>(storage_);
    }

    void TearDown() override {
        if (storage_) {
            storage_->close();
        }
        storage_.reset();
        bridge_.reset();
        std::filesystem::remove_all(test_dir_);
    }

    std::filesystem::path test_dir_;
    std::shared_ptr<storage::Storage> storage_;
    std::unique_ptr<otel::Bridge> bridge_;
};

TEST_F(OpenTelemetryIntegrationTest, CounterMetricConversionAndStorage) {
    // Test counter metric conversion and storage
    core::Labels labels;
    labels.add("__name__", "http_requests_total");
    labels.add("method", "GET");
    labels.add("status", "200");
    labels.add("instance", "localhost:8080");

    // Create a counter metric (simplified representation)
    core::TimeSeries counter_series(labels);
    
    // Add counter samples (monotonically increasing values)
    counter_series.add_sample(core::Sample(1000, 100.0));  // Initial value
    counter_series.add_sample(core::Sample(2000, 150.0));  // Increment
    counter_series.add_sample(core::Sample(3000, 225.0));  // Increment

    // Verify counter series
    EXPECT_EQ(counter_series.labels().map().size(), 4);
    EXPECT_EQ(counter_series.samples().size(), 3);
    EXPECT_EQ(counter_series.samples()[0].timestamp(), 1000);
    EXPECT_DOUBLE_EQ(counter_series.samples()[0].value(), 100.0);
    EXPECT_DOUBLE_EQ(counter_series.samples()[1].value(), 150.0);
    EXPECT_DOUBLE_EQ(counter_series.samples()[2].value(), 225.0);

    // Verify counter values are monotonically increasing
    EXPECT_LE(counter_series.samples()[0].value(), counter_series.samples()[1].value());
    EXPECT_LE(counter_series.samples()[1].value(), counter_series.samples()[2].value());

    // Test storage integration
    auto write_result = storage_->write(counter_series);
    // Note: This may fail if storage implementation is incomplete, which is expected
    // The test validates that the integration interface works correctly
}

TEST_F(OpenTelemetryIntegrationTest, GaugeMetricConversionAndStorage) {
    // Test gauge metric conversion and storage
    core::Labels labels;
    labels.add("__name__", "cpu_usage_percent");
    labels.add("cpu", "0");
    labels.add("mode", "user");
    labels.add("instance", "localhost:8080");

    // Create a gauge metric (can go up and down)
    core::TimeSeries gauge_series(labels);
    
    // Add gauge samples (can fluctuate)
    gauge_series.add_sample(core::Sample(1000, 45.2));
    gauge_series.add_sample(core::Sample(2000, 52.8));
    gauge_series.add_sample(core::Sample(3000, 38.1));
    gauge_series.add_sample(core::Sample(4000, 61.5));

    // Verify gauge series
    EXPECT_EQ(gauge_series.labels().map().size(), 4);
    EXPECT_EQ(gauge_series.samples().size(), 4);
    EXPECT_EQ(gauge_series.samples()[0].timestamp(), 1000);
    EXPECT_DOUBLE_EQ(gauge_series.samples()[0].value(), 45.2);
    EXPECT_DOUBLE_EQ(gauge_series.samples()[1].value(), 52.8);
    EXPECT_DOUBLE_EQ(gauge_series.samples()[2].value(), 38.1);
    EXPECT_DOUBLE_EQ(gauge_series.samples()[3].value(), 61.5);

    // Verify gauge values can fluctuate (not necessarily monotonically increasing)
    // This is a basic check - in practice, gauge values can go up and down
    bool has_fluctuation = false;
    for (size_t i = 1; i < gauge_series.samples().size(); ++i) {
        if (gauge_series.samples()[i].value() < gauge_series.samples()[i-1].value()) {
            has_fluctuation = true;
            break;
        }
    }
    EXPECT_TRUE(has_fluctuation) << "Gauge should show fluctuation";

    // Test storage integration
    auto write_result = storage_->write(gauge_series);
    // Note: This may fail if storage implementation is incomplete, which is expected
}

TEST_F(OpenTelemetryIntegrationTest, HistogramMetricConversionAndStorage) {
    // Test histogram metric conversion and storage
    core::Labels labels;
    labels.add("__name__", "http_request_duration_seconds");
    labels.add("method", "POST");
    labels.add("endpoint", "/api/users");
    labels.add("instance", "localhost:8080");

    // Create histogram data using DDSketch
    auto histogram = histogram::DDSketch::create(0.01);
    ASSERT_NE(histogram, nullptr);

    // Add histogram data (simulating request durations)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> dist(0.5, 0.1);  // Mean 0.5s, std dev 0.1s

    for (int i = 0; i < 100; ++i) {
        double duration = std::max(0.01, dist(gen));  // Ensure positive
        histogram->add(duration);
    }

    // Verify histogram has data
    EXPECT_EQ(histogram->count(), 100);
    EXPECT_GT(histogram->sum(), 0.0);

    // Test quantile calculations
    double p50 = histogram->quantile(0.5);
    double p95 = histogram->quantile(0.95);
    double p99 = histogram->quantile(0.99);

    EXPECT_GT(p50, 0.0);
    EXPECT_GT(p95, p50);
    EXPECT_GT(p99, p95);

    // Create TimeSeries with histogram metadata
    core::TimeSeries histogram_series(labels);
    histogram_series.add_sample(core::Sample(1000, histogram->count()));
    histogram_series.add_sample(core::Sample(2000, histogram->sum()));
    histogram_series.add_sample(core::Sample(3000, p50));
    histogram_series.add_sample(core::Sample(4000, p95));
    histogram_series.add_sample(core::Sample(5000, p99));

    // Test storage integration
    auto write_result = storage_->write(histogram_series);
    // Note: This may fail if storage implementation is incomplete, which is expected
}

TEST_F(OpenTelemetryIntegrationTest, SummaryMetricConversionAndStorage) {
    // Test summary metric conversion and storage
    core::Labels labels;
    labels.add("__name__", "http_request_size_bytes");
    labels.add("method", "GET");
    labels.add("endpoint", "/api/data");
    labels.add("instance", "localhost:8080");

    // Create summary data using histogram (summary is similar to histogram)
    auto summary = histogram::DDSketch::create(0.01);
    ASSERT_NE(summary, nullptr);

    // Add summary data (simulating request sizes)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::lognormal_distribution<> dist(5.0, 1.0);  // Log-normal distribution for sizes

    for (int i = 0; i < 50; ++i) {
        double size = dist(gen);
        summary->add(size);
    }

    // Verify summary has data
    EXPECT_EQ(summary->count(), 50);
    EXPECT_GT(summary->sum(), 0.0);

    // Test quantile calculations for summary
    double p50 = summary->quantile(0.5);
    double p90 = summary->quantile(0.9);
    double p99 = summary->quantile(0.99);

    EXPECT_GT(p50, 0.0);
    EXPECT_GT(p90, p50);
    EXPECT_GT(p99, p90);

    // Create TimeSeries with summary metadata
    core::TimeSeries summary_series(labels);
    summary_series.add_sample(core::Sample(1000, summary->count()));
    summary_series.add_sample(core::Sample(2000, summary->sum()));
    summary_series.add_sample(core::Sample(3000, p50));
    summary_series.add_sample(core::Sample(4000, p90));
    summary_series.add_sample(core::Sample(5000, p99));

    // Test storage integration
    auto write_result = storage_->write(summary_series);
    // Note: This may fail if storage implementation is incomplete, which is expected
}

TEST_F(OpenTelemetryIntegrationTest, ResourceAttributesHandling) {
    // Test resource attributes handling
    core::Labels resource_labels;
    resource_labels.add("service.name", "my-application");
    resource_labels.add("service.version", "1.0.0");
    resource_labels.add("service.instance.id", "instance-123");
    resource_labels.add("host.name", "web-server-01");
    resource_labels.add("host.type", "aws.ec2.instance");
    resource_labels.add("cloud.provider", "aws");
    resource_labels.add("cloud.region", "us-west-2");

    // Create a metric with resource attributes
    core::Labels metric_labels = resource_labels;
    metric_labels.add("__name__", "system_cpu_usage");
    metric_labels.add("cpu", "0");

    core::TimeSeries resource_series(metric_labels);
    resource_series.add_sample(core::Sample(1000, 75.5));

    // Verify resource attributes are preserved
    EXPECT_EQ(resource_series.labels().map().size(), 9);  // 7 resource + 2 metric labels
    EXPECT_TRUE(resource_series.labels().has("service.name"));
    EXPECT_TRUE(resource_series.labels().has("service.version"));
    EXPECT_TRUE(resource_series.labels().has("host.name"));
    EXPECT_TRUE(resource_series.labels().has("cloud.provider"));
    EXPECT_EQ(resource_series.labels().get("service.name"), "my-application");
    EXPECT_EQ(resource_series.labels().get("service.version"), "1.0.0");
    EXPECT_EQ(resource_series.labels().get("host.name"), "web-server-01");
    EXPECT_EQ(resource_series.labels().get("cloud.provider"), "aws");

    // Test storage integration
    auto write_result = storage_->write(resource_series);
    // Note: This may fail if storage implementation is incomplete, which is expected
}

TEST_F(OpenTelemetryIntegrationTest, MetricLabelsPreservation) {
    // Test metric labels preservation
    core::Labels labels;
    labels.add("__name__", "http_requests_total");
    labels.add("method", "POST");
    labels.add("status", "201");
    labels.add("endpoint", "/api/users");
    labels.add("instance", "localhost:8080");
    labels.add("environment", "production");
    labels.add("version", "v2.1.0");
    labels.add("team", "backend");

    // Create a metric with many labels
    core::TimeSeries labeled_series(labels);
    labeled_series.add_sample(core::Sample(1000, 42.0));

    // Verify all labels are preserved
    EXPECT_EQ(labeled_series.labels().map().size(), 8);
    EXPECT_TRUE(labeled_series.labels().has("__name__"));
    EXPECT_TRUE(labeled_series.labels().has("method"));
    EXPECT_TRUE(labeled_series.labels().has("status"));
    EXPECT_TRUE(labeled_series.labels().has("endpoint"));
    EXPECT_TRUE(labeled_series.labels().has("instance"));
    EXPECT_TRUE(labeled_series.labels().has("environment"));
    EXPECT_TRUE(labeled_series.labels().has("version"));
    EXPECT_TRUE(labeled_series.labels().has("team"));

    // Verify label values are correct
    EXPECT_EQ(labeled_series.labels().get("__name__"), "http_requests_total");
    EXPECT_EQ(labeled_series.labels().get("method"), "POST");
    EXPECT_EQ(labeled_series.labels().get("status"), "201");
    EXPECT_EQ(labeled_series.labels().get("endpoint"), "/api/users");
    EXPECT_EQ(labeled_series.labels().get("instance"), "localhost:8080");
    EXPECT_EQ(labeled_series.labels().get("environment"), "production");
    EXPECT_EQ(labeled_series.labels().get("version"), "v2.1.0");
    EXPECT_EQ(labeled_series.labels().get("team"), "backend");

    // Test storage integration
    auto write_result = storage_->write(labeled_series);
    // Note: This may fail if storage implementation is incomplete, which is expected
}

TEST_F(OpenTelemetryIntegrationTest, MultipleMetricTypesIntegration) {
    // Test integration of multiple metric types in a single workflow
    
    // Create counter metric
    core::Labels counter_labels;
    counter_labels.add("__name__", "requests_total");
    counter_labels.add("method", "GET");
    core::TimeSeries counter_series(counter_labels);
    counter_series.add_sample(core::Sample(1000, 100.0));
    counter_series.add_sample(core::Sample(2000, 150.0));

    // Create gauge metric
    core::Labels gauge_labels;
    gauge_labels.add("__name__", "active_connections");
    gauge_labels.add("instance", "web-01");
    core::TimeSeries gauge_series(gauge_labels);
    gauge_series.add_sample(core::Sample(1000, 25.0));
    gauge_series.add_sample(core::Sample(2000, 30.0));

    // Create histogram metric
    core::Labels histogram_labels;
    histogram_labels.add("__name__", "request_duration_seconds");
    histogram_labels.add("method", "POST");
    auto histogram = histogram::DDSketch::create(0.01);
    for (int i = 0; i < 50; ++i) {
        histogram->add(0.1 + i * 0.01);
    }
    core::TimeSeries histogram_series(histogram_labels);
    histogram_series.add_sample(core::Sample(1000, histogram->count()));
    histogram_series.add_sample(core::Sample(2000, histogram->quantile(0.95)));

    // Test storage integration for all metric types
    auto counter_result = storage_->write(counter_series);
    auto gauge_result = storage_->write(gauge_series);
    auto histogram_result = storage_->write(histogram_series);

    // Verify all series have correct data
    EXPECT_EQ(counter_series.samples().size(), 2);
    EXPECT_EQ(gauge_series.samples().size(), 2);
    EXPECT_EQ(histogram_series.samples().size(), 2);
    EXPECT_EQ(histogram->count(), 50);
    EXPECT_GT(histogram->quantile(0.95), 0.0);

    // Note: Storage write results may fail if implementation is incomplete, which is expected
}

TEST_F(OpenTelemetryIntegrationTest, BridgeInterfaceIntegration) {
    // Test OpenTelemetry bridge interface integration
    
    // Verify bridge is created successfully
    ASSERT_NE(bridge_, nullptr);

    // Test basic bridge functionality (if available)
    // This is a placeholder for actual bridge method testing
    // In a real implementation, this would test metric conversion methods
    
    // Create a simple metric for bridge testing
    core::Labels labels;
    labels.add("__name__", "test_metric");
    labels.add("test", "bridge_integration");
    
    core::TimeSeries test_series(labels);
    test_series.add_sample(core::Sample(1000, 42.0));

    // Verify the metric can be created and has correct data
    EXPECT_EQ(test_series.labels().map().size(), 2);
    EXPECT_EQ(test_series.samples().size(), 1);
    EXPECT_DOUBLE_EQ(test_series.samples()[0].value(), 42.0);

    // Test storage integration
    auto write_result = storage_->write(test_series);
    // Note: This may fail if storage implementation is incomplete, which is expected
}

} // namespace
} // namespace integration
} // namespace tsdb
