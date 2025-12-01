#include "tsdb/prometheus/promql/lexer.h"
#include <cctype>
#include <unordered_map>

namespace tsdb {
namespace prometheus {
namespace promql {

// Keyword mapping
static const std::unordered_map<std::string, TokenType> keywords = {
    {"and", TokenType::AND},
    {"or", TokenType::OR},
    {"unless", TokenType::UNLESS},
    {"sum", TokenType::SUM},
    {"avg", TokenType::AVG},
    {"count", TokenType::COUNT},
    {"min", TokenType::MIN},
    {"max", TokenType::MAX},
    {"stddev", TokenType::STDDEV},
    {"stdvar", TokenType::STDVAR},
    {"topk", TokenType::TOPK},
    {"bottomk", TokenType::BOTTOMK},
    {"count_values", TokenType::COUNT_VALUES},
    {"quantile", TokenType::QUANTILE},
    {"by", TokenType::BY},
    {"without", TokenType::WITHOUT},
    {"on", TokenType::ON},
    {"ignoring", TokenType::IGNORING},
    {"group_left", TokenType::GROUP_LEFT},
    {"group_right", TokenType::GROUP_RIGHT},
    {"offset", TokenType::OFFSET},
    {"bool", TokenType::BOOL},
    // Note: DURATION is handled separately due to its format
    // Note: AT, START, END are handled based on context or specific lexing rules
};

std::string Token::TypeString() const {
    return TokenTypeToString(type);
}

Lexer::Lexer(std::string input)
    : input_(std::move(input)), position_(0), readPosition_(0), ch_(0),
      currentLine_(1), currentPosInLine_(0) {
    readChar(); // Initialize ch_ and readPosition_
}

void Lexer::readChar() {
    if (readPosition_ >= input_.length()) {
        ch_ = 0; // EOF
    } else {
        ch_ = input_[readPosition_];
    }
    position_ = readPosition_;
    readPosition_++;
    currentPosInLine_++;
}

char Lexer::peekChar() const {
    if (readPosition_ >= input_.length()) {
        return 0; // EOF
    }
    return input_[readPosition_];
}

void Lexer::skipWhitespace() {
    while (ch_ == ' ' || ch_ == '\t' || ch_ == '\n' || ch_ == '\r') {
        if (ch_ == '\n') {
            currentLine_++;
            currentPosInLine_ = 0;
        }
        readChar();
    }
}

void Lexer::skipComment() {
    if (ch_ == '#') {
        while (ch_ != '\n' && ch_ != 0) {
            readChar();
        }
        // We might have consumed the newline that skipWhitespace would handle,
        // so we can optionally call skipWhitespace() again here if needed,
        // or ensure the main loop handles it.
    }
}

Token Lexer::readIdentifier() {
    size_t startPos = position_;
    int tokenStartLine = currentLine_;
    int tokenStartPosInLine = currentPosInLine_;
    // Prometheus identifiers can include colons for recording rules/metrics.
    while (std::isalnum(ch_) || ch_ == '_' || ch_ == ':') {
        readChar();
    }
    std::string literal = input_.substr(startPos, position_ - startPos);

    // Check if it's a keyword
    auto it = keywords.find(literal);
    if (it != keywords.end()) {
        return Token(it->second, literal, tokenStartLine, tokenStartPosInLine);
    }
    return Token(TokenType::IDENTIFIER, literal, tokenStartLine, tokenStartPosInLine);
}

Token Lexer::readNumber() {
    size_t startPos = position_;
    int tokenStartLine = currentLine_;
    int tokenStartPosInLine = currentPosInLine_;
    // Basic number reading: integers, floats. Prometheus allows NaN, inf, +inf, -inf.
    // This is a simplified version. A more robust one would handle exponents, hex, etc.
    // and Prometheus specific literals like NaN, inf.
    // For now, let's handle basic numbers and optionally a leading sign.
    if (ch_ == '-' || ch_ == '+') {
      // Check if it's part of a number or a standalone operator
      if (std::isdigit(peekChar()) || peekChar() == '.') {
          readChar(); 
      } else {
          // This will be handled as an operator by the main NextToken loop
          // Return an illegal token for now or let the main loop decide based on ch_
          // This part needs careful handling in NextToken
      }
    }

    while (std::isdigit(ch_)) {
        readChar();
    }
    if (ch_ == '.') {
        readChar();
        while (std::isdigit(ch_)) {
            readChar();
        }
    }
    // TODO: Add more robust number parsing (exponents, NaN, inf, etc.)
    std::string literal = input_.substr(startPos, position_ - startPos);
    // Simple check for Prometheus specials, can be more robust
    if (literal == "NaN" || literal == "Inf" || literal == "+Inf" || literal == "-Inf") {
         return Token(TokenType::NUMBER, literal, tokenStartLine, tokenStartPosInLine);
    }
    // Could add validation here to ensure it's a parseable number
    return Token(TokenType::NUMBER, literal, tokenStartLine, tokenStartPosInLine);
}

Token Lexer::readString() {
    char quoteType = ch_;
    int tokenStartLine = currentLine_;
    int tokenStartPosInLine = currentPosInLine_;
    readChar(); // Consume the opening quote

    std::string literal;
    while (ch_ != quoteType && ch_ != 0) {
        if (ch_ == '\\') { // Handle escape sequences
            readChar(); // Consume '\'
            switch (ch_) {
                case 'n': literal += '\n'; break;
                case 't': literal += '\t'; break;
                case 'r': literal += '\r'; break;
                case '\\': literal += '\\'; break;
                case '\'': literal += '\''; break;
                case '"': literal += '"'; break;
                // Add more escapes if needed, e.g. \uXXXX
                default: literal += ch_; // Or throw error for invalid escape
            }
        } else {
            literal += ch_;
        }
        readChar();
    }

    if (ch_ == 0) { // Unterminated string
        throw LexerError("Unterminated string literal", tokenStartLine, tokenStartPosInLine);
    }
    readChar(); // Consume the closing quote
    return Token(TokenType::STRING, literal, tokenStartLine, tokenStartPosInLine);
}

Token Lexer::readDuration() {
    // Placeholder: PromQL durations are like 5m, 1h. 
    // This needs a proper parser for [0-9]+[smhdwy].
    // For now, assume it's read as an IDENTIFIER if not handled by number logic.
    // A dedicated function is better.
    size_t startPos = position_;
    int tokenStartLine = currentLine_;
    int tokenStartPosInLine = currentPosInLine_;
    
    std::string durationStr;
    while(std::isdigit(ch_)) {
        durationStr += ch_;
        readChar();
    }
    if (durationStr.empty()) {
        return Token(TokenType::ILLEGAL, std::string(1, ch_), currentLine_, currentPosInLine_);
    }

    std::string unitStr;
    // y, w, d, h, m, s, ms (Prometheus also supports 'ms')
    // Check for two-char unit 'ms' first
    if ( (ch_ == 'm' && peekChar() == 's') ) {
        unitStr += ch_;
        readChar();
        unitStr += ch_;
        readChar();
    } else if (ch_ == 's' || ch_ == 'm' || ch_ == 'h' || ch_ == 'd' || ch_ == 'w' || ch_ == 'y') {
        unitStr += ch_;
        readChar();
    } else {
         // Not a valid duration unit after numbers
        return Token(TokenType::ILLEGAL, input_.substr(startPos, position_ - startPos), tokenStartLine, tokenStartPosInLine);
    }
    return Token(TokenType::DURATION, durationStr + unitStr, tokenStartLine, tokenStartPosInLine);
}

// Helper for operators and comparisons
Token Lexer::readOperatorOrComparison() {
    int tokenStartLine = currentLine_;
    int tokenStartPosInLine = currentPosInLine_;
    char firstChar = ch_;
    readChar(); // Consume the first char

    switch (firstChar) {
        case '=':
            if (ch_ == '=') { readChar(); return Token(TokenType::EQL, "==", tokenStartLine, tokenStartPosInLine); }
            if (ch_ == '~') { readChar(); return Token(TokenType::EQL_REGEX, "=~", tokenStartLine, tokenStartPosInLine); }
            return Token(TokenType::ASSIGN, "=", tokenStartLine, tokenStartPosInLine);
        case '!':
            if (ch_ == '=') { readChar(); return Token(TokenType::NEQ, "!=", tokenStartLine, tokenStartPosInLine); }
            if (ch_ == '~') { readChar(); return Token(TokenType::NEQ_REGEX, "!~", tokenStartLine, tokenStartPosInLine); }
            break; // Illegal starting with just '!'
        case '<':
            if (ch_ == '=') { readChar(); return Token(TokenType::LTE, "<=", tokenStartLine, tokenStartPosInLine); }
            return Token(TokenType::LSS, "<", tokenStartLine, tokenStartPosInLine);
        case '>':
            if (ch_ == '=') { readChar(); return Token(TokenType::GTE, ">=", tokenStartLine, tokenStartPosInLine); }
            return Token(TokenType::GTR, ">", tokenStartLine, tokenStartPosInLine);
        // Single char operators are handled by the main NextToken loop
    }
    // If we fall through, it means it was a single char operator that should be handled by the main switch,
    // or an illegal sequence starting with '!'. We need to backtrack or handle it there.
    // For simplicity, let's assume single char operators are handled in NextToken's main switch.
    // This function primarily helps disambiguate multi-char operators.
    // Backtrack for the main loop to process `firstChar`
    position_ = position_ -1; // Go back one char if second char didn't form a two-char token
    readPosition_ = readPosition_ -1;
    currentPosInLine_ --;
    ch_ = firstChar;
    // Let NextToken handle single char ops like +, -, *, /, etc.
    return Token(TokenType::ILLEGAL, std::string(1, firstChar), tokenStartLine, tokenStartPosInLine);
}

Token Lexer::NextToken() {
    Token tok = Token(TokenType::ILLEGAL, "", currentLine_, currentPosInLine_);

    skipWhitespace();
    skipComment(); // Skip comments after whitespace, as comments can start anywhere
    skipWhitespace(); // Skip more whitespace that might appear after a comment

    int tokenStartLine = currentLine_;
    int tokenStartPosInLine = currentPosInLine_;

    switch (ch_) {
        case '=': 
        case '!':
        case '<':
        case '>':
            // For =, <, >, check if it's a multi-character operator
            // For !, it must be part of != or !~
            if ( (ch_ == '=' && (peekChar() == '=' || peekChar() == '~')) ||
                 (ch_ == '!' && (peekChar() == '=' || peekChar() == '~')) ||
                 (ch_ == '<' && peekChar() == '=') ||
                 (ch_ == '>' && peekChar() == '=') ) {
                return readOperatorOrComparison();
            } else if (ch_ == '=') {
                 tok = Token(TokenType::ASSIGN, "=", tokenStartLine, tokenStartPosInLine); readChar(); break;
            } else if (ch_ == '<') {
                 tok = Token(TokenType::LSS, "<", tokenStartLine, tokenStartPosInLine); readChar(); break;
            } else if (ch_ == '>') {
                 tok = Token(TokenType::GTR, ">", tokenStartLine, tokenStartPosInLine); readChar(); break;
            }
            // If it's just '!' and not followed by '=' or '~', it's illegal in PromQL standalone.
            // readOperatorOrComparison would return ILLEGAL, or we handle here.
            tok = Token(TokenType::ILLEGAL, std::string(1, ch_), tokenStartLine, tokenStartPosInLine); readChar(); break;
        case '+': tok = Token(TokenType::ADD, "+", tokenStartLine, tokenStartPosInLine); readChar(); break;
        case '-': tok = Token(TokenType::SUB, "-", tokenStartLine, tokenStartPosInLine); readChar(); break;
        case '*': tok = Token(TokenType::MUL, "*", tokenStartLine, tokenStartPosInLine); readChar(); break;
        case '/': tok = Token(TokenType::DIV, "/", tokenStartLine, tokenStartPosInLine); readChar(); break;
        case '%': tok = Token(TokenType::MOD, "%", tokenStartLine, tokenStartPosInLine); readChar(); break;
        case '^': tok = Token(TokenType::POW, "^", tokenStartLine, tokenStartPosInLine); readChar(); break;
        case '(': tok = Token(TokenType::LEFT_PAREN, "(", tokenStartLine, tokenStartPosInLine); readChar(); break;
        case ')': tok = Token(TokenType::RIGHT_PAREN, ")", tokenStartLine, tokenStartPosInLine); readChar(); break;
        case '{': tok = Token(TokenType::LEFT_BRACE, "{", tokenStartLine, tokenStartPosInLine); readChar(); break;
        case '}': tok = Token(TokenType::RIGHT_BRACE, "}", tokenStartLine, tokenStartPosInLine); readChar(); break;
        case '[': tok = Token(TokenType::LEFT_BRACKET, "[", tokenStartLine, tokenStartPosInLine); readChar(); break;
        case ']': tok = Token(TokenType::RIGHT_BRACKET, "]", tokenStartLine, tokenStartPosInLine); readChar(); break;
        case ',': tok = Token(TokenType::COMMA, ",", tokenStartLine, tokenStartPosInLine); readChar(); break;
        case ':': tok = Token(TokenType::COLON, ":", tokenStartLine, tokenStartPosInLine); readChar(); break;
        case '@': tok = Token(TokenType::AT, "@", tokenStartLine, tokenStartPosInLine); readChar(); break;
        case '\'': // Fallthrough for single quote strings
        case '"': // Double quote strings
        case '`': // Raw strings (backticks)
            // Prometheus uses ", ', and ` for strings. Our lexer.h only has STRING.
            // We can make readString handle all three or differentiate if needed.
            // For now, assuming readString handles based on the opening quote.
            try {
                return readString();
            } catch (const LexerError& e) {
                // Propagate LexerError or handle as ILLEGAL token
                return Token(TokenType::ILLEGAL, e.what(), e.line(), e.pos());
            }
        case 0:   tok = Token(TokenType::EOF_TOKEN, "", tokenStartLine, tokenStartPosInLine); break; // EOF
        default:
            if (std::isalpha(ch_) || ch_ == '_') {
                // Could be an identifier or a keyword or a duration-like string if not perfectly matched.
                // readIdentifier() checks for keywords.
                return readIdentifier();
            } else if (std::isdigit(ch_) || ( (ch_ == '-' || ch_ == '+') && (std::isdigit(peekChar()) || peekChar() == '.')) ) {
                // Try to read as a number. If it's followed by a duration unit, it could be a duration.
                // This logic needs to be robust. Let's try reading number first.
                size_t beforeNumReadPos = position_;
                Token numTok = readNumber();
                // After reading a number, check if it's followed by a duration unit
                // This is a common pattern for durations like `5m`
                if (numTok.type == TokenType::NUMBER && 
                    (ch_ == 's' || ch_ == 'm' || ch_ == 'h' || ch_ == 'd' || ch_ == 'w' || ch_ == 'y')) {
                    // It looks like a duration. We need to combine the number and the unit.
                    // Backtrack the lexer state to before the number was read.
                    position_ = beforeNumReadPos;
                    readPosition_ = beforeNumReadPos + 1; // next char to be read
                    ch_ = input_[beforeNumReadPos];
                    currentPosInLine_ = tokenStartPosInLine; // Approximate, needs more precise tracking if line changes
                    return readDuration();
                }
                return numTok; // It was just a number
            } else {
                tok = Token(TokenType::ILLEGAL, std::string(1, ch_), tokenStartLine, tokenStartPosInLine);
                readChar(); // Consume the illegal character
            }
            break;
    }
    return tok;
}

std::vector<Token> Lexer::GetAllTokens() {
    std::vector<Token> tokens;
    Token tok = NextToken();
    while (tok.type != TokenType::EOF_TOKEN) {
        tokens.push_back(tok);
        tok = NextToken();
    }
    tokens.push_back(tok); // Add EOF token
    return tokens;
}

// Helper function to convert TokenType to string for debugging
std::string TokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::ILLEGAL: return "ILLEGAL";
        case TokenType::EOF_TOKEN: return "EOF";
        case TokenType::COMMENT: return "COMMENT";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::NUMBER: return "NUMBER";
        case TokenType::STRING: return "STRING";
        case TokenType::LEFT_PAREN: return "LEFT_PAREN";
        case TokenType::RIGHT_PAREN: return "RIGHT_PAREN";
        case TokenType::LEFT_BRACE: return "LEFT_BRACE";
        case TokenType::RIGHT_BRACE: return "RIGHT_BRACE";
        case TokenType::LEFT_BRACKET: return "LEFT_BRACKET";
        case TokenType::RIGHT_BRACKET: return "RIGHT_BRACKET";
        case TokenType::COMMA: return "COMMA";
        case TokenType::ASSIGN: return "ASSIGN";
        case TokenType::COLON: return "COLON";
        case TokenType::EQL: return "EQL";
        case TokenType::NEQ: return "NEQ";
        case TokenType::LTE: return "LTE";
        case TokenType::LSS: return "LSS";
        case TokenType::GTE: return "GTE";
        case TokenType::GTR: return "GTR";
        case TokenType::EQL_REGEX: return "EQL_REGEX";
        case TokenType::NEQ_REGEX: return "NEQ_REGEX";
        case TokenType::ADD: return "ADD";
        case TokenType::SUB: return "SUB";
        case TokenType::MUL: return "MUL";
        case TokenType::DIV: return "DIV";
        case TokenType::MOD: return "MOD";
        case TokenType::POW: return "POW";
        case TokenType::AND: return "AND";
        case TokenType::OR: return "OR";
        case TokenType::UNLESS: return "UNLESS";
        case TokenType::SUM: return "SUM";
        case TokenType::AVG: return "AVG";
        case TokenType::COUNT: return "COUNT";
        case TokenType::MIN: return "MIN";
        case TokenType::MAX: return "MAX";
        case TokenType::STDDEV: return "STDDEV";
        case TokenType::STDVAR: return "STDVAR";
        case TokenType::TOPK: return "TOPK";
        case TokenType::BOTTOMK: return "BOTTOMK";
        case TokenType::COUNT_VALUES: return "COUNT_VALUES";
        case TokenType::QUANTILE: return "QUANTILE";
        case TokenType::BY: return "BY";
        case TokenType::WITHOUT: return "WITHOUT";
        case TokenType::ON: return "ON";
        case TokenType::IGNORING: return "IGNORING";
        case TokenType::GROUP_LEFT: return "GROUP_LEFT";
        case TokenType::GROUP_RIGHT: return "GROUP_RIGHT";
        case TokenType::OFFSET: return "OFFSET";
        case TokenType::BOOL: return "BOOL";
        case TokenType::DURATION: return "DURATION";
        case TokenType::AT: return "AT";
        case TokenType::START: return "START";
        case TokenType::END: return "END";
        case TokenType::SUBQUERY_RANGE: return "SUBQUERY_RANGE";
        default: return "UNKNOWN_TOKEN_TYPE";
    }
}

} // namespace promql
} // namespace prometheus
} // namespace tsdb 