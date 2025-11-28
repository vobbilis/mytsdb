#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "tsdb/prometheus/promql/functions.h"
#include "tsdb/prometheus/promql/value.h"
#include <cmath>

using namespace tsdb::prometheus::promql;
using tsdb::prometheus::LabelSet;

namespace {

class FunctionsTest : public ::testing::Test {
protected:
    FunctionRegistry& registry = FunctionRegistry::Instance();
    
    void SetUp() override {
        RegisterMathFunctions(registry);
        RegisterTimeFunctions(registry);
        RegisterExtrapolationFunctions(registry);
    }

    // Helper to create a vector with one sample
    Value CreateVector(double val, int64_t ts = 1000) {
        Vector v;
        v.push_back(Sample{LabelSet(), ts, val});
        return Value(v);
    }
    
    // Helper to create a scalar
    Value CreateScalar(double val, int64_t ts = 1000) {
        return Value(Scalar{ts, val});
    }
    
    // Helper to create a matrix (range vector)
    Value CreateMatrix(const std::vector<double>& values, int64_t start_ts = 1000, int64_t step = 1000) {
        Matrix m;
        Series s;
        for (size_t i = 0; i < values.size(); ++i) {
            s.samples.push_back(tsdb::prometheus::Sample(start_ts + i * step, values[i]));
        }
        m.push_back(s);
        return Value(m);
    }
};

TEST_F(FunctionsTest, MathFunctions) {
    // Abs
    auto abs_func = registry.Get("abs");
    ASSERT_TRUE(abs_func);
    auto res = abs_func->implementation({CreateVector(-5.0)}, nullptr);
    ASSERT_TRUE(res.isVector());
    EXPECT_EQ(res.getVector()[0].value, 5.0);
    
    // Ceil
    auto ceil_func = registry.Get("ceil");
    ASSERT_TRUE(ceil_func);
    res = ceil_func->implementation({CreateVector(5.1)}, nullptr);
    EXPECT_EQ(res.getVector()[0].value, 6.0);
    
    // Exp
    auto exp_func = registry.Get("exp");
    ASSERT_TRUE(exp_func);
    res = exp_func->implementation({CreateVector(1.0)}, nullptr);
    EXPECT_NEAR(res.getVector()[0].value, std::exp(1.0), 0.0001);
}

TEST_F(FunctionsTest, TimeFunctions) {
    // Year
    auto year_func = registry.Get("year");
    ASSERT_TRUE(year_func);
    // 2023-01-01 00:00:00 UTC = 1672531200 seconds
    auto res = year_func->implementation({CreateVector(1672531200.0)}, nullptr);
    EXPECT_EQ(res.getVector()[0].value, 2023.0);
    
    // Hour
    auto hour_func = registry.Get("hour");
    ASSERT_TRUE(hour_func);
    // 2023-01-01 12:30:00 UTC = 1672576200 seconds
    res = hour_func->implementation({CreateVector(1672576200.0)}, nullptr);
    EXPECT_EQ(res.getVector()[0].value, 12.0);
}

TEST_F(FunctionsTest, ExtrapolationFunctions) {
    // Delta
    auto delta_func = registry.Get("delta");
    ASSERT_TRUE(delta_func);
    // 10, 20, 30 -> delta = 30 - 10 = 20
    auto matrix = CreateMatrix({10, 20, 30});
    auto res = delta_func->implementation({matrix}, nullptr);
    ASSERT_TRUE(res.isVector());
    EXPECT_EQ(res.getVector()[0].value, 20.0);
    
    // Deriv
    auto deriv_func = registry.Get("deriv");
    ASSERT_TRUE(deriv_func);
    // 0, 1, 2 at t=0, 1, 2 (seconds) -> slope = 1
    matrix = CreateMatrix({0, 1, 2}, 0, 1000);
    res = deriv_func->implementation({matrix}, nullptr);
    EXPECT_NEAR(res.getVector()[0].value, 1.0, 0.0001);
    
    // Predict Linear
    auto predict_func = registry.Get("predict_linear");
    ASSERT_TRUE(predict_func);
    // 0, 1, 2 at t=0, 1, 2. Predict at t+10 (t=12) -> 12
    matrix = CreateMatrix({0, 1, 2}, 0, 1000);
    res = predict_func->implementation({matrix, CreateScalar(10.0)}, nullptr);
    EXPECT_NEAR(res.getVector()[0].value, 12.0, 0.0001);
}

} // namespace
