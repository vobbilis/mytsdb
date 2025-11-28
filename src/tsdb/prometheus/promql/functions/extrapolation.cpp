#include "tsdb/prometheus/promql/functions.h"
#include "tsdb/prometheus/promql/value.h"
#include <cmath>
#include <numeric>
#include <algorithm>

namespace tsdb {
namespace prometheus {
namespace promql {

namespace {

// Linear regression helper
struct LinearRegression {
    double slope;
    double intercept;
};

LinearRegression CalculateRegression(const std::vector<Sample>& samples, int64_t intercept_time = 0) {
    if (samples.empty()) return {0, 0};
    
    double n = static_cast<double>(samples.size());
    double sum_x = 0;
    double sum_y = 0;
    double sum_xy = 0;
    double sum_xx = 0;
    
    for (const auto& s : samples) {
        double x = static_cast<double>(s.timestamp - intercept_time) / 1000.0; // Convert to seconds
        double y = s.value;
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_xx += x * x;
    }
    
    double slope = (n * sum_xy - sum_x * sum_y) / (n * sum_xx - sum_x * sum_x);
    double intercept = (sum_y - slope * sum_x) / n;
    
    return {slope, intercept};
}

} // namespace

Value FunctionDelta(const std::vector<Value>& args, Evaluator*) {
    const auto& range_vec = args[0].getMatrix();
    Vector result;
    
    for (const auto& series : range_vec) {
        if (series.samples.size() < 2) continue;
        
        const auto& first = series.samples.front();
        const auto& last = series.samples.back();
        
        // Extrapolate to cover the full range
        // For simplicity, just take difference between last and first
        // Ideally should extrapolate to range boundaries
        double val = last.value() - first.value();
        
        result.push_back(Sample{series.metric, last.timestamp(), val});
    }
    return Value(result);
}

Value FunctionDeriv(const std::vector<Value>& args, Evaluator*) {
    const auto& range_vec = args[0].getMatrix();
    Vector result;
    
    for (const auto& series : range_vec) {
        if (series.samples.size() < 2) continue;
        
        // Convert tsdb::prometheus::Sample to local struct if needed, or use directly
        // CalculateRegression expects vector of samples with timestamp and value
        // series.samples contains tsdb::prometheus::Sample
        
        std::vector<Sample> samples;
        for (const auto& s : series.samples) {
            samples.push_back(Sample{LabelSet(), s.timestamp(), s.value()});
        }
        
        auto reg = CalculateRegression(samples);
        result.push_back(Sample{series.metric, series.samples.back().timestamp(), reg.slope});
    }
    return Value(result);
}

Value FunctionPredictLinear(const std::vector<Value>& args, Evaluator*) {
    const auto& range_vec = args[0].getMatrix();
    double t = args[1].getScalar().value;
    Vector result;
    
    for (const auto& series : range_vec) {
        if (series.samples.size() < 2) continue;
        
        std::vector<Sample> samples;
        for (const auto& s : series.samples) {
            samples.push_back(Sample{LabelSet(), s.timestamp(), s.value()});
        }
        
        auto reg = CalculateRegression(samples, 0); // Use 0 to keep absolute timestamps
        
        // Predict value at time T (seconds from now? No, T is seconds from now usually provided as arg)
        // predict_linear(v range-vector, t scalar)
        // Predicts the value of time series t seconds from now
        // So target time = now + t
        // But here we are evaluating at 'now' (last sample time or eval time)
        // Actually PromQL spec: t is seconds in the future
        
        int64_t now = series.samples.back().timestamp();
        double target_time = static_cast<double>(now) / 1000.0 + t;
        
        double val = reg.slope * target_time + reg.intercept;
        result.push_back(Sample{series.metric, now, val});
    }
    return Value(result);
}

Value FunctionHoltWinters(const std::vector<Value>& args, Evaluator*) {
    const auto& range_vec = args[0].getMatrix();
    double sf = args[1].getScalar().value; // Smoothing factor
    double tf = args[2].getScalar().value; // Trend factor
    Vector result;
    
    for (const auto& series : range_vec) {
        if (series.samples.size() < 2) continue;
        
        // Double exponential smoothing
        double s = series.samples[0].value();
        double b = series.samples[1].value() - series.samples[0].value();
        
        for (size_t i = 1; i < series.samples.size(); ++i) {
            double val = series.samples[i].value();
            double last_s = s;
            s = sf * val + (1 - sf) * (s + b);
            b = tf * (s - last_s) + (1 - tf) * b;
        }
        
        result.push_back(Sample{series.metric, series.samples.back().timestamp(), s});
    }
    return Value(result);
}

void RegisterExtrapolationFunctions(FunctionRegistry& registry) {
    registry.Register({"delta", {ValueType::MATRIX}, false, ValueType::VECTOR, FunctionDelta});
    registry.Register({"deriv", {ValueType::MATRIX}, false, ValueType::VECTOR, FunctionDeriv});
    registry.Register({"predict_linear", {ValueType::MATRIX, ValueType::SCALAR}, false, ValueType::VECTOR, FunctionPredictLinear});
    registry.Register({"holt_winters", {ValueType::MATRIX, ValueType::SCALAR, ValueType::SCALAR}, false, ValueType::VECTOR, FunctionHoltWinters});
}

} // namespace promql
} // namespace prometheus
} // namespace tsdb
