#pragma once

#include <string>
#include <vector>
#include <functional>
#include <map>
#include <memory>
#include "tsdb/prometheus/promql/value.h"
#include "tsdb/prometheus/promql/ast.h"

namespace tsdb {
namespace prometheus {
namespace promql {

// Forward declaration
class Evaluator;

/**
 * @brief Function signature for PromQL functions
 */
using FunctionImpl = std::function<Value(const std::vector<Value>& args, Evaluator* evaluator)>;

/**
 * @brief Metadata for a PromQL function
 */
struct FunctionSignature {
    std::string name;
    std::vector<ValueType> arg_types;
    bool variadic{false};
    ValueType return_type;
    FunctionImpl implementation;
};

/**
 * @brief Registry for PromQL functions
 */
class FunctionRegistry {
public:
    static FunctionRegistry& Instance();

    void Register(const FunctionSignature& signature);
    const FunctionSignature* Get(const std::string& name) const;

private:
    FunctionRegistry();
    std::map<std::string, FunctionSignature> functions_;
};

// Registration helpers
void RegisterRateFunctions(FunctionRegistry& registry);
void RegisterMathFunctions(FunctionRegistry& registry);
void RegisterTimeFunctions(FunctionRegistry& registry);
void RegisterExtrapolationFunctions(FunctionRegistry& registry);
void RegisterAggregationFunctions(FunctionRegistry& registry);
void RegisterLabelManipulationFunctions(FunctionRegistry& registry);
void RegisterUtilityFunctions(FunctionRegistry& registry);
void RegisterTrigonometricFunctions(FunctionRegistry& registry);
void RegisterHyperbolicFunctions(FunctionRegistry& registry);
void RegisterOverTimeAggregations(FunctionRegistry& registry);
void RegisterRemainingAggregations(FunctionRegistry& registry);
void RegisterRemainingUtilityFunctions(FunctionRegistry& registry);

} // namespace promql
} // namespace prometheus
} // namespace tsdb
