#include <gtest/gtest.h>
#include "tsdb/histogram/histogram.h"
#include "tsdb/core/error.h"
#include <random>
#include <thread>
#include <cmath>

namespace tsdb {
namespace histogram {
namespace {

class DDSketchTest : public ::testing::Test {
protected:
    void SetUp() override {
        sketch_ = DDSketch::create(0.01);  // 1% relative error
    }
    
    std::unique_ptr<DDSketch> sketch_;
};

TEST_F(DDSketchTest, EmptySketch) {
    EXPECT_EQ(sketch_->count(), 0);
    EXPECT_EQ(sketch_->sum(), 0.0);
    EXPECT_FALSE(sketch_->min().has_value());
    EXPECT_FALSE(sketch_->max().has_value());
    EXPECT_EQ(sketch_->buckets().size(), 0);
    EXPECT_NEAR(sketch_->relative_error(), 0.01, 1e-6);
}

TEST_F(DDSketchTest, SingleValue) {
    sketch_->add(42.0);
    
    EXPECT_EQ(sketch_->count(), 1);
    EXPECT_EQ(sketch_->sum(), 42.0);
    EXPECT_EQ(*sketch_->min(), 42.0);
    EXPECT_EQ(*sketch_->max(), 42.0);
    EXPECT_EQ(sketch_->quantile(0.5), 42.0);
}

TEST_F(DDSketchTest, MultipleValues) {
    for (int i = 1; i <= 100; ++i) {
        sketch_->add(i);
    }
    
    EXPECT_EQ(sketch_->count(), 100);
    EXPECT_EQ(sketch_->sum(), 5050.0);  // Sum of 1 to 100
    EXPECT_EQ(*sketch_->min(), 1.0);
    EXPECT_EQ(*sketch_->max(), 100.0);
    
    // Check quantiles
    EXPECT_NEAR(sketch_->quantile(0.0), 1.0, 1.0);
    EXPECT_NEAR(sketch_->quantile(0.5), 50.0, 1.0);
    EXPECT_NEAR(sketch_->quantile(1.0), 100.0, 1.0);
}

TEST_F(DDSketchTest, RelativeError) {
    // Add exponentially increasing values
    for (int i = 0; i <= 10; ++i) {
        double value = std::pow(10.0, i);
        sketch_->add(value);
    }
    
    // Check relative error for each value
    for (int i = 0; i <= 10; ++i) {
        double value = std::pow(10.0, i);
        double q = static_cast<double>(i) / 10.0;
        double estimated = sketch_->quantile(q);
        double relative_error = std::abs(estimated - value) / value;
        EXPECT_LE(relative_error, 0.01);  // Should be within 1%
    }
}

TEST_F(DDSketchTest, Merge) {
    auto other = DDSketch::create(0.01);
    
    // Add values to first sketch
    for (int i = 1; i <= 50; ++i) {
        sketch_->add(i);
    }
    
    // Add values to second sketch
    for (int i = 51; i <= 100; ++i) {
        other->add(i);
    }
    
    // Merge sketches
    sketch_->merge(*other);
    
    EXPECT_EQ(sketch_->count(), 100);
    EXPECT_EQ(sketch_->sum(), 5050.0);
    EXPECT_EQ(*sketch_->min(), 1.0);
    EXPECT_EQ(*sketch_->max(), 100.0);
    EXPECT_NEAR(sketch_->quantile(0.5), 50.0, 1.0);
}

TEST_F(DDSketchTest, InvalidMerge) {
    auto other = DDSketch::create(0.02);  // Different relative error
    sketch_->add(1.0);
    other->add(2.0);
    EXPECT_THROW(sketch_->merge(*other), core::InvalidArgumentError);
}

TEST_F(DDSketchTest, Concurrent) {
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this]() {
            for (int j = 1; j <= 1000; ++j) {
                sketch_->add(j);
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(sketch_->count(), 10000);
}

TEST_F(DDSketchTest, RandomValues) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::lognormal_distribution<> d(0, 2);
    
    std::vector<double> values;
    for (int i = 0; i < 10000; ++i) {
        double value = d(gen);
        values.push_back(value);
        sketch_->add(value);
    }
    
    std::sort(values.begin(), values.end());
    
    // Check various quantiles
    std::vector<double> quantiles = {0.1, 0.25, 0.5, 0.75, 0.9};
    for (double q : quantiles) {
        size_t index = static_cast<size_t>(q * values.size());
        double expected = values[index];
        double estimated = sketch_->quantile(q);
        double relative_error = std::abs(estimated - expected) / expected;
        EXPECT_LE(relative_error, 0.01);  // Should be within 1%
    }
}

TEST_F(DDSketchTest, Clear) {
    for (int i = 1; i <= 100; ++i) {
        sketch_->add(i);
    }
    
    EXPECT_EQ(sketch_->count(), 100);
    
    sketch_->clear();
    
    EXPECT_EQ(sketch_->count(), 0);
    EXPECT_EQ(sketch_->sum(), 0.0);
    EXPECT_FALSE(sketch_->min().has_value());
    EXPECT_FALSE(sketch_->max().has_value());
    EXPECT_EQ(sketch_->buckets().size(), 0);
}

TEST_F(DDSketchTest, InvalidValues) {
    EXPECT_THROW(sketch_->add(-1.0), core::InvalidArgumentError);
    EXPECT_THROW(sketch_->add(0.0), core::InvalidArgumentError);
    EXPECT_THROW(sketch_->quantile(-0.1), core::InvalidArgumentError);
    EXPECT_THROW(sketch_->quantile(1.1), core::InvalidArgumentError);
}

TEST_F(DDSketchTest, ExtremeValues) {
    sketch_->add(std::numeric_limits<double>::min());
    sketch_->add(std::numeric_limits<double>::max());
    
    EXPECT_EQ(sketch_->count(), 2);
    EXPECT_GT(sketch_->sum(), 0.0);
    EXPECT_TRUE(std::isfinite(sketch_->sum()));
}

} // namespace
} // namespace histogram
} // namespace tsdb 