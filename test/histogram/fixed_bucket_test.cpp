#include <gtest/gtest.h>
#include "tsdb/histogram/histogram.h"
#include "tsdb/core/error.h"
#include <random>
#include <thread>
#include <cmath>

namespace tsdb {
namespace histogram {
namespace {

class FixedBucketTest : public ::testing::Test {
protected:
    void SetUp() override {
        bounds_ = {1.0, 2.0, 5.0, 10.0, 20.0, 50.0, 100.0};
        hist_ = FixedBucketHistogram::create(bounds_);
    }
    
    std::vector<core::Value> bounds_;
    std::unique_ptr<FixedBucketHistogram> hist_;
};

TEST_F(FixedBucketTest, EmptyHistogram) {
    EXPECT_EQ(hist_->count(), 0);
    EXPECT_EQ(hist_->sum(), 0.0);
    EXPECT_FALSE(hist_->min().has_value());
    EXPECT_FALSE(hist_->max().has_value());
    EXPECT_EQ(hist_->buckets().size(), bounds_.size() + 1);
}

TEST_F(FixedBucketTest, SingleValue) {
    hist_->add(42.0);
    
    EXPECT_EQ(hist_->count(), 1);
    EXPECT_EQ(hist_->sum(), 42.0);
    EXPECT_EQ(*hist_->min(), 42.0);
    EXPECT_EQ(*hist_->max(), 42.0);
    
    // Value should be in the second-to-last bucket (20.0-50.0)
    auto buckets = hist_->buckets();
    size_t nonzero_count = 0;
    for (const auto& bucket : buckets) {
        if (bucket->count() > 0) {
            nonzero_count++;
            EXPECT_EQ(bucket->count(), 1);
            EXPECT_GE(42.0, bucket->lower_bound());
            EXPECT_LT(42.0, bucket->upper_bound());
        }
    }
    EXPECT_EQ(nonzero_count, 1);
}

TEST_F(FixedBucketTest, MultipleValues) {
    // Add values in each bucket
    std::vector<double> values = {0.5, 1.5, 3.0, 7.0, 15.0, 30.0, 75.0, 150.0};
    for (double value : values) {
        hist_->add(value);
    }
    
    EXPECT_EQ(hist_->count(), values.size());
    EXPECT_EQ(*hist_->min(), 0.5);
    EXPECT_EQ(*hist_->max(), 150.0);
    
    // Check bucket counts
    auto buckets = hist_->buckets();
    EXPECT_EQ(buckets.size(), bounds_.size() + 1);
    for (const auto& bucket : buckets) {
        EXPECT_EQ(bucket->count(), 1);
    }
}

TEST_F(FixedBucketTest, BucketBoundaries) {
    auto buckets = hist_->buckets();
    
    // First bucket (-inf, 1.0)
    EXPECT_EQ(buckets[0]->lower_bound(), -std::numeric_limits<double>::infinity());
    EXPECT_EQ(buckets[0]->upper_bound(), 1.0);
    
    // Middle buckets
    for (size_t i = 0; i < bounds_.size() - 1; ++i) {
        EXPECT_EQ(buckets[i + 1]->lower_bound(), bounds_[i]);
        EXPECT_EQ(buckets[i + 1]->upper_bound(), bounds_[i + 1]);
    }
    
    // Last bucket (100.0, +inf)
    EXPECT_EQ(buckets.back()->lower_bound(), bounds_.back());
    EXPECT_EQ(buckets.back()->upper_bound(), std::numeric_limits<double>::infinity());
}

TEST_F(FixedBucketTest, Merge) {
    auto other = FixedBucketHistogram::create(bounds_);
    
    // Add values to first histogram
    for (double value : {0.5, 1.5, 3.0, 7.0}) {
        hist_->add(value);
    }
    
    // Add values to second histogram
    for (double value : {15.0, 30.0, 75.0, 150.0}) {
        other->add(value);
    }
    
    // Merge histograms
    hist_->merge(*other);
    
    EXPECT_EQ(hist_->count(), 8);
    EXPECT_EQ(*hist_->min(), 0.5);
    EXPECT_EQ(*hist_->max(), 150.0);
    
    // Check bucket counts
    auto buckets = hist_->buckets();
    for (const auto& bucket : buckets) {
        EXPECT_EQ(bucket->count(), 1);
    }
}

TEST_F(FixedBucketTest, InvalidMerge) {
    std::vector<core::Value> different_bounds = {1.0, 10.0, 100.0};
    auto other = FixedBucketHistogram::create(different_bounds);
    
    hist_->add(1.0);
    other->add(2.0);
    EXPECT_THROW(hist_->merge(*other), core::InvalidArgumentError);
}

TEST_F(FixedBucketTest, Concurrent) {
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this]() {
            for (int j = 0; j < 1000; ++j) {
                hist_->add(static_cast<double>(j % 100));
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(hist_->count(), 10000);
}

TEST_F(FixedBucketTest, Quantiles) {
    // Add 100 values uniformly distributed across buckets
    for (int i = 1; i <= 100; ++i) {
        hist_->add(i);
    }
    
    // Check quantiles
    EXPECT_NEAR(hist_->quantile(0.0), 1.0, 1.0);
    EXPECT_NEAR(hist_->quantile(0.5), 50.0, 10.0);
    EXPECT_NEAR(hist_->quantile(1.0), 100.0, 1.0);
}

TEST_F(FixedBucketTest, Clear) {
    for (int i = 1; i <= 100; ++i) {
        hist_->add(i);
    }
    
    EXPECT_EQ(hist_->count(), 100);
    
    hist_->clear();
    
    EXPECT_EQ(hist_->count(), 0);
    EXPECT_EQ(hist_->sum(), 0.0);
    EXPECT_FALSE(hist_->min().has_value());
    EXPECT_FALSE(hist_->max().has_value());
    
    auto buckets = hist_->buckets();
    for (const auto& bucket : buckets) {
        EXPECT_EQ(bucket->count(), 0);
    }
}

TEST_F(FixedBucketTest, InvalidConstruction) {
    // Empty bounds
    EXPECT_THROW(FixedBucketHistogram::create({}), core::InvalidArgumentError);
    
    // Unsorted bounds
    EXPECT_THROW(FixedBucketHistogram::create({2.0, 1.0}), core::InvalidArgumentError);
}

TEST_F(FixedBucketTest, RelativeError) {
    double max_error = hist_->relative_error();
    
    // Add values and verify that relative error is within bounds
    for (int i = 1; i <= 100; ++i) {
        hist_->add(i);
        double q = static_cast<double>(i) / 100.0;
        double estimated = hist_->quantile(q);
        double relative_error = std::abs(estimated - i) / i;
        EXPECT_LE(relative_error, max_error);
    }
}

} // namespace
} // namespace histogram
} // namespace tsdb 