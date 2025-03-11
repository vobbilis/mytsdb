#ifndef TSDB_HISTOGRAM_DDSKETCH_H_
#define TSDB_HISTOGRAM_DDSKETCH_H_

#include <cmath>
#include <map>
#include <mutex>
#include <memory>
#include "tsdb/histogram/histogram.h"

namespace tsdb {
namespace histogram {

/**
 * @brief Implementation of DDSketch bucket
 */
class DDSketchBucket : public Bucket {
public:
    DDSketchBucket(int index, double gamma);
    
    core::Value lower_bound() const override;
    core::Value upper_bound() const override;
    uint64_t count() const override;
    void add(core::Value value, uint64_t count = 1) override;
    void merge(const Bucket& other) override;
    void clear() override;
    size_t size_bytes() const override;
    
    int index() const { return index_; }
    
private:
    int index_;
    double gamma_;
    uint64_t count_;
};

/**
 * @brief Implementation of DDSketch histogram
 * 
 * DDSketch is a mergeable quantile sketch with relative-error guarantees.
 * It provides accurate quantile estimates with a guaranteed relative error.
 */
class DDSketchImpl : public DDSketch {
public:
    /**
     * @brief Create a new DDSketch with the given relative accuracy
     * 
     * @param alpha The relative accuracy guarantee (e.g., 0.01 for 1%)
     */
    explicit DDSketchImpl(double alpha);
    
    void add(core::Value value) override;
    void add(core::Value value, uint64_t count) override;
    void merge(const Histogram& other) override;
    uint64_t count() const override;
    core::Value sum() const override;
    std::optional<core::Value> min() const override;
    std::optional<core::Value> max() const override;
    core::Value quantile(double q) const override;
    std::vector<std::shared_ptr<Bucket>> buckets() const override;
    void clear() override;
    size_t size_bytes() const override;
    double relative_error() const override;
    
private:
    int value_to_index(core::Value value) const;
    core::Value index_to_value(int index) const;
    
    double alpha_;      // Relative accuracy parameter
    double gamma_;      // Base for exponential buckets
    double multiplier_; // Multiplier for index calculation
    uint64_t total_count_;
    core::Value sum_;
    std::optional<core::Value> min_;
    std::optional<core::Value> max_;
    std::map<int, std::shared_ptr<DDSketchBucket>> buckets_;
    mutable std::mutex mutex_;
};

} // namespace histogram
} // namespace tsdb

#endif // TSDB_HISTOGRAM_DDSKETCH_H_ 