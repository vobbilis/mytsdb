#pragma once

#include <memory>
#include <vector>
#include <shared_mutex>
#include "tsdb/core/types.h"
#include "tsdb/core/interfaces.h"
#include "tsdb/core/result.h"
#include "tsdb/storage/granularity.h"
#include "tsdb/storage/block.h"
#include "tsdb/storage/internal/block_impl.h"

namespace tsdb {
namespace storage {

/**
 * @brief Represents a time series with its metadata and data blocks
 */
class Series {
public:
    Series(
        core::SeriesID id,
        const core::Labels& labels,
        core::MetricType type,
        const struct Granularity& granularity);

    core::Result<void> Write(const std::vector<core::Sample>& samples);
    core::Result<std::vector<core::Sample>> Read(
        core::Timestamp start, core::Timestamp end) const;

    const core::Labels& Labels() const;
    core::MetricType Type() const;
    const struct Granularity& GetGranularity() const;
    size_t NumSamples() const;
    core::Timestamp MinTimestamp() const;
    core::Timestamp MaxTimestamp() const;
    core::SeriesID GetID() const;

private:
    core::SeriesID id_;
    core::Labels labels_;
    core::MetricType type_;
    struct Granularity granularity_;
    std::vector<std::shared_ptr<internal::BlockImpl>> blocks_;
    std::shared_ptr<internal::BlockImpl> current_block_;
    mutable std::shared_mutex mutex_;
};

} // namespace storage
} // namespace tsdb 