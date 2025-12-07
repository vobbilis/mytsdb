#include "tsdb/prometheus/promql/functions.h"
#include "tsdb/prometheus/promql/evaluator.h"
#include <stdexcept>
#include <chrono>
#include <cmath>

namespace tsdb {
namespace prometheus {
namespace promql {

FunctionRegistry& FunctionRegistry::Instance() {
    static FunctionRegistry instance;
    return instance;
}

FunctionRegistry::FunctionRegistry() {
    // Register built-in functions here
    
    // time()
    Register({
        "time",
        {},
        false,
        ValueType::SCALAR,
        [](const std::vector<Value>&, Evaluator* eval) -> Value {
            // Return evaluation timestamp in seconds
            return Value(Scalar{eval->timestamp(), static_cast<double>(eval->timestamp()) / 1000.0});
        }
    });

    RegisterRateFunctions(*this);
    RegisterAggregationFunctions(*this);
    RegisterLabelManipulationFunctions(*this);
    RegisterUtilityFunctions(*this);
    RegisterTrigonometricFunctions(*this);
    RegisterHyperbolicFunctions(*this);
    RegisterOverTimeAggregations(*this);
    RegisterRemainingAggregations(*this);
    RegisterRemainingUtilityFunctions(*this);
    RegisterHistogramFunctions(*this);
    RegisterMathFunctions(*this);
    RegisterTimeFunctions(*this);
}

void FunctionRegistry::Register(const FunctionSignature& signature) {
    functions_[signature.name] = signature;
}

const FunctionSignature* FunctionRegistry::Get(const std::string& name) const {
    auto it = functions_.find(name);
    if (it != functions_.end()) {
        return &it->second;
    }
    return nullptr;
}

void RegisterMathFunctions(FunctionRegistry& registry) {
    registry.Register({"abs", {ValueType::VECTOR}, false, ValueType::VECTOR, [](const std::vector<Value>& args, Evaluator*) -> Value {
        const auto& vec = args[0].getVector();
        Vector result;
        for (const auto& s : vec) {
            result.push_back(Sample{s.metric, s.timestamp, std::abs(s.value)});
        }
        return Value(result);
    }});

    registry.Register({"ceil", {ValueType::VECTOR}, false, ValueType::VECTOR, [](const std::vector<Value>& args, Evaluator*) -> Value {
        const auto& vec = args[0].getVector();
        Vector result;
        for (const auto& s : vec) {
            result.push_back(Sample{s.metric, s.timestamp, std::ceil(s.value)});
        }
        return Value(result);
    }});

    registry.Register({"exp", {ValueType::VECTOR}, false, ValueType::VECTOR, [](const std::vector<Value>& args, Evaluator*) -> Value {
        const auto& vec = args[0].getVector();
        Vector result;
        for (const auto& s : vec) {
            result.push_back(Sample{s.metric, s.timestamp, std::exp(s.value)});
        }
        return Value(result);
    }});
    registry.Register({"floor", {ValueType::VECTOR}, false, ValueType::VECTOR, [](const std::vector<Value>& args, Evaluator*) -> Value {
        const auto& vec = args[0].getVector();
        Vector result;
        for (const auto& s : vec) {
            result.push_back(Sample{s.metric, s.timestamp, std::floor(s.value)});
        }
        return Value(result);
    }});

    registry.Register({"round", {ValueType::VECTOR}, false, ValueType::VECTOR, [](const std::vector<Value>& args, Evaluator*) -> Value {
        const auto& vec = args[0].getVector();
        Vector result;
        for (const auto& s : vec) {
            result.push_back(Sample{s.metric, s.timestamp, std::round(s.value)});
        }
        return Value(result);
    }});

    registry.Register({"sqrt", {ValueType::VECTOR}, false, ValueType::VECTOR, [](const std::vector<Value>& args, Evaluator*) -> Value {
        const auto& vec = args[0].getVector();
        Vector result;
        for (const auto& s : vec) {
            result.push_back(Sample{s.metric, s.timestamp, std::sqrt(s.value)});
        }
        return Value(result);
    }});

    registry.Register({"ln", {ValueType::VECTOR}, false, ValueType::VECTOR, [](const std::vector<Value>& args, Evaluator*) -> Value {
        const auto& vec = args[0].getVector();
        Vector result;
        for (const auto& s : vec) {
            result.push_back(Sample{s.metric, s.timestamp, std::log(s.value)});
        }
        return Value(result);
    }});

    registry.Register({"log2", {ValueType::VECTOR}, false, ValueType::VECTOR, [](const std::vector<Value>& args, Evaluator*) -> Value {
        const auto& vec = args[0].getVector();
        Vector result;
        for (const auto& s : vec) {
            result.push_back(Sample{s.metric, s.timestamp, std::log2(s.value)});
        }
        return Value(result);
    }});

    registry.Register({"log10", {ValueType::VECTOR}, false, ValueType::VECTOR, [](const std::vector<Value>& args, Evaluator*) -> Value {
        const auto& vec = args[0].getVector();
        Vector result;
        for (const auto& s : vec) {
            result.push_back(Sample{s.metric, s.timestamp, std::log10(s.value)});
        }
        return Value(result);
    }});
}

void RegisterTimeFunctions(FunctionRegistry& registry) {
    registry.Register({"year", {ValueType::VECTOR}, false, ValueType::VECTOR, [](const std::vector<Value>& args, Evaluator*) -> Value {
        const auto& vec = args[0].getVector();
        Vector result;
        for (const auto& s : vec) {
            // Convert timestamp (ms) to time_t (s)
            time_t t = static_cast<time_t>(s.value);
            if (args.size() > 0 && args[0].isVector()) {
                // If arg provided, use value as timestamp (seconds)
                // But wait, PromQL time functions usually take an optional vector.
                // If vector provided, use sample value as timestamp.
                // If no arg, use current time? No, usually they take vector.
                // year(v vector)
            }
            struct tm* tm = std::gmtime(&t);
            result.push_back(Sample{s.metric, s.timestamp, static_cast<double>(tm->tm_year + 1900)});
        }
        return Value(result);
    }});

    registry.Register({"hour", {ValueType::VECTOR}, false, ValueType::VECTOR, [](const std::vector<Value>& args, Evaluator*) -> Value {
        const auto& vec = args[0].getVector();
        Vector result;
        for (const auto& s : vec) {
            time_t t = static_cast<time_t>(s.value);
            struct tm* tm = std::gmtime(&t);
            result.push_back(Sample{s.metric, s.timestamp, static_cast<double>(tm->tm_hour)});
        }
        return Value(result);
    }});
    
    registry.Register({"minute", {ValueType::VECTOR}, false, ValueType::VECTOR, [](const std::vector<Value>& args, Evaluator*) -> Value {
        const auto& vec = args[0].getVector();
        Vector result;
        for (const auto& s : vec) {
            time_t t = static_cast<time_t>(s.value);
            struct tm* tm = std::gmtime(&t);
            result.push_back(Sample{s.metric, s.timestamp, static_cast<double>(tm->tm_min)});
        }
        return Value(result);
    }});
    
    registry.Register({"month", {ValueType::VECTOR}, false, ValueType::VECTOR, [](const std::vector<Value>& args, Evaluator*) -> Value {
        const auto& vec = args[0].getVector();
        Vector result;
        for (const auto& s : vec) {
            time_t t = static_cast<time_t>(s.value);
            struct tm* tm = std::gmtime(&t);
            result.push_back(Sample{s.metric, s.timestamp, static_cast<double>(tm->tm_mon + 1)}); // 1-12
        }
        return Value(result);
    }});
    
    registry.Register({"day_of_month", {ValueType::VECTOR}, false, ValueType::VECTOR, [](const std::vector<Value>& args, Evaluator*) -> Value {
        const auto& vec = args[0].getVector();
        Vector result;
        for (const auto& s : vec) {
            time_t t = static_cast<time_t>(s.value);
            struct tm* tm = std::gmtime(&t);
            result.push_back(Sample{s.metric, s.timestamp, static_cast<double>(tm->tm_mday)});
        }
        return Value(result);
    }});
    
    registry.Register({"day_of_week", {ValueType::VECTOR}, false, ValueType::VECTOR, [](const std::vector<Value>& args, Evaluator*) -> Value {
        const auto& vec = args[0].getVector();
        Vector result;
        for (const auto& s : vec) {
            time_t t = static_cast<time_t>(s.value);
            struct tm* tm = std::gmtime(&t);
            result.push_back(Sample{s.metric, s.timestamp, static_cast<double>(tm->tm_wday)}); // 0=Sunday
        }
        return Value(result);
    }});
    
    registry.Register({"days_in_month", {ValueType::VECTOR}, false, ValueType::VECTOR, [](const std::vector<Value>& args, Evaluator*) -> Value {
        const auto& vec = args[0].getVector();
        Vector result;
        for (const auto& s : vec) {
            time_t t = static_cast<time_t>(s.value);
            struct tm* tm = std::gmtime(&t);
            
            // Calculate days in month
            int days;
            int month = tm->tm_mon + 1;
            int year = tm->tm_year + 1900;
            
            if (month == 2) {
                // February: check for leap year
                bool is_leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
                days = is_leap ? 29 : 28;
            } else if (month == 4 || month == 6 || month == 9 || month == 11) {
                days = 30;
            } else {
                days = 31;
            }
            
            result.push_back(Sample{s.metric, s.timestamp, static_cast<double>(days)});
        }
        return Value(result);
    }});
}

} // namespace promql
} // namespace prometheus
} // namespace tsdb
