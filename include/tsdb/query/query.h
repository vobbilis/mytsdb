#ifndef TSDB_QUERY_QUERY_H_
#define TSDB_QUERY_QUERY_H_

#include <memory>
#include <string>
#include <vector>
#include <functional>

#include "tsdb/core/types.h"
#include "tsdb/core/config.h"
#include "tsdb/core/metric.h"

namespace tsdb {
namespace query {

/**
 * @brief Represents a label matcher for filtering time series
 */
class LabelMatcher {
public:
    enum class Type {
        EQ,      // Equal
        NEQ,     // Not equal
        RE,      // Regular expression match
        NRE      // Regular expression not match
    };
    
    virtual ~LabelMatcher() = default;
    
    virtual Type type() const = 0;
    virtual const std::string& name() const = 0;
    virtual const std::string& value() const = 0;
    virtual bool matches(const core::Labels& labels) const = 0;
};

/**
 * @brief Represents an aggregation operation
 */
class Aggregation {
public:
    enum class Op {
        SUM,
        AVG,
        MIN,
        MAX,
        COUNT,
        QUANTILE,
        STDDEV,
        STDVAR
    };
    
    virtual ~Aggregation() = default;
    
    virtual Op op() const = 0;
    virtual const std::vector<std::string>& by() const = 0;
    virtual const std::vector<std::string>& without() const = 0;
    virtual std::optional<double> param() const = 0;  // For quantile
};

/**
 * @brief Interface for query results
 */
class QueryResult {
public:
    virtual ~QueryResult() = default;
    
    virtual size_t num_series() const = 0;
    virtual size_t num_samples() const = 0;
    virtual std::unique_ptr<core::TimeSeriesIterator> series() = 0;
};

/**
 * @brief Interface for instant queries (single timestamp)
 */
class InstantQuery {
public:
    virtual ~InstantQuery() = default;
    
    virtual void set_timestamp(core::Timestamp ts) = 0;
    virtual void add_label_matcher(std::unique_ptr<LabelMatcher> matcher) = 0;
    virtual void set_aggregation(std::unique_ptr<Aggregation> agg) = 0;
    virtual std::shared_ptr<QueryResult> execute() = 0;
};

/**
 * @brief Interface for range queries (time range)
 */
class RangeQuery {
public:
    virtual ~RangeQuery() = default;
    
    virtual void set_time_range(core::Timestamp start, core::Timestamp end) = 0;
    virtual void set_step(core::Duration step) = 0;
    virtual void add_label_matcher(std::unique_ptr<LabelMatcher> matcher) = 0;
    virtual void set_aggregation(std::unique_ptr<Aggregation> agg) = 0;
    virtual std::shared_ptr<QueryResult> execute() = 0;
};

/**
 * @brief Interface for query engine operations
 */
class QueryEngine {
public:
    virtual ~QueryEngine() = default;
    
    /**
     * @brief Initialize the query engine with the given configuration
     */
    virtual void init(const core::QueryConfig& config) = 0;
    
    /**
     * @brief Create a new instant query
     */
    virtual std::unique_ptr<InstantQuery> create_instant_query() = 0;
    
    /**
     * @brief Create a new range query
     */
    virtual std::unique_ptr<RangeQuery> create_range_query() = 0;
    
    /**
     * @brief Get label names matching the given matchers
     */
    virtual std::vector<std::string> label_names(
        const std::vector<std::unique_ptr<LabelMatcher>>& matchers) = 0;
    
    /**
     * @brief Get label values for the given label name and matchers
     */
    virtual std::vector<std::string> label_values(
        const std::string& label_name,
        const std::vector<std::unique_ptr<LabelMatcher>>& matchers) = 0;
    
    /**
     * @brief Get series matching the given matchers
     */
    virtual std::vector<core::Labels> series(
        const std::vector<std::unique_ptr<LabelMatcher>>& matchers) = 0;
    
    /**
     * @brief Get query engine statistics
     */
    virtual std::string stats() const = 0;
};

/**
 * @brief Factory for creating query engine instances
 */
class QueryEngineFactory {
public:
    virtual ~QueryEngineFactory() = default;
    
    /**
     * @brief Create a new query engine instance
     */
    virtual std::shared_ptr<QueryEngine> create(const core::QueryConfig& config) = 0;
};

} // namespace query
} // namespace tsdb

#endif // TSDB_QUERY_QUERY_H_ 