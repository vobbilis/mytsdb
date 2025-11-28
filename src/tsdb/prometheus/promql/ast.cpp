#include "ast.h"
#include "lexer.h" // For TokenTypeToString
#include <sstream>

namespace tsdb {
namespace prometheus {
namespace promql {

std::string NumberLiteralNode::String() const {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

std::string StringLiteralNode::String() const {
    // TODO: Add proper escaping for quotes within the string if needed for representation
    return "\"" + value + "\"";
}

std::string VectorSelectorNode::String() const {
    std::string s = name;
    if (!labelMatchers.empty()) {
        s += "{";
        for (size_t i = 0; i < labelMatchers.size(); ++i) {
            s += labelMatchers[i].name;
            switch (labelMatchers[i].type) {
                case model::MatcherType::EQUAL: s += "="; break;
                case model::MatcherType::NOT_EQUAL: s += "!="; break;
                case model::MatcherType::REGEX_MATCH: s += "=~"; break;
                case model::MatcherType::REGEX_NO_MATCH: s += "!~"; break;
            }
            s += "\"" + labelMatchers[i].value + "\"";
            if (i < labelMatchers.size() - 1) {
                s += ",";
            }
        }
        s += "}";
    }
    if (originalOffset.type != TokenType::ILLEGAL) { // Assuming ILLEGAL means not present
        s += " offset " + originalOffset.literal;
    }
    // TODO: Add @ modifier string representation
    return s;
}

std::string MatrixSelectorNode::String() const {
    return vectorSelector->String() + "[" + range.literal + "]";
}

std::string BinaryExprNode::String() const {
    std::string s = "(" + lhs->String() + " " + TokenTypeToString(op) + " ";
    // TODO: Add vector matching clauses (on, ignoring, group_left, group_right) to string representation
    s += rhs->String() + ")";
    return s;
}

std::string UnaryExprNode::String() const {
    return TokenTypeToString(op) + "(" + expr->String() + ")";
}

std::string ParenExprNode::String() const {
    return "(" + expr->String() + ")";
}

std::string CallNode::String() const {
    std::string s = funcName + "(";
    for (size_t i = 0; i < args.size(); ++i) {
        s += args[i]->String();
        if (i < args.size() - 1) {
            s += ", ";
        }
    }
    s += ")";
    return s;
}

std::string AggregateExprNode::String() const {
    std::string s = TokenTypeToString(opType) + " ";
    if (param) {
        s += "(" + param->String() + ", " + expr->String() + ")";
    } else {
        s += "(" + expr->String() + ")";
    }
    if (!groupingLabels.empty()) {
        s += " " + std::string(without ? "without" : "by") + " (";
        for (size_t i = 0; i < groupingLabels.size(); ++i) {
            s += groupingLabels[i];
            if (i < groupingLabels.size() - 1) {
                s += ", ";
            }
        }
        s += ")";
    }
    return s;
}

std::string SubqueryExprNode::String() const {
    std::string s = expr->String() + "[" + range.literal;
    if (resolution.type != TokenType::ILLEGAL && !resolution.literal.empty()) {
        s += ":" + resolution.literal;
    }
    s += "]";
    if (originalOffset.type != TokenType::ILLEGAL && !originalOffset.literal.empty()) {
        s += " offset " + originalOffset.literal;
    }
    // TODO: Add @ modifier string representation
    return s;
}

} // namespace promql
} // namespace prometheus
} // namespace tsdb 