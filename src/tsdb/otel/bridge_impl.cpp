#include "tsdb/otel/bridge_impl.h"
#include <spdlog/spdlog.h>

namespace tsdb {
namespace otel {

BridgeImpl::BridgeImpl(std::shared_ptr<storage::Storage> storage)
    : storage_(std::move(storage)) {}

void BridgeImpl::init(const OTELMetricsBridgeOptions& config) {
    config_ = config;
    spdlog::info("Initialized OpenTelemetry bridge with endpoint: {}", config.endpoint);
}

core::Result<void> BridgeImpl::process_metrics(const std::string& data) {
    try {
        // TODO: Parse protobuf message and convert to metrics
        // For now, just log the data size
        spdlog::debug("Processing {} bytes of metric data", data.size());
        return core::Result<void>();
    } catch (const std::exception& e) {
        return core::Result<void>(std::make_unique<core::Error>(e.what()));
    }
}

void BridgeImpl::export_metric(
    const core::Metric& metric,
    core::Timestamp timestamp) {
    try {
        // TODO: Convert metric to storage format and write
        spdlog::debug("Exporting metric {} at timestamp {}", metric.name(), timestamp);
    } catch (const std::exception& e) {
        spdlog::error("Failed to export metric: {}", e.what());
    }
}

void BridgeImpl::export_batch(
    const std::vector<std::shared_ptr<core::Metric>>& metrics,
    core::Timestamp timestamp) {
    try {
        for (const auto& metric : metrics) {
            export_metric(*metric, timestamp);
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to export batch: {}", e.what());
    }
}

void BridgeImpl::flush() {
    try {
        auto result = storage_->flush();
        if (!result.ok()) {
            spdlog::error("Failed to flush storage: {}", result.error().what());
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to flush: {}", e.what());
    }
}

void BridgeImpl::shutdown() {
    try {
        flush();
        spdlog::info("OpenTelemetry bridge shutdown");
    } catch (const std::exception& e) {
        spdlog::error("Failed to shutdown: {}", e.what());
    }
}

std::string BridgeImpl::stats() const {
    // TODO: Implement proper statistics
    return "OpenTelemetry bridge stats: TODO";
}

core::Result<void> BridgeImpl::ConvertMetrics(
    const opentelemetry::proto::metrics::v1::MetricsData& metrics_data) {
    try {
        for (const auto& resource_metrics : metrics_data.resource_metrics()) {
            // Convert resource-level attributes (e.g., service.name)
            core::Labels::Map resource_labels;
            for (const auto& kv : resource_metrics.resource().attributes()) {
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
                resource_labels[kv.key()] = std::move(value);
            }
            
            for (const auto& scope_metrics : resource_metrics.scope_metrics()) {
                // Convert instrumentation scope attributes
                core::Labels::Map scope_labels;
                for (const auto& kv : scope_metrics.scope().attributes()) {
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
                    scope_labels[kv.key()] = std::move(value);
                }
                
                for (const auto& metric : scope_metrics.metrics()) {
                    // Combine all labels
                    core::Labels::Map labels;
                    
                    // Add resource labels
                    labels.insert(resource_labels.begin(), resource_labels.end());
                    
                    // Add scope labels
                    labels.insert(scope_labels.begin(), scope_labels.end());
                    
                    // Add metric name
                    labels["__name__"] = metric.name();
                    
                    // Create series
                    auto series = core::TimeSeries(core::Labels(std::move(labels)));
                    
                    // Convert data points based on metric type
                    std::vector<core::Sample> samples;
                    
                    switch (metric.data_case()) {
                        case opentelemetry::proto::metrics::v1::Metric::kGauge: {
                            const auto& gauge = metric.gauge();
                            for (const auto& point : gauge.data_points()) {
                                samples.emplace_back(
                                    point.time_unix_nano() / 1000000,  // Convert to milliseconds
                                    point.as_double());
                            }
                            break;
                        }
                        case opentelemetry::proto::metrics::v1::Metric::kSum: {
                            const auto& sum = metric.sum();
                            for (const auto& point : sum.data_points()) {
                                samples.emplace_back(
                                    point.time_unix_nano() / 1000000,
                                    point.as_double());
                            }
                            break;
                        }
                        case opentelemetry::proto::metrics::v1::Metric::kHistogram: {
                            const auto& histogram = metric.histogram();
                            for (const auto& point : histogram.data_points()) {
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
                            break;
                        }
                        default:
                            spdlog::warn("Unsupported metric type for {}", metric.name());
                            continue;
                    }
                    
                    // Add samples to series and write to storage
                    for (const auto& sample : samples) {
                        series.add_sample(sample);
                    }
                    
                    if (!samples.empty()) {
                        auto result = storage_->write(series);
                        if (!result.ok()) {
                            return result;
                        }
                    }
                }
            }
        }
        
        return core::Result<void>();
    } catch (const std::exception& e) {
        return core::Result<void>(std::make_unique<core::Error>(e.what()));
    }
}

}  // namespace otel
}  // namespace tsdb 