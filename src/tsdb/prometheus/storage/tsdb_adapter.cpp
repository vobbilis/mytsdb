#include "tsdb/prometheus/storage/tsdb_adapter.h"
#include "tsdb/core/matcher.h"
#include <regex>
#include <iostream>
#include <chrono>
#include <future>
#include <algorithm>

namespace tsdb {
namespace prometheus {
namespace storage {

TSDBAdapter::TSDBAdapter(std::shared_ptr<tsdb::storage::Storage> storage)
    : storage_(std::move(storage)) {}

promql::Matrix TSDBAdapter::SelectSeries(
    const std::vector<model::LabelMatcher>& matchers,
    int64_t start,
    int64_t end) {
    
    auto start_time = std::chrono::high_resolution_clock::now();

    // 1. Convert matchers to core::LabelMatcher
    std::vector<core::LabelMatcher> core_matchers;
    core_matchers.reserve(matchers.size());

    for (const auto& matcher : matchers) {
        core::MatcherType type;
        switch (matcher.type) {
            case model::MatcherType::EQUAL: type = core::MatcherType::Equal; break;
            case model::MatcherType::NOT_EQUAL: type = core::MatcherType::NotEqual; break;
            case model::MatcherType::REGEX_MATCH: type = core::MatcherType::RegexMatch; break;
            case model::MatcherType::REGEX_NO_MATCH: type = core::MatcherType::RegexNoMatch; break;
            default: type = core::MatcherType::Equal; break;
        }
        core_matchers.emplace_back(type, matcher.name, matcher.value);
    }

    // 2. Query storage (Pushdown all matchers!)
    auto result = storage_->query(core_matchers, start, end);
    if (!result.ok()) {
        throw std::runtime_error("Storage query failed: " + result.error());
    }

    const auto& ts_series_list = result.value();
    promql::Matrix matrix;
    
    // Reserve memory
    matrix.reserve(ts_series_list.size());

    // 3. Convert (Parallelized) - No filtering needed!
    size_t num_series = ts_series_list.size();
    if (num_series == 0) {
        return matrix;
    }

    // Determine number of threads
    unsigned int num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 4; // Fallback
    
    // Don't spawn threads for small datasets
    if (num_series < 100) {
        num_threads = 1;
    }

    size_t chunk_size = (num_series + num_threads - 1) / num_threads;
    std::vector<std::future<promql::Matrix>> futures;

    auto process_chunk = [&](size_t start_idx, size_t end_idx) {
        promql::Matrix local_matrix;
        local_matrix.reserve(end_idx - start_idx);

        for (size_t i = start_idx; i < end_idx; ++i) {
            const auto& ts_series = ts_series_list[i];
            
            // Convert to PromQL Series
            promql::Series p_series;
            
            // Convert Labels
            for (const auto& [k, v] : ts_series.labels().map()) {
                p_series.metric.AddLabel(k, v);
            }

            // Convert Samples
            for (const auto& sample : ts_series.samples()) {
                p_series.samples.emplace_back(sample.timestamp(), sample.value());
            }
            local_matrix.push_back(std::move(p_series));
        }
        return local_matrix;
    };

    // Launch threads
    for (size_t i = 0; i < num_series; i += chunk_size) {
        size_t end_idx = std::min(i + chunk_size, num_series);
        futures.push_back(std::async(std::launch::async, process_chunk, i, end_idx));
    }

    // Collect results
    for (auto& f : futures) {
        promql::Matrix chunk_result = f.get();
        if (matrix.empty()) {
            matrix = std::move(chunk_result);
        } else {
            matrix.insert(
                matrix.end(),
                std::make_move_iterator(chunk_result.begin()),
                std::make_move_iterator(chunk_result.end())
            );
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();

    if (duration > 50000) { // Log storage queries taking > 50ms
        std::cout << "[SLOW STORAGE] SelectSeries took " << duration / 1000.0 << "ms. "
                  << "Returned " << matrix.size() << " series. "
                  << "Matchers: " << matchers.size() << std::endl;
    }

    return matrix;
}



std::vector<std::string> TSDBAdapter::LabelNames() {
    auto result = storage_->label_names();
    if (!result.ok()) {
        return {};
    }
    return result.value();
}

std::vector<std::string> TSDBAdapter::LabelValues(const std::string& label_name) {
    auto result = storage_->label_values(label_name);
    if (!result.ok()) {
        return {};
    }
    return result.value();
}

promql::Matrix TSDBAdapter::SelectAggregateSeries(
    const std::vector<model::LabelMatcher>& matchers,
    int64_t start,
    int64_t end,
    const core::AggregationRequest& aggregation) {
    
    // 1. Convert matchers to core::LabelMatcher
    std::vector<core::LabelMatcher> core_matchers;
    core_matchers.reserve(matchers.size());

    for (const auto& matcher : matchers) {
        core::MatcherType type;
        switch (matcher.type) {
            case model::MatcherType::EQUAL: type = core::MatcherType::Equal; break;
            case model::MatcherType::NOT_EQUAL: type = core::MatcherType::NotEqual; break;
            case model::MatcherType::REGEX_MATCH: type = core::MatcherType::RegexMatch; break;
            case model::MatcherType::REGEX_NO_MATCH: type = core::MatcherType::RegexNoMatch; break;
            default: type = core::MatcherType::Equal; break;
        }
        core_matchers.emplace_back(type, matcher.name, matcher.value);
    }

    // 2. Query storage with aggregation pushdown
    auto result = storage_->query_aggregate(core_matchers, start, end, aggregation);
    if (!result.ok()) {
        throw std::runtime_error("Storage aggregation query failed: " + result.error());
    }

    const auto& ts_series_list = result.value();
    promql::Matrix matrix;
    
    // Reserve memory
    matrix.reserve(ts_series_list.size());

    // 3. Convert to PromQL Matrix (Parallelized)
    size_t num_series = ts_series_list.size();
    if (num_series == 0) {
        return matrix;
    }

    // Determine number of threads
    unsigned int num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 4; // Fallback
    
    // Don't spawn threads for small datasets
    if (num_series < 100) {
        num_threads = 1;
    }

    size_t chunk_size = (num_series + num_threads - 1) / num_threads;
    std::vector<std::future<promql::Matrix>> futures;

    auto process_chunk = [&](size_t start_idx, size_t end_idx) {
        promql::Matrix local_matrix;
        local_matrix.reserve(end_idx - start_idx);

        for (size_t i = start_idx; i < end_idx; ++i) {
            const auto& ts_series = ts_series_list[i];
            
            // Convert to PromQL Series
            promql::Series p_series;
            
            // Convert Labels
            for (const auto& [k, v] : ts_series.labels().map()) {
                p_series.metric.AddLabel(k, v);
            }

            // Convert Samples
            for (const auto& sample : ts_series.samples()) {
                p_series.samples.emplace_back(sample.timestamp(), sample.value());
            }
            local_matrix.push_back(std::move(p_series));
        }
        return local_matrix;
    };

    // Launch threads
    for (size_t i = 0; i < num_series; i += chunk_size) {
        size_t end_idx = std::min(i + chunk_size, num_series);
        futures.push_back(std::async(std::launch::async, process_chunk, i, end_idx));
    }

    // Collect results
    for (auto& f : futures) {
        auto chunk_result = f.get();
        matrix.insert(matrix.end(), std::make_move_iterator(chunk_result.begin()), std::make_move_iterator(chunk_result.end()));
    }

    return matrix;
}

} // namespace storage
} // namespace prometheus
} // namespace tsdb
