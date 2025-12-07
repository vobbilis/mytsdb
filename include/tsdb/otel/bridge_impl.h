#ifndef TSDB_OTEL_BRIDGE_IMPL_H_
#define TSDB_OTEL_BRIDGE_IMPL_H_

#include "tsdb/otel/bridge.h"
#include "tsdb/storage/storage.h"

namespace tsdb {
namespace otel {

class BridgeImpl : public Bridge {
public:
    explicit BridgeImpl(std::shared_ptr<storage::Storage> storage);
    
    void init(const OTELMetricsBridgeOptions& config) override;
    
    void export_metric(
        const core::Metric& metric,
        core::Timestamp timestamp) override;
    
    void export_batch(
        const std::vector<std::shared_ptr<core::Metric>>& metrics,
        core::Timestamp timestamp) override;
    
    void flush() override;
    
    void shutdown() override;
    
    std::string stats() const override;

    core::Result<void> ConvertMetrics(
        const google::protobuf::RepeatedPtrField<opentelemetry::proto::metrics::v1::ResourceMetrics>& resource_metrics) override;

private:
    core::Result<void> process_metrics(const std::string& data);
    
    std::shared_ptr<storage::Storage> storage_;
    OTELMetricsBridgeOptions config_;
};

}  // namespace otel
}  // namespace tsdb

#endif  // TSDB_OTEL_BRIDGE_IMPL_H_ 