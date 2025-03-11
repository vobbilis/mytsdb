#include "tsdb/otel/bridge.h"
#include <spdlog/spdlog.h>

namespace tsdb {
namespace otel {

MetricsService::MetricsService(std::shared_ptr<storage::Storage> storage)
    : storage_(std::move(storage)) {}

grpc::Status MetricsService::Export(
    grpc::ServerContext* context [[maybe_unused]],
    const opentelemetry::proto::collector::metrics::v1::ExportMetricsServiceRequest* request,
    opentelemetry::proto::collector::metrics::v1::ExportMetricsServiceResponse* response) {
    try {
        spdlog::debug("Received metrics export request");
        
        if (!request->resource_metrics().empty()) {
            spdlog::debug("Processing {} resource metrics", request->resource_metrics_size());
            // TODO: Process the metrics from each resource
            // For now, we'll just acknowledge receipt
        }
        
        // Set response fields if needed
        // For now, we'll just return an empty response indicating success
        response->Clear();
        
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        spdlog::error("Error processing metrics export request: {}", e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

}  // namespace otel
}  // namespace tsdb 