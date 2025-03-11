#include "tsdb/histogram/internal/ddsketch_impl.h"
#include "tsdb/core/error.h"
#include <cmath>
#include <algorithm>

namespace tsdb {
namespace histogram {
namespace internal {

namespace {
    constexpr double kMinValue = 1e-308;  // Minimum positive normal double
    constexpr double kMaxValue = 1e308;   // Maximum normal double
}

// DDSketchBucket implementation
DDSketchBucket::DDSketchBucket(int index, double gamma)
    : index_(index), gamma_(gamma), count_(0) {}

core::Value DDSketchBucket::lower_bound() const {
    return std::pow(gamma_, index_);
}

core::Value DDSketchBucket::upper_bound() const {
    return std::pow(gamma_, index_ + 1);
}

uint64_t DDSketchBucket::count() const {
    return count_;
}

void DDSketchBucket::add(core::Value value, uint64_t count) {
    count_ += count;
}

void DDSketchBucket::merge(const Bucket& other) {
    const auto* ddsketch_bucket = dynamic_cast<const DDSketchBucket*>(&other);
    if (!ddsketch_bucket) {
        throw core::InvalidArgumentError("Can only merge DDSketchBucket instances");
    }
    if (index_ != ddsketch_bucket->index_ || gamma_ != ddsketch_bucket->gamma_) {
        throw core::InvalidArgumentError("Cannot merge buckets with different parameters");
    }
    count_ += ddsketch_bucket->count_;
}

void DDSketchBucket::clear() {
    count_ = 0;
}

size_t DDSketchBucket::size_bytes() const {
    return sizeof(*this);
}

// DDSketchImpl implementation
DDSketchImpl::DDSketchImpl(double alpha)
    : alpha_(alpha),
      gamma_(1.0 + 2.0 * alpha / (1.0 - alpha)),
      multiplier_(1.0 / std::log(gamma_)),
      total_count_(0),
      sum_(0.0) {
    if (alpha <= 0.0 || alpha >= 1.0) {
        throw core::InvalidArgumentError("Alpha must be between 0 and 1");
    }
}

int DDSketchImpl::value_to_index(core::Value value) const {
    if (value <= 0.0) {
        throw core::InvalidArgumentError("DDSketch only supports positive values");
    }
    if (value < kMinValue) value = kMinValue;
    if (value > kMaxValue) value = kMaxValue;
    return static_cast<int>(std::ceil(std::log(value) * multiplier_));
}

core::Value DDSketchImpl::index_to_value(int index) const {
    return std::pow(gamma_, index);
}

void DDSketchImpl::add(core::Value value) {
    add(value, 1);
}

void DDSketchImpl::add(core::Value value, uint64_t count) {
    if (value <= 0.0) {
        throw core::InvalidArgumentError("DDSketch only supports positive values");
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    int index = value_to_index(value);
    auto& bucket = buckets_[index];
    if (!bucket) {
        bucket = std::make_shared<DDSketchBucket>(index, gamma_);
    }
    bucket->add(value, count);
    
    total_count_ += count;
    sum_ += value * count;
    
    if (!min_ || value < *min_) min_ = value;
    if (!max_ || value > *max_) max_ = value;
}

void DDSketchImpl::merge(const Histogram& other) {
    const auto* ddsketch = dynamic_cast<const DDSketchImpl*>(&other);
    if (!ddsketch) {
        throw core::InvalidArgumentError("Can only merge DDSketch instances");
    }
    if (alpha_ != ddsketch->alpha_ || gamma_ != ddsketch->gamma_) {
        throw core::InvalidArgumentError("Cannot merge sketches with different parameters");
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    std::lock_guard<std::mutex> other_lock(ddsketch->mutex_);
    
    for (const auto& [index, bucket] : ddsketch->buckets_) {
        auto& my_bucket = buckets_[index];
        if (!my_bucket) {
            my_bucket = std::make_shared<DDSketchBucket>(index, gamma_);
        }
        my_bucket->merge(*bucket);
    }
    
    total_count_ += ddsketch->total_count_;
    sum_ += ddsketch->sum_;
    
    if (!min_ || (ddsketch->min_ && *ddsketch->min_ < *min_)) {
        min_ = ddsketch->min_;
    }
    if (!max_ || (ddsketch->max_ && *ddsketch->max_ > *max_)) {
        max_ = ddsketch->max_;
    }
}

uint64_t DDSketchImpl::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return total_count_;
}

core::Value DDSketchImpl::sum() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sum_;
}

std::optional<core::Value> DDSketchImpl::min() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return min_;
}

std::optional<core::Value> DDSketchImpl::max() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return max_;
}

core::Value DDSketchImpl::quantile(double q) const {
    if (q < 0.0 || q > 1.0) {
        throw core::InvalidArgumentError("Quantile must be between 0 and 1");
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (total_count_ == 0) {
        return 0.0;
    }
    
    uint64_t rank = static_cast<uint64_t>(q * total_count_);
    uint64_t cumulative = 0;
    
    for (const auto& [index, bucket] : buckets_) {
        cumulative += bucket->count();
        if (cumulative > rank) {
            return index_to_value(index);
        }
    }
    
    // If we get here, return the highest bucket value
    return index_to_value(buckets_.rbegin()->first);
}

std::vector<std::shared_ptr<Bucket>> DDSketchImpl::buckets() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<Bucket>> result;
    result.reserve(buckets_.size());
    for (const auto& [_, bucket] : buckets_) {
        result.push_back(bucket);
    }
    return result;
}

void DDSketchImpl::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    buckets_.clear();
    total_count_ = 0;
    sum_ = 0.0;
    min_ = std::nullopt;
    max_ = std::nullopt;
}

size_t DDSketchImpl::size_bytes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t size = sizeof(*this);
    for (const auto& [_, bucket] : buckets_) {
        size += bucket->size_bytes();
    }
    return size;
}

double DDSketchImpl::relative_error() const {
    return alpha_;
}

std::unique_ptr<DDSketch> DDSketch::create(double alpha) {
    return std::make_unique<DDSketchImpl>(alpha);
}

}  // namespace internal
}  // namespace histogram
}  // namespace tsdb 