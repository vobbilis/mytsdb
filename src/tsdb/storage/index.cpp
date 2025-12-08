#include "tsdb/storage/index.h"
#include <algorithm>
#include <regex>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <chrono>

namespace tsdb {
namespace storage {

Index::Index() {
    // Initialize per-index metrics
    metrics_.reset();
}

Index::~Index() = default;

// Helper to convert posting list to vector
std::vector<core::SeriesID> Index::posting_list_to_vector(const PostingList& pl) const {
    std::vector<core::SeriesID> result;
#if USE_ROARING_BITMAPS
    result.reserve(pl.cardinality());
    for (auto it = pl.begin(); it != pl.end(); ++it) {
        result.push_back(*it);
    }
#else
    result = pl;
#endif
    return result;
}

// Helper to intersect two posting lists
Index::PostingList Index::intersect_posting_lists(const PostingList& a, const PostingList& b) const {
    auto start = std::chrono::high_resolution_clock::now();
    
#if USE_ROARING_BITMAPS
    PostingList result = a & b;  // Roaring bitwise AND - extremely fast!
#else
    PostingList result;
    result.reserve(std::min(a.size(), b.size()));
    std::set_intersection(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(result));
#endif
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    metrics_.intersect_count.fetch_add(1, std::memory_order_relaxed);
    metrics_.intersect_time_us.fetch_add(duration_us, std::memory_order_relaxed);
    
    return result;
}

core::Result<void> Index::add_series(core::SeriesID id, const core::Labels& labels) {
    auto start = std::chrono::high_resolution_clock::now();
    
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    // Store the series labels
    series_labels_[id] = labels;
    
    // Add to inverted index for each label pair
    for (const auto& [key, value] : labels.map()) {
        auto label_pair = std::make_pair(key, value);
        auto& posting_list = postings_[label_pair];
        
#if USE_ROARING_BITMAPS
        posting_list.add(id);  // O(1) for Roaring
#else
        // Binary search to find insertion point (maintain sorted order)
        auto insert_pos = std::lower_bound(posting_list.begin(), posting_list.end(), id);
        // Only insert if not already present
        if (insert_pos == posting_list.end() || *insert_pos != id) {
            posting_list.insert(insert_pos, id);
        }
#endif
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    metrics_.add_count.fetch_add(1, std::memory_order_relaxed);
    metrics_.add_time_us.fetch_add(duration_us, std::memory_order_relaxed);
    
    return core::Result<void>();
}

core::Result<void> Index::remove_series(core::SeriesID id) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    // 1. Find labels for the series
    auto it = series_labels_.find(id);
    if (it == series_labels_.end()) {
        return core::Result<void>();
    }
    
    const auto& labels = it->second;
    
    // 2. Remove from inverted index
    for (const auto& [key, value] : labels.map()) {
        auto label_pair = std::make_pair(key, value);
        auto pit = postings_.find(label_pair);
        if (pit != postings_.end()) {
#if USE_ROARING_BITMAPS
            pit->second.remove(id);  // O(1) for Roaring
            if (pit->second.isEmpty()) {
                postings_.erase(pit);
            }
#else
            auto& posting_list = pit->second;
            auto rm_it = std::remove(posting_list.begin(), posting_list.end(), id);
            if (rm_it != posting_list.end()) {
                posting_list.erase(rm_it, posting_list.end());
            }
            if (posting_list.empty()) {
                postings_.erase(pit);
            }
#endif
        }
    }
    
    // 3. Remove from forward index
    series_labels_.erase(it);
    
    return core::Result<void>();
}

core::Result<std::vector<core::SeriesID>> Index::find_series(const std::vector<core::LabelMatcher>& matchers) {
    auto start = std::chrono::high_resolution_clock::now();
    
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    PostingList candidates;
    bool candidates_initialized = false;

    // 1. Filter by Equality Matchers (Use Inverted Index)
    for (const auto& matcher : matchers) {
        if (matcher.type == core::MatcherType::Equal && !matcher.value.empty()) {
            // Lookup in inverted index
            auto it = postings_.find({matcher.name, matcher.value});
            if (it == postings_.end()) {
                // If any equality matcher finds nothing, the intersection is empty
                auto end = std::chrono::high_resolution_clock::now();
                auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
                metrics_.lookup_count.fetch_add(1, std::memory_order_relaxed);
                metrics_.lookup_time_us.fetch_add(duration_us, std::memory_order_relaxed);
                return core::Result<std::vector<core::SeriesID>>(std::vector<core::SeriesID>{});
            }

            if (!candidates_initialized) {
                candidates = it->second;
                candidates_initialized = true;
            } else {
                // Intersect with existing candidates - THIS IS WHERE ROARING SHINES
                candidates = intersect_posting_lists(candidates, it->second);
            }

#if USE_ROARING_BITMAPS
            if (candidates.isEmpty()) {
#else
            if (candidates.empty()) {
#endif
                auto end = std::chrono::high_resolution_clock::now();
                auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
                metrics_.lookup_count.fetch_add(1, std::memory_order_relaxed);
                metrics_.lookup_time_us.fetch_add(duration_us, std::memory_order_relaxed);
                return core::Result<std::vector<core::SeriesID>>(std::vector<core::SeriesID>{});
            }
        }
    }

    // If no equality matchers were found, start with all series
    if (!candidates_initialized) {
#if USE_ROARING_BITMAPS
        for (const auto& [id, _] : series_labels_) {
            candidates.add(id);
        }
#else
        candidates.reserve(series_labels_.size());
        for (const auto& [id, _] : series_labels_) {
            candidates.push_back(id);
        }
#endif
    }

    // Convert posting list to vector for iteration
    std::vector<core::SeriesID> candidate_vec = posting_list_to_vector(candidates);

    // 2. Filter by Other Matchers (Regex, NotEqual, Empty Equal)
    std::vector<core::SeriesID> result;
    result.reserve(candidate_vec.size());

    for (auto id : candidate_vec) {
        auto labels_it = series_labels_.find(id);
        if (labels_it == series_labels_.end()) continue;
        
        const auto& labels = labels_it->second;
        bool matches = true;

        for (const auto& matcher : matchers) {
            // Skip equality matchers we already handled
            if (matcher.type == core::MatcherType::Equal && !matcher.value.empty()) {
                continue;
            }

            auto label_val_opt = labels.get(matcher.name);
            std::string label_val = label_val_opt.value_or("");

            if (matcher.type == core::MatcherType::Equal) {
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
                    // Ignore invalid regex
                }
            }
        }

        if (matches) {
            result.push_back(id);
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    metrics_.lookup_count.fetch_add(1, std::memory_order_relaxed);
    metrics_.lookup_time_us.fetch_add(duration_us, std::memory_order_relaxed);
    
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
    
    // First get the series IDs
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

size_t Index::num_series() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return series_labels_.size();
}

size_t Index::num_posting_lists() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return postings_.size();
}

size_t Index::memory_usage_bytes() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    size_t total = 0;
    
#if USE_ROARING_BITMAPS
    for (const auto& [key, pl] : postings_) {
        total += key.first.size() + key.second.size();
        total += pl.getSizeInBytes();  // Roaring provides exact size
    }
#else
    for (const auto& [key, pl] : postings_) {
        total += key.first.size() + key.second.size();
        total += pl.size() * sizeof(core::SeriesID);
    }
#endif
    
    // Forward index estimate
    for (const auto& [id, labels] : series_labels_) {
        total += sizeof(core::SeriesID);
        for (const auto& [k, v] : labels.map()) {
            total += k.size() + v.size();
        }
    }
    
    return total;
}

} // namespace storage
} // namespace tsdb
