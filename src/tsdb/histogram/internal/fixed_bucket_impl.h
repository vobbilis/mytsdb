#ifndef TSDB_HISTOGRAM_INTERNAL_FIXED_BUCKET_IMPL_H_
#define TSDB_HISTOGRAM_INTERNAL_FIXED_BUCKET_IMPL_H_

#include <vector>
#include <mutex>
#include <memory>
#include "tsdb/histogram/histogram.h"

namespace tsdb {
namespace histogram {
namespace internal {

/**
 * @brief Implementation of fixed-bucket histogram bucket
 */
class FixedBucketBucket : public Bucket {
public:
    FixedBucketBucket(core::Value lower, core::Value upper);
    
    core::Value lower_bound() const override;
    core::Value upper_bound() const override;
    uint64_t count() const override;
    void add(core::Value value, uint64_t count = 1) override;
    void merge(const Bucket& other) override;
    void clear() override;
    size_t size_bytes() const override;
    
private:
    core::Value lower_;
    core::Value upper_;
    uint64_t count_;
};

/**
 * @brief Implementation of fixed-bucket histogram
 */
class FixedBucketHistogramImpl : public FixedBucketHistogram {
public:
    explicit FixedBucketHistogramImpl(const std::vector<core::Value>& bounds);
    
    void add(core::Value value) override;
    void add(core::Value value, uint64_t count) override;
    void merge(const Histogram& other) override;
    uint64_t count() const override;
    core::Value sum() const override;
    std::optional<core::Value> min() const override;
    std::optional<core::Value> max() const override;
    core::Value quantile(double q) const override;
    std::vector<std::shared_ptr<Bucket>> buckets() const override;
    void clear() override;
    size_t size_bytes() const override;
    double relative_error() const override;
    
private:
    size_t find_bucket_index(core::Value value) const;
    
    std::vector<core::Value> bounds_;
    std::vector<std::shared_ptr<FixedBucketBucket>> buckets_;
    uint64_t total_count_;
    core::Value sum_;
    std::optional<core::Value> min_;
    std::optional<core::Value> max_;
    mutable std::mutex mutex_;
};

}  // namespace internal
}  // namespace histogram
}  // namespace tsdb

#endif  // TSDB_HISTOGRAM_INTERNAL_FIXED_BUCKET_IMPL_H_ 