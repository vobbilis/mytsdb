#include "tsdb/prometheus/promql/functions.h"
#include "tsdb/prometheus/promql/evaluator.h"
#include <algorithm>
#include <cmath>

namespace tsdb {
namespace prometheus {
namespace promql {

// Helper function for quantile calculation using linear interpolation
double CalculateQuantile(std::vector<double> values, double phi) {
    if (values.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    
    if (phi < 0 || phi > 1) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    
    // Sort values
    std::sort(values.begin(), values.end());
    
    size_t n = values.size();
    if (n == 1) {
        return values[0];
    }
    
    // Calculate position using linear interpolation (Prometheus method)
    double pos = phi * (n - 1);
    size_t lower = static_cast<size_t>(std::floor(pos));
    size_t upper = static_cast<size_t>(std::ceil(pos));
    
    if (lower == upper) {
        return values[lower];
    }
    
    // Linear interpolation between lower and upper
    double fraction = pos - lower;
    return values[lower] * (1 - fraction) + values[upper] * fraction;
}

void RegisterAggregationFunctions(FunctionRegistry& registry) {
    // stddev() - Standard deviation
    registry.Register({
        "stddev",
        {ValueType::VECTOR},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            const auto& vec = args[0].getVector();
            if (vec.empty()) {
                return Value(Vector{});
            }
            
            // Calculate mean
            double sum = 0;
            for (const auto& s : vec) sum += s.value;
            double mean = sum / vec.size();
            
            // Calculate variance
            double variance = 0;
            for (const auto& s : vec) {
                double diff = s.value - mean;
                variance += diff * diff;
            }
            variance /= vec.size();
            
            // Return stddev with first sample's labels
            Vector result;
            result.push_back(Sample{vec[0].metric, vec[0].timestamp, std::sqrt(variance)});
            return Value(result);
        }
    });
    
    // stdvar() - Standard variance
    registry.Register({
        "stdvar",
        {ValueType::VECTOR},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            const auto& vec = args[0].getVector();
            if (vec.empty()) {
                return Value(Vector{});
            }
            
            // Calculate mean
            double sum = 0;
            for (const auto& s : vec) sum += s.value;
            double mean = sum / vec.size();
            
            // Calculate variance
            double variance = 0;
            for (const auto& s : vec) {
                double diff = s.value - mean;
                variance += diff * diff;
            }
            variance /= vec.size();
            
            // Return variance with first sample's labels
            Vector result;
            result.push_back(Sample{vec[0].metric, vec[0].timestamp, variance});
            return Value(result);
        }
    });
    
    // topk(k, vector) - Top K elements by value
    registry.Register({
        "topk",
        {ValueType::SCALAR, ValueType::VECTOR},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            int k = static_cast<int>(args[0].getScalar().value);
            const auto& vec = args[1].getVector();
            
            if (k <= 0 || vec.empty()) {
                return Value(Vector{});
            }
            
            // Sort by value descending
            Vector sorted = vec;
            std::sort(sorted.begin(), sorted.end(), 
                [](const Sample& a, const Sample& b) { return a.value > b.value; });
            
            // Take top k
            Vector result;
            for (int i = 0; i < std::min(k, static_cast<int>(sorted.size())); i++) {
                result.push_back(sorted[i]);
            }
            return Value(result);
        }
    });
    
    // bottomk(k, vector) - Bottom K elements by value
    registry.Register({
        "bottomk",
        {ValueType::SCALAR, ValueType::VECTOR},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            int k = static_cast<int>(args[0].getScalar().value);
            const auto& vec = args[1].getVector();
            
            if (k <= 0 || vec.empty()) {
                return Value(Vector{});
            }
            
            // Sort by value ascending
            Vector sorted = vec;
            std::sort(sorted.begin(), sorted.end(), 
                [](const Sample& a, const Sample& b) { return a.value < b.value; });
            
            // Take bottom k
            Vector result;
            for (int i = 0; i < std::min(k, static_cast<int>(sorted.size())); i++) {
                result.push_back(sorted[i]);
            }
            return Value(result);
        }
    });
    
    // quantile(φ, vector) - φ-quantile (0 ≤ φ ≤ 1)
    registry.Register({
        "quantile",
        {ValueType::SCALAR, ValueType::VECTOR},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            double phi = args[0].getScalar().value;
            const auto& vec = args[1].getVector();
            
            if (vec.empty()) {
                return Value(Vector{});
            }
            
            // Extract values
            std::vector<double> values;
            for (const auto& s : vec) {
                values.push_back(s.value);
            }
            
            double quantile_value = CalculateQuantile(values, phi);
            
            // Return quantile with first sample's labels
            Vector result;
            result.push_back(Sample{vec[0].metric, vec[0].timestamp, quantile_value});
            return Value(result);
        }
    });
}

} // namespace promql
} // namespace prometheus
} // namespace tsdb
