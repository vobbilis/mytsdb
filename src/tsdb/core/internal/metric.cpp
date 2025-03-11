#include "metric_impl.h"
#include "tsdb/core/error.h"
#include "tsdb/histogram/histogram.h"
#include <algorithm>
#include <chrono>

namespace tsdb {
namespace core {

namespace {
    Timestamp now() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
}

// GaugeImpl implementation
GaugeImpl::GaugeImpl(const std::string& name,
                     const std::string& help,
                     const Labels& labels)
    : MetricBase(name, help, labels), current_value_(0.0) {}

void GaugeImpl::set(Value value) {
    std::lock_guard<std::mutex> lock(mutex_);
    current_value_ = value;
    history_.push_back(Sample(now(), value));
}

void GaugeImpl::inc(Value amount) {
    std::lock_guard<std::mutex> lock(mutex_);
    current_value_ += amount;
    history_.push_back(Sample(now(), current_value_));
}

void GaugeImpl::dec(Value amount) {
    std::lock_guard<std::mutex> lock(mutex_);
    current_value_ -= amount;
    history_.push_back(Sample(now(), current_value_));
}

Value GaugeImpl::value() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_value_;
}

std::vector<Sample> GaugeImpl::samples(Timestamp start, Timestamp end) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Sample> result;
    for (const auto& sample : history_) {
        if (sample.timestamp() >= start && sample.timestamp() <= end) {
            result.push_back(sample);
        }
    }
    return result;
}

// CounterImpl implementation
CounterImpl::CounterImpl(const std::string& name,
                        const std::string& help,
                        const Labels& labels)
    : MetricBase(name, help, labels), current_value_(0.0) {}

void CounterImpl::inc(Value amount) {
    if (amount < 0) {
        throw InvalidArgumentError("Counter can only be incremented by non-negative values");
    }
    std::lock_guard<std::mutex> lock(mutex_);
    current_value_ += amount;
    history_.push_back(Sample(now(), current_value_));
}

Value CounterImpl::value() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_value_;
}

std::vector<Sample> CounterImpl::samples(Timestamp start, Timestamp end) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Sample> result;
    for (const auto& sample : history_) {
        if (sample.timestamp() >= start && sample.timestamp() <= end) {
            result.push_back(sample);
        }
    }
    return result;
}

// SummaryImpl implementation
SummaryImpl::SummaryImpl(const std::string& name,
                        const std::string& help,
                        const std::vector<double>& quantiles,
                        Duration max_age,
                        int age_buckets,
                        const Labels& labels)
    : MetricBase(name, help, labels),
      quantiles_(quantiles),
      max_age_(max_age),
      age_buckets_(age_buckets),
      total_count_(0),
      total_sum_(0.0) {
    // Validate quantiles
    for (double q : quantiles) {
        if (q < 0.0 || q > 1.0) {
            throw InvalidArgumentError("Quantile must be between 0 and 1");
        }
    }
    std::sort(quantiles_.begin(), quantiles_.end());
}

void SummaryImpl::observe(Value value) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Create new bucket if needed
    Timestamp current_time = now();
    if (buckets_.empty() || 
        current_time - buckets_.back().timestamp >= max_age_ / age_buckets_) {
        buckets_.push_back({current_time, {}});
    }
    
    // Add value to current bucket
    buckets_.back().values.push_back(value);
    total_count_++;
    total_sum_ += value;
    
    // Cleanup old buckets
    cleanup_old_buckets();
}

void SummaryImpl::cleanup_old_buckets() {
    Timestamp current_time = now();
    while (!buckets_.empty() && current_time - buckets_.front().timestamp > max_age_) {
        for (Value v : buckets_.front().values) {
            total_count_--;
            total_sum_ -= v;
        }
        buckets_.pop_front();
    }
}

uint64_t SummaryImpl::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return total_count_;
}

Value SummaryImpl::sum() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return total_sum_;
}

Value SummaryImpl::value() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return total_count_ > 0 ? total_sum_ / total_count_ : 0.0;
}

Value SummaryImpl::quantile(double q) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (q < 0.0 || q > 1.0) {
        throw InvalidArgumentError("Quantile must be between 0 and 1");
    }
    
    if (total_count_ == 0) {
        return 0.0;
    }
    
    // Collect all values
    std::vector<Value> values;
    values.reserve(total_count_);
    for (const auto& bucket : buckets_) {
        values.insert(values.end(), bucket.values.begin(), bucket.values.end());
    }
    
    // Sort values
    std::sort(values.begin(), values.end());
    
    // Calculate index for quantile
    size_t index = static_cast<size_t>(q * (values.size() - 1));
    return values[index];
}

std::vector<std::pair<double, Value>> SummaryImpl::quantiles() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::pair<double, Value>> result;
    result.reserve(quantiles_.size());
    for (double q : quantiles_) {
        result.emplace_back(q, quantile(q));
    }
    return result;
}

std::vector<Sample> SummaryImpl::samples(Timestamp start, Timestamp end) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Sample> result;
    
    // Add count and sum samples
    if (total_count_ > 0) {
        Timestamp current_time = now();
        if (current_time >= start && current_time <= end) {
            result.emplace_back(current_time, total_count_);
            result.emplace_back(current_time, total_sum_);
        }
    }
    
    return result;
}

// MetricFactoryImpl implementation
std::shared_ptr<Gauge> MetricFactoryImpl::create_gauge(
    const std::string& name,
    const std::string& help,
    const Labels& labels) {
    return std::make_shared<GaugeImpl>(name, help, labels);
}

std::shared_ptr<Counter> MetricFactoryImpl::create_counter(
    const std::string& name,
    const std::string& help,
    const Labels& labels) {
    return std::make_shared<CounterImpl>(name, help, labels);
}

std::shared_ptr<Histogram> MetricFactoryImpl::create_histogram(
    const std::string& name,
    const std::string& help,
    const HistogramConfig& config,
    const Labels& labels) {
    if (config.use_fixed_buckets) {
        return histogram::FixedBucketHistogram::create(config.bounds);
    } else {
        return histogram::DDSketch::create(config.relative_accuracy);
    }
}

std::shared_ptr<Summary> MetricFactoryImpl::create_summary(
    const std::string& name,
    const std::string& help,
    const std::vector<double>& quantiles,
    Duration max_age,
    int age_buckets,
    const Labels& labels) {
    return std::make_shared<SummaryImpl>(
        name, help, quantiles, max_age, age_buckets, labels);
}

} // namespace core
} // namespace tsdb 