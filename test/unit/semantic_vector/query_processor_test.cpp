#include <gtest/gtest.h>
#include "tsdb/storage/semantic_vector/query_processor.h"
#include "tsdb/core/semantic_vector_config.h"

using namespace tsdb::storage::semantic_vector;
using namespace tsdb::core;

TEST(SemVecSmoke, QueryProcessorBasic) {
    // Create basic query config
    semantic_vector::SemanticVectorConfig::QueryConfig config;
    config.enable_query_optimization = true;
    config.enable_parallel_execution = true;
    config.enable_result_caching = true;
    config.target_query_time_ms = 10.0;
    
    // Create query processor instance
    auto query_processor = CreateQueryProcessor(config);
    ASSERT_NE(query_processor, nullptr);
    
    // Test basic query execution
    std::string query_spec = "test_vector_similarity";
    auto query_result = query_processor->execute_query(query_spec, semantic_vector::QueryProcessor::QueryType::VECTOR_SIMILARITY);
    ASSERT_TRUE(query_result.ok());
    
    auto result = query_result.value();
    ASSERT_EQ(result.query_type, semantic_vector::QueryProcessor::QueryType::VECTOR_SIMILARITY);
    ASSERT_FALSE(result.original_query.empty());
    ASSERT_GE(result.confidence, 0.0);
    ASSERT_LE(result.confidence, 1.0);
    ASSERT_GT(result.execution_time_ms, 0.0);
    
    // Test query plan parsing and optimization
    auto plan_result = query_processor->parse_and_plan_query(query_spec, semantic_vector::QueryProcessor::QueryType::VECTOR_SIMILARITY);
    ASSERT_TRUE(plan_result.ok());
    
    auto plan = plan_result.value();
    ASSERT_EQ(plan.query_type, semantic_vector::QueryProcessor::QueryType::VECTOR_SIMILARITY);
    ASSERT_FALSE(plan.execution_steps.empty());
    ASSERT_GT(plan.total_estimated_time_ms, 0.0);
    
    // Test query optimization
    auto optimized_plan_result = query_processor->optimize_query_plan(plan);
    ASSERT_TRUE(optimized_plan_result.ok());
    
    auto optimized_plan = optimized_plan_result.value();
    ASSERT_TRUE(optimized_plan.is_optimized());
    
    // Test performance metrics
    auto metrics_result = query_processor->get_performance_metrics();
    ASSERT_TRUE(metrics_result.ok());
}

TEST(SemVecSmoke, VectorSimilarityQueries) {
    // Create query processor for vector operations
    semantic_vector::SemanticVectorConfig::QueryConfig config;
    config.enable_parallel_execution = true;
    config.max_parallel_threads = 8;
    
    auto query_processor = CreateQueryProcessor(config);
    ASSERT_NE(query_processor, nullptr);
    
    // Test vector similarity query
    Vector query_vector = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    auto similarity_result = query_processor->execute_vector_similarity_query(query_vector, 5, 0.7);
    ASSERT_TRUE(similarity_result.ok());
    
    auto result = similarity_result.value();
    ASSERT_EQ(result.query_type, semantic_vector::QueryProcessor::QueryType::VECTOR_SIMILARITY);
    ASSERT_GE(result.matched_series.size(), 0);
    ASSERT_EQ(result.matched_series.size(), result.relevance_scores.size());
    
    // Verify relevance scores are valid
    for (double score : result.relevance_scores) {
        ASSERT_GE(score, 0.0);
        ASSERT_LE(score, 1.0);
    }
    
    // Test batch vector query
    std::vector<Vector> batch_vectors = {
        {1.0f, 2.0f, 3.0f},
        {4.0f, 5.0f, 6.0f},
        {7.0f, 8.0f, 9.0f}
    };
    auto batch_result = query_processor->execute_batch_vector_query(batch_vectors, 3);
    ASSERT_TRUE(batch_result.ok());
    
    auto batch_query_result = batch_result.value();
    ASSERT_EQ(batch_query_result.query_type, semantic_vector::QueryProcessor::QueryType::VECTOR_SIMILARITY);
}

TEST(SemVecSmoke, SemanticSearchQueries) {
    // Create query processor for semantic operations
    semantic_vector::SemanticVectorConfig::QueryConfig config;
    config.enable_query_optimization = true;
    config.enable_cost_based_optimization = true;
    
    auto query_processor = CreateQueryProcessor(config);
    ASSERT_NE(query_processor, nullptr);
    
    // Test natural language semantic search
    std::string nlp_query = "Find all time series related to temperature anomalies";
    auto semantic_result = query_processor->execute_semantic_search_query(nlp_query, 10);
    ASSERT_TRUE(semantic_result.ok());
    
    auto result = semantic_result.value();
    ASSERT_EQ(result.query_type, semantic_vector::QueryProcessor::QueryType::SEMANTIC_SEARCH);
    ASSERT_FALSE(result.original_query.empty());
    ASSERT_GE(result.confidence, 0.0);
    
    // Test semantic similarity with embedding
    Vector semantic_embedding = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    auto embedding_result = query_processor->execute_semantic_similarity_query(semantic_embedding, 0.6);
    ASSERT_TRUE(embedding_result.ok());
    
    auto embedding_query_result = embedding_result.value();
    ASSERT_EQ(embedding_query_result.query_type, semantic_vector::QueryProcessor::QueryType::SEMANTIC_SEARCH);
}

TEST(SemVecSmoke, TemporalAnalysisQueries) {
    // Create query processor for temporal operations
    semantic_vector::SemanticVectorConfig::QueryConfig config;
    config.query_timeout_seconds = 30.0;
    config.max_query_complexity = 1000;
    
    auto query_processor = CreateQueryProcessor(config);
    ASSERT_NE(query_processor, nullptr);
    
    // Test temporal correlation query
    std::vector<SeriesID> series_ids = {"temp_series_1", "temp_series_2", "temp_series_3"};
    auto correlation_result = query_processor->execute_temporal_correlation_query(series_ids, 5);
    ASSERT_TRUE(correlation_result.ok());
    
    auto result = correlation_result.value();
    ASSERT_EQ(result.query_type, semantic_vector::QueryProcessor::QueryType::TEMPORAL_QUERY);
    
    // Test anomaly detection query
    auto anomaly_result = query_processor->execute_anomaly_detection_query("test_series", 3.0);
    ASSERT_TRUE(anomaly_result.ok());
    
    auto anomaly_query_result = anomaly_result.value();
    ASSERT_EQ(anomaly_query_result.query_type, semantic_vector::QueryProcessor::QueryType::ANOMALY_DETECTION);
    
    // Test forecasting query
    auto forecast_result = query_processor->execute_forecasting_query("forecast_series", 10);
    ASSERT_TRUE(forecast_result.ok());
    
    auto forecast_query_result = forecast_result.value();
    ASSERT_EQ(forecast_query_result.query_type, semantic_vector::QueryProcessor::QueryType::FORECASTING);
}

TEST(SemVecSmoke, AdvancedAnalyticsQueries) {
    // Create query processor for advanced analytics
    semantic_vector::SemanticVectorConfig::QueryConfig config;
    config.enable_query_logging = true;
    config.validate_queries = true;
    
    auto query_processor = CreateQueryProcessor(config);
    ASSERT_NE(query_processor, nullptr);
    
    // Test causal analysis query
    std::vector<SeriesID> causal_series = {"cause_series", "effect_series"};
    auto causal_result = query_processor->execute_causal_analysis_query(causal_series);
    ASSERT_TRUE(causal_result.ok());
    
    auto result = causal_result.value();
    ASSERT_EQ(result.query_type, semantic_vector::QueryProcessor::QueryType::CAUSAL_ANALYSIS);
    
    // Test pattern recognition query
    auto pattern_result = query_processor->execute_pattern_recognition_query("pattern_reference", 0.8);
    ASSERT_TRUE(pattern_result.ok());
    
    auto pattern_query_result = pattern_result.value();
    ASSERT_EQ(pattern_query_result.query_type, semantic_vector::QueryProcessor::QueryType::TEMPORAL_QUERY);
    
    // Test complex analytics query
    std::string complex_spec = "multi_modal_analysis:components=[vector,semantic,temporal]:threshold=0.7";
    auto complex_result = query_processor->execute_complex_analytics_query(complex_spec);
    ASSERT_TRUE(complex_result.ok());
    
    auto complex_query_result = complex_result.value();
    ASSERT_EQ(complex_query_result.query_type, semantic_vector::QueryProcessor::QueryType::COMPLEX_ANALYTICS);
}

TEST(SemVecSmoke, QueryProcessorUseCases) {
    // Test high throughput use case
    auto high_throughput_processor = CreateQueryProcessorForUseCase("high_throughput");
    ASSERT_NE(high_throughput_processor, nullptr);
    
    auto throughput_config = high_throughput_processor->get_config();
    ASSERT_TRUE(throughput_config.enable_parallel_execution);
    ASSERT_TRUE(throughput_config.enable_result_caching);
    
    // Test high accuracy use case
    auto high_accuracy_processor = CreateQueryProcessorForUseCase("high_accuracy");
    ASSERT_NE(high_accuracy_processor, nullptr);
    
    auto accuracy_config = high_accuracy_processor->get_config();
    ASSERT_TRUE(accuracy_config.enable_query_optimization);
    ASSERT_TRUE(accuracy_config.enable_cost_based_optimization);
    
    // Test resource efficient use case
    auto resource_efficient_processor = CreateQueryProcessorForUseCase("resource_efficient");
    ASSERT_NE(resource_efficient_processor, nullptr);
    
    // Test real time use case
    auto real_time_processor = CreateQueryProcessorForUseCase("real_time");
    ASSERT_NE(real_time_processor, nullptr);
    
    auto real_time_config = real_time_processor->get_config();
    ASSERT_LE(real_time_config.query_timeout_seconds, 1.0);
}

TEST(SemVecSmoke, QueryProcessorConfigValidation) {
    // Test valid config
    semantic_vector::SemanticVectorConfig::QueryConfig valid_config;
    valid_config.max_results_per_query = 100;
    valid_config.query_timeout_seconds = 30.0;
    valid_config.max_parallel_threads = 8;
    valid_config.target_query_time_ms = 10.0;
    
    auto validation = ValidateQueryProcessorConfig(valid_config);
    ASSERT_TRUE(validation.ok());
    ASSERT_TRUE(validation.value().is_valid);
    
    // Test invalid config - too many results
    semantic_vector::SemanticVectorConfig::QueryConfig invalid_config1;
    invalid_config1.max_results_per_query = 50000; // Too high
    
    auto invalid_validation1 = ValidateQueryProcessorConfig(invalid_config1);
    ASSERT_TRUE(invalid_validation1.ok());
    ASSERT_FALSE(invalid_validation1.value().is_valid);
    
    // Test invalid config - negative timeout
    semantic_vector::SemanticVectorConfig::QueryConfig invalid_config2;
    invalid_config2.query_timeout_seconds = -1.0; // Negative
    
    auto invalid_validation2 = ValidateQueryProcessorConfig(invalid_config2);
    ASSERT_TRUE(invalid_validation2.ok());
    ASSERT_FALSE(invalid_validation2.value().is_valid);
    
    // Test invalid config - too many threads
    semantic_vector::SemanticVectorConfig::QueryConfig invalid_config3;
    invalid_config3.max_parallel_threads = 128; // Too high
    
    auto invalid_validation3 = ValidateQueryProcessorConfig(invalid_config3);
    ASSERT_TRUE(invalid_validation3.ok());
    ASSERT_FALSE(invalid_validation3.value().is_valid);
}

TEST(SemVecSmoke, QueryProcessorCaching) {
    // Create query processor with caching enabled
    semantic_vector::SemanticVectorConfig::QueryConfig config;
    config.enable_result_caching = true;
    config.cache_size = 1000;
    config.cache_ttl_seconds = 3600;
    
    auto query_processor = CreateQueryProcessor(config);
    ASSERT_NE(query_processor, nullptr);
    
    // Execute the same query twice to test caching
    std::string query_spec = "vector_similarity_cache_test";
    auto result1 = query_processor->execute_query(query_spec, semantic_vector::QueryProcessor::QueryType::VECTOR_SIMILARITY);
    ASSERT_TRUE(result1.ok());
    
    auto result2 = query_processor->execute_query(query_spec, semantic_vector::QueryProcessor::QueryType::VECTOR_SIMILARITY);
    ASSERT_TRUE(result2.ok());
    
    // Both queries should succeed
    ASSERT_EQ(result1.value().query_type, result2.value().query_type);
    
    // Test cache invalidation
    auto invalidation_result = query_processor->invalidate_query_cache("*");
    ASSERT_TRUE(invalidation_result.ok());
}
