#include "tsdb/prometheus/promql/functions.h"
#include "tsdb/prometheus/promql/evaluator.h"
#include <cmath>
#include <algorithm>

namespace tsdb {
namespace prometheus {
namespace promql {

namespace {

// Helper to calculate rate with counter reset handling
// Simplified version of Prometheus extrapolatedRate
double CalculateRate(const Series& series, bool is_counter, bool is_rate) {
    if (series.samples.size() < 2) {
        return 0.0;
    }

    const auto& samples = series.samples;
    double result_value = 0.0;
    
    // Calculate duration in seconds
    double duration = (samples.back().timestamp() - samples.front().timestamp()) / 1000.0;
    if (duration == 0) {
        return 0.0;
    }

    if (!is_counter) {
        // Gauge: simple delta
        result_value = samples.back().value() - samples.front().value();
    } else {
        // Counter: handle resets
        double value = 0.0;
        for (size_t i = 1; i < samples.size(); ++i) {
            double prev = samples[i-1].value();
            double curr = samples[i].value();
            
            if (curr < prev) {
                // Counter reset
                value += prev; // Add the value before reset
                // Assume reset to 0, so add current value
                value += curr;
            } else {
                value += (curr - prev);
            }
        }
        result_value = value;
    }

    if (is_rate) {
        return result_value / duration;
    } else {
        // increase()
        // Extrapolate to cover the full range if needed?
        // For now, just return the delta
        return result_value;
    }
}

// Helper for irate (instant rate)
double CalculateInstantRate(const Series& series) {
    if (series.samples.size() < 2) {
        return 0.0;
    }
    
    // Use last two samples
    const auto& last = series.samples.back();
    const auto& prev = series.samples[series.samples.size() - 2];
    
    double duration = (last.timestamp() - prev.timestamp()) / 1000.0;
    if (duration == 0) return 0.0;
    
    double delta = last.value() - prev.value();
    if (delta < 0) {
        // Counter reset: assume reset to 0
        delta = last.value(); 
    }
    
    return delta / duration;
}

} // namespace

void RegisterRateFunctions(FunctionRegistry& registry) {

    // rate(v range-vector)
    registry.Register({
        "rate",
        {ValueType::MATRIX},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator* eval) -> Value {
            const Matrix& matrix = args[0].getMatrix();
            Vector result;
            
            for (const auto& series : matrix) {
                // Drop __name__ label
                LabelSet result_labels = series.metric;
                result_labels.RemoveLabel("__name__");
                
                double rate = CalculateRate(series, true, true);
                result.push_back(Sample{result_labels, eval->timestamp(), rate});
            }
            return Value(result);
        }
    });

    // increase(v range-vector)
    registry.Register({
        "increase",
        {ValueType::MATRIX},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator* eval) -> Value {
            const Matrix& matrix = args[0].getMatrix();
            Vector result;
            
            for (const auto& series : matrix) {
                LabelSet result_labels = series.metric;
                result_labels.RemoveLabel("__name__");
                
                double increase = CalculateRate(series, true, false);
                result.push_back(Sample{result_labels, eval->timestamp(), increase});
            }
            return Value(result);
        }
    });

    // irate(v range-vector)
    registry.Register({
        "irate",
        {ValueType::MATRIX},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator* eval) -> Value {
            const Matrix& matrix = args[0].getMatrix();
            Vector result;
            
            for (const auto& series : matrix) {
                LabelSet result_labels = series.metric;
                result_labels.RemoveLabel("__name__");
                
                double rate = CalculateInstantRate(series);
                result.push_back(Sample{result_labels, eval->timestamp(), rate});
            }
            return Value(result);
        }
    });
}

} // namespace promql
} // namespace prometheus
} // namespace tsdb
