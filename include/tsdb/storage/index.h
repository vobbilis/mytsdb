#ifndef TSDB_STORAGE_INDEX_H_
#define TSDB_STORAGE_INDEX_H_

#include <vector>
#include <string>
#include <map>
#include <set>
#include <mutex>
#include "tsdb/core/types.h"
#include "tsdb/core/result.h"

namespace tsdb {
namespace storage {

class Index {
public:
    core::Result<void> add_series(core::SeriesID id, const core::Labels& labels);
    core::Result<std::vector<core::SeriesID>> find_series(const std::vector<std::pair<std::string, std::string>>& matchers);
    core::Result<core::Labels> get_labels(core::SeriesID id);

private:
    // The inverted index: maps a label pair to a sorted list of series IDs.
    std::map<std::pair<std::string, std::string>, std::vector<core::SeriesID>> postings_;
    
    // A forward index: maps a series ID to its labels.
    std::map<core::SeriesID, core::Labels> series_labels_;
    
    mutable std::mutex mutex_;  // Protects concurrent access to index
};

} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_INDEX_H_
