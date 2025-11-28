#include "tsdb/storage/index.h"
#include <algorithm>

#include <regex>
#include <iostream>

namespace tsdb {
namespace storage {

core::Result<void> Index::add_series(core::SeriesID id, const core::Labels& labels) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    // Store the series labels
    series_labels_[id] = labels;
    
    // Add to inverted index for each label pair
    for (const auto& [key, value] : labels.map()) {
        auto label_pair = std::make_pair(key, value);
        postings_[label_pair].push_back(id);
    }
    
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
                candidates = it->second;
                std::sort(candidates.begin(), candidates.end());
                candidates_initialized = true;
            } else {
                // Intersect with existing candidates
                // Note: postings lists are NOT sorted because SeriesID is a hash
                // We must sort before using set_intersection
                std::vector<core::SeriesID> current_posting = it->second;
                std::sort(current_posting.begin(), current_posting.end());
                
                std::vector<core::SeriesID> intersection;
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

} // namespace storage
} // namespace tsdb
