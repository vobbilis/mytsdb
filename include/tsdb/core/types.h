#ifndef TSDB_CORE_TYPES_H_
#define TSDB_CORE_TYPES_H_

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <mutex>

namespace tsdb {
namespace core {

/**
 * @brief Represents a unique identifier for a time series
 */
using SeriesID = uint64_t;

/**
 * @brief Represents a timestamp in milliseconds since Unix epoch
 */
using Timestamp = int64_t;

/**
 * @brief Represents a duration in milliseconds
 */
using Duration = int64_t;

/**
 * @brief Represents a metric value
 */
using Value = double;

/**
 * @brief Represents a set of labels that identify a time series
 */
class Labels {
public:
    using Map = std::map<std::string, std::string>;
    
    Labels() = default;
    explicit Labels(const Map& labels);
    
    void add(const std::string& name, const std::string& value);
    void remove(const std::string& name);
    void clear();
    bool has(const std::string& name) const;
    std::optional<std::string> get(const std::string& name) const;
    
    const Map& map() const { return labels_; }
    bool empty() const { return labels_.empty(); }
    size_t size() const { return labels_.size(); }
    
    bool operator==(const Labels& other) const;
    bool operator!=(const Labels& other) const;
    bool operator<(const Labels& other) const;
    
    std::string to_string() const;

private:
    Map labels_;
};

/**
 * @brief Represents a set of fields (high-cardinality dimensions)
 */
using Fields = std::map<std::string, std::string>;

/**
 * @brief Represents a single sample in a time series
 */
class Sample {
public:
    Sample(Timestamp ts, Value val);
    Sample(Timestamp ts, Value val, const Fields& fields);
    
    Timestamp timestamp() const { return timestamp_; }
    Value value() const { return value_; }
    const Fields& fields() const { return fields_; }
    
    bool operator==(const Sample& other) const;
    bool operator!=(const Sample& other) const;

private:
    Timestamp timestamp_;
    Value value_;
    Fields fields_;
};

/**
 * @brief Represents a time series with its labels and samples
 */
class TimeSeries {
public:
    TimeSeries() = default;
    explicit TimeSeries(const Labels& labels);
    
    // Copy constructor and assignment operator needed because mutex is not copyable
    TimeSeries(const TimeSeries& other);
    TimeSeries& operator=(const TimeSeries& other);
    
    void add_sample(const Sample& sample);
    void add_sample(Timestamp ts, Value val);
    void add_sample(Timestamp ts, Value val, const Fields& fields);
    
    const Labels& labels() const { return labels_; }
    // Return a copy for thread safety
    std::vector<Sample> samples() const;
    std::vector<Sample> get_samples_snapshot() const;
    
    size_t size() const;
    bool empty() const;
    
    void clear();
    
    bool operator==(const TimeSeries& other) const;
    bool operator!=(const TimeSeries& other) const;

private:
    Labels labels_;
    std::vector<Sample> samples_;
    mutable std::mutex mutex_;
};

/**
 * @brief Interface for time series iterators
 */
class TimeSeriesIterator {
public:
    virtual ~TimeSeriesIterator() = default;
    
    virtual bool next() = 0;
    virtual const TimeSeries& at() const = 0;
    virtual bool error() const = 0;
    virtual std::string error_message() const = 0;
};

} // namespace core
} // namespace tsdb

#endif // TSDB_CORE_TYPES_H_ 