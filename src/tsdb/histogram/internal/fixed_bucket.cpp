#include "tsdb/histogram/internal/fixed_bucket_impl.h"
#include "tsdb/core/error.h"
#include <algorithm>
#include <limits>
#include <cmath>

namespace tsdb {
namespace histogram {
namespace internal {

// FixedBucketBucket implementation
FixedBucketBucket::FixedBucketBucket(core::Value lower, core::Value upper)
    : lower_(lower), upper_(upper), count_(0) {}

core::Value FixedBucketBucket::lower_bound() const {
    return lower_;
}

core::Value FixedBucketBucket::upper_bound() const {
    return upper_;
}

uint64_t FixedBucketBucket::count() const {
    return count_;
}

void FixedBucketBucket::add(core::Value value, uint64_t count) {
    count_ += count;
}

void FixedBucketBucket::merge(const Bucket& other) {
    const auto* fixed_bucket = dynamic_cast<const FixedBucketBucket*>(&other);
    if (!fixed_bucket) {
        throw core::InvalidArgumentError("Can only merge FixedBucketBucket instances");
    }
    if (lower_ != fixed_bucket->lower_ || upper_ != fixed_bucket->upper_) {
        throw core::InvalidArgumentError("Cannot merge buckets with different boundaries");
    }
    count_ += fixed_bucket->count_;
}

void FixedBucketBucket::clear() {
    count_ = 0;
}

size_t FixedBucketBucket::size_bytes() const {
    return sizeof(*this);
}

// FixedBucketHistogramImpl implementation
FixedBucketHistogramImpl::FixedBucketHistogramImpl(const std::vector<core::Value>& bounds)
    : bounds_(bounds), total_count_(0), sum_(0.0) {
    if (bounds_.empty()) {
        throw core::InvalidArgumentError("Bucket boundaries cannot be empty");
    }
    
    // Ensure boundaries are sorted
    if (!std::is_sorted(bounds_.begin(), bounds_.end())) {
        throw core::InvalidArgumentError("Bucket boundaries must be sorted");
    }
    
    // Create buckets
    buckets_.reserve(bounds_.size() + 1);
    
    // First bucket (-inf, bounds[0])
    buckets_.push_back(std::make_shared<FixedBucketBucket>(
        -std::numeric_limits<core::Value>::infinity(),
        bounds_[0]));
    
    // Middle buckets [bounds[i], bounds[i+1])
    for (size_t i = 0; i < bounds_.size() - 1; ++i) {
        buckets_.push_back(std::make_shared<FixedBucketBucket>(
            bounds_[i],
            bounds_[i + 1]));
    }
    
    // Last bucket [bounds.back(), +inf)
    buckets_.push_back(std::make_shared<FixedBucketBucket>(
        bounds_.back(),
        std::numeric_limits<core::Value>::infinity()));
}

size_t FixedBucketHistogramImpl::find_bucket_index(core::Value value) const {
    auto it = std::upper_bound(bounds_.begin(), bounds_.end(), value);
    return std::distance(bounds_.begin(), it);
}

void FixedBucketHistogramImpl::add(core::Value value) {
    add(value, 1);
}

void FixedBucketHistogramImpl::add(core::Value value, uint64_t count) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t index = find_bucket_index(value);
    buckets_[index]->add(value, count);
    
    total_count_ += count;
    sum_ += value * count;
    
    if (!min_ || value < *min_) min_ = value;
    if (!max_ || value > *max_) max_ = value;
}

void FixedBucketHistogramImpl::merge(const Histogram& other) {
    const auto* fixed_hist = dynamic_cast<const FixedBucketHistogramImpl*>(&other);
    if (!fixed_hist) {
        throw core::InvalidArgumentError("Can only merge FixedBucketHistogram instances");
    }
    if (bounds_ != fixed_hist->bounds_) {
        throw core::InvalidArgumentError("Cannot merge histograms with different boundaries");
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    std::lock_guard<std::mutex> other_lock(fixed_hist->mutex_);
    
    for (size_t i = 0; i < buckets_.size(); ++i) {
        buckets_[i]->merge(*fixed_hist->buckets_[i]);
    }
    
    total_count_ += fixed_hist->total_count_;
    sum_ += fixed_hist->sum_;
    
    if (!min_ || (fixed_hist->min_ && *fixed_hist->min_ < *min_)) {
        min_ = fixed_hist->min_;
    }
    if (!max_ || (fixed_hist->max_ && *fixed_hist->max_ > *max_)) {
        max_ = fixed_hist->max_;
    }
}

uint64_t FixedBucketHistogramImpl::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return total_count_;
}

core::Value FixedBucketHistogramImpl::sum() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sum_;
}

std::optional<core::Value> FixedBucketHistogramImpl::min() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return min_;
}

std::optional<core::Value> FixedBucketHistogramImpl::max() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return max_;
}

core::Value FixedBucketHistogramImpl::quantile(double q) const {
    if (q < 0.0 || q > 1.0) {
        throw core::InvalidArgumentError("Quantile must be between 0 and 1");
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (total_count_ == 0) {
        return 0.0;
    }
    
    uint64_t rank = static_cast<uint64_t>(q * total_count_);
    uint64_t cumulative = 0;
    
    for (size_t i = 0; i < buckets_.size(); ++i) {
        cumulative += buckets_[i]->count();
        if (cumulative > rank) {
            // Linear interpolation within the bucket
            core::Value lower = buckets_[i]->lower_bound();
            core::Value upper = buckets_[i]->upper_bound();
            uint64_t bucket_count = buckets_[i]->count();
            uint64_t position = rank - (cumulative - bucket_count);
            double fraction = static_cast<double>(position) / bucket_count;
            return lower + fraction * (upper - lower);
        }
    }
    
    // If we get here, return the highest bucket's upper bound
    return buckets_.back()->upper_bound();
}

std::vector<std::shared_ptr<Bucket>> FixedBucketHistogramImpl::buckets() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<Bucket>> result;
    result.reserve(buckets_.size());
    for (const auto& bucket : buckets_) {
        result.push_back(bucket);
    }
    return result;
}

void FixedBucketHistogramImpl::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& bucket : buckets_) {
        bucket->clear();
    }
    total_count_ = 0;
    sum_ = 0.0;
    min_ = std::nullopt;
    max_ = std::nullopt;
}

size_t FixedBucketHistogramImpl::size_bytes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t size = sizeof(*this) + bounds_.capacity() * sizeof(core::Value);
    for (const auto& bucket : buckets_) {
        size += bucket->size_bytes();
    }
    return size;
}

double FixedBucketHistogramImpl::relative_error() const {
    // For fixed buckets, the relative error varies by bucket size
    // Return the maximum relative error across all buckets
    std::lock_guard<std::mutex> lock(mutex_);
    double max_error = 0.0;
    for (size_t i = 0; i < buckets_.size(); ++i) {
        core::Value lower = buckets_[i]->lower_bound();
        core::Value upper = buckets_[i]->upper_bound();
        if (std::isfinite(lower) && std::isfinite(upper) && lower > 0) {
            double error = (upper - lower) / lower;
            max_error = std::max(max_error, error);
        }
    }
    return max_error;
}

std::unique_ptr<FixedBucketHistogram> FixedBucketHistogram::create(
    const std::vector<core::Value>& bounds) {
    return std::make_unique<FixedBucketHistogramImpl>(bounds);
}

}  // namespace internal
}  // namespace histogram
}  // namespace tsdb 