#include "tsdb/otel/bridge.h"
#include <spdlog/spdlog.h>
#include "tsdb/storage/write_performance_instrumentation.h"
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
        
        // Performance instrumentation
        auto& perf = storage::WritePerformanceInstrumentation::instance();
        bool perf_enabled = perf.is_enabled();
        storage::WritePerformanceInstrumentation::WriteMetrics metrics;
        
        // Measure conversion time
        {
            storage::ScopedTimer timer(metrics.otel_conversion_us, perf_enabled);
            
            // std::cerr << "OTEL Bridge: ConvertMetrics called with " << metrics_data.resource_metrics_size() << " resource metrics" << std::endl;
            
            for (const auto& resource_metrics : metrics_data.resource_metrics()) {
                // Measure resource processing
                storage::ScopedTimer r_timer(metrics.otel_resource_processing_us, perf_enabled);
                
                // Convert resource-level attributes (e.g., service.name)
                core::Labels resource_labels;
                {
                    storage::ScopedTimer l_timer(metrics.otel_label_conversion_us, perf_enabled);
                    resource_labels = ConvertAttributes(resource_metrics.resource().attributes());
                }
                
                for (const auto& scope_metrics : resource_metrics.scope_metrics()) {
                    // Measure scope processing
                    storage::ScopedTimer s_timer(metrics.otel_scope_processing_us, perf_enabled);
                    
                    // Convert instrumentation scope attributes
                    core::Labels scope_labels;
                    {
                        storage::ScopedTimer l_timer(metrics.otel_label_conversion_us, perf_enabled);
                        scope_labels = ConvertAttributes(scope_metrics.scope().attributes());
                    }
                    
                    for (const auto& metric : scope_metrics.metrics()) {
                        // Measure metric processing
                        storage::ScopedTimer m_timer(metrics.otel_metric_processing_us, perf_enabled);
                        
                        size_t points = 0;
                        std::string type = "unknown";
                        if (metric.has_gauge()) { points = metric.gauge().data_points_size(); type = "gauge"; }
                        else if (metric.has_sum()) { points = metric.sum().data_points_size(); type = "sum"; }
                        else if (metric.has_histogram()) { points = metric.histogram().data_points_size(); type = "histogram"; }
                        
                        auto result = ConvertMetric(
                            metric, resource_labels, scope_labels, metrics, perf_enabled);
                        if (!result.ok()) {
                            // std::cerr << "ERROR: Failed to convert metric: " << result.error() << std::endl;
                            spdlog::warn("Failed to convert metric: {}",
                                       result.error());
                            dropped_metrics_++;
                            continue;
                        }
                        processed_metrics_++;
                    }
                }
            }
        }
        
        // Record the conversion metric
        // Note: This records a separate "write" event that only contains conversion time
        // The actual storage writes inside ConvertMetric will record their own metrics
        if (perf_enabled) {
            metrics.total_us = metrics.otel_conversion_us;
            perf.record_write(metrics);
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
        const core::Labels& scope_labels,
        storage::WritePerformanceInstrumentation::WriteMetrics& metrics,
        bool perf_enabled) {
        // Base labels: resource + scope + metric name
        core::Labels::Map base_labels;
        base_labels.insert(resource_labels.map().begin(), resource_labels.map().end());
        base_labels.insert(scope_labels.map().begin(), scope_labels.map().end());
        base_labels["__name__"] = metric.name();
        
        // Convert data points - each data point may have different attributes,
        // so we need to create separate series for each unique set of attributes
        switch (metric.data_case()) {
            case opentelemetry::proto::metrics::v1::Metric::kGauge:
                return ConvertGaugeWithAttributes(metric.gauge(), base_labels, metrics, perf_enabled);
            case opentelemetry::proto::metrics::v1::Metric::kSum:
                return ConvertSumWithAttributes(metric.sum(), base_labels, metrics, perf_enabled);
            case opentelemetry::proto::metrics::v1::Metric::kHistogram:
                return ConvertHistogramWithAttributes(metric.histogram(), base_labels, metrics, perf_enabled);
            default:
                return core::Result<void>(std::make_unique<core::Error>(
                    "Unsupported metric type"));
        }
    }
    
    // Convert gauge with data point attributes
    core::Result<void> ConvertGaugeWithAttributes(
        const opentelemetry::proto::metrics::v1::Gauge& gauge,
        const core::Labels::Map& base_labels,
        storage::WritePerformanceInstrumentation::WriteMetrics& metrics,
        bool perf_enabled) {
        
        storage::ScopedTimer p_timer(metrics.otel_point_conversion_us, perf_enabled);
        
        // Batching map: Labels -> TimeSeries
        std::map<core::Labels, core::TimeSeries> batch;
        
        for (const auto& point : gauge.data_points()) {
            // Combine base labels with data point attributes
            core::Labels::Map labels_map = base_labels;
            
            {
                storage::ScopedTimer l_timer(metrics.otel_label_conversion_us, perf_enabled);
                auto point_labels = ConvertAttributes(point.attributes());
                // Insert point labels (this will overwrite base_labels if keys conflict)
                labels_map.insert(point_labels.map().begin(), point_labels.map().end());
            }
            
            core::Labels labels(labels_map);
            
            // Find or create series in batch
            auto it = batch.find(labels);
            if (it == batch.end()) {
                it = batch.emplace(labels, core::TimeSeries(labels)).first;
            }
            
            it->second.add_sample(core::Sample(
                point.time_unix_nano() / 1000000,  // Convert to milliseconds
                point.as_double()));
        }
        
        // Write batched series
        for (const auto& kv : batch) {
            auto result = storage_->write(kv.second);
            if (!result.ok()) {
                return result;
            }
        }
        
        return core::Result<void>();
    }
    
    // Convert sum with data point attributes
    core::Result<void> ConvertSumWithAttributes(
        const opentelemetry::proto::metrics::v1::Sum& sum,
        const core::Labels::Map& base_labels,
        storage::WritePerformanceInstrumentation::WriteMetrics& metrics,
        bool perf_enabled) {
        
        storage::ScopedTimer p_timer(metrics.otel_point_conversion_us, perf_enabled);
        
        // Batching map: Labels -> TimeSeries
        std::map<core::Labels, core::TimeSeries> batch;
        
        for (const auto& point : sum.data_points()) {
            // Combine base labels with data point attributes
            core::Labels::Map labels_map = base_labels;
            
            {
                storage::ScopedTimer l_timer(metrics.otel_label_conversion_us, perf_enabled);
                auto point_labels = ConvertAttributes(point.attributes());
                labels_map.insert(point_labels.map().begin(), point_labels.map().end());
            }
            
            core::Labels labels(labels_map);
            
            // Find or create series in batch
            auto it = batch.find(labels);
            if (it == batch.end()) {
                it = batch.emplace(labels, core::TimeSeries(labels)).first;
            }
            
            it->second.add_sample(core::Sample(
                point.time_unix_nano() / 1000000,  // Convert to milliseconds
                point.as_double()));
        }
        
        // Write batched series
        for (const auto& kv : batch) {
            auto result = storage_->write(kv.second);
            if (!result.ok()) {
                return result;
            }
        }
        
        return core::Result<void>();
    }
    
    // Convert histogram with data point attributes
    core::Result<void> ConvertHistogramWithAttributes(
        const opentelemetry::proto::metrics::v1::Histogram& histogram,
        const core::Labels::Map& base_labels,
        storage::WritePerformanceInstrumentation::WriteMetrics& metrics,
        bool perf_enabled) {
        
        storage::ScopedTimer p_timer(metrics.otel_point_conversion_us, perf_enabled);
        
        // Batching map: Labels -> TimeSeries
        std::map<core::Labels, core::TimeSeries> batch;
        
        for (const auto& point : histogram.data_points()) {
            // Combine base labels with data point attributes
            core::Labels::Map labels_map = base_labels;
            
            {
                storage::ScopedTimer l_timer(metrics.otel_label_conversion_us, perf_enabled);
                auto point_labels = ConvertAttributes(point.attributes());
                labels_map.insert(point_labels.map().begin(), point_labels.map().end());
            }
            
            core::Labels labels(labels_map);
            
            // Find or create series in batch
            auto it = batch.find(labels);
            if (it == batch.end()) {
                it = batch.emplace(labels, core::TimeSeries(labels)).first;
            }
            
            // Store count and sum
            core::Timestamp ts = point.time_unix_nano() / 1000000;
            it->second.add_sample(core::Sample(ts, point.count()));
            it->second.add_sample(core::Sample(ts + 1, point.sum()));  // Offset by 1ms
            
            // Store bucket counts
            for (size_t i = 0; i < static_cast<size_t>(point.bucket_counts_size()); i++) {
                it->second.add_sample(core::Sample(ts + 2 + i, point.bucket_counts(i)));
            }
        }
        
        // Write batched series
        for (const auto& kv : batch) {
            auto result = storage_->write(kv.second);
            if (!result.ok()) {
                return result;
            }
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
    
    // Old ConvertGauge, ConvertSum, ConvertHistogram methods removed
    // They are now replaced by ConvertGaugeWithAttributes, ConvertSumWithAttributes, ConvertHistogramWithAttributes
    
    std::shared_ptr<storage::Storage> storage_;
    OTELMetricsBridgeOptions options_;
    std::atomic<size_t> processed_metrics_;
    std::atomic<size_t> dropped_metrics_;
    std::atomic<size_t> pending_metrics_;
};

} // namespace

// Factory function for creating OTEL metrics bridge
std::shared_ptr<Bridge> CreateOTELMetricsBridge(
    std::shared_ptr<storage::Storage> storage,
    const OTELMetricsBridgeOptions& options) {
    return std::make_shared<OTELMetricsBridgeImpl>(std::move(storage), options);
}

#ifdef HAVE_GRPC

MetricsService::MetricsService(std::shared_ptr<storage::Storage> storage)
    : Service()
    , storage_(std::move(storage))
    , bridge_(std::make_shared<OTELMetricsBridgeImpl>(storage_, OTELMetricsBridgeOptions{})) {
    spdlog::info("MetricsService initialized");
}

grpc::Status MetricsService::Export(
    grpc::ServerContext* /*context*/,
    const opentelemetry::proto::collector::metrics::v1::ExportMetricsServiceRequest* request,
    opentelemetry::proto::collector::metrics::v1::ExportMetricsServiceResponse* /*response*/) {
    
    // Performance instrumentation
    auto& perf = storage::WritePerformanceInstrumentation::instance();
    bool perf_enabled = perf.is_enabled();
    storage::WritePerformanceInstrumentation::WriteMetrics metrics;
    
    // Measure total gRPC handling time
    {
        storage::ScopedTimer timer(metrics.grpc_handling_us, perf_enabled);
        
        try {
            // std::cerr << "MetricsService::Export called with " << request->resource_metrics_size() << " resource metrics" << std::endl;
            
            for (const auto& resource_metrics : request->resource_metrics()) {
                opentelemetry::proto::metrics::v1::MetricsData metrics_data;
                *metrics_data.add_resource_metrics() = resource_metrics;
                
                // std::cerr << "Calling bridge_->ConvertMetrics..." << std::endl;
                auto result = bridge_->ConvertMetrics(metrics_data);
                if (!result.ok()) {
                    std::cerr << "ERROR: Failed to convert metrics: " << result.error() << std::endl;
                    spdlog::error("Failed to convert metrics: {}", result.error());
                    return grpc::Status(grpc::StatusCode::INTERNAL, result.error());
                }
            }
            
            bridge_->flush();
        } catch (const std::exception& e) {
            std::cerr << "ERROR in Export: " << e.what() << std::endl;
            spdlog::error("Error in Export: {}", e.what());
            return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
        }
    }
    
    // Record the gRPC handling metric
    // Note: This records a separate "write" event that only contains gRPC handling time
    if (perf_enabled) {
        metrics.total_us = metrics.grpc_handling_us;
        perf.record_write(metrics);
    }
    
    return grpc::Status::OK;
}

#endif // HAVE_GRPC

} // namespace otel
} // namespace tsdb 