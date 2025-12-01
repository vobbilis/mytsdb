#pragma once

#include "tsdb/core/types.h"
#include "tsdb/core/matcher.h"
#include <vector>
#include <string>

// Forward declarations for protobuf types
namespace prometheus {
class WriteRequest;
class ReadResponse;
class LabelMatcher;
class Label;
class Sample;
}

namespace tsdb {
namespace prometheus {
namespace remote {

/**
 * @brief Converter between Prometheus protobuf and internal types
 */
class Converter {
public:
    /**
     * @brief Convert Prometheus WriteRequest to internal TimeSeries
     * @param request Prometheus WriteRequest protobuf message
     * @return Vector of internal TimeSeries objects
     */
    static std::vector<core::TimeSeries> FromWriteRequest(
        const ::prometheus::WriteRequest& request);
    
    /**
     * @brief Convert internal query results to Prometheus ReadResponse
     * @param series Vector of internal TimeSeries objects
     * @return Prometheus ReadResponse protobuf message
     */
    static ::prometheus::ReadResponse ToReadResponse(
        const std::vector<core::TimeSeries>& series);
    
    /**
     * @brief Convert Prometheus LabelMatcher to internal LabelMatcher
     * @param matcher Prometheus LabelMatcher protobuf message
     * @return Internal LabelMatcher object
     */
    static core::LabelMatcher FromProtoMatcher(
        const ::prometheus::LabelMatcher& matcher);
    
    /**
     * @brief Convert Prometheus Sample to internal Sample
     * @param sample Prometheus Sample protobuf message
     * @return Internal Sample object
     */
    static core::Sample FromProtoSample(
        const ::prometheus::Sample& sample);
    
    /**
     * @brief Convert internal Sample to Prometheus Sample
     * @param sample Internal Sample object
     * @return Prometheus Sample protobuf message
     */
    static ::prometheus::Sample ToProtoSample(
        const core::Sample& sample);
};

} // namespace remote
} // namespace prometheus
} // namespace tsdb
