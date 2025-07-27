#ifndef TSDB_PROMETHEUS_PROMQL_AST_H_
#define TSDB_PROMETHEUS_PROMQL_AST_H_

#include "lexer.h" // For TokenType and Token
#include "../model/types.h" // For potential model types like LabelMatcher
#include <string>
#include <vector>
#include <memory>
#include <variant>

namespace tsdb {
namespace prometheus {
namespace promql {

// Forward declarations for node types
struct ExprNode;
struct NumberLiteralNode;
struct StringLiteralNode;
struct VectorSelectorNode;
struct MatrixSelectorNode;
struct BinaryExprNode;
struct UnaryExprNode;
struct ParenExprNode;
struct CallNode;
struct AggregateExprNode;
struct SubqueryExprNode; // For subqueries like <expr>[<duration>:<resolution>]

// Base interface for all AST expression nodes
struct ExprNode {
    virtual ~ExprNode() = default;
    // For type identification or visitor pattern, if needed later
    // virtual void accept(ASTVisitor& visitor) = 0;
    virtual std::string String() const = 0; // For debugging and representation
    // TODO: Add position information (start/end tokens or line/pos)
};

// Represents a numeric literal
struct NumberLiteralNode : ExprNode {
    double value;
    explicit NumberLiteralNode(double val) : value(val) {}
    std::string String() const override;
};

// Represents a string literal
struct StringLiteralNode : ExprNode {
    std::string value;
    explicit StringLiteralNode(std::string val) : value(std::move(val)) {}
    std::string String() const override;
};

// Represents a vector selector, e.g., http_requests_total{method="GET"}
struct VectorSelectorNode : ExprNode {
    std::string name; // Metric name, can be empty for selectors like {job="api"}
    std::vector<model::LabelMatcher> labelMatchers;
    // For offset and @ modifier
    Token originalOffset; // The original offset literal if present, e.g., "5m"
    Token atModifier;     // The original @ timestamp or function like start(), end()
    int64_t parsedOffsetSeconds; // Offset in seconds, 0 if not present
    // parsedAt a bit more complex, might be timestamp or special func

    VectorSelectorNode(std::string n, std::vector<model::LabelMatcher> matchers)
        : name(std::move(n)), labelMatchers(std::move(matchers)), 
          originalOffset(TokenType::ILLEGAL, "", 0, 0), 
          atModifier(TokenType::ILLEGAL, "", 0, 0), 
          parsedOffsetSeconds(0) {}
    std::string String() const override;
};

// Represents a matrix selector, e.g., http_requests_total[5m]
struct MatrixSelectorNode : ExprNode {
    std::unique_ptr<VectorSelectorNode> vectorSelector; // The underlying vector selector
    Token range; // The original duration literal, e.g., "5m"
    int64_t parsedRangeSeconds; // Range in seconds

    MatrixSelectorNode(std::unique_ptr<VectorSelectorNode> vecSel, Token r, int64_t pRange)
        : vectorSelector(std::move(vecSel)), range(std::move(r)), parsedRangeSeconds(pRange) {}
    std::string String() const override;
};

// Represents a binary expression, e.g., lhs + rhs
struct BinaryExprNode : ExprNode {
    TokenType op; // The operator (e.g., ADD, SUB, EQL, AND, etc.)
    std::unique_ptr<ExprNode> lhs;
    std::unique_ptr<ExprNode> rhs;
    // For vector matching (ON, IGNORING, GROUP_LEFT, GROUP_RIGHT)
    std::vector<std::string> matchingLabels; // For ON or IGNORING
    bool on; // True if ON, false if IGNORING. Irrelevant if matchingLabels is empty.
    std::string groupSide; // "left" or "right" for GROUP_LEFT/GROUP_RIGHT
    std::vector<std::string> includeLabels; // For group_left/right include param


    BinaryExprNode(TokenType o, std::unique_ptr<ExprNode> l, std::unique_ptr<ExprNode> r)
        : op(o), lhs(std::move(l)), rhs(std::move(r)), on(false) {}
    std::string String() const override;
};

// Represents a unary expression, e.g., -expr
struct UnaryExprNode : ExprNode {
    TokenType op; // Typically SUB for negation
    std::unique_ptr<ExprNode> expr;

    UnaryExprNode(TokenType o, std::unique_ptr<ExprNode> e)
        : op(o), expr(std::move(e)) {}
    std::string String() const override;
};

// Represents a parenthesized expression, e.g., (expr)
struct ParenExprNode : ExprNode {
    std::unique_ptr<ExprNode> expr;

    explicit ParenExprNode(std::unique_ptr<ExprNode> e)
        : expr(std::move(e)) {}
    std::string String() const override;
};

// Represents a function call, e.g., rate(metric[5m])
struct CallNode : ExprNode {
    std::string funcName;
    std::vector<std::unique_ptr<ExprNode>> args;

    CallNode(std::string name, std::vector<std::unique_ptr<ExprNode>> a)
        : funcName(std::move(name)), args(std::move(a)) {}
    std::string String() const override;
};

// Represents an aggregation expression, e.g., sum by (job) (metric)
struct AggregateExprNode : ExprNode {
    TokenType op; // Aggregation operator (e.g., SUM, AVG)
    std::unique_ptr<ExprNode> expr; // The expression to aggregate
    // Grouping (BY or WITHOUT)
    std::vector<std::string> groupingLabels;
    bool without; // True if WITHOUT, false if BY. Irrelevant if groupingLabels is empty.
    // Parameter for aggregators like topk, count_values, quantile (optional)
    std::unique_ptr<ExprNode> param; 

    AggregateExprNode(TokenType o, std::unique_ptr<ExprNode> e, std::vector<std::string> grp, bool w)
        : op(o), expr(std::move(e)), groupingLabels(std::move(grp)), without(w), param(nullptr) {}
    std::string String() const override;
};

// Represents a subquery expression e.g. up[1h:5m]
// <metric_expr>[<duration>:<duration_or_nothing>]
// or <metric_expr>[<duration>:<duration_or_nothing>] @ <timestamp>
// or <metric_expr>[<duration>:<duration_or_nothing>] offset <duration>
struct SubqueryExprNode : ExprNode {
    std::unique_ptr<ExprNode> expr;
    Token range;      // Original range duration string, e.g., "1h"
    Token resolution; // Original resolution string, e.g., "5m", can be empty
    Token originalOffset;
    Token atModifier;

    int64_t parsedRangeSeconds;
    int64_t parsedResolutionSeconds; // 0 if not specified
    int64_t parsedOffsetSeconds;

    SubqueryExprNode(std::unique_ptr<ExprNode> e, Token rng, Token res, Token off, Token at)
        : expr(std::move(e)), range(std::move(rng)), resolution(std::move(res)), 
          originalOffset(std::move(off)), atModifier(std::move(at)),
          parsedRangeSeconds(0), parsedResolutionSeconds(0), parsedOffsetSeconds(0) {}
    std::string String() const override;
};

// TODO: Add other node types as identified by the PromQL grammar (e.g., AtModifierNode, OffsetNode if they need to be separate)
// For now, @ and offset are embedded in VectorSelectorNode and SubqueryExprNode

} // namespace promql
} // namespace prometheus
} // namespace tsdb

#endif // TSDB_PROMETHEUS_PROMQL_AST_H_ 