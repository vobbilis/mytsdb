#ifndef TSDB_OTEL_BRIDGE_H_
#define TSDB_OTEL_BRIDGE_H_

#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <grpcpp/grpcpp.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/impl/codegen/service_type.h>
#include "opentelemetry/proto/collector/metrics/v1/metrics_service.grpc.pb.h"

#include "tsdb/core/types.h"
#include "tsdb/core/config.h"
#include "tsdb/core/metric.h"
#include "tsdb/storage/storage.h"

namespace tsdb {
namespace otel {

class Bridge;

/**
 * @brief Configuration for OpenTelemetry bridge
 */
struct OTELMetricsBridgeOptions {
    std::string endpoint;           // OpenTelemetry endpoint
    std::string service_name;       // Service name for metrics
    std::string service_version;    // Service version
    std::string service_namespace;  // Service namespace
    bool enable_tls;               // Whether to enable TLS
    std::string ca_cert;           // CA certificate for TLS
    std::string client_cert;       // Client certificate for TLS
    std::string client_key;        // Client key for TLS
    size_t max_batch_size;         // Maximum batch size for sending metrics
    std::chrono::milliseconds batch_timeout;  // Timeout for sending batches
    
    static OTELMetricsBridgeOptions Default() {
        return {
            "localhost:4317",       // Default OpenTelemetry endpoint
            "tsdb",                 // Default service name
            "1.0.0",               // Default version
            "default",             // Default namespace
            false,                 // TLS disabled by default
            "",                    // No CA cert
            "",                    // No client cert
            "",                    // No client key
            1000,                  // Max 1000 metrics per batch
            std::chrono::milliseconds(1000)  // 1 second batch timeout
        };
    }
};

/**
 * @brief Interface for OpenTelemetry metric data
 */
class MetricData {
public:
    virtual ~MetricData() = default;
    
    /**
     * @brief Get the metric name
     */
    virtual std::string name() const = 0;
    
    /**
     * @brief Get the metric description
     */
    virtual std::string description() const = 0;
    
    /**
     * @brief Get the metric unit
     */
    virtual std::string unit() const = 0;
    
    /**
     * @brief Get the metric type
     */
    virtual core::MetricType type() const = 0;
    
    /**
     * @brief Get the metric labels
     */
    virtual core::Labels labels() const = 0;
    
    /**
     * @brief Get the timestamp
     */
    virtual core::Timestamp timestamp() const = 0;
    
    /**
     * @brief Get the metric value
     */
    virtual core::Value value() const = 0;
    
    /**
     * @brief Get histogram data if this is a histogram metric
     */
    virtual std::shared_ptr<core::Histogram> histogram() const = 0;
};

/**
 * @brief Interface for OpenTelemetry metric exporter
 */
class MetricExporter {
public:
    virtual ~MetricExporter() = default;
    
    /**
     * @brief Initialize the exporter with the given configuration
     */
    virtual void init(const OTELMetricsBridgeOptions& config) = 0;
    
    /**
     * @brief Export a batch of metrics
     */
    virtual void export_batch(const std::vector<std::shared_ptr<MetricData>>& metrics) = 0;
    
    /**
     * @brief Flush any pending metrics
     */
    virtual void flush() = 0;
    
    /**
     * @brief Shutdown the exporter
     */
    virtual void shutdown() = 0;
};

/**
 * @brief Interface for OpenTelemetry metric converter
 */
class MetricConverter {
public:
    virtual ~MetricConverter() = default;
    
    /**
     * @brief Convert a TSDB metric to OpenTelemetry format
     */
    virtual std::shared_ptr<MetricData> convert(
        const core::Metric& metric,
        core::Timestamp timestamp) = 0;
    
    /**
     * @brief Convert a batch of TSDB metrics to OpenTelemetry format
     */
    virtual std::vector<std::shared_ptr<MetricData>> convert_batch(
        const std::vector<std::shared_ptr<core::Metric>>& metrics,
        core::Timestamp timestamp) = 0;
};

/**
 * @brief Interface for OpenTelemetry bridge
 */
class Bridge {
public:
    virtual ~Bridge() = default;
    
    /**
     * @brief Initialize the bridge with the given configuration
     */
    virtual void init(const OTELMetricsBridgeOptions& config) = 0;
    
    /**
     * @brief Export a single metric
     */
    virtual void export_metric(
        const core::Metric& metric,
        core::Timestamp timestamp) = 0;
    
    /**
     * @brief Export a batch of metrics
     */
    virtual void export_batch(
        const std::vector<std::shared_ptr<core::Metric>>& metrics,
        core::Timestamp timestamp) = 0;
    
    /**
     * @brief Flush any pending metrics
     */
    virtual void flush() = 0;
    
    /**
     * @brief Shutdown the bridge
     */
    virtual void shutdown() = 0;
    
    /**
     * @brief Get bridge statistics
     */
    virtual std::string stats() const = 0;

    /**
     * @brief Convert OpenTelemetry metrics to TSDB format
     */
    virtual core::Result<void> ConvertMetrics(
        const opentelemetry::proto::metrics::v1::MetricsData& metrics_data) = 0;
};

/**
 * @brief Factory for creating bridge instances
 */
class BridgeFactory {
public:
    virtual ~BridgeFactory() = default;
    
    /**
     * @brief Create a new bridge instance
     */
    virtual std::shared_ptr<Bridge> create(const OTELMetricsBridgeOptions& config) = 0;
};

/**
 * @brief OpenTelemetry metrics service implementation
 */
class MetricsService : public opentelemetry::proto::collector::metrics::v1::MetricsService::Service {
public:
    explicit MetricsService(std::shared_ptr<storage::Storage> storage);
    ~MetricsService() = default;

    grpc::Status Export(
        grpc::ServerContext* context,
        const opentelemetry::proto::collector::metrics::v1::ExportMetricsServiceRequest* request,
        opentelemetry::proto::collector::metrics::v1::ExportMetricsServiceResponse* response) override;

private:
    std::shared_ptr<storage::Storage> storage_;
    std::shared_ptr<Bridge> bridge_;
};

/**
 * @brief Create a new OpenTelemetry metrics bridge
 */
std::shared_ptr<Bridge> CreateOTELMetricsBridge(
    std::shared_ptr<storage::Storage> storage,
    const OTELMetricsBridgeOptions& options);

} // namespace otel
} // namespace tsdb

#endif // TSDB_OTEL_BRIDGE_H_ 