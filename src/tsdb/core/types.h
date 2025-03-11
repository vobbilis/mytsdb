#ifndef TSDB_CORE_TYPES_H_
#define TSDB_CORE_TYPES_H_

#include <string>
#include <vector>
#include <utility>
#include <cstdint>

namespace tsdb {
namespace core {

// Basic label type representing a key-value pair
struct Label {
    std::string name;
    std::string value;
    
    Label(const std::string& n, const std::string& v)
        : name(n), value(v) {}
        
    bool operator==(const Label& other) const {
        return name == other.name && value == other.value;
    }
    
    bool operator<(const Label& other) const {
        return name < other.name || (name == other.name && value < other.value);
    }
};

// Collection of labels
using Labels = std::vector<Label>;

// Sample represents a single timestamped value
struct Sample {
    int64_t timestamp;
    double value;
    
    Sample(int64_t ts, double v)
        : timestamp(ts), value(v) {}
        
    bool operator==(const Sample& other) const {
        return timestamp == other.timestamp && value == other.value;
    }
};

// Collection of samples
using Samples = std::vector<Sample>;

// Time range
struct TimeRange {
    int64_t min_time;
    int64_t max_time;
    
    TimeRange(int64_t min, int64_t max)
        : min_time(min), max_time(max) {}
        
    bool contains(int64_t ts) const {
        return ts >= min_time && ts <= max_time;
    }
};

// Metric types
enum class MetricType {
    COUNTER,
    GAUGE,
    HISTOGRAM,
    SUMMARY
};

} // namespace core
} // namespace tsdb

#endif // TSDB_CORE_TYPES_H_ 