#pragma once
#include "tsdb/prometheus/promql/ast.h"
#include "tsdb/prometheus/promql/value.h"
#include "tsdb/prometheus/storage/adapter.h"
#include <memory>

namespace tsdb {
namespace prometheus {
namespace promql {

/**
 * @brief Evaluates PromQL AST nodes
 */
class Evaluator {
public:
    Evaluator(int64_t timestamp, int64_t lookback_delta, storage::StorageAdapter* storage);

    Evaluator(int64_t start, int64_t end, int64_t step, int64_t lookback_delta, storage::StorageAdapter* storage);

    Value Evaluate(const ExprNode* node);
    Value EvaluateRange(const ExprNode* node);
    
    int64_t timestamp() const { return timestamp_; }
    int64_t lookback_delta() const { return lookback_delta_; }
    storage::StorageAdapter* storage() const { return storage_; }

private:
    int64_t timestamp_;
    int64_t start_ = 0;
    int64_t end_ = 0;
    int64_t step_ = 0;
    int64_t lookback_delta_;
    storage::StorageAdapter* storage_;

    // Instant evaluation methods
    Value EvaluateAggregate(const AggregateExprNode* node);
    Value EvaluateBinary(const BinaryExprNode* node);
    Value EvaluateCall(const CallNode* node);
    Value EvaluateMatrixSelector(const MatrixSelectorNode* node);
    Value EvaluateNumberLiteral(const NumberLiteralNode* node);
    Value EvaluateParen(const ParenExprNode* node);
    Value EvaluateStringLiteral(const StringLiteralNode* node);
    Value EvaluateSubquery(const SubqueryExprNode* node);
    Value EvaluateUnary(const UnaryExprNode* node);
    Value EvaluateVectorSelector(const VectorSelectorNode* node);

    // Helpers
    Value AggregateVector(const Vector& input, const AggregateExprNode* node);

    // Range evaluation methods (Vectorized)
    Value EvaluateRangeAggregate(const AggregateExprNode* node);
    Value EvaluateRangeBinary(const BinaryExprNode* node);
    Value EvaluateRangeCall(const CallNode* node);
    Value EvaluateRangeMatrixSelector(const MatrixSelectorNode* node);
    Value EvaluateRangeParen(const ParenExprNode* node);
    Value EvaluateRangeSubquery(const SubqueryExprNode* node);
    Value EvaluateRangeUnary(const UnaryExprNode* node);
    Value EvaluateRangeVectorSelector(const VectorSelectorNode* node);
    Value EvaluateRangeLiteral(const ExprNode* node); // Handles Number and String
    Value EvaluateRangeDefault(const ExprNode* node); // Fallback to loop
};

} // namespace promql
} // namespace prometheus
} // namespace tsdb
