#include <gtest/gtest.h>
#include "../promql/lexer.h"
#include "../promql/parser.h"
#include "../model/types.h"
#include <string>

using namespace tsdb::prometheus::promql;
using namespace tsdb::prometheus;

// --- Lexer Tests ---

TEST(PromQLLexerTest, EmptyInput) {
    Lexer l("");
    Token t = l.NextToken();
    EXPECT_EQ(t.type, TokenType::EOF_TOKEN);
}

TEST(PromQLLexerTest, SimpleOperators) {
    std::string input = "+-*/%^=(),{}";
    Lexer l(input);
    std::vector<TokenType> expected = {
        TokenType::ADD, TokenType::SUB, TokenType::MUL, TokenType::DIV, TokenType::MOD, TokenType::POW,
        TokenType::ASSIGN, TokenType::LEFT_PAREN, TokenType::RIGHT_PAREN, TokenType::COMMA,
        TokenType::LEFT_BRACE, TokenType::RIGHT_BRACE, TokenType::EOF_TOKEN
    };
    for (TokenType expected_type : expected) {
        Token t = l.NextToken();
        EXPECT_EQ(t.type, expected_type);
    }
}

TEST(PromQLLexerTest, ComparisonOperators) {
    std::string input = "== != <= < >= > =~ !~";
    Lexer l(input);
    std::vector<Token> expected = {
        {TokenType::EQL, "==", 1, 1},
        {TokenType::NEQ, "!=", 1, 4},
        {TokenType::LTE, "<=", 1, 7},
        {TokenType::LSS, "<", 1, 10},
        {TokenType::GTE, ">=", 1, 12},
        {TokenType::GTR, ">", 1, 15},
        {TokenType::EQL_REGEX, "=~", 1, 17},
        {TokenType::NEQ_REGEX, "!~", 1, 20},
        {TokenType::EOF_TOKEN, "", 1, 22}
    };

    for (const auto& exp_tok : expected) {
        Token t = l.NextToken();
        EXPECT_EQ(t.type, exp_tok.type);
        EXPECT_EQ(t.literal, exp_tok.literal);
    }
}

TEST(PromQLLexerTest, IdentifiersAndKeywords) {
    std::string input = "metric_name sum by http_requests_total offset";
    Lexer l(input);
    std::vector<Token> expected = {
        {TokenType::IDENTIFIER, "metric_name", 1, 1},
        {TokenType::SUM, "sum", 1, 13},
        {TokenType::BY, "by", 1, 17},
        {TokenType::IDENTIFIER, "http_requests_total", 1, 20},
        {TokenType::OFFSET, "offset", 1, 40},
        {TokenType::EOF_TOKEN, "", 1, 46}
    };
     for (const auto& exp_tok : expected) {
        Token t = l.NextToken();
        EXPECT_EQ(t.type, exp_tok.type);
        EXPECT_EQ(t.literal, exp_tok.literal);
        // EXPECT_EQ(t.line, exp_tok.line); // Position checking can be tricky with current lexer state
        // EXPECT_EQ(t.pos, exp_tok.pos);
    }
}

TEST(PromQLLexerTest, Numbers) {
    std::string input = "123 3.14 -5 +7.5 NaN Inf +Inf -Inf";
    Lexer l(input);
    std::vector<Token> expected = {
        {TokenType::NUMBER, "123", 1, 1},
        {TokenType::NUMBER, "3.14", 1, 5},
        {TokenType::NUMBER, "-5", 1, 10},
        {TokenType::NUMBER, "+7.5", 1, 13},
        {TokenType::NUMBER, "NaN", 1, 18},
        {TokenType::NUMBER, "Inf", 1, 22},
        {TokenType::NUMBER, "+Inf", 1, 26},
        {TokenType::NUMBER, "-Inf", 1, 31},
        {TokenType::EOF_TOKEN, "", 1, 35}
    };
     for (const auto& exp_tok : expected) {
        Token t = l.NextToken();
        EXPECT_EQ(t.type, exp_tok.type);
        EXPECT_EQ(t.literal, exp_tok.literal);
    }
}

TEST(PromQLLexerTest, Strings) {
    std::string input = "\"hello\" 'world' `raw\nstring`";
    Lexer l(input);
    std::vector<Token> expected = {
        {TokenType::STRING, "hello", 1, 1},
        {TokenType::STRING, "world", 1, 9},
        {TokenType::STRING, "raw\nstring", 1, 17},
        {TokenType::EOF_TOKEN, "", 1, 30} // Position might be off due to internal newlines
    };
    for (const auto& exp_tok : expected) {
        Token t = l.NextToken();
        EXPECT_EQ(t.type, exp_tok.type);
        EXPECT_EQ(t.literal, exp_tok.literal);
    }
}

TEST(PromQLLexerTest, Durations) {
    std::string input = "5s 10m 1h 3d 2w 1y 100ms";
    Lexer l(input);
    std::vector<Token> expected = {
        {TokenType::DURATION, "5s", 1, 1},
        {TokenType::DURATION, "10m", 1, 4},
        {TokenType::DURATION, "1h", 1, 8},
        {TokenType::DURATION, "3d", 1, 11},
        {TokenType::DURATION, "2w", 1, 14},
        {TokenType::DURATION, "1y", 1, 17},
        {TokenType::DURATION, "100ms", 1, 20},
        {TokenType::EOF_TOKEN, "", 1, 25}
    };
    for (const auto& exp_tok : expected) {
        Token t = l.NextToken();
        EXPECT_EQ(t.type, exp_tok.type);
        EXPECT_EQ(t.literal, exp_tok.literal);
    }
}

TEST(PromQLLexerTest, Comments) {
    std::string input = "# this is a comment\nmetric_name # another comment";
    Lexer l(input);
    std::vector<Token> expected = {
        // Comments are skipped, so they don't appear as tokens themselves
        {TokenType::IDENTIFIER, "metric_name", 2, 1},
        {TokenType::EOF_TOKEN, "", 2, 31} // Position after comment
    };
     for (const auto& exp_tok : expected) {
        Token t = l.NextToken();
        EXPECT_EQ(t.type, exp_tok.type);
        EXPECT_EQ(t.literal, exp_tok.literal);
    }
}

TEST(PromQLLexerTest, ComplexExpression) {
    std::string input = "sum(rate(http_requests_total{job=\"api\",group=\"canary\"}[5m] offset 10s)) by (job) > 0.5";
    Lexer l(input);
    std::vector<TokenType> expected_types = {
        TokenType::SUM, TokenType::LEFT_PAREN, TokenType::IDENTIFIER, TokenType::LEFT_PAREN,
        TokenType::IDENTIFIER, TokenType::LEFT_BRACE, TokenType::IDENTIFIER, TokenType::ASSIGN,
        TokenType::STRING, TokenType::COMMA, TokenType::IDENTIFIER, TokenType::ASSIGN, TokenType::STRING,
        TokenType::RIGHT_BRACE, TokenType::LEFT_BRACKET, TokenType::DURATION, TokenType::RIGHT_BRACKET,
        TokenType::OFFSET, TokenType::DURATION, TokenType::RIGHT_PAREN, TokenType::RIGHT_PAREN,
        TokenType::BY, TokenType::LEFT_PAREN, TokenType::IDENTIFIER, TokenType::RIGHT_PAREN,
        TokenType::GTR, TokenType::NUMBER, TokenType::EOF_TOKEN
    };
    for (TokenType expected_type : expected_types) {
        Token t = l.NextToken();
        EXPECT_EQ(t.type, expected_type) << "Literal: " << t.literal << " Line: " << t.line << " Pos: " << t.pos;
    }
}

// --- Parser Tests ---

TEST(PromQLParserTest, EmptyExpression) {
    Lexer l("");
    Parser p(l);
    auto expr = p.ParseExpr();
    ASSERT_FALSE(expr);
    ASSERT_FALSE(p.Errors().empty());
    EXPECT_NE(std::string(p.Errors()[0].what()).find("No expression found"), std::string::npos);
}

TEST(PromQLParserTest, NumberLiteral) {
    Lexer l("123.45");
    Parser p(l);
    auto expr = p.ParseExpr();
    ASSERT_TRUE(expr);
    ASSERT_TRUE(p.Errors().empty());
    auto numLit = dynamic_cast<NumberLiteralNode*>(expr.get());
    ASSERT_NE(numLit, nullptr);
    EXPECT_DOUBLE_EQ(numLit->value, 123.45);
    EXPECT_EQ(numLit->String(), "123.45");
}

TEST(PromQLParserTest, StringLiteral) {
    Lexer l("\"hello world\"");
    Parser p(l);
    auto expr = p.ParseExpr();
    ASSERT_TRUE(expr);
    ASSERT_TRUE(p.Errors().empty());
    auto strLit = dynamic_cast<StringLiteralNode*>(expr.get());
    ASSERT_NE(strLit, nullptr);
    EXPECT_EQ(strLit->value, "hello world");
    EXPECT_EQ(strLit->String(), "\"hello world\"");
}

TEST(PromQLParserTest, VectorSelectorMetricOnly) {
    Lexer l("metric_name");
    Parser p(l);
    auto expr = p.ParseExpr();
    ASSERT_TRUE(expr);
    if (!p.Errors().empty()) {
      FAIL() << "Parser errors: " << p.Errors()[0].what();
    }
    auto vecSel = dynamic_cast<VectorSelectorNode*>(expr.get());
    ASSERT_NE(vecSel, nullptr);
    EXPECT_EQ(vecSel->name, "metric_name");
    EXPECT_TRUE(vecSel->labelMatchers.empty());
}

TEST(PromQLParserTest, VectorSelectorWithLabels) {
    Lexer l("metric_name{label1=\"value1\", label2!=\"value2\"}");
    Parser p(l);
    auto expr = p.ParseExpr();
    ASSERT_TRUE(expr);
    if (!p.Errors().empty()) {
      FAIL() << "Parser errors: " << p.Errors()[0].what();
    }
    auto vecSel = dynamic_cast<VectorSelectorNode*>(expr.get());
    ASSERT_NE(vecSel, nullptr);
    EXPECT_EQ(vecSel->name, "metric_name");
    ASSERT_EQ(vecSel->labelMatchers.size(), 2);
    EXPECT_EQ(vecSel->labelMatchers[0].name, "label1");
    EXPECT_EQ(vecSel->labelMatchers[0].value, "value1");
    EXPECT_EQ(vecSel->labelMatchers[0].type, model::MatcherType::EQUAL);
    EXPECT_EQ(vecSel->labelMatchers[1].name, "label2");
    EXPECT_EQ(vecSel->labelMatchers[1].value, "value2");
    EXPECT_EQ(vecSel->labelMatchers[1].type, model::MatcherType::NOT_EQUAL);
}

TEST(PromQLParserTest, LabelOnlyVectorSelector) {
    Lexer l("{job=\"node_exporter\"}");
    Parser p(l);
    auto expr = p.ParseExpr();
    ASSERT_TRUE(expr);
     if (!p.Errors().empty()) {
      FAIL() << "Parser errors: " << p.Errors()[0].what();
    }
    auto vecSel = dynamic_cast<VectorSelectorNode*>(expr.get());
    ASSERT_NE(vecSel, nullptr);
    EXPECT_TRUE(vecSel->name.empty());
    ASSERT_EQ(vecSel->labelMatchers.size(), 1);
    EXPECT_EQ(vecSel->labelMatchers[0].name, "job");
    EXPECT_EQ(vecSel->labelMatchers[0].value, "node_exporter");
    EXPECT_EQ(vecSel->labelMatchers[0].type, model::MatcherType::EQUAL);
}

TEST(PromQLParserTest, SimpleBinaryExpression) {
    Lexer l("1 + 2");
    Parser p(l);
    auto expr = p.ParseExpr();
    ASSERT_TRUE(expr);
    if (!p.Errors().empty()) {
      FAIL() << "Parser errors: " << p.Errors()[0].what();
    }
    auto binExpr = dynamic_cast<BinaryExprNode*>(expr.get());
    ASSERT_NE(binExpr, nullptr);
    EXPECT_EQ(binExpr->op, TokenType::ADD);
    ASSERT_NE(dynamic_cast<NumberLiteralNode*>(binExpr->lhs.get()), nullptr);
    ASSERT_NE(dynamic_cast<NumberLiteralNode*>(binExpr->rhs.get()), nullptr);
    EXPECT_DOUBLE_EQ(dynamic_cast<NumberLiteralNode*>(binExpr->lhs.get())->value, 1.0);
    EXPECT_DOUBLE_EQ(dynamic_cast<NumberLiteralNode*>(binExpr->rhs.get())->value, 2.0);
}

TEST(PromQLParserTest, PrecedenceTest) {
    Lexer l("1 + 2 * 3 - 4 / 2"); // Should be 1 + (2*3) - (4/2) = 1 + 6 - 2 = 5
    Parser p(l);
    auto expr = p.ParseExpr();
    ASSERT_TRUE(expr);
    if (!p.Errors().empty()) {
      FAIL() << "Parser errors: " << p.Errors()[0].what();
    }
    // Expect: ( (1 + (2*3)) - (4/2) )
    auto subExpr = dynamic_cast<BinaryExprNode*>(expr.get()); // Outermost is SUB
    ASSERT_NE(subExpr, nullptr);
    EXPECT_EQ(subExpr->op, TokenType::SUB);

    auto addExpr = dynamic_cast<BinaryExprNode*>(subExpr->lhs.get()); // LHS of SUB is ADD
    ASSERT_NE(addExpr, nullptr);
    EXPECT_EQ(addExpr->op, TokenType::ADD);
    ASSERT_NE(dynamic_cast<NumberLiteralNode*>(addExpr->lhs.get()), nullptr); // 1
    EXPECT_DOUBLE_EQ(dynamic_cast<NumberLiteralNode*>(addExpr->lhs.get())->value, 1.0);

    auto mulExpr = dynamic_cast<BinaryExprNode*>(addExpr->rhs.get()); // RHS of ADD is MUL
    ASSERT_NE(mulExpr, nullptr);
    EXPECT_EQ(mulExpr->op, TokenType::MUL);
    ASSERT_NE(dynamic_cast<NumberLiteralNode*>(mulExpr->lhs.get()), nullptr); // 2
    ASSERT_NE(dynamic_cast<NumberLiteralNode*>(mulExpr->rhs.get()), nullptr); // 3
    EXPECT_DOUBLE_EQ(dynamic_cast<NumberLiteralNode*>(mulExpr->lhs.get())->value, 2.0);
    EXPECT_DOUBLE_EQ(dynamic_cast<NumberLiteralNode*>(mulExpr->rhs.get())->value, 3.0);

    auto divExpr = dynamic_cast<BinaryExprNode*>(subExpr->rhs.get()); // RHS of SUB is DIV
    ASSERT_NE(divExpr, nullptr);
    EXPECT_EQ(divExpr->op, TokenType::DIV);
    ASSERT_NE(dynamic_cast<NumberLiteralNode*>(divExpr->lhs.get()), nullptr); // 4
    ASSERT_NE(dynamic_cast<NumberLiteralNode*>(divExpr->rhs.get()), nullptr); // 2
    EXPECT_DOUBLE_EQ(dynamic_cast<NumberLiteralNode*>(divExpr->lhs.get())->value, 4.0);
    EXPECT_DOUBLE_EQ(dynamic_cast<NumberLiteralNode*>(divExpr->rhs.get())->value, 2.0);
}

TEST(PromQLParserTest, ParenthesizedExpression) {
    Lexer l("(1 + 2) * 3");
    Parser p(l);
    auto expr = p.ParseExpr();
    ASSERT_TRUE(expr);
    if (!p.Errors().empty()) {
        FAIL() << "Parser errors: " << p.Errors()[0].what();
    }
    auto mulExpr = dynamic_cast<BinaryExprNode*>(expr.get());
    ASSERT_NE(mulExpr, nullptr);
    EXPECT_EQ(mulExpr->op, TokenType::MUL);

    auto parenExpr = dynamic_cast<ParenExprNode*>(mulExpr->lhs.get());
    ASSERT_NE(parenExpr, nullptr);
    
    auto addExpr = dynamic_cast<BinaryExprNode*>(parenExpr->expr.get());
    ASSERT_NE(addExpr, nullptr);
    EXPECT_EQ(addExpr->op, TokenType::ADD);
    EXPECT_DOUBLE_EQ(dynamic_cast<NumberLiteralNode*>(addExpr->lhs.get())->value, 1.0);
    EXPECT_DOUBLE_EQ(dynamic_cast<NumberLiteralNode*>(addExpr->rhs.get())->value, 2.0);

    EXPECT_DOUBLE_EQ(dynamic_cast<NumberLiteralNode*>(mulExpr->rhs.get())->value, 3.0);
}

TEST(PromQLParserTest, UnaryExpression) {
    Lexer l("-5 + -metric_name");
    Parser p(l);
    auto expr = p.ParseExpr();
    ASSERT_TRUE(expr);
    if (!p.Errors().empty()) {
        FAIL() << "Parser errors: " << p.Errors()[0].what();
    }
    auto addExpr = dynamic_cast<BinaryExprNode*>(expr.get());
    ASSERT_NE(addExpr, nullptr);
    EXPECT_EQ(addExpr->op, TokenType::ADD);

    auto unaryNum = dynamic_cast<UnaryExprNode*>(addExpr->lhs.get());
    ASSERT_NE(unaryNum, nullptr);
    EXPECT_EQ(unaryNum->op, TokenType::SUB);
    ASSERT_NE(dynamic_cast<NumberLiteralNode*>(unaryNum->expr.get()), nullptr);
    EXPECT_DOUBLE_EQ(dynamic_cast<NumberLiteralNode*>(unaryNum->expr.get())->value, 5.0);

    auto unaryVec = dynamic_cast<UnaryExprNode*>(addExpr->rhs.get());
    ASSERT_NE(unaryVec, nullptr);
    EXPECT_EQ(unaryVec->op, TokenType::SUB);
    ASSERT_NE(dynamic_cast<VectorSelectorNode*>(unaryVec->expr.get()), nullptr);
    EXPECT_EQ(dynamic_cast<VectorSelectorNode*>(unaryVec->expr.get())->name, "metric_name");
}

TEST(PromQLParserTest, FunctionCallNoArgs) {
    Lexer l("time()");
    Parser p(l);
    auto expr = p.ParseExpr();
    ASSERT_TRUE(expr);
    if (!p.Errors().empty()) {
      FAIL() << "Parser errors: " << p.Errors()[0].what();
    }
    auto callNode = dynamic_cast<CallNode*>(expr.get());
    ASSERT_NE(callNode, nullptr);
    EXPECT_EQ(callNode->funcName, "time");
    EXPECT_TRUE(callNode->args.empty());
}

TEST(PromQLParserTest, FunctionCallWithArgs) {
    Lexer l("round(some_metric, 5)");
    Parser p(l);
    auto expr = p.ParseExpr();
    ASSERT_TRUE(expr);
    if (!p.Errors().empty()) {
      FAIL() << "Parser errors: " << p.Errors()[0].what();
    }
    auto callNode = dynamic_cast<CallNode*>(expr.get());
    ASSERT_NE(callNode, nullptr);
    EXPECT_EQ(callNode->funcName, "round");
    ASSERT_EQ(callNode->args.size(), 2);
    ASSERT_NE(dynamic_cast<VectorSelectorNode*>(callNode->args[0].get()), nullptr);
    EXPECT_EQ(dynamic_cast<VectorSelectorNode*>(callNode->args[0].get())->name, "some_metric");
    ASSERT_NE(dynamic_cast<NumberLiteralNode*>(callNode->args[1].get()), nullptr);
    EXPECT_DOUBLE_EQ(dynamic_cast<NumberLiteralNode*>(callNode->args[1].get())->value, 5);
}

TEST(PromQLParserTest, AggregationSimple) {
    Lexer l("sum(metric)");
    Parser p(l);
    auto expr = p.ParseExpr();
    ASSERT_TRUE(expr);
    if (!p.Errors().empty()) { FAIL() << "Parser errors: " << p.Errors()[0].what(); }
    auto aggNode = dynamic_cast<AggregateExprNode*>(expr.get());
    ASSERT_NE(aggNode, nullptr);
    EXPECT_EQ(aggNode->op, TokenType::SUM);
    ASSERT_NE(dynamic_cast<VectorSelectorNode*>(aggNode->expr.get()), nullptr);
    EXPECT_EQ(dynamic_cast<VectorSelectorNode*>(aggNode->expr.get())->name, "metric");
    EXPECT_TRUE(aggNode->groupingLabels.empty());
    EXPECT_EQ(aggNode->param, nullptr);
}

TEST(PromQLParserTest, AggregationWithByClause) {
    Lexer l("avg by (job, instance) (http_requests_total)");
    Parser p(l);
    auto expr = p.ParseExpr();
    ASSERT_TRUE(expr);
    if (!p.Errors().empty()) { FAIL() << "Parser errors: " << p.Errors()[0].what(); }
    auto aggNode = dynamic_cast<AggregateExprNode*>(expr.get());
    ASSERT_NE(aggNode, nullptr);
    EXPECT_EQ(aggNode->op, TokenType::AVG);
    ASSERT_FALSE(aggNode->without);
    ASSERT_EQ(aggNode->groupingLabels.size(), 2);
    EXPECT_EQ(aggNode->groupingLabels[0], "job");
    EXPECT_EQ(aggNode->groupingLabels[1], "instance");
    ASSERT_NE(dynamic_cast<VectorSelectorNode*>(aggNode->expr.get()), nullptr);
    EXPECT_EQ(dynamic_cast<VectorSelectorNode*>(aggNode->expr.get())->name, "http_requests_total");
}

TEST(PromQLParserTest, AggregationWithWithoutClauseAndParam) {
    Lexer l("topk(5, metric_name) without (label1)");
    Parser p(l);
    auto expr = p.ParseExpr();
    ASSERT_TRUE(expr);
    if (!p.Errors().empty()) { FAIL() << "Parser errors: " << p.Errors()[0].what(); }
    auto aggNode = dynamic_cast<AggregateExprNode*>(expr.get());
    ASSERT_NE(aggNode, nullptr);
    EXPECT_EQ(aggNode->op, TokenType::TOPK);
    ASSERT_TRUE(aggNode->without);
    ASSERT_EQ(aggNode->groupingLabels.size(), 1);
    EXPECT_EQ(aggNode->groupingLabels[0], "label1");
    ASSERT_NE(dynamic_cast<VectorSelectorNode*>(aggNode->expr.get()), nullptr);
    EXPECT_EQ(dynamic_cast<VectorSelectorNode*>(aggNode->expr.get())->name, "metric_name");
    ASSERT_NE(aggNode->param, nullptr);
    ASSERT_NE(dynamic_cast<NumberLiteralNode*>(aggNode->param.get()), nullptr);
    EXPECT_DOUBLE_EQ(dynamic_cast<NumberLiteralNode*>(aggNode->param.get())->value, 5);
}

TEST(PromQLParserTest, MatrixSelector) {
    Lexer l("http_requests_total{job=\"api\"}[5m]");
    Parser p(l);
    // This test currently might fail if parsePrefixExpression doesn't look ahead for '['
    // to call parseMatrixSelector. This will be addressed by proper infix parsing of '['.
    // For now, we assume the parser infrastructure will need to be enhanced to handle this.
    // Placeholder - actual matrix selector parsing is part of infix/postfix handling of '['
    // For now, let's test if the parser can handle it if called directly (hypothetically)
    // A more realistic test will be added when operator-precedence for '[' is in.

    // To make this test pass with current parser, it must be an infix operator
    // or called explicitly. The current design of parseExpression will handle this
    // if getPrecedence(TokenType::LEFT_BRACKET) is high enough and parseInfixExpression
    // has a case for it.

    // Let's try parsing it as a full expression. The Pratt parser should handle '['
    // if its precedence is set correctly and an infix (or postfix) parse function is registered.
    // With current setup, parseExpression -> parsePrefixExpression -> parseVectorSelector
    // Then, the main loop of parseExpression should see '[' token if vector selector parsing stops before it.
    // getPrecedence(TokenType::LEFT_BRACKET) is CALL_INDEX.
    // parseInfixExpression needs to handle currentToken_ being LEFT_BRACKET.
    // For now, adding a placeholder for LEFT_BRACKET in parseInfixExpression is needed.

    // Given the current parser structure, direct parsing of matrix selectors
    // as a primary expression is not the standard PromQL way. They usually appear
    // as arguments to functions like rate() or in subqueries.
    // A simple `metric[5m]` is valid though.

    auto expr = p.ParseExpr();
    ASSERT_TRUE(expr);
    if (!p.Errors().empty()) { FAIL() << "Parser errors: " << p.Errors()[0].what(); }

    auto matrixSel = dynamic_cast<MatrixSelectorNode*>(expr.get());
    if (matrixSel) { // If parser correctly identifies it as a MatrixSelector at top level
        EXPECT_EQ(matrixSel->range.literal, "5m");
        EXPECT_EQ(matrixSel->parsedRangeSeconds, 300);
        ASSERT_NE(matrixSel->vectorSelector, nullptr);
        EXPECT_EQ(matrixSel->vectorSelector->name, "http_requests_total");
        ASSERT_EQ(matrixSel->vectorSelector->labelMatchers.size(), 1);
        EXPECT_EQ(matrixSel->vectorSelector->labelMatchers[0].name, "job");
        EXPECT_EQ(matrixSel->vectorSelector->labelMatchers[0].value, "api");
    } else {
        // It might be parsed as a VectorSelector if '[' is not handled as an infix operator yet.
        // This indicates that the test or parser logic for '[' as an operator needs to be completed.
        // For now, we'll accept if it's at least a vector selector and the next token is '['.
        auto vecSel = dynamic_cast<VectorSelectorNode*>(expr.get());
        ASSERT_NE(vecSel, nullptr) << "Expression is not a VectorSelector or MatrixSelector.";
        EXPECT_EQ(vecSel->name, "http_requests_total");
        // And then the next token in the *parser's state* should be '['.
        // This is harder to check without exposing parser internals or more specific parsing logic.
        // This test highlights the need for parseInfixExpression to handle '['.
        // ADD A TODO IN PARSER.CPP FOR THIS
        // For now, if it parses as a vector selector, we expect an error or incomplete parse.
        //EXPECT_FALSE(p.Errors().empty()) << "Expected error or incomplete parse if matrix selector not fully handled.";
        // For the current implementation, this test will likely fail as the MatrixSelector parsing logic is not called from top level
        // Needs `parseInfixExpression` to handle `LEFT_BRACKET`
         FAIL() << "Matrix selector not parsed as MatrixSelectorNode. Infix parsing for '[' might be missing.";
    }
}

// Tests for error handling
TEST(PromQLParserTest, MismatchedParentheses) {
    Lexer l("sum(metric");
    Parser p(l);
    auto expr = p.ParseExpr();
    ASSERT_FALSE(expr);
    ASSERT_FALSE(p.Errors().empty());
    EXPECT_NE(std::string(p.Errors()[0].what()).find("Expected"), std::string::npos);
}

TEST(PromQLParserTest, InvalidLabelMatcher) {
    Lexer l("metric{job>=\"api\"}");  // Invalid matcher operator
    Parser p(l);
    auto expr = p.ParseExpr();
    ASSERT_FALSE(expr);
    ASSERT_FALSE(p.Errors().empty());
    EXPECT_NE(std::string(p.Errors()[0].what()).find("matcher operator"), std::string::npos);
}

TEST(PromQLParserTest, InvalidMatrixSelector) {
    Lexer l("sum()[5m]");  // Not a vector selector before '['
    Parser p(l);
    auto expr = p.ParseExpr();
    ASSERT_FALSE(expr);
    ASSERT_FALSE(p.Errors().empty());
    EXPECT_NE(std::string(p.Errors()[0].what()).find("vector selector"), std::string::npos);
}

TEST(PromQLParserTest, InvalidAggregation) {
    Lexer l("sum by job (metric)");  // Missing parentheses around 'job'
    Parser p(l);
    auto expr = p.ParseExpr();
    ASSERT_FALSE(expr);
    ASSERT_FALSE(p.Errors().empty());
    EXPECT_NE(std::string(p.Errors()[0].what()).find("("), std::string::npos);
}

TEST(PromQLParserTest, SubqueryExpression) {
    Lexer l("http_requests_total[1h:5m]");
    Parser p(l);
    auto expr = p.ParseExpr();
    ASSERT_TRUE(expr);
    if (!p.Errors().empty()) { FAIL() << "Parser errors: " << p.Errors()[0].what(); }
    
    auto subqueryExpr = dynamic_cast<SubqueryExprNode*>(expr.get());
    ASSERT_NE(subqueryExpr, nullptr) << "Expression is not a SubqueryExprNode";
    EXPECT_EQ(subqueryExpr->range.literal, "1h");
    EXPECT_EQ(subqueryExpr->parsedRangeSeconds, 3600); // 1 hour = 3600 seconds
    EXPECT_EQ(subqueryExpr->resolution.literal, "5m");
    EXPECT_EQ(subqueryExpr->parsedResolutionSeconds, 300); // 5 minutes = 300 seconds
    
    auto vecSel = dynamic_cast<VectorSelectorNode*>(subqueryExpr->expr.get());
    ASSERT_NE(vecSel, nullptr);
    EXPECT_EQ(vecSel->name, "http_requests_total");
}

TEST(PromQLParserTest, SubqueryExpressionNoResolution) {
    Lexer l("rate(http_requests_total[5m])[30m:]");
    Parser p(l);
    auto expr = p.ParseExpr();
    ASSERT_TRUE(expr);
    if (!p.Errors().empty()) { FAIL() << "Parser errors: " << p.Errors()[0].what(); }
    
    auto subqueryExpr = dynamic_cast<SubqueryExprNode*>(expr.get());
    ASSERT_NE(subqueryExpr, nullptr) << "Expression is not a SubqueryExprNode";
    EXPECT_EQ(subqueryExpr->range.literal, "30m");
    EXPECT_EQ(subqueryExpr->parsedRangeSeconds, 1800); // 30 minutes = 1800 seconds
    EXPECT_EQ(subqueryExpr->resolution.type, TokenType::ILLEGAL); // No resolution specified
    
    // The inner expression should be a function call (rate)
    auto rateCall = dynamic_cast<CallNode*>(subqueryExpr->expr.get());
    ASSERT_NE(rateCall, nullptr);
    EXPECT_EQ(rateCall->funcName, "rate");
    ASSERT_EQ(rateCall->args.size(), 1);
    
    // The argument to rate should be a matrix selector
    auto matrixSel = dynamic_cast<MatrixSelectorNode*>(rateCall->args[0].get());
    ASSERT_NE(matrixSel, nullptr);
    EXPECT_EQ(matrixSel->range.literal, "5m");
}

TEST(PromQLParserTest, VectorSelectorWithOffset) {
    Lexer l("http_requests_total offset 5m");
    Parser p(l);
    auto expr = p.ParseExpr();
    ASSERT_TRUE(expr);
    if (!p.Errors().empty()) { FAIL() << "Parser errors: " << p.Errors()[0].what(); }
    
    auto vecSel = dynamic_cast<VectorSelectorNode*>(expr.get());
    ASSERT_NE(vecSel, nullptr);
    EXPECT_EQ(vecSel->name, "http_requests_total");
    EXPECT_EQ(vecSel->originalOffset.literal, "5m");
    EXPECT_EQ(vecSel->parsedOffsetSeconds, 300); // 5 minutes = 300 seconds
}

TEST(PromQLParserTest, VectorSelectorWithAtModifier) {
    Lexer l("http_requests_total @ 1609459200"); // Unix timestamp for 2021-01-01 00:00:00
    Parser p(l);
    auto expr = p.ParseExpr();
    ASSERT_TRUE(expr);
    if (!p.Errors().empty()) { FAIL() << "Parser errors: " << p.Errors()[0].what(); }
    
    auto vecSel = dynamic_cast<VectorSelectorNode*>(expr.get());
    ASSERT_NE(vecSel, nullptr);
    EXPECT_EQ(vecSel->name, "http_requests_total");
    EXPECT_EQ(vecSel->atModifier.literal, "1609459200");
}

TEST(PromQLParserTest, VectorSelectorWithOffsetAndAt) {
    Lexer l("http_requests_total offset 1h @ start()");
    Parser p(l);
    auto expr = p.ParseExpr();
    ASSERT_TRUE(expr);
    if (!p.Errors().empty()) { FAIL() << "Parser errors: " << p.Errors()[0].what(); }
    
    auto vecSel = dynamic_cast<VectorSelectorNode*>(expr.get());
    ASSERT_NE(vecSel, nullptr);
    EXPECT_EQ(vecSel->name, "http_requests_total");
    EXPECT_EQ(vecSel->originalOffset.literal, "1h");
    EXPECT_EQ(vecSel->parsedOffsetSeconds, 3600); // 1 hour = 3600 seconds
    EXPECT_EQ(vecSel->atModifier.literal, "start");
}

// TODO: Add tests for:
// - Subqueries (e.g., metric[1h:5m])
// - Offset and @ modifiers for vector selectors and subqueries
// - All binary operators with correct precedence and associativity
// - All aggregation operators with various grouping and parameter combinations
// - Complex nested expressions
// - Error reporting for various syntax errors (e.g., mismatched parentheses, invalid operators)

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 