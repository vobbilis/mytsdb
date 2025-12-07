#pragma once

#include <memory>
#include <string>
#include <arrow/flight/server.h>
#include <arrow/flight/types.h>
#include "tsdb/storage/storage.h"

namespace tsdb {
namespace arrow {

/**
 * @brief Arrow Flight server for high-performance metrics ingestion.
 * 
 * Provides a zero-copy, columnar data path that bypasses OTEL/Protobuf overhead.
 * Expected schema: {labels: map<utf8,utf8>, timestamp: int64, value: float64}
 */
class MetricsFlightServer : public ::arrow::flight::FlightServerBase {
public:
    explicit MetricsFlightServer(std::shared_ptr<storage::Storage> storage);
    ~MetricsFlightServer() override = default;

    /**
     * @brief Initialize and start the Flight server.
     * @param port Port to listen on (default: 8815)
     * @return Status
     */
    ::arrow::Status Init(int port = 8815);

    /**
     * @brief Handle incoming metric batches.
     * 
     * Clients send RecordBatches via DoPut. Each batch contains:
     * - labels: map<utf8,utf8> - metric labels
     * - timestamp: int64 - Unix milliseconds
     * - value: float64 - metric value
     */
    ::arrow::Status DoPut(
        const ::arrow::flight::ServerCallContext& context,
        std::unique_ptr<::arrow::flight::FlightMessageReader> reader,
        std::unique_ptr<::arrow::flight::FlightMetadataWriter> writer) override;

    /**
     * @brief Get server statistics.
     */
    struct Stats {
        uint64_t samples_ingested = 0;
        uint64_t batches_processed = 0;
        uint64_t errors = 0;
    };
    Stats GetStats() const;

private:
    std::shared_ptr<storage::Storage> storage_;
    std::atomic<uint64_t> samples_ingested_{0};
    std::atomic<uint64_t> batches_processed_{0};
    std::atomic<uint64_t> errors_{0};
};

/**
 * @brief Create and start a metrics Flight server.
 * @param storage Storage backend
 * @param port Port to listen on
 * @return Unique pointer to the server
 */
std::unique_ptr<MetricsFlightServer> CreateMetricsFlightServer(
    std::shared_ptr<storage::Storage> storage,
    int port = 8815);

}  // namespace arrow
}  // namespace tsdb
