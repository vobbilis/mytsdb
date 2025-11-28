#ifndef TSDB_CORE_MATCHER_H_
#define TSDB_CORE_MATCHER_H_

#include <string>

namespace tsdb {
namespace core {

/**
 * @brief Type of label matching
 */
enum class MatcherType {
    Equal,          // =
    NotEqual,       // !=
    RegexMatch,     // =~
    RegexNoMatch    // !~
};

/**
 * @brief Matcher for filtering time series by labels
 */
struct LabelMatcher {
    MatcherType type;
    std::string name;
    std::string value;

    LabelMatcher(MatcherType t, std::string n, std::string v)
        : type(t), name(std::move(n)), value(std::move(v)) {}
        
    // Default constructor
    LabelMatcher() : type(MatcherType::Equal) {}
};

} // namespace core
} // namespace tsdb

#endif // TSDB_CORE_MATCHER_H_
