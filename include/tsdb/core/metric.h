#ifndef TSDB_CORE_METRIC_H_
#define TSDB_CORE_METRIC_H_

#include <memory>
#include <string>
#include <vector>

#include "tsdb/core/types.h"
#include "tsdb/core/config.h"

namespace tsdb {
namespace core {

/**
 * @brief Defines the type of metric
 */
enum class MetricType {
    GAUGE,      // A value that can go up and down
    COUNTER,    // A value that can only increase
    HISTOGRAM,  // A distribution of values
    SUMMARY     // A summary of observations
};

/**
 * @brief Interface for all metric types
 */
class Metric {
public:
    virtual ~Metric() = default;
    
    /**
     * @brief Get the type of this metric
     */
    virtual MetricType type() const = 0;
    
    /**
     * @brief Get the labels associated with this metric
     */
    virtual const Labels& labels() const = 0;
    
    /**
     * @brief Get the name of this metric
     */
    virtual std::string name() const = 0;
    
    /**
     * @brief Get the help text for this metric
     */
    virtual std::string help() const = 0;
    
    /**
     * @brief Get the current value of this metric
     */
    virtual Value value() const = 0;
    
    /**
     * @brief Get all samples for this metric within the given time range
     */
    virtual std::vector<Sample> samples(Timestamp start, Timestamp end) const = 0;
};

/**
 * @brief A gauge metric that can increase and decrease
 */
class Gauge : public Metric {
public:
    virtual void set(Value value) = 0;
    virtual void inc(Value amount = 1.0) = 0;
    virtual void dec(Value amount = 1.0) = 0;
    
    MetricType type() const override { return MetricType::GAUGE; }
};

/**
 * @brief A counter metric that can only increase
 */
class Counter : public Metric {
public:
    virtual void inc(Value amount = 1.0) = 0;
    
    MetricType type() const override { return MetricType::COUNTER; }
};

/**
 * @brief A histogram metric that tracks value distributions
 */
class Histogram : public Metric {
public:
    virtual void observe(Value value) = 0;
    
    /**
     * @brief Get the count of observations
     */
    virtual uint64_t count() const = 0;
    
    /**
     * @brief Get the sum of all observations
     */
    virtual Value sum() const = 0;
    
    /**
     * @brief Get bucket counts for predefined bucket boundaries
     */
    virtual std::vector<uint64_t> bucket_counts() const = 0;
    
    /**
     * @brief Get bucket boundaries
     */
    virtual std::vector<Value> bucket_bounds() const = 0;
    
    /**
     * @brief Get the value at the given quantile
     */
    virtual Value quantile(double q) const = 0;
    
    MetricType type() const override { return MetricType::HISTOGRAM; }
};

/**
 * @brief A summary metric that tracks quantiles over a sliding time window
 */
class Summary : public Metric {
public:
    virtual void observe(Value value) = 0;
    
    /**
     * @brief Get the count of observations in the current window
     */
    virtual uint64_t count() const = 0;
    
    /**
     * @brief Get the sum of observations in the current window
     */
    virtual Value sum() const = 0;
    
    /**
     * @brief Get the value at the given quantile
     */
    virtual Value quantile(double q) const = 0;
    
    /**
     * @brief Get all tracked quantiles and their values
     */
    virtual std::vector<std::pair<double, Value>> quantiles() const = 0;
    
    MetricType type() const override { return MetricType::SUMMARY; }
};

/**
 * @brief Factory for creating metric instances
 */
class MetricFactory {
public:
    virtual ~MetricFactory() = default;
    
    virtual std::shared_ptr<Gauge> create_gauge(
        const std::string& name,
        const std::string& help,
        const Labels& labels = Labels()) = 0;
        
    virtual std::shared_ptr<Counter> create_counter(
        const std::string& name,
        const std::string& help,
        const Labels& labels = Labels()) = 0;
        
    virtual std::shared_ptr<Histogram> create_histogram(
        const std::string& name,
        const std::string& help,
        const HistogramConfig& config,
        const Labels& labels = Labels()) = 0;
        
    virtual std::shared_ptr<Summary> create_summary(
        const std::string& name,
        const std::string& help,
        const std::vector<double>& quantiles,
        Duration max_age,
        int age_buckets,
        const Labels& labels = Labels()) = 0;
};

} // namespace core
} // namespace tsdb

#endif // TSDB_CORE_METRIC_H_ 