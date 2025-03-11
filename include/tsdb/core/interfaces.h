#ifndef TSDB_CORE_INTERFACES_H_
#define TSDB_CORE_INTERFACES_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "tsdb/core/metric.h"
#include "tsdb/core/result.h"
#include "tsdb/core/types.h"

namespace tsdb {
namespace core {

// Forward declarations
using Samples = std::vector<Sample>;
class TimeSeries;
class Labels;

// Use MetricType from core namespace
using MetricType = tsdb::core::MetricType;

struct TimeRange {
    Timestamp start;
    Timestamp end;
    
    TimeRange(Timestamp s, Timestamp e) : start(s), end(e) {}
    bool contains(Timestamp ts) const { return ts >= start && ts <= end; }
};

/**
 * @brief Interface for time series operations
 */
class ITimeSeries {
public:
    virtual ~ITimeSeries() = default;

    // Data access
    virtual Result<void> add_sample(const Sample& sample) = 0;
    virtual Result<void> add_samples(const Samples& samples) = 0;
    virtual Result<Sample> get_sample(int64_t timestamp) const = 0;
    virtual Result<Samples> get_samples(const TimeRange& range) const = 0;
    
    // Metadata
    virtual const Labels& labels() const = 0;
    virtual TimeRange time_range() const = 0;
    virtual size_t sample_count() const = 0;
};

/**
 * @brief Interface for metric families
 */
class MetricFamily {
public:
    virtual ~MetricFamily() = default;

    // Metadata
    virtual const std::string& name() const = 0;
    virtual const std::string& help() const = 0;
    virtual MetricType type() const = 0;
    
    // Series management
    virtual Result<std::shared_ptr<ITimeSeries>> create_series(const Labels& labels) = 0;
    virtual Result<std::shared_ptr<ITimeSeries>> get_series(const Labels& labels) = 0;
    virtual Result<std::vector<std::shared_ptr<ITimeSeries>>> get_all_series() = 0;
    virtual Result<void> remove_series(const Labels& labels) = 0;
};

/**
 * @brief Interface for database operations
 */
class Database {
public:
    virtual ~Database() = default;

    // Lifecycle
    virtual Result<void> open() = 0;
    virtual Result<void> close() = 0;
    virtual Result<void> flush() = 0;
    virtual Result<void> compact() = 0;
    
    // Metric family management
    virtual Result<std::shared_ptr<MetricFamily>> create_metric_family(
        const std::string& name,
        const std::string& help,
        MetricType type) = 0;
    virtual Result<std::shared_ptr<MetricFamily>> get_metric_family(
        const std::string& name) = 0;
    virtual Result<std::vector<std::string>> get_metric_names() = 0;
    
    // Label operations
    virtual Result<std::vector<std::string>> get_label_names() = 0;
    virtual Result<std::vector<std::string>> get_label_values(
        const std::string& label_name) = 0;
};

/**
 * @brief Factory for creating database instances
 */
class DatabaseFactory {
public:
    virtual ~DatabaseFactory() = default;

    struct Config {
        std::string data_dir;
        size_t max_blocks;
        size_t block_size;
        bool enable_compression;
        bool enable_mmap;
    };

    static Result<std::unique_ptr<Database>> create(const Config& config);
};

/**
 * @brief Interface for query operations
 */
class QueryEngine {
public:
    virtual ~QueryEngine() = default;

    // Query execution
    virtual Result<Samples> query_range(
        const std::string& query,
        const TimeRange& range,
        int64_t step) = 0;
        
    virtual Result<Sample> query_instant(
        const std::string& query,
        int64_t timestamp) = 0;
};

} // namespace core
} // namespace tsdb

#endif // TSDB_CORE_INTERFACES_H_ 