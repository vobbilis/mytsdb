#include "tsdb/storage/index.h"
#include <algorithm>

#include <regex>
#include <iostream>
#include <mutex>
#include <shared_mutex>

namespace tsdb {
namespace storage {

core::Result<void> Index::add_series(core::SeriesID id, const core::Labels& labels) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    // Store the series labels
    series_labels_[id] = labels;
    
    // Add to inverted index for each label pair
    // Insert in sorted order to avoid sorting on every query
    for (const auto& [key, value] : labels.map()) {
        auto label_pair = std::make_pair(key, value);
        auto& posting_list = postings_[label_pair];
        // Binary search to find insertion point (maintain sorted order)
        auto insert_pos = std::lower_bound(posting_list.begin(), posting_list.end(), id);
        // Only insert if not already present
        if (insert_pos == posting_list.end() || *insert_pos != id) {
            posting_list.insert(insert_pos, id);
        }
    }
    
    return core::Result<void>();
}

core::Result<void> Index::remove_series(core::SeriesID id) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    // 1. Find labels for the series
    auto it = series_labels_.find(id);
    if (it == series_labels_.end()) {
        // Series not found, nothing to do
        return core::Result<void>();
    }
    
    const auto& labels = it->second;
    
    // 2. Remove from inverted index
    for (const auto& [key, value] : labels.map()) {
        auto label_pair = std::make_pair(key, value);
        auto& posting_list = postings_[label_pair];
        
        // Remove id from posting list
        // Note: vector erase is O(N), but posting lists shouldn't be massive for high cardinality
        // If they are, we might need a better structure (e.g. Roaring Bitmap)
        auto pit = std::remove(posting_list.begin(), posting_list.end(), id);
        if (pit != posting_list.end()) {
            posting_list.erase(pit, posting_list.end());
        }
        
        // Clean up empty posting lists?
        if (posting_list.empty()) {
            postings_.erase(label_pair);
        }
    }
    
    // 3. Remove from forward index
    series_labels_.erase(it);
    
    return core::Result<void>();
}
core::Result<std::vector<core::SeriesID>> Index::find_series(const std::vector<core::LabelMatcher>& matchers) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::vector<core::SeriesID> candidates;
    bool candidates_initialized = false;



    // 1. Filter by Equality Matchers (Use Inverted Index)
    for (const auto& matcher : matchers) {
        if (matcher.type == core::MatcherType::Equal && !matcher.value.empty()) {
            // Lookup in inverted index
            auto it = postings_.find({matcher.name, matcher.value});
            if (it == postings_.end()) {
                // If any equality matcher finds nothing, the intersection is empty
                return core::Result<std::vector<core::SeriesID>>(std::vector<core::SeriesID>{});
            }

            if (!candidates_initialized) {
                // Postings lists are now maintained in sorted order at insert time
                candidates = it->second;
                candidates_initialized = true;
            } else {
                // Intersect with existing candidates
                // Postings lists are pre-sorted, so no sorting needed here
                const auto& current_posting = it->second;
                
                std::vector<core::SeriesID> intersection;
                intersection.reserve(std::min(candidates.size(), current_posting.size()));
                std::set_intersection(
                    candidates.begin(), candidates.end(),
                    current_posting.begin(), current_posting.end(),
                    std::back_inserter(intersection)
                );
                candidates = std::move(intersection);
            }

            if (candidates.empty()) {
                return core::Result<std::vector<core::SeriesID>>(std::vector<core::SeriesID>{});
            }
        }
    }

    // If no equality matchers were found (or only empty ones), start with all series
    if (!candidates_initialized) {
        candidates.reserve(series_labels_.size());
        for (const auto& [id, _] : series_labels_) {
            candidates.push_back(id);
        }
    }

    // 2. Filter by Other Matchers (Regex, NotEqual, Empty Equal)
    std::vector<core::SeriesID> result;
    result.reserve(candidates.size());

    for (auto id : candidates) {
        const auto& labels = series_labels_.at(id);
        bool matches = true;

        for (const auto& matcher : matchers) {
            // Skip equality matchers we already handled (optimization)
            // UNLESS it was an empty equality matcher (label="") which we skipped in step 1
            if (matcher.type == core::MatcherType::Equal && !matcher.value.empty()) {
                continue;
            }

            auto label_val_opt = labels.get(matcher.name);
            std::string label_val = label_val_opt.value_or("");

            if (matcher.type == core::MatcherType::Equal) {
                // Handle empty equality (label="")
                if (label_val != matcher.value) {
                    matches = false;
                    break;
                }
            } else if (matcher.type == core::MatcherType::NotEqual) {
                if (label_val == matcher.value) {
                    matches = false;
                    break;
                }
            } else if (matcher.type == core::MatcherType::RegexMatch) {
                try {
                    std::regex re(matcher.value);
                    if (!std::regex_match(label_val, re)) {
                        matches = false;
                        break;
                    }
                } catch (const std::regex_error&) {
                    matches = false;
                    break;
                }
            } else if (matcher.type == core::MatcherType::RegexNoMatch) {
                try {
                    std::regex re(matcher.value);
                    if (std::regex_match(label_val, re)) {
                        matches = false;
                        break;
                    }
                } catch (const std::regex_error&) {
                    // Ignore invalid regex?
                }
            }
        }

        if (matches) {
            result.push_back(id);
        }
    }
    
    return core::Result<std::vector<core::SeriesID>>(result);
}

core::Result<core::Labels> Index::get_labels(core::SeriesID id) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    auto it = series_labels_.find(id);
    if (it != series_labels_.end()) {
        return core::Result<core::Labels>(it->second);
    }
    return core::Result<core::Labels>::error("Series not found");
}

core::Result<std::vector<std::pair<core::SeriesID, core::Labels>>> Index::find_series_with_labels(
    const std::vector<core::LabelMatcher>& matchers) {
    
    // First get the series IDs using existing logic (already holds lock internally)
    auto ids_result = find_series(matchers);
    if (!ids_result.ok()) {
        return core::Result<std::vector<std::pair<core::SeriesID, core::Labels>>>::error(ids_result.error());
    }
    
    const auto& ids = ids_result.value();
    std::vector<std::pair<core::SeriesID, core::Labels>> results;
    results.reserve(ids.size());
    
    // Single lock acquisition for ALL label lookups
    std::shared_lock<std::shared_mutex> lock(mutex_);
    for (const auto& id : ids) {
        auto it = series_labels_.find(id);
        if (it != series_labels_.end()) {
            results.emplace_back(id, it->second);
        }
    }
    
    return core::Result<std::vector<std::pair<core::SeriesID, core::Labels>>>(std::move(results));
}

} // namespace storage
} // namespace tsdb
