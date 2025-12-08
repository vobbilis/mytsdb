#ifndef TSDB_STORAGE_INDEX_H_
#define TSDB_STORAGE_INDEX_H_

#include <vector>
#include <string>
#include <map>
#include <set>
#include <shared_mutex>
#include <atomic>
#include "tsdb/core/types.h"
#include "tsdb/core/result.h"
#include "tsdb/core/matcher.h"
#include "tsdb/storage/index_metrics.h"

// Use Roaring Bitmaps if available (10-50x faster intersections)
#ifdef HAVE_ROARING
#include <roaring/roaring64map.hh>
#define USE_ROARING_BITMAPS 1
#else
#define USE_ROARING_BITMAPS 0
#endif

// Use absl::flat_hash_map if available (2-5x faster lookups)
// Arrow already links abseil, so we can use it
#include <absl/container/flat_hash_map.h>
#define USE_FLAT_HASH_MAP 1

namespace tsdb {
namespace storage {

/**
 * @brief Per-index metrics for performance monitoring
 */
struct PerIndexMetrics {
    std::atomic<uint64_t> add_count{0};
    std::atomic<uint64_t> lookup_count{0};
    std::atomic<uint64_t> intersect_count{0};
    std::atomic<uint64_t> add_time_us{0};
    std::atomic<uint64_t> lookup_time_us{0};
    std::atomic<uint64_t> intersect_time_us{0};
    
    void reset() {
        add_count = 0;
        lookup_count = 0;
        intersect_count = 0;
        add_time_us = 0;
        lookup_time_us = 0;
        intersect_time_us = 0;
    }
};

/**
 * @brief High-performance inverted index for time series labels
 * 
 * Optimizations:
 * 1. Roaring Bitmaps for posting lists (10-50x faster intersections)
 * 2. absl::flat_hash_map for O(1) lookups with better cache locality
 * 3. Performance metrics for monitoring and validation
 */
class Index {
public:
    Index();
    ~Index();
    
    core::Result<void> add_series(core::SeriesID id, const core::Labels& labels);
    core::Result<void> remove_series(core::SeriesID id);
    core::Result<std::vector<core::SeriesID>> find_series(const std::vector<core::LabelMatcher>& matchers);
    core::Result<core::Labels> get_labels(core::SeriesID id);
    
    // Optimized: Returns series IDs with their labels in a single lock acquisition
    core::Result<std::vector<std::pair<core::SeriesID, core::Labels>>> find_series_with_labels(
        const std::vector<core::LabelMatcher>& matchers);
    
    // Get index statistics for monitoring
    size_t num_series() const;
    size_t num_posting_lists() const;
    size_t memory_usage_bytes() const;
    
    // Get per-index metrics
    PerIndexMetrics& get_metrics() { return metrics_; }
    const PerIndexMetrics& get_metrics() const { return metrics_; }

private:
    // Hash function for label pair keys
    struct LabelPairHash {
        size_t operator()(const std::pair<std::string, std::string>& p) const {
            // FNV-1a hash combination
            size_t h1 = std::hash<std::string>{}(p.first);
            size_t h2 = std::hash<std::string>{}(p.second);
            return h1 ^ (h2 * 0x9e3779b97f4a7c15ULL);
        }
    };

#if USE_ROARING_BITMAPS
    // Roaring bitmap for fast set operations
    using PostingList = roaring::Roaring64Map;
#else
    // Fallback to sorted vector
    using PostingList = std::vector<core::SeriesID>;
#endif

#if USE_FLAT_HASH_MAP
    // The inverted index: maps a label pair to posting list
    absl::flat_hash_map<std::pair<std::string, std::string>, PostingList, LabelPairHash> postings_;
    
    // A forward index: maps a series ID to its labels
    absl::flat_hash_map<core::SeriesID, core::Labels> series_labels_;
#else
    // Fallback to std::map
    std::map<std::pair<std::string, std::string>, PostingList> postings_;
    std::map<core::SeriesID, core::Labels> series_labels_;
#endif

    mutable std::shared_mutex mutex_;  // Protects concurrent access to index
    mutable PerIndexMetrics metrics_;  // Per-index performance metrics
    
    // Helper to convert posting list to vector of IDs
    std::vector<core::SeriesID> posting_list_to_vector(const PostingList& pl) const;
    
    // Helper to intersect two posting lists
    PostingList intersect_posting_lists(const PostingList& a, const PostingList& b) const;
};

} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_INDEX_H_
