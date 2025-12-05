#include "tsdb/storage/rule_manager.h"
#include "tsdb/prometheus/promql/parser.h"
#include "tsdb/prometheus/promql/lexer.h"
#include "tsdb/common/logger.h"
#include <iostream>
#include <string>

namespace tsdb {
namespace storage {

// --- RuleSet Implementation ---

RuleSet::RuleSet(const RuleSet& other) {
    drop_exact_names = other.drop_exact_names;
    if (other.drop_prefix_names) {
        drop_prefix_names = other.drop_prefix_names->clone();
    }
    drop_regex_names = other.drop_regex_names;
    drop_label_rules = other.drop_label_rules;
    mapping_rules = other.mapping_rules;
}

RuleSet& RuleSet::operator=(const RuleSet& other) {
    if (this != &other) {
        drop_exact_names = other.drop_exact_names;
        if (other.drop_prefix_names) {
            drop_prefix_names = other.drop_prefix_names->clone();
        } else {
            drop_prefix_names.reset();
        }
        drop_regex_names = other.drop_regex_names;
        drop_label_rules = other.drop_label_rules;
        mapping_rules = other.mapping_rules;
    }
    return *this;
}

void RuleSet::add_drop_prefix(const std::string& prefix) {
    if (!drop_prefix_names) {
        drop_prefix_names = std::make_unique<TrieNode>();
    }
    TrieNode* current = drop_prefix_names.get();
    for (char c : prefix) {
        if (current->children.find(c) == current->children.end()) {
            current->children[c] = std::make_unique<TrieNode>();
        }
        current = current->children[c].get();
    }
    current->is_leaf = true;
}

bool RuleSet::should_drop(const core::TimeSeries& series) const {
    // 1. Check Metric Name
    std::string name;
    if (series.labels().has("__name__")) {
        auto opt_name = series.labels().get("__name__");
        if (opt_name) {
            name = *opt_name;
            
            // Exact match
            if (drop_exact_names.count(name)) {
                return true;
            }
            
            // Prefix match (Trie)
            if (drop_prefix_names) {
                TrieNode* current = drop_prefix_names.get();
                bool matched = true;
                for (char c : name) {
                    if (current->is_leaf) return true; // Prefix matched
                    if (current->children.find(c) == current->children.end()) {
                        matched = false;
                        break;
                    }
                    current = current->children[c].get();
                }
                if (matched && current->is_leaf) return true;
            }
            
            // Regex match
            for (const auto& re : drop_regex_names) {
                if (std::regex_match(name, re)) {
                    return true;
                }
            }
        }
    }
    
    // 2. Check Labels
    for (const auto& [label_name, rules] : drop_label_rules) {
        if (series.labels().has(label_name)) {
            auto opt_value = series.labels().get(label_name);
            if (opt_value) {
                std::string value = *opt_value;
                
                // Exact value match
                if (rules.exact_values.count(value)) {
                    return true;
                }
                
                // Regex value match
                for (const auto& re : rules.regex_values) {
                    if (std::regex_match(value, re)) {
                        return true;
                    }
                }
            }
        }
    }
    
    return false;
}

core::TimeSeries RuleSet::apply_mapping(const core::TimeSeries& series) const {
    if (mapping_rules.empty()) {
        return series; 
    }
    
    // Placeholder implementation
    return series;
}

// --- RuleManager Implementation ---

RuleManager::RuleManager() {
    // Start with empty rules
    std::atomic_store(&current_rules_, std::make_shared<RuleSet>());
}

std::shared_ptr<RuleSet> RuleManager::get_current_rules() const {
    return std::atomic_load(&current_rules_);
}

void RuleManager::add_drop_rule(const std::string& selector) {
    std::lock_guard<std::mutex> lock(update_mutex_);
    
    // 1. Create a copy of the current rules
    auto current = std::atomic_load(&current_rules_);
    auto new_rules = std::make_shared<RuleSet>(*current); // Copy constructor
    
    // 2. Parse selector and update new_rules
    parse_selector_into_rules(selector, *new_rules);
    
    // 3. Atomically swap
    std::atomic_store(&current_rules_, new_rules);
    
    TSDB_INFO("Added drop rule: {}", selector);
}

void RuleManager::clear_rules() {
    std::lock_guard<std::mutex> lock(update_mutex_);
    std::atomic_store(&current_rules_, std::make_shared<RuleSet>());
    TSDB_INFO("Cleared all rules");
}

void RuleManager::parse_selector_into_rules(const std::string& selector, RuleSet& rules) {
    // Use PromQL parser to parse the selector
    try {
        prometheus::promql::Lexer lexer(selector);
        prometheus::promql::Parser parser(lexer);
        
        // We expect a VectorSelector
        auto expr = parser.ParseExpr();
        
        if (!expr) {
            TSDB_ERROR("Failed to parse drop rule selector: {}", selector);
            return;
        }
        
        auto vecSel = dynamic_cast<prometheus::promql::VectorSelectorNode*>(expr.get());
        if (!vecSel) {
            TSDB_ERROR("Drop rule must be a vector selector: {}", selector);
            return;
        }
        
        // Process Metric Name
        if (!vecSel->name.empty()) {
            rules.drop_exact_names[vecSel->name] = true;
        }
        
        // Process Matchers
        for (const auto& matcher : vecSel->labelMatchers) {
            if (matcher.name == "__name__") {
                // Name matchers
                switch (matcher.type) {
                    case prometheus::model::MatcherType::EQUAL:
                        rules.drop_exact_names[matcher.value] = true;
                        break;
                    case prometheus::model::MatcherType::REGEX_MATCH:
                        rules.drop_regex_names.push_back(std::regex(matcher.value));
                        break;
                    default:
                        TSDB_WARN("Unsupported matcher type for __name__ in drop rule");
                        break;
                }
            } else {
                // Label matchers
                auto& label_rule = rules.drop_label_rules[matcher.name];
                switch (matcher.type) {
                    case prometheus::model::MatcherType::EQUAL:
                        label_rule.exact_values[matcher.value] = true;
                        break;
                    case prometheus::model::MatcherType::REGEX_MATCH:
                        label_rule.regex_values.push_back(std::regex(matcher.value));
                        break;
                    default:
                        TSDB_WARN("Unsupported matcher type for label {} in drop rule", matcher.name);
                        break;
                }
            }
        }
        
    } catch (const std::exception& e) {
        TSDB_ERROR("Exception parsing drop rule: {}", e.what());
    }
}

} // namespace storage
} // namespace tsdb
