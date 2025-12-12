#include "tsdb/storage/sharded_index.h"
#include <iostream>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <mutex>

namespace tsdb {
namespace storage {

ShardedIndex::ShardedIndex(size_t num_shards)
    : num_shards_(num_shards) {
    
    shards_.reserve(num_shards_);
    for (size_t i = 0; i < num_shards_; ++i) {
        shards_.push_back(std::make_unique<Index>());
    }

    shard_query_counts_ = std::make_unique<std::atomic<uint64_t>[]>(num_shards_);
    for (size_t i = 0; i < num_shards_; ++i) {
        shard_query_counts_[i].store(0, std::memory_order_relaxed);
    }
}

core::Result<void> ShardedIndex::add_series(core::SeriesID id, const core::Labels& labels) {
    size_t shard_idx = get_shard_index(id);
    auto result = shards_[shard_idx]->add_series(id, labels);
    
    if (result.ok()) {
        total_series_++;

        // Step 1.3: record metric -> shard mapping for routing.
        auto metric_opt = labels.get("__name__");
        const std::string metric = metric_opt.value_or("");
        if (!metric.empty()) {
            std::lock_guard<std::mutex> g(routing_mutex_);
            auto& counts = metric_shard_counts_[metric];
            if (counts.size() != num_shards_) {
                counts.assign(num_shards_, 0);
            }
            counts[shard_idx] += 1;
        }
    }
    
    return result;
}

core::Result<void> ShardedIndex::remove_series(core::SeriesID id) {
    size_t shard_idx = get_shard_index(id);
    // Step 1.3: record metric name before removal so we can update routing map.
    std::string metric;
    {
        auto labels_res = shards_[shard_idx]->get_labels(id);
        if (labels_res.ok()) {
            metric = labels_res.value().get("__name__").value_or("");
        }
    }

    auto result = shards_[shard_idx]->remove_series(id);
    
    if (result.ok()) {
        total_series_--;

        if (!metric.empty()) {
            std::lock_guard<std::mutex> g(routing_mutex_);
            auto it = metric_shard_counts_.find(metric);
            if (it != metric_shard_counts_.end() && it->second.size() == num_shards_) {
                auto& counts = it->second;
                if (counts[shard_idx] > 0) {
                    counts[shard_idx] -= 1;
                }
                bool any = false;
                for (auto c : counts) {
                    if (c > 0) {
                        any = true;
                        break;
                    }
                }
                if (!any) {
                    metric_shard_counts_.erase(it);
                }
            }
        }
    }
    
    return result;
}

std::vector<size_t> ShardedIndex::get_routed_shards_for_metric(const std::string& metric_name) const {
    std::vector<size_t> shards;
    if (metric_name.empty()) {
        return shards;
    }

    std::lock_guard<std::mutex> g(routing_mutex_);
    auto it = metric_shard_counts_.find(metric_name);
    if (it == metric_shard_counts_.end()) {
        return shards; // unknown -> fallback to scatter-gather for correctness
    }

    const auto& counts = it->second;
    if (counts.size() != num_shards_) {
        return shards;
    }
    for (size_t i = 0; i < num_shards_; ++i) {
        if (counts[i] > 0) {
            shards.push_back(i);
        }
    }
    return shards;
}

core::Result<std::vector<core::SeriesID>> ShardedIndex::find_series(
    const std::vector<core::LabelMatcher>& matchers) {
    
    total_lookups_++;

    // Step 1.3: route __name__="metric" to only shards containing that metric
    std::string metric_name;
    for (const auto& m : matchers) {
        if (m.type == core::MatcherType::Equal && m.name == "__name__" && !m.value.empty()) {
            metric_name = m.value;
            break;
        }
    }
    std::vector<size_t> routed = get_routed_shards_for_metric(metric_name);
    const bool use_routing = !metric_name.empty() && !routed.empty() && routed.size() < num_shards_;
    
    // OPTIMIZATION: Parallel scatter-gather across all shards using TBB
    // This reduces index lookup time by ~8-16x on multi-core systems
    const size_t shard_count = use_routing ? routed.size() : num_shards_;
    std::vector<std::vector<core::SeriesID>> shard_results(num_shards_);
    
    tbb::parallel_for(tbb::blocked_range<size_t>(0, shard_count),
        [&](const tbb::blocked_range<size_t>& range) {
            for (size_t i = range.begin(); i < range.end(); ++i) {
                const size_t shard_idx = use_routing ? routed[i] : i;
                shard_query_counts_[shard_idx].fetch_add(1, std::memory_order_relaxed);
                auto shard_result = shards_[shard_idx]->find_series(matchers);
                if (shard_result.ok()) {
                    shard_results[shard_idx] = std::move(shard_result.value());
                }
            }
        });
    
    // Gather results
    std::vector<core::SeriesID> result;
    size_t total_size = 0;
    for (const auto& sr : shard_results) {
        total_size += sr.size();
    }
    result.reserve(total_size);
    
    for (auto& sr : shard_results) {
        result.insert(result.end(), 
                     std::make_move_iterator(sr.begin()),
                     std::make_move_iterator(sr.end()));
    }
    
    return core::Result<std::vector<core::SeriesID>>(std::move(result));
}

core::Result<core::Labels> ShardedIndex::get_labels(core::SeriesID id) {
    size_t shard_idx = get_shard_index(id);
    return shards_[shard_idx]->get_labels(id);
}

core::Result<std::vector<std::pair<core::SeriesID, core::Labels>>> ShardedIndex::find_series_with_labels(
    const std::vector<core::LabelMatcher>& matchers) {
    
    total_lookups_++;

    // Step 1.3: route __name__="metric" to only shards containing that metric
    std::string metric_name;
    for (const auto& m : matchers) {
        if (m.type == core::MatcherType::Equal && m.name == "__name__" && !m.value.empty()) {
            metric_name = m.value;
            break;
        }
    }
    std::vector<size_t> routed = get_routed_shards_for_metric(metric_name);
    const bool use_routing = !metric_name.empty() && !routed.empty() && routed.size() < num_shards_;
    
    // OPTIMIZATION: Parallel scatter-gather across all shards using TBB
    std::vector<std::vector<std::pair<core::SeriesID, core::Labels>>> shard_results(num_shards_);
    
    const size_t shard_count = use_routing ? routed.size() : num_shards_;
    tbb::parallel_for(tbb::blocked_range<size_t>(0, shard_count),
        [&](const tbb::blocked_range<size_t>& range) {
            for (size_t i = range.begin(); i < range.end(); ++i) {
                const size_t shard_idx = use_routing ? routed[i] : i;
                shard_query_counts_[shard_idx].fetch_add(1, std::memory_order_relaxed);
                auto shard_result = shards_[shard_idx]->find_series_with_labels(matchers);
                if (shard_result.ok()) {
                    shard_results[shard_idx] = std::move(shard_result.value());
                }
            }
        });
    
    // Gather results
    std::vector<std::pair<core::SeriesID, core::Labels>> result;
    size_t total_size = 0;
    for (const auto& sr : shard_results) {
        total_size += sr.size();
    }
    result.reserve(total_size);
    
    for (auto& sr : shard_results) {
        result.insert(result.end(), 
                     std::make_move_iterator(sr.begin()),
                     std::make_move_iterator(sr.end()));
    }
    
    return core::Result<std::vector<std::pair<core::SeriesID, core::Labels>>>(std::move(result));
}

IndexStats ShardedIndex::get_stats() const {
    return IndexStats{
        total_series_.load(),
        total_lookups_.load()
    };
}

AggregatedIndexMetrics ShardedIndex::get_aggregated_metrics() const {
    AggregatedIndexMetrics agg;
    
    for (const auto& shard : shards_) {
        const auto& metrics = shard->get_metrics();
        agg.total_add_count += metrics.add_count.load();
        agg.total_lookup_count += metrics.lookup_count.load();
        agg.total_intersect_count += metrics.intersect_count.load();
        agg.total_add_time_us += metrics.add_time_us.load();
        agg.total_lookup_time_us += metrics.lookup_time_us.load();
        agg.total_intersect_time_us += metrics.intersect_time_us.load();
    }
    
    return agg;
}

void ShardedIndex::reset_metrics() {
    total_series_ = 0;
    total_lookups_ = 0;
    
    for (auto& shard : shards_) {
        shard->get_metrics().reset();
    }
}

std::vector<uint64_t> ShardedIndex::get_shard_query_counts() const {
    std::vector<uint64_t> out;
    out.reserve(num_shards_);
    for (size_t i = 0; i < num_shards_; ++i) {
        out.push_back(shard_query_counts_[i].load(std::memory_order_relaxed));
    }
    return out;
}

void ShardedIndex::reset_shard_query_counts() {
    for (size_t i = 0; i < num_shards_; ++i) {
        shard_query_counts_[i].store(0, std::memory_order_relaxed);
    }
}

size_t ShardedIndex::get_shard_index(core::SeriesID id) const {
    // Simple modulo sharding for integer IDs
    return id % num_shards_;
}

} // namespace storage
} // namespace tsdb
