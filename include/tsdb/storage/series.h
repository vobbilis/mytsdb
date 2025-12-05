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
 * @brief Immutable metadata for a time series
 */
struct SeriesMetadata {
    core::SeriesID id;
    core::Labels labels;
    core::MetricType type;
    struct Granularity granularity;
};

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

    // core::Result<void> Write(const std::vector<core::Sample>& samples);
    core::Result<std::vector<core::Sample>> Read(
        core::Timestamp start, core::Timestamp end) const;

    // New methods for Phase 2 of write path refactoring
    bool append(const core::Sample& sample);
    std::shared_ptr<internal::BlockImpl> seal_block();
    std::vector<std::shared_ptr<internal::BlockInternal>> GetBlocks() const;
    void AddBlock(std::shared_ptr<internal::BlockInternal> block); // New method to add cold blocks
    bool ReplaceBlock(std::shared_ptr<internal::BlockInternal> old_block, std::shared_ptr<internal::BlockInternal> new_block);

    // Metadata accessors - No locking required as metadata is immutable
    const core::Labels& Labels() const;
    core::MetricType Type() const;
    const struct Granularity& GetGranularity() const;
    core::SeriesID GetID() const;

    // Storage state accessors - Locking required
    size_t NumSamples() const;
    core::Timestamp MinTimestamp() const;
    core::Timestamp MaxTimestamp() const;

private:
    const SeriesMetadata metadata_;
    std::vector<std::shared_ptr<internal::BlockInternal>> blocks_;
    std::shared_ptr<internal::BlockImpl> current_block_;
    mutable std::shared_mutex mutex_;
};

} // namespace storage
} // namespace tsdb