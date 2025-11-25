#include "tsdb/storage/index.h"
#include <algorithm>

namespace tsdb {
namespace storage {

core::Result<void> Index::add_series(core::SeriesID id, const core::Labels& labels) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Store the series labels
    series_labels_[id] = labels;
    
    // Add to inverted index for each label pair
    for (const auto& [key, value] : labels.map()) {
        auto label_pair = std::make_pair(key, value);
        postings_[label_pair].push_back(id);
    }
    
    return core::Result<void>();
}

core::Result<std::vector<core::SeriesID>> Index::find_series(const std::vector<std::pair<std::string, std::string>>& matchers) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<core::SeriesID> result;
    
    if (matchers.empty()) {
        // Return all series if no matchers
        for (const auto& [id, labels] : series_labels_) {
            result.push_back(id);
        }
        return core::Result<std::vector<core::SeriesID>>(result);
    }
    
    // Find series that match all matchers
    for (const auto& [id, labels] : series_labels_) {
        bool matches = true;
        for (const auto& [key, value] : matchers) {
            bool found = false;
            for (const auto& [label_key, label_value] : labels.map()) {
                if (label_key == key && label_value == value) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                matches = false;
                break;
            }
        }
        if (matches) {
            result.push_back(id);
        }
    }
    
    return core::Result<std::vector<core::SeriesID>>(result);
}

core::Result<core::Labels> Index::get_labels(core::SeriesID id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = series_labels_.find(id);
    if (it != series_labels_.end()) {
        return core::Result<core::Labels>(it->second);
    }
    return core::Result<core::Labels>::error("Series not found");
}

} // namespace storage
} // namespace tsdb
