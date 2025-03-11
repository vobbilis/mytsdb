#ifndef TSDB_CORE_METRIC_IMPL_H_
#define TSDB_CORE_METRIC_IMPL_H_

#include <mutex>
#include <deque>
#include "tsdb/core/metric.h"

namespace tsdb {
namespace core {

/**
 * @brief Base class for metric implementations
 */
class MetricBase {
public:
    MetricBase(const std::string& name,
               const std::string& help,
               const Labels& labels)
        : name_(name), help_(help), labels_(labels) {}
    
    const Labels& labels() const { return labels_; }
    std::string name() const { return name_; }
    std::string help() const { return help_; }
    
protected:
    std::string name_;
    std::string help_;
    Labels labels_;
    mutable std::mutex mutex_;
};

/**
 * @brief Implementation of gauge metric
 */
class GaugeImpl : public Gauge, public MetricBase {
public:
    GaugeImpl(const std::string& name,
              const std::string& help,
              const Labels& labels = Labels());
    
    void set(Value value) override;
    void inc(Value amount = 1.0) override;
    void dec(Value amount = 1.0) override;
    Value value() const override;
    std::vector<Sample> samples(Timestamp start, Timestamp end) const override;
    
private:
    Value current_value_;
    std::deque<Sample> history_;
};

/**
 * @brief Implementation of counter metric
 */
class CounterImpl : public Counter, public MetricBase {
public:
    CounterImpl(const std::string& name,
                const std::string& help,
                const Labels& labels = Labels());
    
    void inc(Value amount = 1.0) override;
    Value value() const override;
    std::vector<Sample> samples(Timestamp start, Timestamp end) const override;
    
private:
    Value current_value_;
    std::deque<Sample> history_;
};

/**
 * @brief Implementation of summary metric
 */
class SummaryImpl : public Summary, public MetricBase {
public:
    SummaryImpl(const std::string& name,
                const std::string& help,
                const std::vector<double>& quantiles,
                Duration max_age,
                int age_buckets,
                const Labels& labels = Labels());
    
    void observe(Value value) override;
    uint64_t count() const override;
    Value sum() const override;
    Value quantile(double q) const override;
    std::vector<std::pair<double, Value>> quantiles() const override;
    std::vector<Sample> samples(Timestamp start, Timestamp end) const override;
    Value value() const override;
    
private:
    void cleanup_old_buckets();
    
    struct Bucket {
        Timestamp timestamp;
        std::vector<Value> values;
    };
    
    std::vector<double> quantiles_;
    Duration max_age_;
    int age_buckets_;
    std::deque<Bucket> buckets_;
    uint64_t total_count_;
    Value total_sum_;
};

/**
 * @brief Implementation of metric factory
 */
class MetricFactoryImpl : public MetricFactory {
public:
    std::shared_ptr<Gauge> create_gauge(
        const std::string& name,
        const std::string& help,
        const Labels& labels = Labels()) override;
        
    std::shared_ptr<Counter> create_counter(
        const std::string& name,
        const std::string& help,
        const Labels& labels = Labels()) override;
        
    std::shared_ptr<Histogram> create_histogram(
        const std::string& name,
        const std::string& help,
        const HistogramConfig& config,
        const Labels& labels = Labels()) override;
        
    std::shared_ptr<Summary> create_summary(
        const std::string& name,
        const std::string& help,
        const std::vector<double>& quantiles,
        Duration max_age,
        int age_buckets,
        const Labels& labels = Labels()) override;
};

} // namespace core
} // namespace tsdb

#endif // TSDB_CORE_METRIC_IMPL_H_ 