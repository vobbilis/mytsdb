#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>

namespace tsdb {
namespace prometheus {

// Forward declarations
class Sample;
class TimeSeries;
class LabelSet;
class MetricFamily;

/**
 * @brief Represents a single sample in a time series
 */
class Sample {
public:
    Sample(int64_t timestamp, double value);
    
    int64_t timestamp() const { return timestamp_; }
    double value() const { return value_; }
    
    bool operator==(const Sample& other) const;
    bool operator!=(const Sample& other) const;

private:
    int64_t timestamp_;  // Unix timestamp in milliseconds
    double value_;       // Sample value
};

/**
 * @brief Represents a set of labels that identify a time series
 */
class LabelSet {
public:
    using LabelMap = std::map<std::string, std::string>;
    
    LabelSet() = default;
    explicit LabelSet(const LabelMap& labels);
    
    void AddLabel(const std::string& name, const std::string& value);
    void RemoveLabel(const std::string& name);
    bool HasLabel(const std::string& name) const;
    std::optional<std::string> GetLabelValue(const std::string& name) const;
    
    const LabelMap& labels() const { return labels_; }
    
    bool operator==(const LabelSet& other) const;
    bool operator!=(const LabelSet& other) const;
    
    std::string ToString() const;

private:
    LabelMap labels_;
    
    void ValidateLabel(const std::string& name, const std::string& value) const;
};

/**
 * @brief Represents a time series with its labels and samples
 */
class TimeSeries {
public:
    TimeSeries() = default;
    explicit TimeSeries(const LabelSet& labels);
    
    void AddSample(const Sample& sample);
    void AddSample(int64_t timestamp, double value);
    
    const LabelSet& labels() const { return labels_; }
    const std::vector<Sample>& samples() const { return samples_; }
    
    size_t size() const { return samples_.size(); }
    bool empty() const { return samples_.empty(); }
    
    void Clear();
    
    bool operator==(const TimeSeries& other) const;
    bool operator!=(const TimeSeries& other) const;

private:
    LabelSet labels_;
    std::vector<Sample> samples_;
    
    void ValidateTimestamp(int64_t timestamp) const;
};

/**
 * @brief Represents a family of metrics with the same name but different labels
 */
class MetricFamily {
public:
    enum class Type {
        COUNTER,
        GAUGE,
        HISTOGRAM,
        SUMMARY
    };
    
    MetricFamily(const std::string& name, Type type, const std::string& help = "");
    
    void AddTimeSeries(const TimeSeries& series);
    void RemoveTimeSeries(const LabelSet& labels);
    
    const std::string& name() const { return name_; }
    Type type() const { return type_; }
    const std::string& help() const { return help_; }
    
    const std::vector<TimeSeries>& series() const { return series_; }
    
    bool operator==(const MetricFamily& other) const;
    bool operator!=(const MetricFamily& other) const;

private:
    std::string name_;
    Type type_;
    std::string help_;
    std::vector<TimeSeries> series_;
    
    void ValidateMetricName(const std::string& name) const;
};

// Exception classes
class PrometheusError : public std::runtime_error {
public:
    explicit PrometheusError(const std::string& message) 
        : std::runtime_error(message) {}
};

class InvalidLabelError : public PrometheusError {
public:
    explicit InvalidLabelError(const std::string& message) 
        : PrometheusError(message) {}
};

class InvalidMetricError : public PrometheusError {
public:
    explicit InvalidMetricError(const std::string& message) 
        : PrometheusError(message) {}
};

class InvalidTimestampError : public PrometheusError {
public:
    explicit InvalidTimestampError(const std::string& message) 
        : PrometheusError(message) {}
};

} // namespace prometheus
} // namespace tsdb 