#include "tsdb/prometheus/promql/engine.h"
#include "tsdb/prometheus/promql/parser.h"
#include "tsdb/prometheus/promql/lexer.h"
#include "tsdb/prometheus/promql/evaluator.h"
#include "tsdb/prometheus/model/types.h"
#include "tsdb/prometheus/promql/query_metrics.h"
#include <iostream>
#include <map>

namespace tsdb {
namespace prometheus {
namespace promql {

// Helper to serialize matchers for cache key
std::string SerializeMatchers(const std::vector<tsdb::prometheus::model::LabelMatcher>& matchers) {
    std::string s;
    for (const auto& m : matchers) {
        s += std::to_string(static_cast<int>(m.type)) + ":" + m.name + "=" + m.value + ";";
    }
    return s;
}

class BufferedStorageAdapter : public tsdb::prometheus::storage::StorageAdapter {
public:
    BufferedStorageAdapter(tsdb::prometheus::storage::StorageAdapter* underlying) : underlying_(underlying) {}

    struct CacheEntry {
        int64_t start;
        int64_t end;
        promql::Matrix data;
    };

    // Find any cached entry that fully covers the requested range
    // Optimized: O(1) matcher lookup + O(m) time range search where m = entries per matcher
    // Instead of O(n) scan over all entries
    std::pair<bool, promql::Matrix*> FindCoveringEntry(
        const std::vector<tsdb::prometheus::model::LabelMatcher>& matchers,
        int64_t start, int64_t end) {
        
        std::string matcher_key = SerializeMatchers(matchers);
        
        // O(1) lookup by matcher key
        auto it = cache_.find(matcher_key);
        if (it == cache_.end()) {
            return {false, nullptr};
        }
        
        // O(m) search through time range entries for this matcher
        for (auto& entry : it->second) {
            if (entry.start <= start && entry.end >= end) {
                return {true, &entry.data};
            }
        }
        return {false, nullptr};
    }

    void Buffer(const std::vector<tsdb::prometheus::model::LabelMatcher>& matchers, int64_t start, int64_t end) {
        // Check if we already have a covering entry
        auto [found, _] = FindCoveringEntry(matchers, start, end);
        if (found) {
            return;  // Already covered
        }
        
        std::string matcher_key = SerializeMatchers(matchers);
        
        // Check if new range is a superset of any existing entries (can replace them)
        auto& entries = cache_[matcher_key];
        
        // Remove entries that are subsets of the new range (optimization)
        entries.erase(
            std::remove_if(entries.begin(), entries.end(),
                [start, end](const CacheEntry& e) {
                    return start <= e.start && end >= e.end;
                }),
            entries.end());
        
        // Add new entry
        entries.push_back({start, end, underlying_->SelectSeries(matchers, start, end)});
    }

    promql::Matrix SelectSeries(const std::vector<tsdb::prometheus::model::LabelMatcher>& matchers, int64_t start, int64_t end) override {
        // Look for any cached entry that covers the requested range
        auto [found, data_ptr] = FindCoveringEntry(matchers, start, end);
        if (found && data_ptr) {
            // Filter from cache
            promql::Matrix result;
            result.reserve(data_ptr->size());
            
            for (const auto& series : *data_ptr) {
                Series s;
                s.metric = series.metric;
                s.samples.reserve(series.samples.size());
                
                // Use binary search to find the start sample
                auto it_start = std::lower_bound(series.samples.begin(), series.samples.end(), start,
                    [](const tsdb::prometheus::Sample& sample, int64_t t) {
                        return sample.timestamp() < t;
                    });
                
                for (auto it_samp = it_start; it_samp != series.samples.end(); ++it_samp) {
                    if (it_samp->timestamp() > end) {
                        break;
                    }
                    s.samples.push_back(*it_samp);
                }
                if (!s.samples.empty()) {
                    result.push_back(std::move(s));
                }
            }
            return result;
        }
        // Fallback
        return underlying_->SelectSeries(matchers, start, end);
    }

    promql::Matrix SelectAggregateSeries(
        const std::vector<tsdb::prometheus::model::LabelMatcher>& matchers,
        int64_t start,
        int64_t end,
        const tsdb::core::AggregationRequest& aggregation) override {
        return underlying_->SelectAggregateSeries(matchers, start, end, aggregation);
    }

    std::vector<std::string> LabelNames() override { return underlying_->LabelNames(); }
    std::vector<std::string> LabelValues(const std::string& name) override { return underlying_->LabelValues(name); }

    // Get cache statistics for debugging/monitoring
    std::pair<size_t, size_t> CacheStats() const {
        size_t matcher_count = cache_.size();
        size_t total_entries = 0;
        for (const auto& [_, entries] : cache_) {
            total_entries += entries.size();
        }
        return {matcher_count, total_entries};
    }

private:
    tsdb::prometheus::storage::StorageAdapter* underlying_;
    // Two-level structure: matcher_key -> vector of time-range entries
    // O(1) matcher lookup + O(m) time range search instead of O(n) full scan
    std::map<std::string, std::vector<CacheEntry>> cache_;
};

struct SelectorContext {
    const VectorSelectorNode* node;
    int64_t range; // 0 for instant vector (uses lookback)
};

void CollectSelectors(const ExprNode* node, std::vector<SelectorContext>& selectors) {
    if (!node) return;
    
    switch (node->type()) {
        case ExprNode::Type::VECTOR_SELECTOR:
            selectors.push_back({static_cast<const VectorSelectorNode*>(node), 0});
            break;
        case ExprNode::Type::AGGREGATE:
            CollectSelectors(static_cast<const AggregateExprNode*>(node)->expr.get(), selectors);
            if (static_cast<const AggregateExprNode*>(node)->param) {
                CollectSelectors(static_cast<const AggregateExprNode*>(node)->param.get(), selectors);
            }
            break;
        case ExprNode::Type::BINARY:
            CollectSelectors(static_cast<const BinaryExprNode*>(node)->lhs.get(), selectors);
            CollectSelectors(static_cast<const BinaryExprNode*>(node)->rhs.get(), selectors);
            break;
        case ExprNode::Type::CALL:
            for (const auto& arg : static_cast<const CallNode*>(node)->arguments()) {
                CollectSelectors(arg.get(), selectors);
            }
            break;
        case ExprNode::Type::MATRIX_SELECTOR: {
            const auto* mat = static_cast<const MatrixSelectorNode*>(node);
            const auto* vec = dynamic_cast<const VectorSelectorNode*>(mat->vector_selector());
            if (vec) {
                selectors.push_back({vec, mat->parsedRangeSeconds * 1000});
            }
            break;
        }
        case ExprNode::Type::PAREN:
            CollectSelectors(static_cast<const ParenExprNode*>(node)->expr.get(), selectors);
            break;
        case ExprNode::Type::SUBQUERY:
            CollectSelectors(static_cast<const SubqueryExprNode*>(node)->expr.get(), selectors);
            break;
        case ExprNode::Type::UNARY:
            CollectSelectors(static_cast<const UnaryExprNode*>(node)->expr.get(), selectors);
            break;
        default:
            break;
    }
}

Engine::Engine(EngineOptions options) : options_(std::move(options)) {}

Engine::~Engine() = default;

QueryResult Engine::ExecuteInstant(const std::string& query, int64_t time) {
    ScopedQueryTimer timer(ScopedQueryTimer::Type::QUERY);
    auto start_time = std::chrono::high_resolution_clock::now();
    try {
        // 1. Parse the query
        auto parse_start = std::chrono::high_resolution_clock::now();
        ScopedQueryTimer parse_timer(ScopedQueryTimer::Type::PARSE);
        Lexer lexer(query);
        Parser parser(lexer);
        auto ast = parser.ParseExpr();
        parse_timer.Stop();
        auto parse_end = std::chrono::high_resolution_clock::now();

        if (!ast) {
            return QueryResult{Value{}, {}, "Failed to parse query"};
        }

        // 2. Create Evaluator
        Evaluator evaluator(time, options_.lookback_delta.count(), options_.storage_adapter);

        // 3. Evaluate
        auto eval_start = std::chrono::high_resolution_clock::now();
        ScopedQueryTimer eval_timer(ScopedQueryTimer::Type::EXEC);
        Value result = evaluator.Evaluate(ast.get());
        eval_timer.Stop();
        auto eval_end = std::chrono::high_resolution_clock::now();

        auto total_end = std::chrono::high_resolution_clock::now();
        
        auto total_duration = std::chrono::duration_cast<std::chrono::microseconds>(total_end - start_time).count();
        auto parse_duration = std::chrono::duration_cast<std::chrono::microseconds>(parse_end - parse_start).count();
        auto eval_duration = std::chrono::duration_cast<std::chrono::microseconds>(eval_end - eval_start).count();

        // Log slow queries (> 100ms)
        if (total_duration > 100000) {
            std::cout << "[SLOW QUERY] Duration: " << total_duration / 1000.0 << "ms "
                      << "(Parse: " << parse_duration / 1000.0 << "ms, "
                      << "Eval: " << eval_duration / 1000.0 << "ms) "
                      << "Query: " << query << std::endl;
        }

        return QueryResult{result, {}, ""};
    } catch (const std::exception& e) {
        return QueryResult{Value{}, {}, std::string("Execution error: ") + e.what()};
    }
}

QueryResult Engine::ExecuteRange(const std::string& query, int64_t start, int64_t end, int64_t step) {
    if (step <= 0) {
        return QueryResult{Value{}, {}, "Zero or negative step is not allowed"};
    }
    if (start > end) {
        return QueryResult{Value{}, {}, "Start time cannot be after end time"};
    }

    try {
        // 1. Parse the query
        Lexer lexer(query);
        Parser parser(lexer);
        ScopedQueryTimer parse_timer(ScopedQueryTimer::Type::PARSE);
        auto ast = parser.ParseExpr();
        parse_timer.Stop();
        if (!ast) {
            return QueryResult{Value{}, {}, "Failed to parse query"};
        }

        // 2. Pre-fetch data
        std::vector<SelectorContext> selectors;
        CollectSelectors(ast.get(), selectors);
        
        BufferedStorageAdapter bufferedAdapter(options_.storage_adapter);
        
        for (const auto& ctx : selectors) {
            int64_t fetch_start = start;
            int64_t fetch_end = end;
            
            // Handle offset
            fetch_start -= ctx.node->offset();
            fetch_end -= ctx.node->offset();
            
            // Handle lookback/range
            int64_t lookback = ctx.range;
            if (lookback == 0) {
                lookback = options_.lookback_delta.count();
            }
            fetch_start -= lookback;
            
            // Construct full matchers (include __name__ if present)
            std::vector<tsdb::prometheus::model::LabelMatcher> matchers = ctx.node->matchers();
            if (!ctx.node->name.empty()) {
                bool found = false;
                for (const auto& m : matchers) {
                    if (m.name == "__name__") {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    matchers.emplace_back(tsdb::prometheus::model::MatcherType::EQUAL, "__name__", ctx.node->name);
                }
            }

            bufferedAdapter.Buffer(matchers, fetch_start, fetch_end);
        }

        // Map to aggregate series by labels
        struct LabelSetComparator {
            bool operator()(const LabelSet& a, const LabelSet& b) const {
                return a.labels() < b.labels();
            }
        };
        std::map<LabelSet, Series, LabelSetComparator> seriesMap;

        // Loop through time steps
        int stepCount = 0;
        for (int64_t t = start; t <= end; t += step) {
            stepCount++;
            if (stepCount % 100 == 0) {
                std::cout << "[RANGE QUERY] Processed " << stepCount << " steps..." << std::endl;
            }
            
            // Use BufferedStorageAdapter in Evaluator
            Evaluator evaluator(t, options_.lookback_delta.count(), &bufferedAdapter);
            ScopedQueryTimer eval_timer(ScopedQueryTimer::Type::EXEC);
            Value val = evaluator.Evaluate(ast.get());
            eval_timer.Stop();
            
            if (val.isVector()) {
                const Vector& vec = val.getVector();
                for (const auto& sample : vec) {
                    // Find or create series
                    auto& series = seriesMap[sample.metric];
                    if (series.metric.labels().empty()) {
                         series.metric = sample.metric;
                    }
                    // Append sample
                    series.samples.emplace_back(t, sample.value);
                }
            } else if (val.isScalar()) {
                const Scalar& scalar = val.getScalar();
                // Scalar result creates a series with no labels
                LabelSet emptyLabels;
                auto& series = seriesMap[emptyLabels];
                series.metric = emptyLabels;
                series.samples.emplace_back(t, scalar.value);
            }
        }

        // Convert map to Matrix
        Matrix resultMatrix;
        resultMatrix.reserve(seriesMap.size());
        for (auto& pair : seriesMap) {
            resultMatrix.push_back(std::move(pair.second));
        }

        return QueryResult{Value(resultMatrix), {}, ""};

    } catch (const std::exception& e) {
        return QueryResult{Value{}, {}, std::string("Execution error: ") + e.what()};
    }
}

std::vector<std::string> Engine::LabelValues(const std::string& label_name) {
    if (!options_.storage_adapter) {
        return {};
    }
    return options_.storage_adapter->LabelValues(label_name);
}

} // namespace promql
} // namespace prometheus
} // namespace tsdb
