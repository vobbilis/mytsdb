#include "tsdb/storage/histogram_ops.h"

// System headers
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <string>
#include <vector>

// AVX-512 headers with guards
#if defined(__x86_64__) || defined(_M_X64)
#ifdef TSDB_USE_AVX512
#include <immintrin.h>
#endif
#endif

// Project headers
#include "tsdb/storage/internal/block_types.h"
#include "tsdb/core/result.h"
#include "tsdb/core/types.h"

namespace tsdb {
namespace storage {

core::Result<void> HistogramOps::updateBucketsAVX512(
    SIMDHistogram& hist,
    const double* values,
    size_t count) {
#if defined(__x86_64__) || defined(_M_X64) && defined(TSDB_USE_AVX512)
    if (!values || count == 0) {
        return core::Result<void>::error("Invalid input: values is null or count is 0");
    }

    for (size_t i = 0; i < count; i += 8) {
        size_t batch_size = std::min(size_t(8), count - i);
        __m512 batch = _mm512_loadu_pd(&values[i]);
        
        // Process each value in the batch
        for (size_t j = 0; j < batch_size; ++j) {
            double value = values[i + j];
            auto result = hist.add(value);
            if (!result.ok()) {
                return result;
            }
        }
    }
    return core::Result<void>();
#else
    (void)hist;    // Suppress unused parameter warning
    (void)values;  // Suppress unused parameter warning
    (void)count;   // Suppress unused parameter warning
    return core::Result<void>::error("AVX-512 not supported");
#endif
}

core::Result<void> HistogramOps::mergeBucketsAVX512(
    SIMDHistogram& hist,
    const histogram::DDSketch* sketches,
    size_t count) {
#if defined(__x86_64__) || defined(_M_X64) && defined(TSDB_USE_AVX512)
    if (!sketches || count == 0) {
        return core::Result<void>::error("Invalid input: sketches is null or count is 0");
    }

    for (size_t i = 0; i < count; ++i) {
        auto result = hist.merge(sketches[i]);
        if (!result.ok()) {
            return result;
        }
    }
    return core::Result<void>();
#else
    (void)hist;     // Suppress unused parameter warning
    (void)sketches; // Suppress unused parameter warning
    (void)count;    // Suppress unused parameter warning
    return core::Result<void>::error("AVX-512 not supported");
#endif
}

namespace internal {

class DDSketchImpl : public histogram::DDSketch {
public:
    explicit DDSketchImpl(double alpha)
        : alpha_(alpha), min_(INFINITY), max_(-INFINITY), count_(0) {
        buckets_.reserve(128);  // Initial capacity
    }

    void add(core::Value value) override {
        if (std::isnan(value) || std::isinf(value)) {
            return;
        }
        
        if (value < min_) min_ = value;
        if (value > max_) max_ = value;
        
        // Calculate bucket index
        int bucket = static_cast<int>(std::log(value) / std::log(1.0 + alpha_));
        
        // Ensure bucket exists
        if (bucket >= static_cast<int>(buckets_.size())) {
            buckets_.resize(bucket + 1);
        }
        
        ++buckets_[bucket];
        ++count_;
    }

    void merge(const histogram::Histogram& other) override {
        const auto* other_impl = dynamic_cast<const DDSketchImpl*>(&other);
        if (!other_impl || alpha_ != other_impl->alpha_) {
            return;
        }
        
        // Resize buckets if needed
        if (other_impl->buckets_.size() > buckets_.size()) {
            buckets_.resize(other_impl->buckets_.size());
        }
        
        // Merge buckets
        for (size_t i = 0; i < other_impl->buckets_.size(); ++i) {
            buckets_[i] += other_impl->buckets_[i];
        }
        
        min_ = std::min(min_, other_impl->min_);
        max_ = std::max(max_, other_impl->max_);
        count_ += other_impl->count_;
    }

    core::Value quantile(double q) const override {
        if (q < 0.0 || q > 1.0 || count_ == 0) {
            return 0.0;
        }
        
        uint64_t target = static_cast<uint64_t>(q * count_);
        uint64_t cumulative = 0;
        
        for (size_t i = 0; i < buckets_.size(); ++i) {
            cumulative += buckets_[i];
            if (cumulative > target) {
                // Found the bucket containing the quantile
                double bucket_value = std::pow(1.0 + alpha_, i);
                return bucket_value;
            }
        }
        
        return max_;
    }

    uint64_t count() const override { return count_; }
    core::Value sum() const override { return 0.0; }  // Not implemented
    std::optional<core::Value> min() const override { return min_; }
    std::optional<core::Value> max() const override { return max_; }
    std::vector<std::shared_ptr<histogram::Bucket>> buckets() const override { return {}; }  // Not implemented
    void clear() override {
        buckets_.clear();
        min_ = INFINITY;
        max_ = -INFINITY;
        count_ = 0;
    }
    size_t size_bytes() const override { return sizeof(*this) + buckets_.capacity() * sizeof(uint64_t); }
    double relative_error() const override { return alpha_; }

private:
    double alpha_;
    double min_;
    double max_;
    uint64_t count_;
    std::vector<uint64_t> buckets_;
};

}  // namespace internal
}  // namespace storage
}  // namespace tsdb