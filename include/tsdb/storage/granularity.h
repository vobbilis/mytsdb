#pragma once

#include "tsdb/core/types.h"

namespace tsdb {
namespace storage {

/**
 * @brief Block granularity for time series data
 */
struct Granularity {
    core::Duration interval;    // Sample interval
    core::Duration retention;   // Data retention period
};

} // namespace storage
} // namespace tsdb 