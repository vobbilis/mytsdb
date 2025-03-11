#ifndef TSDB_HISTOGRAM_HISTOGRAM_H_
#define TSDB_HISTOGRAM_HISTOGRAM_H_

#include <memory>
#include <vector>
#include <optional>
#include <cstdint>

#include "tsdb/core/types.h"
#include "tsdb/core/config.h"

namespace tsdb {
namespace histogram {

/**
 * @brief Interface for histogram buckets
 */
class Bucket {
public:
    virtual ~Bucket() = default;
    
    /**
     * @brief Get the lower bound of this bucket
     */
    virtual core::Value lower_bound() const = 0;
    
    /**
     * @brief Get the upper bound of this bucket
     */
    virtual core::Value upper_bound() const = 0;
    
    /**
     * @brief Get the count of values in this bucket
     */
    virtual uint64_t count() const = 0;
    
    /**
     * @brief Add a value to this bucket
     */
    virtual void add(core::Value value, uint64_t count = 1) = 0;
    
    /**
     * @brief Merge another bucket into this one
     */
    virtual void merge(const Bucket& other) = 0;
    
    /**
     * @brief Reset the bucket to empty state
     */
    virtual void clear() = 0;
    
    /**
     * @brief Get the size of this bucket in bytes
     */
    virtual size_t size_bytes() const = 0;
};

/**
 * @brief Interface for histogram implementations
 */
class Histogram {
public:
    virtual ~Histogram() = default;
    
    /**
     * @brief Add a single value to the histogram
     */
    virtual void add(core::Value value) = 0;
    
    /**
     * @brief Add multiple occurrences of a value
     */
    virtual void add(core::Value value, uint64_t count) = 0;
    
    /**
     * @brief Merge another histogram into this one
     */
    virtual void merge(const Histogram& other) = 0;
    
    /**
     * @brief Get the total count of values
     */
    virtual uint64_t count() const = 0;
    
    /**
     * @brief Get the sum of all values
     */
    virtual core::Value sum() const = 0;
    
    /**
     * @brief Get the minimum value
     */
    virtual std::optional<core::Value> min() const = 0;
    
    /**
     * @brief Get the maximum value
     */
    virtual std::optional<core::Value> max() const = 0;
    
    /**
     * @brief Get the value at the given quantile
     */
    virtual core::Value quantile(double q) const = 0;
    
    /**
     * @brief Get all buckets in the histogram
     */
    virtual std::vector<std::shared_ptr<Bucket>> buckets() const = 0;
    
    /**
     * @brief Reset the histogram to empty state
     */
    virtual void clear() = 0;
    
    /**
     * @brief Get the size of this histogram in bytes
     */
    virtual size_t size_bytes() const = 0;
    
    /**
     * @brief Get the relative error of this histogram
     */
    virtual double relative_error() const = 0;
};

/**
 * @brief Interface for DDSketch histogram
 */
class DDSketch : public Histogram {
public:
    /**
     * @brief Create a new DDSketch histogram
     * 
     * @param alpha Relative accuracy parameter (e.g., 0.01 for 1% relative error)
     * @return std::unique_ptr<DDSketch>
     */
    static std::unique_ptr<DDSketch> create(double alpha);
};

/**
 * @brief Interface for fixed-bucket histogram
 */
class FixedBucketHistogram : public Histogram {
public:
    /**
     * @brief Create a new fixed-bucket histogram
     * 
     * @param bounds Vector of bucket boundaries
     * @return std::unique_ptr<FixedBucketHistogram>
     */
    static std::unique_ptr<FixedBucketHistogram> create(
        const std::vector<core::Value>& bounds);
};

} // namespace histogram
} // namespace tsdb

#endif // TSDB_HISTOGRAM_HISTOGRAM_H_ 