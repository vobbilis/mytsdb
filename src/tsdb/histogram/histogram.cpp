#include "tsdb/histogram/histogram.h"
#include "tsdb/core/result.h"
#include "tsdb/core/types.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace tsdb {
namespace histogram {

namespace {

// Helper function to compute bucket index for exponential histogram
int32_t ComputeExponentialBucket(double value, double base, int32_t scale) {
    if (value <= 0) {
        return 0;
    }
    return static_cast<int32_t>(std::floor(std::log(value) / std::log(base) * scale));
}

} // namespace

class BucketImpl final : public Bucket {
public:
    BucketImpl(core::Value lower, core::Value upper, uint64_t count)
        : lower_(lower)
        , upper_(upper)
        , count_(count) {}

    core::Value lower_bound() const override { return lower_; }
    core::Value upper_bound() const override { return upper_; }
    uint64_t count() const override { return count_; }

    void add(core::Value value, uint64_t count = 1) override {
        if (value >= lower_ && (value < upper_ || (value == upper_ && std::isinf(upper_)))) {
            count_ += count;
        }
    }

    void merge(const Bucket& other) override {
        const auto* bucket = dynamic_cast<const BucketImpl*>(&other);
        if (!bucket) {
            throw std::invalid_argument("Can only merge with BucketImpl");
        }
        if (lower_ != bucket->lower_ || upper_ != bucket->upper_) {
            throw std::invalid_argument("Cannot merge buckets with different boundaries");
        }
        count_ += bucket->count_;
    }

    void clear() override {
        count_ = 0;
    }

    size_t size_bytes() const override {
        return sizeof(*this);
    }

private:
    core::Value lower_;
    core::Value upper_;
    uint64_t count_;
};

class FixedBucketHistogramImpl final : public FixedBucketHistogram {
public:
    explicit FixedBucketHistogramImpl(const std::vector<core::Value>& bounds)
        : bounds_(bounds)
        , buckets_(bounds.size() + 1, 0)
        , count_(0)
        , sum_(0)
        , min_(std::nullopt)
        , max_(std::nullopt) {
        if (bounds.empty()) {
            throw std::invalid_argument("Bounds cannot be empty");
        }
        // Verify bounds are sorted
        if (!std::is_sorted(bounds.begin(), bounds.end())) {
            throw std::invalid_argument("Bounds must be sorted");
        }
    }
    
    void add(core::Value value) override {
        add(value, 1);
    }

    void add(core::Value value, uint64_t count) override {
        if (std::isnan(value)) {
            throw std::invalid_argument("Cannot add NaN value");
        }
        
        size_t bucket_idx = 0;
        while (bucket_idx < bounds_.size() && value >= bounds_[bucket_idx]) {
            ++bucket_idx;
        }

        buckets_[bucket_idx] += count;
        count_ += count;
        sum_ += value * count;

        if (!min_.has_value() || value < min_.value()) {
            min_ = value;
        }
        if (!max_.has_value() || value > max_.value()) {
            max_ = value;
        }
    }
    
    void merge(const Histogram& other) override {
        const auto* other_impl = dynamic_cast<const FixedBucketHistogramImpl*>(&other);
        if (!other_impl) {
            throw std::invalid_argument("Can only merge with FixedBucketHistogramImpl");
        }

        if (bounds_ != other_impl->bounds_) {
            throw std::invalid_argument("Incompatible bucket boundaries");
        }

        for (size_t i = 0; i < buckets_.size(); ++i) {
            buckets_[i] += other_impl->buckets_[i];
        }

        count_ += other_impl->count_;
        sum_ += other_impl->sum_;

        if (!min_.has_value() || 
            (other_impl->min_.has_value() && other_impl->min_.value() < min_.value())) {
            min_ = other_impl->min_;
        }
        if (!max_.has_value() || 
            (other_impl->max_.has_value() && other_impl->max_.value() > max_.value())) {
            max_ = other_impl->max_;
        }
    }
    
    uint64_t count() const override {
        return count_;
    }
    
    core::Value sum() const override {
        return sum_;
    }
    
    std::optional<core::Value> min() const override {
        return min_;
    }
    
    std::optional<core::Value> max() const override {
        return max_;
    }
    
    core::Value quantile(double q) const override {
        if (q < 0 || q > 1) {
            throw std::invalid_argument("Invalid quantile");
        }

        if (count_ == 0) {
            throw std::runtime_error("Empty histogram");
        }

        double target = q * count_;
        double cumsum = 0;
        size_t bucket_idx = 0;

        while (bucket_idx < buckets_.size() && 
               cumsum + buckets_[bucket_idx] < target) {
            cumsum += buckets_[bucket_idx];
            ++bucket_idx;
        }

        if (bucket_idx == 0) {
            return min_.value();
        }
        if (bucket_idx == buckets_.size()) {
            return max_.value();
        }

        double bucket_start = bucket_idx > 0 ? bounds_[bucket_idx - 1] : min_.value();
        double bucket_end = bucket_idx < bounds_.size() ? bounds_[bucket_idx] : max_.value();
        double bucket_fraction = (target - cumsum) / buckets_[bucket_idx];

        return bucket_start + (bucket_end - bucket_start) * bucket_fraction;
    }
    
    std::vector<std::shared_ptr<Bucket>> buckets() const override {
        std::vector<std::shared_ptr<Bucket>> result;
        result.reserve(buckets_.size());

        for (size_t i = 0; i < buckets_.size(); ++i) {
            double lower = i > 0 ? bounds_[i - 1] : -std::numeric_limits<double>::infinity();
            double upper = i < bounds_.size() ? bounds_[i] : std::numeric_limits<double>::infinity();
            result.push_back(std::make_shared<BucketImpl>(
                lower, upper, buckets_[i]));
        }

        return result;
    }
    
    void clear() override {
        std::fill(buckets_.begin(), buckets_.end(), 0);
        count_ = 0;
        sum_ = 0;
        min_ = std::nullopt;
        max_ = std::nullopt;
    }
    
    size_t size_bytes() const override {
        return sizeof(*this) + buckets_.size() * sizeof(uint64_t) + bounds_.size() * sizeof(core::Value);
    }
    
    double relative_error() const override {
        if (bounds_.empty()) {
            return 0.0;
        }

        double max_error = 0.0;
        for (size_t i = 1; i < bounds_.size(); ++i) {
            double error = (bounds_[i] - bounds_[i - 1]) / bounds_[i - 1];
            max_error = std::max(max_error, error);
        }
        return max_error;
    }

private:
    std::vector<core::Value> bounds_;
    std::vector<uint64_t> buckets_;
    uint64_t count_;
    double sum_;
    std::optional<core::Value> min_;
    std::optional<core::Value> max_;
};

class DDSketchImpl final : public DDSketch {
public:
    explicit DDSketchImpl(double alpha)
        : alpha_(alpha)
        , counts_()
        , sums_()
        , total_count_(0)
        , total_sum_(0)
        , min_(std::nullopt)
        , max_(std::nullopt) {
        if (alpha <= 0 || alpha >= 1) {
            throw std::invalid_argument("Alpha must be between 0 and 1");
        }
    }
    
    void add(core::Value value) override {
        add(value, 1);
    }

    void add(core::Value value, uint64_t count) override {
        if (std::isnan(value)) {
            throw std::invalid_argument("Cannot add NaN value");
        }
        
        int bucket_idx = ComputeExponentialBucket(value, 1.0 + alpha_, 1);
        
        counts_[bucket_idx] += count;
        sums_[bucket_idx] += value * count;
        total_count_ += count;
        total_sum_ += value * count;
        
        if (!min_.has_value() || value < min_.value()) {
            min_ = value;
        }
        if (!max_.has_value() || value > max_.value()) {
            max_ = value;
        }
    }
    
    void merge(const Histogram& other) override {
        const auto* dd_hist = dynamic_cast<const DDSketchImpl*>(&other);
        if (!dd_hist) {
            throw std::invalid_argument("Can only merge with DDSketchImpl");
        }
        
        if (alpha_ != dd_hist->alpha_) {
            throw std::invalid_argument("Cannot merge sketches with different alpha values");
        }
        
        for (const auto& [idx, count] : dd_hist->counts_) {
            counts_[idx] += count;
            sums_[idx] += dd_hist->sums_.at(idx);
        }
        
        total_count_ += dd_hist->total_count_;
        total_sum_ += dd_hist->total_sum_;
        
        if (!min_.has_value() || 
            (dd_hist->min_.has_value() && dd_hist->min_.value() < min_.value())) {
            min_ = dd_hist->min_;
        }
        if (!max_.has_value() || 
            (dd_hist->max_.has_value() && dd_hist->max_.value() > max_.value())) {
            max_ = dd_hist->max_;
        }
    }
    
    core::Value quantile(double q) const override {
        if (q < 0 || q > 1) {
            throw std::invalid_argument("Invalid quantile");
        }

        if (total_count_ == 0) {
            throw std::runtime_error("Empty histogram");
        }
        
        uint64_t target = static_cast<uint64_t>(q * total_count_);
        uint64_t cumulative = 0;
        
        // Find the bucket containing the quantile
        auto it = counts_.begin();
        while (it != counts_.end() && cumulative + it->second <= target) {
            cumulative += it->second;
            ++it;
        }
        
        if (it == counts_.end()) {
            return max_.value();
        }
        
        // Interpolate within the bucket
        double bucket_start = std::pow(1.0 + alpha_, it->first);
        double bucket_end = std::pow(1.0 + alpha_, it->first + 1);
        double bucket_fraction = static_cast<double>(target - cumulative) / it->second;
        
        return bucket_start + bucket_fraction * (bucket_end - bucket_start);
    }
    
    uint64_t count() const override {
        return total_count_;
    }
    
    core::Value sum() const override {
        return total_sum_;
    }
    
    std::optional<core::Value> min() const override {
        return min_;
    }
    
    std::optional<core::Value> max() const override {
        return max_;
    }
    
    std::vector<std::shared_ptr<Bucket>> buckets() const override {
        std::vector<std::shared_ptr<Bucket>> result;
        result.reserve(counts_.size());
        
        for (const auto& [idx, count] : counts_) {
            double lower = std::pow(1.0 + alpha_, idx);
            double upper = std::pow(1.0 + alpha_, idx + 1);
            result.push_back(std::make_shared<BucketImpl>(lower, upper, count));
        }
        
        return result;
    }
    
    void clear() override {
        counts_.clear();
        sums_.clear();
        total_count_ = 0;
        total_sum_ = 0;
        min_ = std::nullopt;
        max_ = std::nullopt;
    }
    
    size_t size_bytes() const override {
        return sizeof(*this) + 
               counts_.size() * (sizeof(int) + sizeof(uint64_t)) +
               sums_.size() * (sizeof(int) + sizeof(double));
    }
    
    double relative_error() const override {
        return alpha_;
    }

private:
    double alpha_;
    std::map<int, uint64_t> counts_;
    std::map<int, double> sums_;
    uint64_t total_count_;
    double total_sum_;
    std::optional<core::Value> min_;
    std::optional<core::Value> max_;
};

std::unique_ptr<FixedBucketHistogram> FixedBucketHistogram::create(
    const std::vector<core::Value>& bounds) {
    return std::make_unique<FixedBucketHistogramImpl>(bounds);
}

std::unique_ptr<DDSketch> DDSketch::create(double alpha) {
    return std::make_unique<DDSketchImpl>(alpha);
}

// Placeholder implementation
void init() {}

} // namespace histogram
} // namespace tsdb 