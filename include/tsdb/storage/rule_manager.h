#ifndef TSDB_STORAGE_RULE_MANAGER_H_
#define TSDB_STORAGE_RULE_MANAGER_H_

#include "tsdb/core/types.h"
#include "tsdb/core/matcher.h"
#include <memory>
#include <atomic>
#include <vector>
#include <string>
#include <unordered_map>
#include <regex>
#include <mutex>

namespace tsdb {
namespace storage {

/**
 * @brief Action to take when a rule matches
 */
enum class RuleAction {
    DROP,
    KEEP, // Default
    MAP   // For mapping rules
};

/**
 * @brief A compiled rule set optimized for fast lookup.
 * This structure is immutable once created to support RCU.
 */
class RuleSet {
public:
    RuleSet() = default;
    
    // Copy constructor (Deep Copy)
    RuleSet(const RuleSet& other);
    
    // Move constructor
    RuleSet(RuleSet&& other) = default;
    
    // Assignment operators
    RuleSet& operator=(const RuleSet& other);
    RuleSet& operator=(RuleSet&& other) = default;
    
    // Check if a series should be dropped
    bool should_drop(const core::TimeSeries& series) const;
    
    // Apply mapping rules to a series (returns a new series if modified, or the original)
    core::TimeSeries apply_mapping(const core::TimeSeries& series) const;

    // --- Data Structures for Drop Rules ---
    
    // Exact match on metric name: name -> true (drop)
    std::unordered_map<std::string, bool> drop_exact_names;
    
    // Prefix match on metric name: simple Trie implementation
    struct TrieNode {
        std::unordered_map<char, std::unique_ptr<TrieNode>> children;
        bool is_leaf = false; // If true, drop any metric matching this prefix
        
        TrieNode() = default;
        
        // Deep copy helper
        std::unique_ptr<TrieNode> clone() const {
            auto new_node = std::make_unique<TrieNode>();
            new_node->is_leaf = is_leaf;
            for (const auto& [key, child] : children) {
                new_node->children[key] = child->clone();
            }
            return new_node;
        }
    };
    std::unique_ptr<TrieNode> drop_prefix_names;
    
    // Regex match on metric name: vector of regexes
    std::vector<std::regex> drop_regex_names;
    
    // Label rules: Label Name -> Matchers
    struct LabelRules {
        // Exact value match: value -> true (drop)
        std::unordered_map<std::string, bool> exact_values;
        // Regex value match: vector of regexes
        std::vector<std::regex> regex_values;
    };
    std::unordered_map<std::string, LabelRules> drop_label_rules;
    
    // --- Data Structures for Mapping Rules ---
    // (To be implemented: simpler for now, just a list of renames)
    struct MappingRule {
        std::string label_name;
        std::string old_value;
        std::string new_value;
        // Could add regex support here too
    };
    std::vector<MappingRule> mapping_rules;
    
    // Helper to build the Trie
    void add_drop_prefix(const std::string& prefix);
};

/**
 * @brief Manages filtering rules using RCU pattern for lock-free reads.
 */
class RuleManager {
public:
    RuleManager();
    
    // --- Hot Path (Lock-Free) ---
    std::shared_ptr<RuleSet> get_current_rules() const;
    
    // --- Configuration (Slow Path, Locked) ---
    // Add a drop rule based on a PromQL selector string (e.g., "up{env='dev'}")
    void add_drop_rule(const std::string& selector);
    
    // Clear all rules
    void clear_rules();
    
private:
    // The current active rule set
    // C++17: Use atomic_load/store free functions on shared_ptr
    std::shared_ptr<RuleSet> current_rules_;
    
    // Mutex for configuration updates (writers)
    std::mutex update_mutex_;
    
    // Helper to parse selector and populate a RuleSet builder
    void parse_selector_into_rules(const std::string& selector, RuleSet& rules);
};

} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_RULE_MANAGER_H_
