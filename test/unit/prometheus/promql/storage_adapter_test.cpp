#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "tsdb/prometheus/promql/engine.h"
#include "tsdb/prometheus/storage/adapter.h"
#include "tsdb/prometheus/model/types.h"

using namespace tsdb::prometheus::promql;
using namespace tsdb::prometheus::model;
using namespace tsdb::prometheus::storage;

// Mock Storage Adapter
class MockStorageAdapter : public StorageAdapter {
public:
    MOCK_METHOD(Matrix, SelectSeries, (const std::vector<LabelMatcher>&, int64_t, int64_t), (override));
    MOCK_METHOD(std::vector<std::string>, LabelNames, (), (override));
    MOCK_METHOD(std::vector<std::string>, LabelValues, (const std::string&), (override));
    
    // Helper to implement SelectAggregateSeries by delegating to SelectSeries (default behavior)
    Matrix SelectAggregateSeries(
        const std::vector<LabelMatcher>& matchers,
        int64_t start,
        int64_t end,
        const tsdb::core::AggregationRequest& aggregation) override {
        return SelectSeries(matchers, start, end);
    }
};

class StorageAdapterTest : public ::testing::Test {
protected:
    void SetUp() override {
        storage_ = new MockStorageAdapter();
        options_.storage_adapter = storage_;
        engine_ = std::make_unique<Engine>(options_);
    }

    void TearDown() override {
        delete storage_;
    }

    MockStorageAdapter* storage_;
    EngineOptions options_;
    std::unique_ptr<Engine> engine_;
};

TEST_F(StorageAdapterTest, TestOverlappingRangesInSingleQuery) {
    // Query: rate(test_metric[1m]) + rate(test_metric[10m])
    // This requires fetching test_metric for [T-1m, T] and [T-10m, T].
    // Due to the bug, the second fetch might reuse the cache from the first one (or vice versa),
    // leading to incorrect results.
    
    int64_t eval_time = 600000; // 10 minutes (600s)
    // We use a simple instant query to trigger the logic
    std::string query = "rate(test_metric[1m]) + rate(test_metric[10m])";
    
    // Mock data
    // We expect SelectSeries to be called.
    // Ideally, it should be called with the superset range [T-10m, T] or called twice.
    // But with the bug, if [1m] is processed first, [10m] might see the [1m] cache.
    
    Matrix matrix_1m;
    Series s1;
    s1.metric.AddLabel("__name__", "test_metric");
    // Matrix 1m: T-60s (1080), T (1140). Rate = 1.
    s1.samples.emplace_back(eval_time - 60000, 1080.0); 
    s1.samples.emplace_back(eval_time, 1140.0);
    matrix_1m.push_back(s1);
    
    Matrix matrix_10m;
    Series s2;
    s2.metric.AddLabel("__name__", "test_metric");
    // Matrix 10m: T-600s (0), T-60s (1080), T (1140).
    // Rate 1m = (1140-1080)/60 = 1.
    // Rate 10m = (1140-0)/600 = 1.9.
    s2.samples.emplace_back(eval_time - 600000, 0.0);
    s2.samples.emplace_back(eval_time - 60000, 1080.0);
    s2.samples.emplace_back(eval_time, 1140.0);
    matrix_10m.push_back(s2);
    
    EXPECT_CALL(*storage_, SelectSeries(::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Invoke([&](const std::vector<LabelMatcher>& matchers, int64_t start, int64_t end) {
            // Check range duration
            int64_t duration = end - start;
            
            if (duration <= 60000 + 1000) { // ~1m (plus lookback/buffer)
                return matrix_1m;
            } else {
                return matrix_10m;
            }
        }));

    // Use ExecuteRange to trigger BufferedStorageAdapter usage.
    // We use a single step to mimic instant evaluation but via the range path.
    auto result = engine_->ExecuteRange(query, eval_time, eval_time, 1000);
    
    ASSERT_FALSE(result.hasError()) << result.error;
    ASSERT_TRUE(result.value.isMatrix()); // ExecuteRange returns Matrix (Value containing Matrix)
    const auto& matrix = result.value.getMatrix();
    ASSERT_EQ(matrix.size(), 1);
    ASSERT_EQ(matrix[0].samples.size(), 1);
    
    // With bug: rate([10m]) uses matrix_1m -> rate = 1. Total = 1 + 1 = 2.
    // Without bug: rate([10m]) uses matrix_10m -> rate = 1.9. Total = 1 + 1.9 = 2.9.
    EXPECT_DOUBLE_EQ(matrix[0].samples[0].value(), 2.9) << "Cache bug detected! Expected 2.9 (1+1.9), got " << matrix[0].samples[0].value();
}
