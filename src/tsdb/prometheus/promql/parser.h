#ifndef TSDB_PROMETHEUS_PROMQL_PARSER_H_
#define TSDB_PROMETHEUS_PROMQL_PARSER_H_

#include "lexer.h"
#include "ast.h"
#include <memory>
#include <stdexcept>
#include <vector>

namespace tsdb {
namespace prometheus {
namespace promql {

class ParserError : public std::runtime_error {
public:
    ParserError(const std::string& message, int line, int pos)
        : std::runtime_error(message + " at line " + std::to_string(line) + ":" + std::to_string(pos)),
          line_(line), pos_(pos) {}

    int line() const { return line_; }
    int pos() const { return pos_; }
private:
    int line_;
    int pos_;
};

class Parser {
public:
    explicit Parser(Lexer& lexer);

    // Pratt parser helper methods for precedence
    enum class Precedence {
        LOWEST      = 0,
        OR          = 1, // or
        AND_UNLESS  = 2, // and, unless
        COMPARISON  = 3, // ==, !=, <, <=, >, >=
        SUM_SUB     = 4, // +, -
        MUL_DIV_MOD = 5, // *, /, %
        POWER       = 6, // ^
        UNARY       = 7, // - (unary), + (unary)
        CALL_INDEX  = 8  // my_func(), my_metric[5m]
    };

    // Parses the input and returns the root of the AST.
    // Throws ParserError on syntax errors.
    std::unique_ptr<ExprNode> ParseExpr();

    const std::vector<ParserError>& Errors() const { return errors_; }

private:
    Lexer& lexer_;
    Token currentToken_;
    Token peekToken_;

    std::vector<ParserError> errors_;

    void nextToken();
    bool expectPeek(TokenType t);
    void peekError(TokenType t);

    Precedence getPrecedence(TokenType type);

    // Parsing functions for different expression types, following Pratt parsing principles
    std::unique_ptr<ExprNode> parseExpression(Precedence precedence);
    std::unique_ptr<ExprNode> parsePrefixExpression(); // For unary, literals, identifiers, parens, etc.
    std::unique_ptr<ExprNode> parseInfixExpression(std::unique_ptr<ExprNode> left);

    // Helper to check if we're looking at a subquery range
    bool isSubqueryRange();
    
    std::unique_ptr<NumberLiteralNode> parseNumberLiteral();
    std::unique_ptr<StringLiteralNode> parseStringLiteral();
    std::unique_ptr<VectorSelectorNode> parseVectorSelector();
    // MatrixSelector is often parsed as part of a VectorSelector or a specific context
    std::unique_ptr<MatrixSelectorNode> parseMatrixSelector(std::unique_ptr<VectorSelectorNode> vecSel);
    std::unique_ptr<ExprNode> parseIdentifierOrFunctionCall(); // Handles metric names or function calls
    std::unique_ptr<CallNode> parseCallExpression(std::string funcName);
    std::unique_ptr<AggregateExprNode> parseAggregateExpression();
    std::unique_ptr<ParenExprNode> parseParenExpression();
    std::unique_ptr<UnaryExprNode> parseUnaryExpression();
    std::unique_ptr<SubqueryExprNode> parseSubqueryExpression(std::unique_ptr<ExprNode> expr); 

    // Helper to parse label matchers, e.g. {foo="bar", baz!~"qux"}
    std::vector<model::LabelMatcher> parseLabelMatchers();
    // Helper to parse argument list for functions or aggregations
    std::vector<std::unique_ptr<ExprNode>> parseExpressionList(TokenType endToken);
    std::vector<std::string> parseGroupingLabels(); // For by/without

    // Helper to parse duration string (e.g., "5m") into seconds
    // Might throw if format is invalid
    int64_t parseDuration(const std::string& durationStr);

    // Helper to parse @ and offset modifiers
    void parseOptionalAtOffset(VectorSelectorNode& vsNode);
    void parseOptionalAtOffsetSubquery(SubqueryExprNode& sqNode);
};

} // namespace promql
} // namespace prometheus
} // namespace tsdb

#endif // TSDB_PROMETHEUS_PROMQL_PARSER_H_ 