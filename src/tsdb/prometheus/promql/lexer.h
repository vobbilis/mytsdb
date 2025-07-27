#ifndef TSDB_PROMETHEUS_PROMQL_LEXER_H_
#define TSDB_PROMETHEUS_PROMQL_LEXER_H_

#include <string>
#include <vector>
#include <stdexcept>

namespace tsdb {
namespace prometheus {
namespace promql {

// Token types
// Referenced from https://github.com/prometheus/prometheus/blob/main/promql/parser/lex.go
enum class TokenType {
    // Special tokens
    ILLEGAL,    // Illegal token, e.g., an unrecognized character
    EOF_TOKEN,  // End Of File
    COMMENT,    // A comment

    // Identifiers and basic types
    IDENTIFIER, // e.g., metric_name, label_name, function_name
    NUMBER,     // e.g., 123, 3.14, -5, NaN, inf
    STRING,     // e.g., "this is a string"

    // Operators
    LEFT_PAREN,    // (
    RIGHT_PAREN,   // )
    LEFT_BRACE,    // {
    RIGHT_BRACE,   // }
    LEFT_BRACKET,  // [
    RIGHT_BRACKET, // ]
    COMMA,         // ,
    ASSIGN,        // =
    COLON,         // :

    // Comparison operators
    EQL, // ==
    NEQ, // !=
    LTE, // <=
    LSS, // <
    GTE, // >=
    GTR, // >

    // Regex-augmented comparison operators
    EQL_REGEX, // =~
    NEQ_REGEX, // !~

    // Arithmetic operators
    ADD, // +
    SUB, // -
    MUL, // *
    DIV, // /
    MOD, // %
    POW, // ^ (power)

    // Logical operators
    AND,    // and
    OR,     // or
    UNLESS, // unless

    // Aggregation operators (keywords)
    SUM,
    AVG,
    COUNT,
    MIN,
    MAX,
    STDDEV,
    STDVAR,
    TOPK,
    BOTTOMK,
    COUNT_VALUES,
    QUANTILE,

    // Keywords
    BY,
    WITHOUT,
    ON,
    IGNORING,
    GROUP_LEFT,
    GROUP_RIGHT,
    OFFSET,
    BOOL, // "bool" modifier for comparison operators

    // Durations
    DURATION, // e.g., 5m, 1h, 30s

    // For @ modifier
    AT, // @
    START, // start()
    END,   // end()

    // Subquery keywords
    SUBQUERY_RANGE, // for [<duration>:<resolution>]
};

struct Token {
    TokenType type;
    std::string literal; // The actual string value of the token
    int line;            // Line number where the token starts
    int pos;             // Position in line where the token starts

    Token(TokenType t, std::string lit, int l, int p)
        : type(t), literal(std::move(lit)), line(l), pos(p) {}

    std::string TypeString() const; // For debugging
};

class LexerError : public std::runtime_error {
public:
    LexerError(const std::string& message, int line, int pos)
        : std::runtime_error(message + " at line " + std::to_string(line) + ":" + std::to_string(pos)),
          line_(line), pos_(pos) {}

    int line() const { return line_; }
    int pos() const { return pos_; }
private:
    int line_;
    int pos_;
};


class Lexer {
public:
    explicit Lexer(std::string input);

    /**
     * @brief Get the next token from the input
     */
    Token NextToken();

    /**
     * @brief Get all tokens from the input
     */
    std::vector<Token> GetAllTokens();

    /**
     * @brief Get the current position in the input
     */
    size_t GetPosition() const { return position_; }

    /**
     * @brief Set the position in the input
     * @note This is for advanced use cases like lookahead
     */
    void SetPosition(size_t pos) {
        position_ = pos;
        readPosition_ = pos + 1;
        if (pos < input_.length()) {
            ch_ = input_[pos];
        } else {
            ch_ = 0;
        }
    }

private:
    void readChar();
    char peekChar() const;
    void skipWhitespace();
    void skipComment();

    Token readIdentifier();
    Token readNumber();
    Token readString();
    Token readDuration();
    // Helper to read potential two-char tokens like ==, !=, =~, !~
    Token readOperatorOrComparison();


    std::string input_;
    size_t position_;     // current position in input (points to current char)
    size_t readPosition_; // current reading position in input (after current char)
    char ch_;             // current char under examination

    int currentLine_;
    int currentPosInLine_; // Tracks char position in current line for error reporting
};

// Helper function to convert TokenType to string for debugging
std::string TokenTypeToString(TokenType type);

} // namespace promql
} // namespace prometheus
} // namespace tsdb

#endif // TSDB_PROMETHEUS_PROMQL_LEXER_H_ 