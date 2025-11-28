#include "tsdb/prometheus/promql/functions.h"
#include "tsdb/prometheus/promql/evaluator.h"
#include <algorithm>
#include <cmath>

namespace tsdb {
namespace prometheus {
namespace promql {

void RegisterUtilityFunctions(FunctionRegistry& registry) {
    // sort(v instant-vector) - Sort vector elements in ascending order by sample value
    registry.Register({
        "sort",
        {ValueType::VECTOR},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            Vector vec = args[0].getVector();
            std::sort(vec.begin(), vec.end(), 
                [](const Sample& a, const Sample& b) { return a.value < b.value; });
            return Value(vec);
        }
    });
    
    // sort_desc(v instant-vector) - Sort vector elements in descending order by sample value
    registry.Register({
        "sort_desc",
        {ValueType::VECTOR},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            Vector vec = args[0].getVector();
            std::sort(vec.begin(), vec.end(), 
                [](const Sample& a, const Sample& b) { return a.value > b.value; });
            return Value(vec);
        }
    });
    
    // clamp(v instant-vector, min scalar, max scalar)
    // Clamps the sample values of all elements in v to have a lower limit of min and upper limit of max
    registry.Register({
        "clamp",
        {ValueType::VECTOR, ValueType::SCALAR, ValueType::SCALAR},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            const auto& vec = args[0].getVector();
            double min_val = args[1].getScalar().value;
            double max_val = args[2].getScalar().value;
            
            Vector result;
            for (const auto& sample : vec) {
                double clamped = std::max(min_val, std::min(max_val, sample.value));
                result.push_back(Sample{sample.metric, sample.timestamp, clamped});
            }
            return Value(result);
        }
    });
    
    // clamp_max(v instant-vector, max scalar)
    // Clamps the sample values of all elements in v to have an upper limit of max
    registry.Register({
        "clamp_max",
        {ValueType::VECTOR, ValueType::SCALAR},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            const auto& vec = args[0].getVector();
            double max_val = args[1].getScalar().value;
            
            Vector result;
            for (const auto& sample : vec) {
                double clamped = std::min(max_val, sample.value);
                result.push_back(Sample{sample.metric, sample.timestamp, clamped});
            }
            return Value(result);
        }
    });
    
    // clamp_min(v instant-vector, min scalar)
    // Clamps the sample values of all elements in v to have a lower limit of min
    registry.Register({
        "clamp_min",
        {ValueType::VECTOR, ValueType::SCALAR},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            const auto& vec = args[0].getVector();
            double min_val = args[1].getScalar().value;
            
            Vector result;
            for (const auto& sample : vec) {
                double clamped = std::max(min_val, sample.value);
                result.push_back(Sample{sample.metric, sample.timestamp, clamped});
            }
            return Value(result);
        }
    });
    
    // vector(s scalar) - Returns the scalar s as a vector with no labels
    registry.Register({
        "vector",
        {ValueType::SCALAR},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            const auto& scalar = args[0].getScalar();
            Vector result;
            LabelSet empty_labels;
            result.push_back(Sample{empty_labels, scalar.timestamp, scalar.value});
            return Value(result);
        }
    });
    
    // scalar(v instant-vector) - Given a single-element input vector, returns the sample value as a scalar
    // If the input vector does not have exactly one element, scalar returns NaN
    registry.Register({
        "scalar",
        {ValueType::VECTOR},
        false,
        ValueType::SCALAR,
        [](const std::vector<Value>& args, Evaluator* eval) -> Value {
            const auto& vec = args[0].getVector();
            if (vec.size() == 1) {
                return Value(Scalar{vec[0].timestamp, vec[0].value});
            }
            // Return NaN if not exactly one element
            return Value(Scalar{eval->timestamp(), std::numeric_limits<double>::quiet_NaN()});
        }
    });
    
    // absent(v instant-vector) - Returns an empty vector if the vector passed to it has any elements
    // Returns a 1-element vector with value 1 if the vector passed to it has no elements
    registry.Register({
        "absent",
        {ValueType::VECTOR},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator* eval) -> Value {
            const auto& vec = args[0].getVector();
            if (vec.empty()) {
                // Return vector with single element of value 1
                Vector result;
                LabelSet empty_labels;
                result.push_back(Sample{empty_labels, eval->timestamp(), 1.0});
                return Value(result);
            }
            // Return empty vector
            return Value(Vector{});
        }
    });
    
    // changes(v range-vector) - For each input time series, returns the number of times its value has changed
    // This is a range-vector function, but we'll implement a simplified version
    registry.Register({
        "changes",
        {ValueType::MATRIX},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            const auto& matrix = args[0].getMatrix();
            Vector result;
            
            for (const auto& series : matrix) {
                if (series.samples.empty()) {
                    continue;
                }
                
                // Count the number of times the value changes
                int changes = 0;
                for (size_t i = 1; i < series.samples.size(); i++) {
                    if (series.samples[i].value() != series.samples[i-1].value()) {
                        changes++;
                    }
                }
                
                // Use the timestamp of the last sample
                int64_t timestamp = series.samples.back().timestamp();
                result.push_back(Sample{series.metric, timestamp, static_cast<double>(changes)});
            }
            
            return Value(result);
        }
    });
}

} // namespace promql
} // namespace prometheus
} // namespace tsdb
