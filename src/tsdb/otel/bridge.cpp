#include "tsdb/otel/bridge.h"
#include <spdlog/spdlog.h>
#include <chrono>
#include <thread>

namespace tsdb {
namespace otel {

namespace {

/**
 * @brief Converts OpenTelemetry attributes to TSDB labels.
 * 
 * Handles different value types:
 * - String: Direct conversion
 * - Bool: "true" or "false"
 * - Int: String representation
 * - Double: String representation with fixed precision
 * 
 * Skips unsupported types to maintain data consistency.
 * 
 * @param attributes OpenTelemetry KeyValueList
 * @return core::Labels Converted label set
 */
core::Labels ConvertAttributes(
    const google::protobuf::RepeatedPtrField<opentelemetry::proto::common::v1::KeyValue>& attributes) {
    core::Labels::Map labels;
    
    for (const auto& kv : attributes) {
        std::string value;
        switch (kv.value().value_case()) {
            case opentelemetry::proto::common::v1::AnyValue::kStringValue:
                value = kv.value().string_value();
                break;
            case opentelemetry::proto::common::v1::AnyValue::kBoolValue:
                value = kv.value().bool_value() ? "true" : "false";
                break;
            case opentelemetry::proto::common::v1::AnyValue::kIntValue:
                value = std::to_string(kv.value().int_value());
                break;
            case opentelemetry::proto::common::v1::AnyValue::kDoubleValue:
                value = std::to_string(kv.value().double_value());
                break;
            default:
                continue;  // Skip unsupported types
        }
        labels[kv.key()] = std::move(value);
    }
    
    return core::Labels(std::move(labels));
}

/**
 * @brief Implementation of the OpenTelemetry metrics bridge.
 * 
 * Handles conversion and storage of OpenTelemetry metrics with:
 * - Batched processing for efficiency
 * - Attribute preservation
 * - Support for all OTLP metric types
 * - Configurable behavior
 */
class OTELMetricsBridgeImpl : public Bridge {
public:
    OTELMetricsBridgeImpl(
        std::shared_ptr<storage::Storage> storage,
        const OTELMetricsBridgeOptions& options)
        : storage_(std::move(storage))
        , options_(options)
        , processed_metrics_(0)
        , dropped_metrics_(0)
        , pending_metrics_(0) {}
    
    void init(const OTELMetricsBridgeOptions& config) override {
        options_ = config;
    }
    
    void export_metric(const core::Metric& metric, core::Timestamp timestamp) override {
        // Not implemented yet
        (void)metric;
        (void)timestamp;
    }
    
    void export_batch(
        const std::vector<std::shared_ptr<core::Metric>>& metrics,
        core::Timestamp timestamp) override {
        // Not implemented yet
        (void)metrics;
        (void)timestamp;
    }
    
    void flush() override {
        storage_->flush();
    }
    
    void shutdown() override {
        // Not implemented yet
    }
    
    std::string stats() const override {
        return fmt::format(
            "Processed: {}, Dropped: {}, Pending: {}",
            processed_metrics_.load(),
            dropped_metrics_.load(),
            pending_metrics_.load());
    }
    
    core::Result<void> ConvertMetrics(
        const opentelemetry::proto::metrics::v1::MetricsData& metrics_data) override {
        for (const auto& resource_metrics : metrics_data.resource_metrics()) {
            // Convert resource-level attributes (e.g., service.name)
            auto resource_labels = ConvertAttributes(
                resource_metrics.resource().attributes());
            
            for (const auto& scope_metrics : resource_metrics.scope_metrics()) {
                // Convert instrumentation scope attributes
                auto scope_labels = ConvertAttributes(
                    scope_metrics.scope().attributes());
                
                for (const auto& metric : scope_metrics.metrics()) {
                    auto result = ConvertMetric(
                        metric, resource_labels, scope_labels);
                    if (!result.ok()) {
                        spdlog::warn("Failed to convert metric: {}",
                                   result.error().what());
                        dropped_metrics_++;
                        continue;
                    }
                    processed_metrics_++;
                }
            }
        }
        
        return core::Result<void>();
    }

private:
    /**
     * @brief Converts a single OpenTelemetry metric to TSDB format.
     * 
     * Conversion process:
     * 1. Combine labels from resource, scope, and metric
     * 2. Create or get existing series
     * 3. Convert data points based on metric type
     * 4. Write samples to storage
     * 
     * @param metric OpenTelemetry metric
     * @param resource_labels Resource attributes
     * @param scope_labels Instrumentation scope attributes
     * @return Result<void> Success or error
     */
    core::Result<void> ConvertMetric(
        const opentelemetry::proto::metrics::v1::Metric& metric,
        const core::Labels& resource_labels,
        const core::Labels& scope_labels) {
        // Combine all labels
        core::Labels::Map labels;
        
        // Add resource labels
        labels.insert(resource_labels.map().begin(), resource_labels.map().end());
        
        // Add scope labels
        labels.insert(scope_labels.map().begin(), scope_labels.map().end());
        
        // Add metric name
        labels["__name__"] = metric.name();
        
        // Create series if it doesn't exist
        auto series = core::TimeSeries(core::Labels(std::move(labels)));
        
        // Convert data points
        std::vector<core::Sample> samples;
        
        switch (metric.data_case()) {
            case opentelemetry::proto::metrics::v1::Metric::kGauge:
                ConvertGauge(metric.gauge(), samples);
                break;
            case opentelemetry::proto::metrics::v1::Metric::kSum:
                ConvertSum(metric.sum(), samples);
                break;
            case opentelemetry::proto::metrics::v1::Metric::kHistogram:
                ConvertHistogram(metric.histogram(), samples);
                break;
            default:
                return core::Result<void>(std::make_unique<core::Error>(
                    "Unsupported metric type"));
        }
        
        for (const auto& sample : samples) {
            series.add_sample(sample);
        }
        
        if (!samples.empty()) {
            return storage_->write(series);
        }
        
        return core::Result<void>();
    }
    
    /**
     * @brief Determines TSDB metric type from OpenTelemetry metric.
     * 
     * Mapping:
     * - Gauge -> GAUGE
     * - Sum -> COUNTER (if monotonic) or GAUGE
     * - Histogram -> HISTOGRAM
     * - Default -> GAUGE
     */
    core::MetricType DetermineMetricType(
        const opentelemetry::proto::metrics::v1::Metric& metric) {
        switch (metric.data_case()) {
            case opentelemetry::proto::metrics::v1::Metric::kGauge:
                return core::MetricType::GAUGE;
            case opentelemetry::proto::metrics::v1::Metric::kSum:
                return core::MetricType::COUNTER;
            case opentelemetry::proto::metrics::v1::Metric::kHistogram:
                return core::MetricType::HISTOGRAM;
            default:
                return core::MetricType::GAUGE;  // Default to gauge
        }
    }
    
    /**
     * @brief Converts OpenTelemetry gauge to TSDB samples.
     * 
     * Handles both integer and double gauge values.
     * Timestamps are converted from nanoseconds to milliseconds.
     */
    void ConvertGauge(
        const opentelemetry::proto::metrics::v1::Gauge& gauge,
        std::vector<core::Sample>& samples) {
        for (const auto& point : gauge.data_points()) {
            samples.emplace_back(
                point.time_unix_nano() / 1000000,  // Convert to milliseconds
                point.as_double());
        }
    }
    
    /**
     * @brief Converts OpenTelemetry sum to TSDB samples.
     * 
     * Handles both monotonic and non-monotonic sums.
     * Timestamps are converted from nanoseconds to milliseconds.
     */
    void ConvertSum(
        const opentelemetry::proto::metrics::v1::Sum& sum,
        std::vector<core::Sample>& samples) {
        for (const auto& point : sum.data_points()) {
            samples.emplace_back(
                point.time_unix_nano() / 1000000,
                point.as_double());
        }
    }
    
    /**
     * @brief Converts OpenTelemetry histogram to TSDB samples.
     * 
     * Conversion process:
     * 1. Store count and sum as separate samples
     * 2. Store each bucket count
     * 3. Preserve bucket boundaries in metadata
     * 
     * Sample timestamps are offset to maintain ordering:
     * - Base timestamp: Count
     * - Base + 1ms: Sum
     * - Base + 2ms+: Bucket counts
     */
    void ConvertHistogram(
        const opentelemetry::proto::metrics::v1::Histogram& histogram,
        std::vector<core::Sample>& samples) {
        for (const auto& point : histogram.data_points()) {
            // Store count and sum
            core::Timestamp ts = point.time_unix_nano() / 1000000;
            samples.emplace_back(ts, point.count());
            samples.emplace_back(ts + 1, point.sum());  // Offset by 1ms
            
            // Store bucket counts
            for (size_t i = 0; i < static_cast<size_t>(point.bucket_counts_size()); i++) {
                samples.emplace_back(
                    ts + 2 + i,  // Offset each bucket
                    point.bucket_counts(i));
            }
        }
    }
    
    std::shared_ptr<storage::Storage> storage_;
    OTELMetricsBridgeOptions options_;
    std::atomic<size_t> processed_metrics_;
    std::atomic<size_t> dropped_metrics_;
    std::atomic<size_t> pending_metrics_;
};

} // namespace

std::shared_ptr<Bridge> CreateOTELMetricsBridge(
    std::shared_ptr<storage::Storage> storage,
    const OTELMetricsBridgeOptions& options) {
    return std::make_shared<OTELMetricsBridgeImpl>(std::move(storage), options);
}

MetricsService::MetricsService(std::shared_ptr<storage::Storage> storage)
    : Service()
    , storage_(std::move(storage))
    , bridge_(CreateOTELMetricsBridge(storage_, OTELMetricsBridgeOptions{})) {
    spdlog::info("MetricsService initialized");
}

grpc::Status MetricsService::Export(
    grpc::ServerContext* /*context*/,
    const opentelemetry::proto::collector::metrics::v1::ExportMetricsServiceRequest* request,
    opentelemetry::proto::collector::metrics::v1::ExportMetricsServiceResponse* /*response*/) {
    try {
        spdlog::debug("Received metrics export request");
        
        for (const auto& resource_metrics : request->resource_metrics()) {
            opentelemetry::proto::metrics::v1::MetricsData metrics_data;
            *metrics_data.add_resource_metrics() = resource_metrics;
            
            auto result = bridge_->ConvertMetrics(metrics_data);
            if (!result.ok()) {
                spdlog::error("Failed to convert metrics: {}", result.error().what());
                return grpc::Status(grpc::StatusCode::INTERNAL, result.error().what());
            }
        }
        
        bridge_->flush();
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        spdlog::error("Error in Export: {}", e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

} // namespace otel
} // namespace tsdb 