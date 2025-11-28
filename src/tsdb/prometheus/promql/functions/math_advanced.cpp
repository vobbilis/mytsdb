#include "tsdb/prometheus/promql/functions.h"
#include "tsdb/prometheus/promql/evaluator.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace tsdb {
namespace prometheus {
namespace promql {

void RegisterTrigonometricFunctions(FunctionRegistry& registry) {
    // sin(v instant-vector) - Sine (input in radians)
    registry.Register({
        "sin",
        {ValueType::VECTOR},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            const auto& vec = args[0].getVector();
            Vector result;
            for (const auto& s : vec) {
                result.push_back(Sample{s.metric, s.timestamp, std::sin(s.value)});
            }
            return Value(result);
        }
    });
    
    // cos(v instant-vector) - Cosine (input in radians)
    registry.Register({
        "cos",
        {ValueType::VECTOR},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            const auto& vec = args[0].getVector();
            Vector result;
            for (const auto& s : vec) {
                result.push_back(Sample{s.metric, s.timestamp, std::cos(s.value)});
            }
            return Value(result);
        }
    });
    
    // tan(v instant-vector) - Tangent (input in radians)
    registry.Register({
        "tan",
        {ValueType::VECTOR},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            const auto& vec = args[0].getVector();
            Vector result;
            for (const auto& s : vec) {
                result.push_back(Sample{s.metric, s.timestamp, std::tan(s.value)});
            }
            return Value(result);
        }
    });
    
    // asin(v instant-vector) - Arcsine (returns radians)
    registry.Register({
        "asin",
        {ValueType::VECTOR},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            const auto& vec = args[0].getVector();
            Vector result;
            for (const auto& s : vec) {
                // asin domain is [-1, 1]
                double val = (s.value < -1.0 || s.value > 1.0) 
                    ? std::numeric_limits<double>::quiet_NaN() 
                    : std::asin(s.value);
                result.push_back(Sample{s.metric, s.timestamp, val});
            }
            return Value(result);
        }
    });
    
    // acos(v instant-vector) - Arccosine (returns radians)
    registry.Register({
        "acos",
        {ValueType::VECTOR},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            const auto& vec = args[0].getVector();
            Vector result;
            for (const auto& s : vec) {
                // acos domain is [-1, 1]
                double val = (s.value < -1.0 || s.value > 1.0) 
                    ? std::numeric_limits<double>::quiet_NaN() 
                    : std::acos(s.value);
                result.push_back(Sample{s.metric, s.timestamp, val});
            }
            return Value(result);
        }
    });
    
    // atan(v instant-vector) - Arctangent (returns radians)
    registry.Register({
        "atan",
        {ValueType::VECTOR},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            const auto& vec = args[0].getVector();
            Vector result;
            for (const auto& s : vec) {
                result.push_back(Sample{s.metric, s.timestamp, std::atan(s.value)});
            }
            return Value(result);
        }
    });
    
    // deg(v instant-vector) - Convert radians to degrees
    registry.Register({
        "deg",
        {ValueType::VECTOR},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            const auto& vec = args[0].getVector();
            Vector result;
            for (const auto& s : vec) {
                result.push_back(Sample{s.metric, s.timestamp, s.value * 180.0 / M_PI});
            }
            return Value(result);
        }
    });
    
    // rad(v instant-vector) - Convert degrees to radians
    registry.Register({
        "rad",
        {ValueType::VECTOR},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            const auto& vec = args[0].getVector();
            Vector result;
            for (const auto& s : vec) {
                result.push_back(Sample{s.metric, s.timestamp, s.value * M_PI / 180.0});
            }
            return Value(result);
        }
    });
    
    // pi() - Returns π as a scalar
    registry.Register({
        "pi",
        {},
        false,
        ValueType::SCALAR,
        [](const std::vector<Value>&, Evaluator* eval) -> Value {
            return Value(Scalar{eval->timestamp(), M_PI});
        }
    });
    
    // sgn(v instant-vector) - Sign function: -1 for negative, 0 for zero, 1 for positive
    registry.Register({
        "sgn",
        {ValueType::VECTOR},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            const auto& vec = args[0].getVector();
            Vector result;
            for (const auto& s : vec) {
                double sign = (s.value > 0) ? 1.0 : (s.value < 0) ? -1.0 : 0.0;
                result.push_back(Sample{s.metric, s.timestamp, sign});
            }
            return Value(result);
        }
    });
}

void RegisterHyperbolicFunctions(FunctionRegistry& registry) {
    // sinh(v instant-vector) - Hyperbolic sine
    registry.Register({
        "sinh",
        {ValueType::VECTOR},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            const auto& vec = args[0].getVector();
            Vector result;
            for (const auto& s : vec) {
                result.push_back(Sample{s.metric, s.timestamp, std::sinh(s.value)});
            }
            return Value(result);
        }
    });
    
    // cosh(v instant-vector) - Hyperbolic cosine
    registry.Register({
        "cosh",
        {ValueType::VECTOR},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            const auto& vec = args[0].getVector();
            Vector result;
            for (const auto& s : vec) {
                result.push_back(Sample{s.metric, s.timestamp, std::cosh(s.value)});
            }
            return Value(result);
        }
    });
    
    // tanh(v instant-vector) - Hyperbolic tangent
    registry.Register({
        "tanh",
        {ValueType::VECTOR},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            const auto& vec = args[0].getVector();
            Vector result;
            for (const auto& s : vec) {
                result.push_back(Sample{s.metric, s.timestamp, std::tanh(s.value)});
            }
            return Value(result);
        }
    });
    
    // asinh(v instant-vector) - Inverse hyperbolic sine
    registry.Register({
        "asinh",
        {ValueType::VECTOR},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            const auto& vec = args[0].getVector();
            Vector result;
            for (const auto& s : vec) {
                result.push_back(Sample{s.metric, s.timestamp, std::asinh(s.value)});
            }
            return Value(result);
        }
    });
    
    // acosh(v instant-vector) - Inverse hyperbolic cosine
    registry.Register({
        "acosh",
        {ValueType::VECTOR},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            const auto& vec = args[0].getVector();
            Vector result;
            for (const auto& s : vec) {
                // acosh domain is [1, ∞)
                double val = (s.value < 1.0) 
                    ? std::numeric_limits<double>::quiet_NaN() 
                    : std::acosh(s.value);
                result.push_back(Sample{s.metric, s.timestamp, val});
            }
            return Value(result);
        }
    });
    
    // atanh(v instant-vector) - Inverse hyperbolic tangent
    registry.Register({
        "atanh",
        {ValueType::VECTOR},
        false,
        ValueType::VECTOR,
        [](const std::vector<Value>& args, Evaluator*) -> Value {
            const auto& vec = args[0].getVector();
            Vector result;
            for (const auto& s : vec) {
                // atanh domain is (-1, 1)
                double val = (s.value <= -1.0 || s.value >= 1.0) 
                    ? std::numeric_limits<double>::quiet_NaN() 
                    : std::atanh(s.value);
                result.push_back(Sample{s.metric, s.timestamp, val});
            }
            return Value(result);
        }
    });
}

} // namespace promql
} // namespace prometheus
} // namespace tsdb
