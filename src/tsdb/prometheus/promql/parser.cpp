#include "parser.h"
#include <stdexcept> // For std::invalid_argument, std::stod, std::stoll
#include <unordered_map>

namespace tsdb {
namespace prometheus {
namespace promql {

// Helper to map TokenType to Precedence
static const std::unordered_map<TokenType, Parser::Precedence> precedenceMap = {
    {TokenType::OR, Parser::Precedence::OR},
    {TokenType::AND, Parser::Precedence::AND_UNLESS},
    {TokenType::UNLESS, Parser::Precedence::AND_UNLESS},
    {TokenType::EQL, Parser::Precedence::COMPARISON},
    {TokenType::NEQ, Parser::Precedence::COMPARISON},
    {TokenType::LTE, Parser::Precedence::COMPARISON},
    {TokenType::LSS, Parser::Precedence::COMPARISON},
    {TokenType::GTE, Parser::Precedence::COMPARISON},
    {TokenType::GTR, Parser::Precedence::COMPARISON},
    {TokenType::EQL_REGEX, Parser::Precedence::COMPARISON},
    {TokenType::NEQ_REGEX, Parser::Precedence::COMPARISON},
    {TokenType::ADD, Parser::Precedence::SUM_SUB},
    {TokenType::SUB, Parser::Precedence::SUM_SUB},
    {TokenType::MUL, Parser::Precedence::MUL_DIV_MOD},
    {TokenType::DIV, Parser::Precedence::MUL_DIV_MOD},
    {TokenType::MOD, Parser::Precedence::MUL_DIV_MOD},
    {TokenType::POW, Parser::Precedence::POWER},
    {TokenType::LEFT_PAREN, Parser::Precedence::CALL_INDEX}, // For function calls
    {TokenType::LEFT_BRACKET, Parser::Precedence::CALL_INDEX} // For matrix selectors / subqueries
};

Parser::Parser(Lexer& lexer)
    : lexer_(lexer),
      currentToken_(lexer_.NextToken()),
      peekToken_(lexer_.NextToken()) {}

void Parser::nextToken() {
    currentToken_ = peekToken_;
    peekToken_ = lexer_.NextToken();
}

bool Parser::expectPeek(TokenType t) {
    if (peekToken_.type == t) {
        nextToken();
        return true;
    }
    peekError(t);
    return false;
}

void Parser::peekError(TokenType t) {
    std::string msg = "Expected next token to be " + TokenTypeToString(t) +
                      ", got " + TokenTypeToString(peekToken_.type) + " instead";
    errors_.emplace_back(msg, peekToken_.line, peekToken_.pos);
    // No throw here, allow parser to potentially recover or collect more errors
}

Parser::Precedence Parser::getPrecedence(TokenType type) {
    if (precedenceMap.count(type)) {
        return precedenceMap.at(type);
    }
    return Precedence::LOWEST;
}

std::unique_ptr<ExprNode> Parser::ParseExpr() {
    auto expr = parseExpression(Precedence::LOWEST);
    // Check for any remaining tokens that are not EOF, indicating an incomplete parse or trailing garbage
    if (currentToken_.type != TokenType::EOF_TOKEN && expr) {
        // If we parsed something but didn't reach EOF, it might be an error unless it's a specific context
        // For a general expression, we expect EOF
        // errors_.emplace_back("Unexpected token " + currentToken_.literal + " after expression", currentToken_.line, currentToken_.pos);
        // return nullptr; // Or let it be, depending on how strict we want to be at this stage
    } else if (!expr && errors_.empty()) {
        // No expression parsed and no errors recorded yet (e.g. empty input)
        errors_.emplace_back("No expression found", currentToken_.line, currentToken_.pos);
    }
    return expr;
}

std::unique_ptr<ExprNode> Parser::parseExpression(Precedence precedence) {
    auto prefixParseFn = [this]() { return this->parsePrefixExpression(); };
    std::unique_ptr<ExprNode> leftExpr = prefixParseFn();

    if (!leftExpr) {
        return nullptr; // Error already logged by prefixParseFn or no token to parse
    }

    while (currentToken_.type != TokenType::EOF_TOKEN && precedence < getPrecedence(currentToken_.type)) {
        // currentToken_ is now the infix operator
        auto infixParseFn = [this, &leftExpr]() { return this->parseInfixExpression(std::move(leftExpr)); };
        std::unique_ptr<ExprNode> newLeftExpr = infixParseFn();
        if (!newLeftExpr) {
            return nullptr; // Error in parsing infix expression
        }
        leftExpr = std::move(newLeftExpr);
    }
    return leftExpr;
}

// Simplified stubs for now, will be expanded
std::unique_ptr<ExprNode> Parser::parsePrefixExpression() {
    // TODO: Implement logic for various prefix tokens (literals, identifiers, unary ops, parens, aggregations)
    // This is a very simplified placeholder.
    switch (currentToken_.type) {
        case TokenType::NUMBER:
            return parseNumberLiteral();
        case TokenType::STRING:
            return parseStringLiteral();
        case TokenType::IDENTIFIER:
            // If peek is '(', it's a function call.
            // PromQL function names are identifiers.
            if (peekToken_.type == TokenType::LEFT_PAREN) {
                 // The identifier is a function name
                 std::string funcName = currentToken_.literal;
                 nextToken(); // Consume identifier (func name)
                 return parseCallExpression(funcName);
            }
            // Otherwise, assume it's a vector selector (metric name or just labels)
            return parseVectorSelector();
        case TokenType::LEFT_BRACE: // Vector selector without metric name: {label="value"}
            return parseVectorSelector();
        case TokenType::LEFT_PAREN:
            return parseParenExpression();
        case TokenType::SUB: // Unary minus
        case TokenType::ADD: // Unary plus (often ignored but syntactically valid)
            return parseUnaryExpression();
        // Handle aggregation keywords that act as prefix operators
        case TokenType::SUM:
        case TokenType::AVG:
        case TokenType::COUNT:
        case TokenType::MIN:
        case TokenType::MAX:
        case TokenType::STDDEV:
        case TokenType::STDVAR:
        case TokenType::TOPK:
        case TokenType::BOTTOMK:
        case TokenType::COUNT_VALUES:
        case TokenType::QUANTILE:
            return parseAggregateExpression();
        default:
            errors_.emplace_back("Unexpected token " + currentToken_.literal + " ('" + TokenTypeToString(currentToken_.type) + "') at start of expression", currentToken_.line, currentToken_.pos);
            return nullptr;
    }
}

// Helper to check if a subquery is coming up
bool Parser::isSubqueryRange() {
    // currentToken_ should be LEFT_BRACKET
    if (currentToken_.type != TokenType::LEFT_BRACKET) return false;
    
    // Save current parser state
    size_t position = lexer_.GetPosition();
    Token currentToken = currentToken_;
    Token peekToken = peekToken_;
    
    // Look ahead to see if there's a colon after a duration
    nextToken(); // Consume '['
    if (currentToken_.type != TokenType::DURATION) {
        // Restore state and return false
        lexer_.SetPosition(position);
        currentToken_ = currentToken;
        peekToken_ = peekToken;
        return false;
    }
    
    nextToken(); // Consume DURATION
    bool isSubquery = (currentToken_.type == TokenType::COLON);
    
    // Restore parser state
    lexer_.SetPosition(position);
    currentToken_ = currentToken;
    peekToken_ = peekToken;
    
    return isSubquery;
}

std::unique_ptr<ExprNode> Parser::parseInfixExpression(std::unique_ptr<ExprNode> left) {
    TokenType operatorToken = currentToken_.type;
    
    // Handle matrix selector special case - when we see a '['
    if (operatorToken == TokenType::LEFT_BRACKET) {
        // Check if this is a subquery (with colon) or a matrix selector
        if (isSubqueryRange()) {
            return parseSubqueryExpression(std::move(left));
        }
        
        // For matrix selectors, the left expression should be a VectorSelectorNode
        auto vecSel = dynamic_cast<VectorSelectorNode*>(left.get());
        if (!vecSel) {
            errors_.emplace_back("Expected vector selector before '[' for matrix selector", 
                                currentToken_.line, currentToken_.pos);
            return nullptr;
        }
        
        // Create a copy of the VectorSelectorNode
        auto vecSelCopy = std::make_unique<VectorSelectorNode>(vecSel->name, vecSel->labelMatchers);
        vecSelCopy->originalOffset = vecSel->originalOffset;
        vecSelCopy->atModifier = vecSel->atModifier;
        vecSelCopy->parsedOffsetSeconds = vecSel->parsedOffsetSeconds;
        
        // Parse the matrix selector using the copy
        return parseMatrixSelector(std::move(vecSelCopy));
    }
    
    // Handle normal binary expressions
    Precedence currentPrecedence = getPrecedence(operatorToken);
    nextToken(); // Consume the operator

    bool returnBool = false;
    // Check for "bool" modifier (only for comparison operators)
    if (currentToken_.type == TokenType::BOOL) {
        if (operatorToken >= TokenType::EQL && operatorToken <= TokenType::GTR) { // Assuming EQL..GTR are comparisons
            returnBool = true;
            nextToken(); // Consume "bool"
        } else {
            errors_.emplace_back("'bool' modifier can only be used with comparison operators", currentToken_.line, currentToken_.pos);
            return nullptr;
        }
    }

    // Check for vector matching (on/ignoring)
    std::vector<std::string> matchingLabels;
    bool on = false;
    bool hasMatching = false;

    if (currentToken_.type == TokenType::ON || currentToken_.type == TokenType::IGNORING) {
        hasMatching = true;
        on = (currentToken_.type == TokenType::ON);
        nextToken(); // Consume ON/IGNORING
        
        if (currentToken_.type != TokenType::LEFT_PAREN) {
            errors_.emplace_back("Expected '(' after on/ignoring", currentToken_.line, currentToken_.pos);
            return nullptr;
        }
        matchingLabels = parseGroupingLabels(); // Re-use parseGroupingLabels as it parses (label, list)
    }

    // Check for group_left/group_right
    std::string groupSide;
    std::vector<std::string> includeLabels;
    
    if (currentToken_.type == TokenType::GROUP_LEFT || currentToken_.type == TokenType::GROUP_RIGHT) {
        if (currentToken_.type == TokenType::GROUP_LEFT) groupSide = "left";
        else groupSide = "right";
        
        nextToken(); // Consume GROUP_LEFT/RIGHT
        
        if (currentToken_.type == TokenType::LEFT_PAREN) {
            includeLabels = parseGroupingLabels();
        }
    }

    auto rightExpr = parseExpression(currentPrecedence);
    if (!rightExpr) {
        // Error already logged by parseExpression
        return nullptr;
    }
    
    auto binaryExpr = std::make_unique<BinaryExprNode>(operatorToken, std::move(left), std::move(rightExpr));
    binaryExpr->returnBool = returnBool;
    if (hasMatching) {
        binaryExpr->matchingLabels = std::move(matchingLabels);
        binaryExpr->on = on;
    }
    if (!groupSide.empty()) {
        binaryExpr->groupSide = groupSide;
        binaryExpr->includeLabels = std::move(includeLabels);
    }

    return binaryExpr;
}

std::unique_ptr<NumberLiteralNode> Parser::parseNumberLiteral() {
    try {
        double val = std::stod(currentToken_.literal);
        auto node = std::make_unique<NumberLiteralNode>(val);
        nextToken(); // Consume number literal
        return node;
    } catch (const std::logic_error& e) {
        errors_.emplace_back("Invalid number: " + currentToken_.literal, currentToken_.line, currentToken_.pos);
        return nullptr;
    }
}

std::unique_ptr<StringLiteralNode> Parser::parseStringLiteral() {
    auto node = std::make_unique<StringLiteralNode>(currentToken_.literal);
    nextToken(); // Consume string literal
    return node;
}

std::unique_ptr<VectorSelectorNode> Parser::parseVectorSelector() {
    std::string metricName;
    if (currentToken_.type == TokenType::IDENTIFIER) {
        metricName = currentToken_.literal;
        nextToken(); // Consume metric name
    }

    std::vector<model::LabelMatcher> matchers;
    if (currentToken_.type == TokenType::LEFT_BRACE) {
        matchers = parseLabelMatchers(); // This will consume up to RIGHT_BRACE
    } else if (!metricName.empty()) {
        // Metric name without label matchers is a valid vector selector
    } else {
        // If no metric name and no '{', it's not a valid start for a vector selector here.
        // This case should ideally be handled by the caller or parsePrefixExpression logic.
        errors_.emplace_back("Expected label matchers or metric name for vector selector", currentToken_.line, currentToken_.pos);
        return nullptr;
    }
    
    auto vsNode = std::make_unique<VectorSelectorNode>(metricName, std::move(matchers));

    // Optional: Parse matrix selector part [duration] or subquery part [<duration>:<resolution>]
    // This is handled by checking peekToken_ for LEFT_BRACKET in parseExpression or specific contexts
    // For now, just parse optional @ and offset for the vector selector itself.
    parseOptionalAtOffset(*vsNode);

    return vsNode;
}

std::unique_ptr<MatrixSelectorNode> Parser::parseMatrixSelector(std::unique_ptr<VectorSelectorNode> vecSel) {
    if (!vecSel) return nullptr;
    // currentToken_ should be LEFT_BRACKET
    if (currentToken_.type != TokenType::LEFT_BRACKET) {
        errors_.emplace_back("Expected '[' for matrix selector", currentToken_.line, currentToken_.pos);
        return nullptr;
    }
    nextToken(); // Consume '['

    if (currentToken_.type != TokenType::DURATION) {
        errors_.emplace_back("Expected duration in matrix selector", currentToken_.line, currentToken_.pos);
        return nullptr;
    }
    Token durationToken = currentToken_;
    nextToken(); // Consume DURATION

    if (currentToken_.type != TokenType::RIGHT_BRACKET) {
        errors_.emplace_back("Expected ']' after duration in matrix selector", currentToken_.line, currentToken_.pos);
        return nullptr;
    }
    nextToken(); // Consume ']'

    int64_t rangeSeconds = 0;
    try {
        rangeSeconds = parseDuration(durationToken.literal);
    } catch (const std::runtime_error& e) {
        errors_.emplace_back(e.what(), durationToken.line, durationToken.pos);
        return nullptr;
    }

    // Matrix selector can also have offset and @
    // Prometheus parser structure suggests these are attached to the *vector selector* part
    // which is then wrapped by the MatrixSelector. Our AST `MatrixSelectorNode` embeds a `VectorSelectorNode`.
    // So, if `vecSel` already had its offset/@ parsed, we don't re-parse here.
    // However, Prometheus grammar seems to allow `metric[5m] offset 1m @ 1234`.
    // Let's assume offset/@ for matrix selector is parsed after the `]`
    // For simplicity for now, the ast.h MatrixSelectorNode does not have its own offset/at fields.
    // It would re-use the ones in the embedded VectorSelectorNode if the grammar implies they apply to the whole matrix.
    // This needs careful check against PromQL grammar for `metric[5m] offset X` vs `(metric offset X)[5m]`

    return std::make_unique<MatrixSelectorNode>(std::move(vecSel), durationToken, rangeSeconds);
}

std::unique_ptr<ExprNode> Parser::parseIdentifierOrFunctionCall() {
    std::string identName = currentToken_.literal;
    nextToken(); // Consume identifier

    if (currentToken_.type != TokenType::LEFT_PAREN) {
        // It's not a function call, must be a vector selector (metric name)
        // We need to backtrack the token we just consumed (the identifier)
        // This indicates a need for more lookahead or a different parsing strategy for this part.
        // For now, this function will assume it *is* a function call if called.
        // The caller (parsePrefixExpression) should make this decision.
        // This function could be simplified to just parseCallExpression assuming identName is the func.
        errors_.emplace_back("Expected '(' for function call after identifier " + identName, currentToken_.line, currentToken_.pos);
        return nullptr;
    }
    return parseCallExpression(identName);
}

std::unique_ptr<CallNode> Parser::parseCallExpression(std::string funcName) {
    // Current token is LEFT_PAREN
    nextToken(); // Consume '('
    auto args = parseExpressionList(TokenType::RIGHT_PAREN);
    if (currentToken_.type != TokenType::RIGHT_PAREN) {
        errors_.emplace_back("Expected ')' to close function call", currentToken_.line, currentToken_.pos);
        return nullptr;
    }
    nextToken(); // Consume ')' 

    return std::make_unique<CallNode>(std::move(funcName), std::move(args));
}

std::unique_ptr<AggregateExprNode> Parser::parseAggregateExpression() {
    TokenType aggOp = currentToken_.type;
    std::string aggName = currentToken_.literal;
    nextToken(); // Consume aggregation keyword

    std::vector<std::string> groupingLabels;
    bool without = false;
    if (currentToken_.type == TokenType::BY || currentToken_.type == TokenType::WITHOUT) {
        without = (currentToken_.type == TokenType::WITHOUT);
        nextToken(); // Consume 'by' or 'without'
        if (currentToken_.type != TokenType::LEFT_PAREN) {
            errors_.emplace_back("Expected '(' after by/without", currentToken_.line, currentToken_.pos);
            return nullptr;
        }
        groupingLabels = parseGroupingLabels(); // Consumes up to and including ')'
    }

    if (currentToken_.type != TokenType::LEFT_PAREN) {
        errors_.emplace_back("Expected '(' for aggregation expression arguments", currentToken_.line, currentToken_.pos);
        return nullptr;
    }
    nextToken(); // Consume '('

    std::unique_ptr<ExprNode> param = nullptr;
    std::unique_ptr<ExprNode> expr = nullptr;

    // Some aggregators like topk, quantile take a parameter first
    // TODO: Make this list accurate based on PromQL
    if (aggOp == TokenType::TOPK || aggOp == TokenType::BOTTOMK || aggOp == TokenType::QUANTILE || aggOp == TokenType::COUNT_VALUES) {
        param = parseExpression(Precedence::LOWEST); // Parse the parameter
        if (!param) return nullptr;
        if (currentToken_.type != TokenType::COMMA) {
            errors_.emplace_back("Expected ',' after aggregation parameter", currentToken_.line, currentToken_.pos);
            return nullptr;
        }
        nextToken(); // Consume ','
    }

    expr = parseExpression(Precedence::LOWEST); // Parse the main expression to aggregate
    if (!expr) return nullptr;

    if (currentToken_.type != TokenType::RIGHT_PAREN) {
        errors_.emplace_back("Expected ')' to close aggregation arguments", currentToken_.line, currentToken_.pos);
        return nullptr;
    }
    nextToken(); // Consume ')'

    // Check for suffix "by" or "without" clause if not already parsed
    if (groupingLabels.empty() && !without) {
        if (currentToken_.type == TokenType::BY || currentToken_.type == TokenType::WITHOUT) {
            without = (currentToken_.type == TokenType::WITHOUT);
            nextToken(); // Consume 'by' or 'without'
            if (currentToken_.type != TokenType::LEFT_PAREN) {
                errors_.emplace_back("Expected '(' after by/without", currentToken_.line, currentToken_.pos);
                return nullptr;
            }
            groupingLabels = parseGroupingLabels(); // Consumes up to and including ')'
        }
    }

    auto aggNode = std::make_unique<AggregateExprNode>(aggOp, std::move(expr), std::move(groupingLabels), without);
    aggNode->param = std::move(param);
    return aggNode;
}

std::unique_ptr<ParenExprNode> Parser::parseParenExpression() {
    nextToken(); // Consume '('
    auto expr = parseExpression(Precedence::LOWEST);
    if (!expr) return nullptr;

    if (currentToken_.type != TokenType::RIGHT_PAREN) {
        errors_.emplace_back("Expected ')' to close parenthesized expression", currentToken_.line, currentToken_.pos);
        return nullptr;
    }
    nextToken(); // Consume ')'

    return std::make_unique<ParenExprNode>(std::move(expr));
}

std::unique_ptr<UnaryExprNode> Parser::parseUnaryExpression() {
    TokenType op = currentToken_.type;
    nextToken(); // Consume operator like '-'
    auto expr = parseExpression(Precedence::UNARY); // Parse operand with higher precedence
    if (!expr) return nullptr;
    return std::make_unique<UnaryExprNode>(op, std::move(expr));
}

std::unique_ptr<SubqueryExprNode> Parser::parseSubqueryExpression([[maybe_unused]] std::unique_ptr<ExprNode> expr) {
    // Basic implementation for subquery expression
    if (!expr) return nullptr;
    
    // currentToken_ should be LEFT_BRACKET
    if (currentToken_.type != TokenType::LEFT_BRACKET) {
        errors_.emplace_back("Expected '[' for subquery", currentToken_.line, currentToken_.pos);
        return nullptr;
    }
    nextToken(); // Consume '['
    
    if (currentToken_.type != TokenType::DURATION) {
        errors_.emplace_back("Expected range duration in subquery", currentToken_.line, currentToken_.pos);
        return nullptr;
    }
    Token rangeToken = currentToken_;
    nextToken(); // Consume DURATION
    
    if (currentToken_.type != TokenType::COLON) {
        errors_.emplace_back("Expected ':' after range in subquery", currentToken_.line, currentToken_.pos);
        return nullptr;
    }
    nextToken(); // Consume ':'
    
    Token resolutionToken(TokenType::ILLEGAL, "", 0, 0);
    if (currentToken_.type == TokenType::DURATION) {
        resolutionToken = currentToken_;
        nextToken(); // Consume optional resolution
    }
    
    if (currentToken_.type != TokenType::RIGHT_BRACKET) {
        errors_.emplace_back("Expected ']' after subquery", currentToken_.line, currentToken_.pos);
        return nullptr;
    }
    nextToken(); // Consume ']'
    
    // Default values for offset and at
    Token offsetToken(TokenType::ILLEGAL, "", 0, 0);
    Token atToken(TokenType::ILLEGAL, "", 0, 0);
    
    auto sqNode = std::make_unique<SubqueryExprNode>(
        std::move(expr), 
        rangeToken, 
        resolutionToken, 
        offsetToken, 
        atToken
    );
    
    // Parse range seconds
    try {
        sqNode->parsedRangeSeconds = parseDuration(rangeToken.literal);
        if (resolutionToken.type != TokenType::ILLEGAL) {
            sqNode->parsedResolutionSeconds = parseDuration(resolutionToken.literal);
        }
    } catch (const std::runtime_error& e) {
        errors_.emplace_back(e.what(), rangeToken.line, rangeToken.pos);
        return nullptr;
    }
    
    // Parse optional offset and @ modifiers
    parseOptionalAtOffsetSubquery(*sqNode);
    
    return sqNode;
}

std::vector<model::LabelMatcher> Parser::parseLabelMatchers() {
    // currentToken_ is LEFT_BRACE
    nextToken(); // Consume '{'
    std::vector<model::LabelMatcher> matchers;

    if (currentToken_.type == TokenType::RIGHT_BRACE) {
        nextToken(); // Consume '}' for empty matchers
        return matchers;
    }

    while (true) {
        if (currentToken_.type != TokenType::IDENTIFIER) {
            errors_.emplace_back("Expected label name in matcher", currentToken_.line, currentToken_.pos);
            return {}; // Return empty, error logged
        }
        std::string name = currentToken_.literal;
        nextToken(); // Consume label name

        TokenType matcherOp;
        switch (currentToken_.type) {
            case TokenType::ASSIGN: matcherOp = TokenType::ASSIGN; break; // Will map to EQUAL
            case TokenType::EQL:    matcherOp = TokenType::EQL; break;
            case TokenType::NEQ:    matcherOp = TokenType::NEQ; break;
            case TokenType::EQL_REGEX: matcherOp = TokenType::EQL_REGEX; break;
            case TokenType::NEQ_REGEX: matcherOp = TokenType::NEQ_REGEX; break;
            default:
                errors_.emplace_back("Expected matcher operator (=, !=, =~, !~) after label name", currentToken_.line, currentToken_.pos);
                return {};
        }
        nextToken(); // Consume matcher operator

        if (currentToken_.type != TokenType::STRING) {
            errors_.emplace_back("Expected string value for label matcher", currentToken_.line, currentToken_.pos);
            return {};
        }
        std::string value = currentToken_.literal;
        nextToken(); // Consume string value

        model::MatcherType mt;
        if (matcherOp == TokenType::ASSIGN || matcherOp == TokenType::EQL) mt = model::MatcherType::EQUAL;
        else if (matcherOp == TokenType::NEQ) mt = model::MatcherType::NOT_EQUAL;
        else if (matcherOp == TokenType::EQL_REGEX) mt = model::MatcherType::REGEX_MATCH;
        else mt = model::MatcherType::REGEX_NO_MATCH; // NEQ_REGEX
        
        matchers.emplace_back(mt, name, value);

        if (currentToken_.type == TokenType::RIGHT_BRACE) {
            nextToken(); // Consume '}'
            break;
        }
        if (currentToken_.type != TokenType::COMMA) {
            errors_.emplace_back("Expected ',' or '}' in label matchers", currentToken_.line, currentToken_.pos);
            return {};
        }
        nextToken(); // Consume ','
    }
    return matchers;
}

std::vector<std::unique_ptr<ExprNode>> Parser::parseExpressionList(TokenType endToken) {
    std::vector<std::unique_ptr<ExprNode>> list;
    if (currentToken_.type == endToken) { // Empty list
        return list;
    }

    auto firstExpr = parseExpression(Precedence::LOWEST);
    if (!firstExpr) return {}; // Error already logged
    list.push_back(std::move(firstExpr));

    while (currentToken_.type == TokenType::COMMA) {
        nextToken(); // Consume ','
        auto nextExpr = parseExpression(Precedence::LOWEST);
        if (!nextExpr) return {}; // Error already logged
        list.push_back(std::move(nextExpr));
    }
    // Caller is responsible for checking and consuming the endToken
    return list;
}

std::vector<std::string> Parser::parseGroupingLabels() {
    // currentToken_ is LEFT_PAREN
    nextToken(); // Consume '('
    std::vector<std::string> labels;
    if (currentToken_.type == TokenType::RIGHT_PAREN) { // Empty list
        nextToken(); // Consume ')'
        return labels;
    }

    while (true) {
        if (currentToken_.type != TokenType::IDENTIFIER) {
            errors_.emplace_back("Expected label name in grouping", currentToken_.line, currentToken_.pos);
            return {};
        }
        labels.push_back(currentToken_.literal);
        nextToken(); // Consume identifier

        if (currentToken_.type == TokenType::RIGHT_PAREN) {
            nextToken(); // Consume ')'
            break;
        }
        if (currentToken_.type != TokenType::COMMA) {
            errors_.emplace_back("Expected ',' or ')' in grouping labels", currentToken_.line, currentToken_.pos);
            return {};
        }
        nextToken(); // Consume ','
    }
    return labels;
}

// Helper to parse duration string (e.g., "5m") into seconds
int64_t Parser::parseDuration(const std::string& durationStr) {
    if (durationStr.empty()) {
        throw std::runtime_error("Empty duration string");
    }

    size_t unitPos = 0;
    while (unitPos < durationStr.length() && std::isdigit(durationStr[unitPos])) {
        unitPos++;
    }

    if (unitPos == 0) {
        throw std::runtime_error("Duration string has no number: " + durationStr);
    }

    long long number = std::stoll(durationStr.substr(0, unitPos));
    std::string unit = durationStr.substr(unitPos);

    if (unit == "ms") return number / 1000; // Convert ms to s, ensure it's what PromQL expects (usually whole seconds for range)
    if (unit == "s") return number;
    if (unit == "m") return number * 60;
    if (unit == "h") return number * 60 * 60;
    if (unit == "d") return number * 60 * 60 * 24;
    if (unit == "w") return number * 60 * 60 * 24 * 7;
    if (unit == "y") return number * 60 * 60 * 24 * 365; // Prometheus definition of year

    throw std::runtime_error("Invalid time unit in duration string: " + durationStr);
}

void Parser::parseOptionalAtOffset([[maybe_unused]] VectorSelectorNode& vsNode) {
    // Check for offset modifier
    if (currentToken_.type == TokenType::OFFSET) {
        nextToken(); // Consume 'offset'
        if (currentToken_.type != TokenType::DURATION) {
            errors_.emplace_back("Expected duration after 'offset'", currentToken_.line, currentToken_.pos);
            return;
        }
        vsNode.originalOffset = currentToken_; // Store the original token
        try {
            vsNode.parsedOffsetSeconds = parseDuration(currentToken_.literal);
        } catch (const std::runtime_error& e) {
            errors_.emplace_back(e.what(), currentToken_.line, currentToken_.pos);
            return;
        }
        nextToken(); // Consume duration
    }
    
    // Check for @ modifier
    if (currentToken_.type == TokenType::AT) {
        nextToken(); // Consume '@'
        vsNode.atModifier = currentToken_; // Store the modifier token
        
        // For now, just consume the timestamp or function token
        // In a full implementation, this would parse timestamps, "start()", "end()" etc.
        if (currentToken_.type == TokenType::NUMBER || 
            currentToken_.type == TokenType::IDENTIFIER) {
            nextToken(); // Consume the timestamp or function name
            // Additional parsing for functions would be done here
        } else {
            errors_.emplace_back("Expected timestamp or function after '@'", currentToken_.line, currentToken_.pos);
            return;
        }
    }
}

void Parser::parseOptionalAtOffsetSubquery([[maybe_unused]] SubqueryExprNode& sqNode) {
    // Check for offset modifier
    if (currentToken_.type == TokenType::OFFSET) {
        nextToken(); // Consume 'offset'
        if (currentToken_.type != TokenType::DURATION) {
            errors_.emplace_back("Expected duration after 'offset'", currentToken_.line, currentToken_.pos);
            return;
        }
        sqNode.originalOffset = currentToken_; // Store the original token
        try {
            sqNode.parsedOffsetSeconds = parseDuration(currentToken_.literal);
        } catch (const std::runtime_error& e) {
            errors_.emplace_back(e.what(), currentToken_.line, currentToken_.pos);
            return;
        }
        nextToken(); // Consume duration
    }
    
    // Check for @ modifier
    if (currentToken_.type == TokenType::AT) {
        nextToken(); // Consume '@'
        sqNode.atModifier = currentToken_; // Store the modifier token
        
        // For now, just consume the timestamp or function token
        // In a full implementation, this would parse timestamps, "start()", "end()" etc.
        if (currentToken_.type == TokenType::NUMBER || 
            currentToken_.type == TokenType::IDENTIFIER) {
            nextToken(); // Consume the timestamp or function name
            // Additional parsing for functions would be done here
        } else {
            errors_.emplace_back("Expected timestamp or function after '@'", currentToken_.line, currentToken_.pos);
            return;
        }
    }
}


} // namespace promql
} // namespace prometheus
} // namespace tsdb 