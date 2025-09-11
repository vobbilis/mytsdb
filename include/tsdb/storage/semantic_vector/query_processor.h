#ifndef TSDB_STORAGE_SEMANTIC_VECTOR_QUERY_PROCESSOR_H_
#define TSDB_STORAGE_SEMANTIC_VECTOR_QUERY_PROCESSOR_H_

#include <memory>
#include <vector>
#include <string>
#include <map>
#include <optional>
#include <shared_mutex>

#include "tsdb/core/semantic_vector_types.h"
#include "tsdb/core/semantic_vector_config.h"
#include "tsdb/core/result.h"
#include "tsdb/core/error.h"

namespace tsdb {
namespace storage {
namespace semantic_vector {

/**
 * @brief Query Processor Implementation
 * 
 * Provides advanced query processing capabilities including query parsing,
 * cost-based optimization, parallel execution, and result caching.
 * Updated for architecture unity and linter cache refresh.
 */
class QueryProcessorImpl {
public:
    explicit QueryProcessorImpl(const core::semantic_vector::SemanticVectorConfig::QueryConfig& config);
    virtual ~QueryProcessorImpl() = default;
    
    // Query processing pipeline
    core::Result<core::QueryResult> execute_query(const std::string& query_specification, core::QueryProcessor::QueryType query_type);
    core::Result<core::QueryPlan> parse_and_plan_query(const std::string& query_specification, core::QueryProcessor::QueryType query_type);
    core::Result<core::QueryPlan> optimize_query_plan(const core::QueryPlan& initial_plan);
    core::Result<core::QueryResult> execute_query_plan(const core::QueryPlan& optimized_plan);
    
    // Vector similarity queries
    core::Result<core::QueryResult> execute_vector_similarity_query(const core::Vector& query_vector, size_t top_k, double similarity_threshold);
    core::Result<core::QueryResult> execute_batch_vector_query(const std::vector<core::Vector>& query_vectors, size_t top_k);
    
    // Semantic search queries
    core::Result<core::QueryResult> execute_semantic_search_query(const std::string& natural_language_query, size_t max_results);
    core::Result<core::QueryResult> execute_semantic_similarity_query(const core::Vector& semantic_embedding, double similarity_threshold);
    
    // Temporal analysis queries
    core::Result<core::QueryResult> execute_temporal_correlation_query(const std::vector<core::SeriesID>& series_ids, size_t max_lag);
    core::Result<core::QueryResult> execute_anomaly_detection_query(const core::SeriesID& series_id, double threshold);
    core::Result<core::QueryResult> execute_forecasting_query(const core::SeriesID& series_id, size_t forecast_horizon);
    
    // Advanced analytics queries
    core::Result<core::QueryResult> execute_causal_analysis_query(const std::vector<core::SeriesID>& series_ids);
    core::Result<core::QueryResult> execute_pattern_recognition_query(const core::SeriesID& reference_series, double similarity_threshold);
    core::Result<core::QueryResult> execute_complex_analytics_query(const std::string& analytics_specification);
    
    // Query optimization and caching
    core::Result<void> update_query_statistics(const core::QueryResult& result);
    core::Result<std::optional<core::QueryResult>> check_query_cache(const std::string& query_key);
    core::Result<void> cache_query_result(const std::string& query_key, const core::QueryResult& result);
    core::Result<void> invalidate_query_cache(const std::string& cache_pattern);
    
    // Performance monitoring  
    core::Result<core::PerformanceMetrics> get_performance_metrics() const;
    core::Result<void> reset_performance_metrics();
    
    // Configuration management
    void update_config(const core::semantic_vector::SemanticVectorConfig::QueryConfig& config);
    core::semantic_vector::SemanticVectorConfig::QueryConfig get_config() const;

private:
    // Performance monitoring structures
    struct PerformanceMonitoring {
        std::atomic<double> average_query_execution_time_ms{0.0};
        std::atomic<double> average_query_optimization_time_ms{0.0};
        std::atomic<size_t> total_queries_executed{0};
        std::atomic<size_t> total_queries_optimized{0};
        std::atomic<size_t> total_cache_hits{0};
        std::atomic<size_t> total_cache_misses{0};
        std::atomic<double> average_query_complexity{0.0};
        std::atomic<size_t> query_execution_errors{0};
        std::atomic<size_t> query_optimization_errors{0};
    };
    
    // Configuration and state
    core::semantic_vector::SemanticVectorConfig::QueryConfig config_;
    PerformanceMonitoring performance_monitoring_;
    mutable std::shared_mutex mutex_;
    
    // Internal query processing structures
    std::unique_ptr<class QueryParser> query_parser_;
    std::unique_ptr<class QueryOptimizer> query_optimizer_;
    std::unique_ptr<class QueryExecutor> query_executor_;
    std::unique_ptr<class QueryCache> query_cache_;
    std::unique_ptr<class QueryStatistics> query_statistics_;
    std::unique_ptr<class ResultProcessor> result_processor_;
    
    // Internal helper methods
    core::Result<void> initialize_query_processing_structures();
    core::Result<std::string> generate_query_cache_key(const std::string& query_spec, core::QueryProcessor::QueryType type);
    core::Result<void> update_performance_metrics(const std::string& operation, double latency, bool success) const;
};

// Factory functions
std::unique_ptr<QueryProcessorImpl> CreateQueryProcessor(
    const core::semantic_vector::SemanticVectorConfig::QueryConfig& config);

std::unique_ptr<QueryProcessorImpl> CreateQueryProcessorForUseCase(
    const std::string& use_case,
    const core::semantic_vector::SemanticVectorConfig::QueryConfig& base_config = core::semantic_vector::SemanticVectorConfig::QueryConfig());

core::Result<core::semantic_vector::ConfigValidationResult> ValidateQueryProcessorConfig(
    const core::semantic_vector::SemanticVectorConfig::QueryConfig& config);

} // namespace semantic_vector
} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_SEMANTIC_VECTOR_QUERY_PROCESSOR_H_