#pragma once

#include <vector>
#include <string>

namespace tsdb {
namespace core {

enum class AggregationOp {
    SUM,
    MIN,
    MAX,
    COUNT,
    AVG,
    STDDEV,
    STDVAR,
    COUNT_VALUES,
    BOTTOMK,
    TOPK,
    QUANTILE
};

struct AggregationRequest {
    AggregationOp op;
    std::vector<std::string> grouping_keys;
    bool without; // true = without, false = by
    
    // For topk, bottomk, quantile
    double param = 0.0; 
};

} // namespace core
} // namespace tsdb
