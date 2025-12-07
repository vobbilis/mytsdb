#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <chrono>
#include <map>

#include "tsdb/prometheus/promql/evaluator.h"
#include "tsdb/prometheus/storage/adapter.h"
#include "tsdb/prometheus/model/types.h"
#include "tsdb/prometheus/promql/ast.h"

namespace tsdb {
namespace prometheus {
namespace promql {
namespace test {

// Configurable mock storage
class ConfigurableMockStorage : public storage::StorageAdapter {
public:
    std::vector<Series> data;

    void AddSeries(const std::string& name, const std::vector<std::pair<int64_t, double>>& samples) {
        Series s;
        s.metric.AddLabel("__name__", name);
        // Add job/instance labels to distinguish if needed, but for now just name
        s.metric.AddLabel("job", "test");
        s.metric.AddLabel("instance", "localhost:9090");
        
        for (const auto& p : samples) {
            s.samples.emplace_back(p.first, p.second);
        }
        data.push_back(s);
    }
    
    // Support adding multiple series with same name but different labels
    void AddSeries(const LabelSet& labels, const std::vector<std::pair<int64_t, double>>& samples) {
        Series s;
        s.metric = labels;
        for (const auto& p : samples) {
            s.samples.emplace_back(p.first, p.second);
        }
        data.push_back(s);
    }

    Matrix SelectSeries(const std::vector<model::LabelMatcher>& matchers, 
                        int64_t start, int64_t end) override {
        Matrix result;
        for (const auto& s : data) {
            // Check matchers
            bool match = true;
            for (const auto& m : matchers) {
                auto val = s.metric.GetLabelValue(m.name);
                if (m.type == model::MatcherType::EQUAL) {
                    if (!val || *val != m.value) {
                        match = false;
                        break;
                    }
                }
                // Implement other matchers if needed
            }
            
            if (match) {
                // Filter samples by time
                Series resSeries;
                resSeries.metric = s.metric;
                for (const auto& sample : s.samples) {
                    if (sample.timestamp() >= start && sample.timestamp() <= end) {
                        resSeries.samples.push_back(sample);
                    }
                }
                if (!resSeries.samples.empty()) {
                    result.push_back(resSeries);
                }
            }
        }
        return result;
    }

    Matrix SelectAggregateSeries(const std::vector<model::LabelMatcher>&,
                                 int64_t, int64_t,
                                 const core::AggregationRequest&) override {
        return {}; // Not using pushdown for these tests
    }

    std::vector<std::string> LabelNames() override { return {}; }
    std::vector<std::string> LabelValues(const std::string&) override { return {}; }
};

class VectorizedEvaluatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        storage_ = std::make_unique<ConfigurableMockStorage>();
    }

    std::unique_ptr<ConfigurableMockStorage> storage_;
    
    // Helpers to create nodes
    std::unique_ptr<VectorSelectorNode> MakeVectorSelector(const std::string& name, int64_t offset = 0) {
        // Constructor: VectorSelectorNode(std::string n, std::vector<model::LabelMatcher> matchers)
        auto node = std::make_unique<VectorSelectorNode>(name, std::vector<model::LabelMatcher>{});
        node->parsedOffsetSeconds = offset;
        return node;
    }
    
    std::unique_ptr<MatrixSelectorNode> MakeMatrixSelector(const std::string& name, int64_t range_ms) {
        auto vec = MakeVectorSelector(name);
        // Constructor: MatrixSelectorNode(std::unique_ptr<VectorSelectorNode> vecSel, Token r, int64_t pRange)
        Token dummy(TokenType::DURATION, "", 0, 0); 
        int64_t range_sec = range_ms / 1000; 
        return std::make_unique<MatrixSelectorNode>(std::move(vec), dummy, range_sec);
    }
    
    std::unique_ptr<CallNode> MakeCall(const std::string& func, std::unique_ptr<ExprNode> arg) {
        // Constructor: CallNode(std::string name, std::vector<std::unique_ptr<ExprNode>> a)
        std::vector<std::unique_ptr<ExprNode>> args;
        args.push_back(std::move(arg));
        return std::make_unique<CallNode>(func, std::move(args));
    }
    
    std::unique_ptr<AggregateExprNode> MakeAggregate(const std::string& op_name, std::unique_ptr<ExprNode> arg, 
                                                    const std::vector<std::string>& grouping = {}, bool without = false) {
        // Constructor: AggregateExprNode(TokenType o, std::unique_ptr<ExprNode> e, std::vector<std::string> grp, bool w)
        TokenType op = TokenType::SUM;
        if (op_name == "sum") op = TokenType::SUM;
        else if (op_name == "avg") op = TokenType::AVG;
        else if (op_name == "max") op = TokenType::MAX;
        else if (op_name == "count") op = TokenType::COUNT;
        
        return std::make_unique<AggregateExprNode>(op, std::move(arg), grouping, without);
    }
};

TEST_F(VectorizedEvaluatorTest, TestSumAggregation) {
    // Setup data: 2 series
    std::vector<std::pair<int64_t, double>> s1_data = {
        {1000, 10}, {2000, 20}, {3000, 30}
    };
    std::vector<std::pair<int64_t, double>> s2_data = {
        {1000, 5}, {2000, 15}, {3000, 25}
    };
    
    storage_->AddSeries("http_requests", s1_data);
    storage_->AddSeries("http_requests", s2_data); // Same name, so 2 series
    
    // Query: sum(http_requests)
    // Range: 1000 to 3000, step 1000
    int64_t start = 1000;
    int64_t end = 3000;
    int64_t step = 1000;
    
    Evaluator evaluator(start, end, step, 5000 /* lookback */, storage_.get());
    
    auto vs = MakeVectorSelector("http_requests");
    auto sum = MakeAggregate("sum", std::move(vs));
    
    Value result = evaluator.EvaluateRange(sum.get());
    
    ASSERT_TRUE(result.isMatrix());
    const Matrix& m = result.getMatrix();
    
    ASSERT_EQ(m.size(), 1); // Should be 1 series (summed)
    const Series& res_series = m[0];
    
    ASSERT_EQ(res_series.samples.size(), 3);
    EXPECT_EQ(res_series.samples[0].timestamp(), 1000);
    EXPECT_EQ(res_series.samples[0].value(), 15); // 10+5
    EXPECT_EQ(res_series.samples[1].timestamp(), 2000);
    EXPECT_EQ(res_series.samples[1].value(), 35); // 20+15
    EXPECT_EQ(res_series.samples[2].timestamp(), 3000);
    EXPECT_EQ(res_series.samples[2].value(), 55); // 30+25
}

TEST_F(VectorizedEvaluatorTest, TestRate) {
    // Setup counter data
    // Time: 0, 60s, 120s
    // Val:  0, 10,  30
    // Rate[120s] at 120s: (30-0)/120 = 0.25 (approx)
    // Actually rate is per-second.
    
    int64_t t0 = 100000;
    int64_t t1 = t0 + 60000;
    int64_t t2 = t1 + 60000;
    
    std::vector<std::pair<int64_t, double>> data = {
        {t0, 0}, {t1, 10}, {t2, 30}
    };
    storage_->AddSeries("http_counts", data);
    
    // rate(http_counts[2m])
    int64_t range = 120000; // 2m
    
    Evaluator evaluator(t2, t2, 1000, 0, storage_.get()); // Evaluate at t2
    
    auto ms = MakeMatrixSelector("http_counts", range);
    auto rate = MakeCall("rate", std::move(ms));
    
    Value result = evaluator.EvaluateRange(rate.get());
    
    ASSERT_TRUE(result.isMatrix());
    const Matrix& m = result.getMatrix();
    ASSERT_EQ(m.size(), 1);
    
    EXPECT_NEAR(m[0].samples[0].value(), 0.25, 0.001);
}

TEST_F(VectorizedEvaluatorTest, TestSumRate) {
    // sum(rate(foo[2m]))
    int64_t t0 = 100000;
    int64_t t1 = t0 + 60000;
    int64_t t2 = t1 + 60000; // Total 2m
    
    // S1: 0, 10, 30 -> rate = 0.25
    storage_->AddSeries("foo", {{t0, 0}, {t1, 10}, {t2, 30}});
    
    // S2: 0, 20, 60 -> rate = 0.5
    storage_->AddSeries("foo", {{t0, 0}, {t1, 20}, {t2, 60}});
    
    Evaluator evaluator(t2, t2, 1000, 0, storage_.get());
    
    auto ms = MakeMatrixSelector("foo", 120000); // 2m
    auto rate = MakeCall("rate", std::move(ms));
    auto sum = MakeAggregate("sum", std::move(rate));
    
    Value result = evaluator.EvaluateRange(sum.get());
    
    ASSERT_TRUE(result.isMatrix());
    const Matrix& m = result.getMatrix();
    ASSERT_EQ(m.size(), 1);
    
    EXPECT_NEAR(m[0].samples[0].value(), 0.75, 0.001); // 0.25 + 0.5
}

} // namespace test
} // namespace promql
} // namespace prometheus
} // namespace tsdb
