#include "test_fixture.h"
#include <gtest/gtest.h>
#include <cmath>
#include "tsdb/core/types.h"

namespace tsdb {
namespace comprehensive {

class SubqueryTest : public ComprehensivePromQLTest {
protected:
    void SetUp() override {
        ComprehensivePromQLTest::SetUp();
        
        // Create test data
        // metric: test_metric_subquery
        // value: 0, 10, 20... at 10s intervals
        GenerateSeries("test_metric_subquery", 
                      {{"job", "test"}}, 
                      300, 0.0, 10.0); // 300 samples = 3000s = 50m
    }

    void GenerateSeries(const std::string& name, 
                       const std::map<std::string, std::string>& labels_map,
                       int count, double start_val, double step_val) {
        tsdb::core::Labels::Map map = labels_map;
        map["__name__"] = name;
        tsdb::core::Labels labels(map);
        
        tsdb::core::TimeSeries series(labels);
        int64_t now = GetQueryTime();
        // Generate data ending at 'now'
        int64_t start_ts = now - ((count - 1) * 10000); // 10s step, ending at now
        
        for (int i = 0; i < count; ++i) {
            series.add_sample(start_ts + i * 10000, start_val + i * step_val);
        }
        
        auto result = storage_->write(series);
        if (!result.ok()) {
            std::cerr << "Failed to write series: " << result.error() << std::endl;
        }
    }
};

TEST_F(SubqueryTest, BasicSubquery) {
    int64_t timestamp = GetQueryTime();

    // Subquery: test_metric_subquery[5m:1m]
    // Range: 5m. Resolution: 1m.
    // Should return a Matrix (Range Vector).
    // The inner expression is a Vector Selector, which returns a Vector.
    // The subquery evaluates it at T, T-1m, T-2m... T-5m?
    // Range is [T-range, T].
    // Step is resolution.
    // So T-5m, T-4m, T-3m, T-2m, T-1m, T. (6 samples?)
    // Or T-5m to T.
    
    auto result = ExecuteQuery("test_metric_subquery[5m:1m]", timestamp);
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::MATRIX);
    ASSERT_EQ(result.getMatrix().size(), 1); // 1 series
    
    const auto& series = result.getMatrix()[0];
    // Check samples count. 5m range, 1m step -> 6 samples (0, 1, 2, 3, 4, 5)?
    // Start: T-5m. End: T.
    // Steps: 0, 60, 120, 180, 240, 300. (6 steps)
    EXPECT_GE(series.samples.size(), 5);
    EXPECT_LE(series.samples.size(), 6);
    
    // Check values. The metric increases by 10 every 10s.
    // So 1m (60s) difference -> +60 value.
    // Samples should be separated by ~60 value.
    for (size_t i = 1; i < series.samples.size(); ++i) {
        double diff = series.samples[i].value() - series.samples[i-1].value();
        EXPECT_NEAR(diff, 60.0, 1.0);
    }
}

TEST_F(SubqueryTest, SubqueryWithFunction) {
    int64_t timestamp = GetQueryTime();

    // rate(test_metric_subquery[1m])[5m:1m]
    // Inner: rate over 1m.
    // Outer: subquery over 5m with 1m resolution.
    // Metric increases by 10 every 10s -> rate is 1.0 per second.
    // So rate should be constant 1.0.
    
    auto result = ExecuteQuery("rate(test_metric_subquery[1m])[5m:1m]", timestamp);
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::MATRIX);
    ASSERT_EQ(result.getMatrix().size(), 1);
    
    const auto& series = result.getMatrix()[0];
    EXPECT_GE(series.samples.size(), 5);
    
    for (const auto& sample : series.samples) {
        EXPECT_NEAR(sample.value(), 1.0, 0.1);
    }
}

} // namespace comprehensive
} // namespace tsdb
