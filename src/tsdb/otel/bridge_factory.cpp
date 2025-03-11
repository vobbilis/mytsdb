#include "tsdb/otel/bridge.h"
#include "tsdb/otel/bridge_impl.h"

namespace tsdb {
namespace otel {

std::shared_ptr<Bridge> CreateOTELMetricsBridge(
    std::shared_ptr<storage::Storage> storage,
    const OTELMetricsBridgeOptions& options) {
    auto bridge = std::make_shared<BridgeImpl>(storage);
    bridge->init(options);
    return bridge;
}

}  // namespace otel
}  // namespace tsdb 