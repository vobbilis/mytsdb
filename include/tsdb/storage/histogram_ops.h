#ifndef TSDB_STORAGE_HISTOGRAM_OPS_H_
#define TSDB_STORAGE_HISTOGRAM_OPS_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "tsdb/core/result.h"
#include "tsdb/histogram/histogram.h"
#include "tsdb/storage/internal/block_types.h"

namespace tsdb {
namespace storage {

class SIMDHistogram {
public:
    virtual ~SIMDHistogram() = default;

    virtual core::Result<void> add(double value) = 0;
    virtual core::Result<void> merge(const histogram::DDSketch& other) = 0;
    virtual core::Result<double> quantile(double q) const = 0;
    virtual core::Result<void> addBatchAVX512(const double* values, size_t count) = 0;
    virtual core::Result<void> mergeBatchAVX512(const histogram::DDSketch* sketches, size_t count) = 0;
    virtual core::Result<internal::HistogramBlockFormat::BucketData> toBucketData() const = 0;
    static core::Result<histogram::DDSketch> fromBucketData(
        const internal::HistogramBlockFormat::BucketData& data);
};

class HistogramOps {
public:
    static core::Result<void> updateBucketsAVX512(
        SIMDHistogram& hist,
        const double* values,
        size_t count);

    static core::Result<void> mergeBucketsAVX512(
        SIMDHistogram& hist,
        const histogram::DDSketch* sketches,
        size_t count);
};

} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_HISTOGRAM_OPS_H_ 