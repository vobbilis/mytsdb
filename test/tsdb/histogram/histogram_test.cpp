#include <gtest/gtest.h>
#include "tsdb/histogram/histogram.h"
#include <random>
#include <cmath>
#include <thread>
#include <future>
#include <algorithm>
#include <limits>

namespace tsdb {
namespace histogram {
namespace {

// Helper function to generate test data
std::vector<double> GenerateTestData(
    size_t count,
    std::function<double(std::mt19937&)> generator) {
    std::random_device rd;
    std::mt19937 gen(rd());
    
    std::vector<double> values;
    values.reserve(count);
    for (size_t i = 0; i < count; i++) {
        values.push_back(generator(gen));
    }
    return values;
}

// Helper function to verify histogram quantiles
void VerifyQuantiles(
    const Histogram& hist,
    const std::vector<double>& sorted_values,
    const std::vector<double>& quantiles,
    double max_relative_error) {
    for (double q : quantiles) {
        size_t index = static_cast<size_t>(q * sorted_values.size());
        double actual = sorted_values[index];
        double estimated = hist.Quantile(q);
        
        double relative_error = std::abs((estimated - actual) / actual);
        EXPECT_LE(relative_error, max_relative_error)
            << "Quantile " << q << " estimate " << estimated
            << " differs from actual " << actual
            << " by relative error " << relative_error;
    }
}

class HistogramTest : public ::testing::Test {
protected:
    // Common quantiles to test
    const std::vector<double> test_quantiles_ = {
        0.0, 0.1, 0.25, 0.5, 0.75, 0.9, 0.95, 0.99, 1.0
    };
};

TEST(FixedHistogramTest, BasicOperations) {
    std::vector<double> boundaries = {1.0, 2.0, 5.0, 10.0};
    auto hist = CreateFixedHistogram(boundaries);
    
    // Record some values
    hist->Record(0.5);   // Below first boundary
    hist->Record(1.5);   // Between 1.0 and 2.0
    hist->Record(3.0);   // Between 2.0 and 5.0
    hist->Record(7.0);   // Between 5.0 and 10.0
    hist->Record(15.0);  // Above last boundary
    
    // Check statistics
    const auto& stats = hist->Stats();
    EXPECT_EQ(stats.count, 5);
    EXPECT_DOUBLE_EQ(stats.sum, 27.0);
    EXPECT_DOUBLE_EQ(stats.min, 0.5);
    EXPECT_DOUBLE_EQ(stats.max, 15.0);
    
    // Check quantiles
    EXPECT_DOUBLE_EQ(hist->Quantile(0.0), 0.5);
    EXPECT_NEAR(hist->Quantile(0.5), 3.0, 0.1);
    EXPECT_DOUBLE_EQ(hist->Quantile(1.0), 15.0);
}

TEST(FixedHistogramTest, MergeHistograms) {
    std::vector<double> boundaries = {1.0, 10.0, 100.0};
    
    auto hist1 = CreateFixedHistogram(boundaries);
    auto hist2 = CreateFixedHistogram(boundaries);
    
    // Record values in both histograms
    hist1->Record(0.5);
    hist1->Record(5.0);
    hist1->Record(50.0);
    
    hist2->Record(2.0);
    hist2->Record(20.0);
    hist2->Record(200.0);
    
    // Merge histograms
    hist1->Merge(*hist2);
    
    // Check merged statistics
    const auto& stats = hist1->Stats();
    EXPECT_EQ(stats.count, 6);
    EXPECT_DOUBLE_EQ(stats.sum, 277.5);
    EXPECT_DOUBLE_EQ(stats.min, 0.5);
    EXPECT_DOUBLE_EQ(stats.max, 200.0);
}

TEST(ExponentialHistogramTest, BasicOperations) {
    auto hist = CreateExponentialHistogram(2.0, 1);  // Base 2, scale 1
    
    // Record some values
    hist->Record(1.0);
    hist->Record(2.0);
    hist->Record(4.0);
    hist->Record(8.0);
    
    // Check statistics
    const auto& stats = hist->Stats();
    EXPECT_EQ(stats.count, 4);
    EXPECT_DOUBLE_EQ(stats.sum, 15.0);
    EXPECT_DOUBLE_EQ(stats.min, 1.0);
    EXPECT_DOUBLE_EQ(stats.max, 8.0);
    
    // Check quantiles
    EXPECT_DOUBLE_EQ(hist->Quantile(0.0), 1.0);
    EXPECT_NEAR(hist->Quantile(0.5), 3.0, 1.0);
    EXPECT_DOUBLE_EQ(hist->Quantile(1.0), 8.0);
}

TEST(ExponentialHistogramTest, NegativeValues) {
    auto hist = CreateExponentialHistogram(2.0, 1);
    
    // Record both positive and negative values
    hist->Record(-8.0);
    hist->Record(-4.0);
    hist->Record(-2.0);
    hist->Record(2.0);
    hist->Record(4.0);
    hist->Record(8.0);
    
    // Check statistics
    const auto& stats = hist->Stats();
    EXPECT_EQ(stats.count, 6);
    EXPECT_DOUBLE_EQ(stats.sum, 0.0);
    EXPECT_DOUBLE_EQ(stats.min, -8.0);
    EXPECT_DOUBLE_EQ(stats.max, 8.0);
    
    // Check quantiles
    EXPECT_DOUBLE_EQ(hist->Quantile(0.0), -8.0);
    EXPECT_NEAR(hist->Quantile(0.5), 0.0, 1.0);
    EXPECT_DOUBLE_EQ(hist->Quantile(1.0), 8.0);
}

TEST(ExponentialHistogramTest, MergeHistograms) {
    auto hist1 = CreateExponentialHistogram(2.0, 1);
    auto hist2 = CreateExponentialHistogram(2.0, 1);
    
    // Record values in both histograms
    hist1->Record(1.0);
    hist1->Record(2.0);
    hist1->Record(4.0);
    
    hist2->Record(8.0);
    hist2->Record(16.0);
    hist2->Record(32.0);
    
    // Merge histograms
    hist1->Merge(*hist2);
    
    // Check merged statistics
    const auto& stats = hist1->Stats();
    EXPECT_EQ(stats.count, 6);
    EXPECT_DOUBLE_EQ(stats.sum, 63.0);
    EXPECT_DOUBLE_EQ(stats.min, 1.0);
    EXPECT_DOUBLE_EQ(stats.max, 32.0);
}

TEST(HistogramTest, LargeDataset) {
    auto fixed_hist = CreateFixedHistogram({1.0, 10.0, 100.0, 1000.0});
    auto exp_hist = CreateExponentialHistogram(2.0, 2);
    
    // Generate random data from a log-normal distribution
    std::random_device rd;
    std::mt19937 gen(rd());
    std::lognormal_distribution<> d(2.0, 1.0);
    
    const int num_samples = 100000;
    std::vector<double> values;
    values.reserve(num_samples);
    
    for (int i = 0; i < num_samples; i++) {
        double value = d(gen);
        values.push_back(value);
        fixed_hist->Record(value);
        exp_hist->Record(value);
    }
    
    // Sort values for comparison
    std::sort(values.begin(), values.end());
    
    // Check quantiles against actual values
    const std::vector<double> quantiles = {0.1, 0.5, 0.9};
    
    for (double q : quantiles) {
        size_t index = static_cast<size_t>(q * values.size());
        double actual = values[index];
        
        double fixed_q = fixed_hist->Quantile(q);
        double exp_q = exp_hist->Quantile(q);
        
        // Allow for some error in the approximation
        EXPECT_NEAR(fixed_q, actual, actual * 0.2);  // Within 20%
        EXPECT_NEAR(exp_q, actual, actual * 0.2);    // Within 20%
    }
}

// New comprehensive tests

// Test histogram accuracy with different distributions
TEST_F(HistogramTest, DistributionAccuracy) {
    struct TestCase {
        std::string name;
        std::function<double(std::mt19937&)> generator;
        double max_relative_error;
    };
    
    std::vector<TestCase> test_cases = {
        {
            "Uniform",
            [](std::mt19937& gen) {
                std::uniform_real_distribution<> d(0.0, 100.0);
                return d(gen);
            },
            0.05  // 5% error
        },
        {
            "Normal",
            [](std::mt19937& gen) {
                std::normal_distribution<> d(50.0, 10.0);
                return d(gen);
            },
            0.1  // 10% error
        },
        {
            "LogNormal",
            [](std::mt19937& gen) {
                std::lognormal_distribution<> d(0.0, 1.0);
                return d(gen);
            },
            0.15  // 15% error
        },
        {
            "Exponential",
            [](std::mt19937& gen) {
                std::exponential_distribution<> d(1.0);
                return d(gen);
            },
            0.15  // 15% error
        }
    };
    
    const size_t num_samples = 100000;
    
    for (const auto& test : test_cases) {
        SCOPED_TRACE(test.name);
        
        // Create histograms
        auto fixed_hist = CreateFixedHistogram({
            0.1, 0.5, 1.0, 2.0, 5.0, 10.0, 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0
        });
        auto exp_hist = CreateExponentialHistogram(2.0, 4);  // Base 2, scale 4
        
        // Generate and record data
        auto values = GenerateTestData(num_samples, test.generator);
        auto sorted_values = values;
        std::sort(sorted_values.begin(), sorted_values.end());
        
        for (double value : values) {
            fixed_hist->Record(value);
            exp_hist->Record(value);
        }
        
        // Verify statistics
        const auto& fixed_stats = fixed_hist->Stats();
        const auto& exp_stats = exp_hist->Stats();
        
        EXPECT_EQ(fixed_stats.count, num_samples);
        EXPECT_EQ(exp_stats.count, num_samples);
        
        EXPECT_DOUBLE_EQ(fixed_stats.min, sorted_values.front());
        EXPECT_DOUBLE_EQ(fixed_stats.max, sorted_values.back());
        
        // Verify quantiles
        VerifyQuantiles(*fixed_hist, sorted_values, test_quantiles_, test.max_relative_error);
        VerifyQuantiles(*exp_hist, sorted_values, test_quantiles_, test.max_relative_error);
    }
}

// Test concurrent histogram operations
TEST_F(HistogramTest, ConcurrentOperations) {
    const size_t num_threads = 4;
    const size_t samples_per_thread = 25000;
    
    auto hist = CreateExponentialHistogram(2.0, 4);
    
    // Generate test data
    std::vector<std::vector<double>> thread_data;
    std::vector<double> all_data;
    all_data.reserve(num_threads * samples_per_thread);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::lognormal_distribution<> d(0.0, 1.0);
    
    for (size_t t = 0; t < num_threads; t++) {
        auto values = GenerateTestData(samples_per_thread,
            [&d](std::mt19937& g) { return d(g); });
        thread_data.push_back(values);
        all_data.insert(all_data.end(), values.begin(), values.end());
    }
    
    // Launch concurrent recorders
    std::vector<std::future<void>> futures;
    for (size_t t = 0; t < num_threads; t++) {
        futures.push_back(std::async(std::launch::async,
            [&hist, &thread_data, t]() {
                for (double value : thread_data[t]) {
                    hist->Record(value);
                }
            }));
    }
    
    // Wait for all threads
    for (auto& future : futures) {
        future.get();
    }
    
    // Verify results
    const auto& stats = hist->Stats();
    EXPECT_EQ(stats.count, num_threads * samples_per_thread);
    
    std::sort(all_data.begin(), all_data.end());
    VerifyQuantiles(*hist, all_data, test_quantiles_, 0.15);
}

// Test histogram merging accuracy
TEST_F(HistogramTest, MergeAccuracy) {
    const size_t num_histograms = 5;
    const size_t samples_per_hist = 20000;
    
    // Create histograms with different parameters
    std::vector<std::unique_ptr<Histogram>> fixed_hists;
    std::vector<std::unique_ptr<Histogram>> exp_hists;
    
    std::vector<double> all_values;
    all_values.reserve(num_histograms * samples_per_hist);
    
    // Generate data and record in separate histograms
    for (size_t i = 0; i < num_histograms; i++) {
        fixed_hists.push_back(CreateFixedHistogram({
            0.1, 1.0, 10.0, 100.0, 1000.0
        }));
        
        exp_hists.push_back(CreateExponentialHistogram(2.0, 4));
        
        auto values = GenerateTestData(samples_per_hist,
            [](std::mt19937& gen) {
                std::lognormal_distribution<> d(0.0, 1.0);
                return d(gen);
            });
        
        for (double value : values) {
            fixed_hists[i]->Record(value);
            exp_hists[i]->Record(value);
        }
        
        all_values.insert(all_values.end(), values.begin(), values.end());
    }
    
    // Merge all histograms into the first one
    for (size_t i = 1; i < num_histograms; i++) {
        fixed_hists[0]->Merge(*fixed_hists[i]);
        exp_hists[0]->Merge(*exp_hists[i]);
    }
    
    // Verify merged results
    std::sort(all_values.begin(), all_values.end());
    
    VerifyQuantiles(*fixed_hists[0], all_values, test_quantiles_, 0.15);
    VerifyQuantiles(*exp_hists[0], all_values, test_quantiles_, 0.15);
    
    // Verify statistics
    const auto& fixed_stats = fixed_hists[0]->Stats();
    const auto& exp_stats = exp_hists[0]->Stats();
    
    EXPECT_EQ(fixed_stats.count, num_histograms * samples_per_hist);
    EXPECT_EQ(exp_stats.count, num_histograms * samples_per_hist);
    
    EXPECT_DOUBLE_EQ(fixed_stats.min, all_values.front());
    EXPECT_DOUBLE_EQ(fixed_stats.max, all_values.back());
    EXPECT_DOUBLE_EQ(exp_stats.min, all_values.front());
    EXPECT_DOUBLE_EQ(exp_stats.max, all_values.back());
}

// Test error handling
TEST_F(HistogramTest, ErrorHandling) {
    // Test invalid quantile values
    auto hist = CreateFixedHistogram({1.0, 10.0});
    hist->Record(5.0);
    
    EXPECT_THROW(hist->Quantile(-0.1), core::Error);
    EXPECT_THROW(hist->Quantile(1.1), core::Error);
    
    // Test merging incompatible histograms
    auto hist1 = CreateFixedHistogram({1.0, 10.0});
    auto hist2 = CreateFixedHistogram({1.0, 5.0, 10.0});
    EXPECT_THROW(hist1->Merge(*hist2), core::Error);
    
    auto exp1 = CreateExponentialHistogram(2.0, 1);
    auto exp2 = CreateExponentialHistogram(2.0, 2);
    EXPECT_THROW(exp1->Merge(*exp2), core::Error);
    
    // Test invalid histogram parameters
    EXPECT_THROW(CreateExponentialHistogram(1.0, 1), core::Error);
    EXPECT_THROW(CreateExponentialHistogram(0.5, 1), core::Error);
}

// Test memory usage
TEST_F(HistogramTest, MemoryUsage) {
    const size_t num_samples = 1000000;
    
    // Create histograms with different configurations
    auto fixed_sparse = CreateFixedHistogram({1.0, 10.0, 100.0});  // Few buckets
    auto fixed_dense = CreateFixedHistogram({
        0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0, 20.0, 50.0, 100.0,
        200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0
    });  // Many buckets
    
    auto exp_low_res = CreateExponentialHistogram(2.0, 1);   // Low resolution
    auto exp_high_res = CreateExponentialHistogram(2.0, 8);  // High resolution
    
    // Record samples from a wide range
    auto values = GenerateTestData(num_samples,
        [](std::mt19937& gen) {
            std::lognormal_distribution<> d(0.0, 2.0);  // Wide range
            return d(gen);
        });
    
    for (double value : values) {
        fixed_sparse->Record(value);
        fixed_dense->Record(value);
        exp_low_res->Record(value);
        exp_high_res->Record(value);
    }
    
    // Sort values for verification
    std::sort(values.begin(), values.end());
    
    // Verify accuracy vs memory trade-off
    // Fixed histograms: More buckets = better accuracy, more memory
    double sparse_error = 0.0;
    double dense_error = 0.0;
    
    for (double q : test_quantiles_) {
        size_t idx = static_cast<size_t>(q * values.size());
        double actual = values[idx];
        
        double sparse_est = fixed_sparse->Quantile(q);
        double dense_est = fixed_dense->Quantile(q);
        
        sparse_error += std::abs((sparse_est - actual) / actual);
        dense_error += std::abs((dense_est - actual) / actual);
    }
    
    sparse_error /= test_quantiles_.size();
    dense_error /= test_quantiles_.size();
    
    EXPECT_GT(sparse_error, dense_error)
        << "Dense histogram should be more accurate than sparse";
    
    // Exponential histograms: Higher scale = better accuracy, more memory
    double low_res_error = 0.0;
    double high_res_error = 0.0;
    
    for (double q : test_quantiles_) {
        size_t idx = static_cast<size_t>(q * values.size());
        double actual = values[idx];
        
        double low_res_est = exp_low_res->Quantile(q);
        double high_res_est = exp_high_res->Quantile(q);
        
        low_res_error += std::abs((low_res_est - actual) / actual);
        high_res_error += std::abs((high_res_est - actual) / actual);
    }
    
    low_res_error /= test_quantiles_.size();
    high_res_error /= test_quantiles_.size();
    
    EXPECT_GT(low_res_error, high_res_error)
        << "High resolution histogram should be more accurate than low resolution";
}

// Test boundary conditions for histogram values
TEST_F(HistogramTest, ValueBoundaries) {
    auto hist = CreateFixedHistogram({1.0, 10.0, 100.0});
    
    // Test special floating point values
    hist->Record(std::numeric_limits<double>::infinity());
    hist->Record(-std::numeric_limits<double>::infinity());
    hist->Record(std::numeric_limits<double>::quiet_NaN());
    hist->Record(std::numeric_limits<double>::min());
    hist->Record(std::numeric_limits<double>::max());
    hist->Record(std::numeric_limits<double>::denorm_min());
    hist->Record(-0.0);
    hist->Record(0.0);
    
    const auto& stats = hist->Stats();
    EXPECT_EQ(stats.count, 8);
    
    // Test quantiles with special values
    EXPECT_TRUE(std::isfinite(hist->Quantile(0.0)));
    EXPECT_TRUE(std::isfinite(hist->Quantile(1.0)));
    
    // Test exact bucket boundaries
    hist = CreateFixedHistogram({1.0, 10.0});
    hist->Record(1.0);  // Exactly on first boundary
    hist->Record(10.0); // Exactly on second boundary
    
    EXPECT_DOUBLE_EQ(hist->Quantile(0.0), 1.0);
    EXPECT_DOUBLE_EQ(hist->Quantile(1.0), 10.0);
}

// Test histogram creation edge cases
TEST_F(HistogramTest, CreationEdgeCases) {
    // Test empty boundaries
    EXPECT_THROW(CreateFixedHistogram({}), core::Error);
    
    // Test single boundary
    auto hist = CreateFixedHistogram({1.0});
    EXPECT_NO_THROW(hist->Record(0.5));
    EXPECT_NO_THROW(hist->Record(1.5));
    
    // Test duplicate boundaries
    EXPECT_THROW(CreateFixedHistogram({1.0, 1.0}), core::Error);
    
    // Test unsorted boundaries
    EXPECT_THROW(CreateFixedHistogram({10.0, 1.0}), core::Error);
    
    // Test negative boundaries
    EXPECT_NO_THROW(CreateFixedHistogram({-10.0, -1.0, 0.0, 1.0, 10.0}));
    
    // Test invalid exponential histogram parameters
    EXPECT_THROW(CreateExponentialHistogram(0.5, 1), core::Error);  // base < 1
    EXPECT_THROW(CreateExponentialHistogram(2.0, 0), core::Error);  // scale = 0
    EXPECT_THROW(CreateExponentialHistogram(2.0, -1), core::Error); // negative scale
}

// Test quantile edge cases
TEST_F(HistogramTest, QuantileEdgeCases) {
    auto hist = CreateFixedHistogram({1.0, 10.0, 100.0});
    
    // Test empty histogram
    EXPECT_DOUBLE_EQ(hist->Quantile(0.5), 0.0);
    
    // Test invalid quantile values
    EXPECT_THROW(hist->Quantile(-0.1), core::Error);
    EXPECT_THROW(hist->Quantile(1.1), core::Error);
    
    // Test single value
    hist->Record(5.0);
    EXPECT_DOUBLE_EQ(hist->Quantile(0.0), 5.0);
    EXPECT_DOUBLE_EQ(hist->Quantile(0.5), 5.0);
    EXPECT_DOUBLE_EQ(hist->Quantile(1.0), 5.0);
    
    // Test identical values
    hist = CreateFixedHistogram({1.0, 10.0});
    for (int i = 0; i < 100; i++) {
        hist->Record(5.0);
    }
    
    EXPECT_DOUBLE_EQ(hist->Quantile(0.0), 5.0);
    EXPECT_DOUBLE_EQ(hist->Quantile(0.5), 5.0);
    EXPECT_DOUBLE_EQ(hist->Quantile(1.0), 5.0);
}

// Test merge edge cases
TEST_F(HistogramTest, MergeEdgeCases) {
    // Test merging empty histograms
    auto hist1 = CreateFixedHistogram({1.0, 10.0});
    auto hist2 = CreateFixedHistogram({1.0, 10.0});
    EXPECT_NO_THROW(hist1->Merge(*hist2));
    
    const auto& stats = hist1->Stats();
    EXPECT_EQ(stats.count, 0);
    EXPECT_TRUE(std::isinf(stats.min) && stats.min > 0);  // Positive infinity
    EXPECT_TRUE(std::isinf(stats.max) && stats.max < 0);  // Negative infinity
    
    // Test merging with empty histogram
    hist1->Record(5.0);
    EXPECT_NO_THROW(hist1->Merge(*hist2));
    EXPECT_EQ(hist1->Stats().count, 1);
    
    // Test merging incompatible histograms
    auto hist3 = CreateFixedHistogram({1.0, 5.0, 10.0});  // Different boundaries
    EXPECT_THROW(hist1->Merge(*hist3), core::Error);
    
    auto exp1 = CreateExponentialHistogram(2.0, 1);
    auto exp2 = CreateExponentialHistogram(2.0, 2);  // Different scale
    EXPECT_THROW(exp1->Merge(*exp2), core::Error);
    
    auto exp3 = CreateExponentialHistogram(4.0, 1);  // Different base
    EXPECT_THROW(exp1->Merge(*exp3), core::Error);
}

// Test concurrent histogram updates
TEST_F(HistogramTest, ConcurrentUpdates) {
    const size_t num_threads = 4;
    const size_t values_per_thread = 25000;
    
    // Test fixed histogram
    {
        auto hist = CreateFixedHistogram({1.0, 10.0, 100.0, 1000.0});
        std::vector<std::future<void>> futures;
        
        for (size_t t = 0; t < num_threads; t++) {
            futures.push_back(std::async(std::launch::async,
                [&hist, values_per_thread]() {
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::lognormal_distribution<> d(0.0, 1.0);
                    
                    for (size_t i = 0; i < values_per_thread; i++) {
                        hist->Record(d(gen));
                    }
                }));
        }
        
        for (auto& future : futures) {
            future.get();
        }
        
        EXPECT_EQ(hist->Stats().count, num_threads * values_per_thread);
    }
    
    // Test exponential histogram
    {
        auto hist = CreateExponentialHistogram(2.0, 4);
        std::vector<std::future<void>> futures;
        
        for (size_t t = 0; t < num_threads; t++) {
            futures.push_back(std::async(std::launch::async,
                [&hist, values_per_thread]() {
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::lognormal_distribution<> d(0.0, 1.0);
                    
                    for (size_t i = 0; i < values_per_thread; i++) {
                        hist->Record(d(gen));
                    }
                }));
        }
        
        for (auto& future : futures) {
            future.get();
        }
        
        EXPECT_EQ(hist->Stats().count, num_threads * values_per_thread);
    }
}

// Test histogram accuracy with extreme distributions
TEST_F(HistogramTest, ExtremeDistributions) {
    const size_t num_samples = 100000;
    
    struct TestCase {
        std::string name;
        std::function<double(std::mt19937&)> generator;
        double expected_mean;
        double expected_stddev;
    };
    
    std::vector<TestCase> test_cases = {
        {
            "Bimodal",
            [](std::mt19937& gen) {
                std::uniform_real_distribution<> d(0.0, 1.0);
                return d(gen) < 0.5 ? 1.0 : 1000.0;
            },
            500.5,
            499.5
        },
        {
            "Exponential Decay",
            [](std::mt19937& gen) {
                std::exponential_distribution<> d(0.1);
                return d(gen);
            },
            10.0,
            10.0
        },
        {
            "Heavy Tail",
            [](std::mt19937& gen) {
                std::cauchy_distribution<> d(0.0, 1.0);
                return std::abs(d(gen));  // Use abs to ensure positive values
            },
            0.0,  // Undefined for Cauchy
            0.0   // Undefined for Cauchy
        },
        {
            "Sparse",
            [](std::mt19937& gen) {
                std::uniform_real_distribution<> d(0.0, 1.0);
                return d(gen) < 0.001 ? 1000.0 : 0.0;
            },
            1.0,
            31.6
        }
    };
    
    for (const auto& test : test_cases) {
        SCOPED_TRACE(test.name);
        
        auto fixed_hist = CreateFixedHistogram({
            0.1, 1.0, 10.0, 100.0, 1000.0, 10000.0
        });
        auto exp_hist = CreateExponentialHistogram(2.0, 8);
        
        // Generate and record values
        auto values = GenerateTestData(num_samples, test.generator);
        auto sorted_values = values;
        std::sort(sorted_values.begin(), sorted_values.end());
        
        for (double value : values) {
            fixed_hist->Record(value);
            exp_hist->Record(value);
        }
        
        // Verify basic statistics
        const auto& fixed_stats = fixed_hist->Stats();
        const auto& exp_stats = exp_hist->Stats();
        
        EXPECT_EQ(fixed_stats.count, num_samples);
        EXPECT_EQ(exp_stats.count, num_samples);
        
        EXPECT_DOUBLE_EQ(fixed_stats.min, sorted_values.front());
        EXPECT_DOUBLE_EQ(fixed_stats.max, sorted_values.back());
        
        // Verify quantile accuracy for specific points of interest
        std::vector<double> quantiles = {0.001, 0.01, 0.1, 0.5, 0.9, 0.99, 0.999};
        for (double q : quantiles) {
            size_t idx = static_cast<size_t>(q * sorted_values.size());
            double actual = sorted_values[idx];
            
            double fixed_err = std::abs((fixed_hist->Quantile(q) - actual) / actual);
            double exp_err = std::abs((exp_hist->Quantile(q) - actual) / actual);
            
            // Allow larger errors for extreme quantiles and heavy-tailed distributions
            double max_error = (q <= 0.001 || q >= 0.999 || test.name == "Heavy Tail")
                ? 0.3  // 30% error allowed for extreme cases
                : 0.2; // 20% error allowed for normal cases
            
            EXPECT_LE(fixed_err, max_error)
                << "Fixed histogram quantile " << q << " error too large";
            EXPECT_LE(exp_err, max_error)
                << "Exponential histogram quantile " << q << " error too large";
        }
    }
}

} // namespace
} // namespace histogram
} // namespace tsdb 