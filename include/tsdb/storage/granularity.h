#pragma once

#include "tsdb/core/types.h"

namespace tsdb {
namespace storage {

/**
 * @brief Block granularity for time series data
 */
struct Granularity {
    int64_t interval;    // Sample interval in milliseconds
    int64_t retention;   // Data retention period in milliseconds
};

} // namespace storage
} // namespace tsdb 