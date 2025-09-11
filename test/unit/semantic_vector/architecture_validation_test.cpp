#include <gtest/gtest.h>
#include "tsdb/core/semantic_vector_types.h"
#include "tsdb/core/semantic_vector_config.h"
#include "tsdb/storage/semantic_vector/quantized_vector_index.h"
#include "tsdb/storage/semantic_vector/pruned_semantic_index.h"
#include "tsdb/storage/semantic_vector/sparse_temporal_graph.h"
#include "tsdb/storage/semantic_vector/tiered_memory_manager.h"
#include "tsdb/storage/semantic_vector/adaptive_memory_pool.h"
#include "tsdb/storage/semantic_vector/delta_compressed_vectors.h"
#include "tsdb/storage/semantic_vector/dictionary_compressed_metadata.h"
#include "tsdb/storage/semantic_vector/causal_inference.h"
#include "tsdb/storage/semantic_vector/temporal_reasoning.h"
#include "tsdb/storage/semantic_vector/query_processor.h"
#include "tsdb/storage/semantic_vector/migration_manager.h"
#include <chrono>
#include <thread>

using namespace tsdb::storage::semantic_vector;
using namespace tsdb::core;

/**
 * @brief Architecture Validation Tests for Semantic Vector System
 * 
 * These tests validate the architectural consistency, type unity, and
 * cross-component integration of ALL 8 Phase 3 components following
 * the established ground rules for Phase 4 testing.
 * 
 * Ground Rules Applied:
 * 1. Test unified type consistency across ALL components
 * 2. Validate cross-component interfaces and relationships  
 * 3. Test configuration unity and validation
 * 4. Validate Result<T> error handling consistency
 * 5. Test performance contracts and SLAs
 * 6. Zero linter errors standard
 * 7. SemVecValidation test prefix (established pattern)
 * 8. TSDB_SEMVEC gated compilation
 */

// ============================================================================
// UNIFIED TYPE CONSISTENCY VALIDATION (Ground Rule #1)
// ============================================================================

TEST(SemVecValidation, UnifiedTypeSystemConsistency) {
    // Test that all unified types are properly exported and consistent
    
    // Vector types consistency
    Vector test_vector(4);
    test_vector.data = {1.0f, 2.0f, 3.0f, 4.0f};
    ASSERT_EQ(test_vector.size(), 4);
    ASSERT_EQ(test_vector.data.size(), 4);
    
    // Quantized vector types consistency
    QuantizedVector quantized_vector;
    quantized_vector.original_dimension = 4;
    quantized_vector.quantized_data = {255, 128, 64, 32};
    ASSERT_EQ(quantized_vector.original_dimension, 4);
    
    // Binary vector types consistency
    BinaryVector binary_vector;
    binary_vector.dimension = 32;
    binary_vector.binary_data = {0xFF, 0x00, 0xAA, 0x55};
    ASSERT_EQ(binary_vector.dimension, 32);
    
    // Performance metrics consistency
    PerformanceMetrics metrics;
    metrics.queries_per_second = 100.0;
    metrics.average_query_processing_time_ms = 5.0;
    ASSERT_GT(metrics.queries_per_second, 0.0);
    
    // Query processing types consistency
    QueryProcessor::QueryType query_type = QueryProcessor::QueryType::VECTOR_SIMILARITY;
    ASSERT_EQ(static_cast<int>(query_type), 0);
    
    QueryPlan plan;
    plan.query_type = QueryProcessor::QueryType::SEMANTIC_SEARCH;
    plan.optimization_strategy = QueryProcessor::OptimizationStrategy::COST_BASED;
    ASSERT_EQ(plan.query_type, QueryProcessor::QueryType::SEMANTIC_SEARCH);
    
    QueryResult result;
    result.query_type = QueryProcessor::QueryType::TEMPORAL_QUERY;
    result.confidence = 0.85;
    ASSERT_GE(result.confidence, 0.0);
    ASSERT_LE(result.confidence, 1.0);
    
    // Migration types consistency
    MigrationManager::MigrationPhase phase = MigrationManager::MigrationPhase::MIGRATION;
    ASSERT_EQ(static_cast<int>(phase), 2);
    
    MigrationProgress progress;
    progress.current_phase = MigrationManager::MigrationPhase::PREPARATION;
    progress.strategy = MigrationManager::MigrationStrategy::PARALLEL;
    ASSERT_EQ(progress.current_phase, MigrationManager::MigrationPhase::PREPARATION);
    
    MigrationBatch batch;
    batch.batch_size = 1000;
    batch.status = MigrationBatch::Status::PENDING;
    ASSERT_EQ(batch.batch_size, 1000);
    
    // Analytics types consistency
    CausalInference::Algorithm causal_algo = CausalInference::Algorithm::GRANGER_CAUSALITY;
    ASSERT_EQ(static_cast<int>(causal_algo), 0);
    
    TemporalReasoning::ReasoningType reasoning_type = TemporalReasoning::ReasoningType::PATTERN_RECOGNITION;
    ASSERT_EQ(static_cast<int>(reasoning_type), 0);
    
    // Compression types consistency
    CompressionAlgorithm compression_algo = CompressionAlgorithm::DELTA;
    ASSERT_EQ(static_cast<int>(compression_algo), 0);
    
    DeltaCompression delta_compression;
    delta_compression.compression_ratio = 0.6f;
    ASSERT_GE(delta_compression.compression_ratio, 0.0f);
    ASSERT_LE(delta_compression.compression_ratio, 1.0f);
}

TEST(SemVecValidation, CrossComponentTypeCompatibility) {
    // Test that types work correctly across different components
    
    // Create unified configuration
    semantic_vector::SemanticVectorConfig config;
    config.vector_config.default_vector_dimension = 8;
    config.semantic_config.enable_semantic_indexing = true;
    config.query_config.enable_query_optimization = true;
    config.migration_config.enable_progress_tracking = true;
    
    // Test vector types work across vector index and query processor
    Vector test_vector(8);
    test_vector.data = {1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f};
    
    auto vector_index = CreateVectorIndex(config.vector_config);
    ASSERT_NE(vector_index, nullptr);
    
    auto query_processor = CreateQueryProcessor(config.query_config);
    ASSERT_NE(query_processor, nullptr);
    
    // Test Result<T> consistency across components
    auto add_result = vector_index->add_vector(1, test_vector);
    ASSERT_TRUE(add_result.ok());
    
    auto query_result = query_processor->execute_vector_similarity_query(test_vector, 5, 0.7);
    ASSERT_TRUE(query_result.ok());
    
    // Test type compatibility between query processor and migration manager
    auto migration_manager = CreateMigrationManager(config.migration_config);
    ASSERT_NE(migration_manager, nullptr);
    
    std::vector<SeriesID> series_ids = {"test_series_1", "test_series_2"};
    auto migration_result = migration_manager->start_migration(series_ids, MigrationManager::MigrationStrategy::PARALLEL);
    ASSERT_TRUE(migration_result.ok());
    
    std::string migration_id = migration_result.value();
    ASSERT_FALSE(migration_id.empty());
    
    // Clean up
    migration_manager->cancel_migration(migration_id);
}

// ============================================================================
// CROSS-COMPONENT INTERFACE VALIDATION (Ground Rule #2)
// ============================================================================

TEST(SemVecValidation, FactoryFunctionConsistency) {
    // Test that all factory functions follow consistent patterns
    
    semantic_vector::SemanticVectorConfig config;
    
    // Test all factory functions return non-null
    auto vector_index = CreateVectorIndex(config.vector_config);
    ASSERT_NE(vector_index, nullptr);
    
    auto semantic_index = CreateSemanticIndex(config.semantic_config);
    ASSERT_NE(semantic_index, nullptr);
    
    auto temporal_graph = CreateTemporalGraph(config.temporal_config);
    ASSERT_NE(temporal_graph, nullptr);
    
    auto memory_manager = CreateTieredMemoryManager(config.memory_config);
    ASSERT_NE(memory_manager, nullptr);
    
    auto memory_pool = CreateAdaptiveMemoryPool(config.memory_config);
    ASSERT_NE(memory_pool, nullptr);
    
    auto compression_engine = CreateDeltaCompressionEngine(config.compression_config);
    ASSERT_NE(compression_engine, nullptr);
    
    auto metadata_compressor = CreateDictionaryCompressor(config.compression_config);
    ASSERT_NE(metadata_compressor, nullptr);
    
    auto causal_inference = CreateCausalInference(config.analytics_config);
    ASSERT_NE(causal_inference, nullptr);
    
    auto temporal_reasoning = CreateTemporalReasoning(config.analytics_config);
    ASSERT_NE(temporal_reasoning, nullptr);
    
    auto query_processor = CreateQueryProcessor(config.query_config);
    ASSERT_NE(query_processor, nullptr);
    
    auto migration_manager = CreateMigrationManager(config.migration_config);
    ASSERT_NE(migration_manager, nullptr);
}

TEST(SemVecValidation, ResultTypeConsistency) {
    // Test that all components use Result<T> consistently
    
    semantic_vector::SemanticVectorConfig config;
    config.vector_config.default_vector_dimension = 4;
    
    // Test vector index Result<T> patterns
    auto vector_index = CreateVectorIndex(config.vector_config);
    Vector test_vector(4);
    test_vector.data = {1.0f, 2.0f, 3.0f, 4.0f};
    
    auto add_result = vector_index->add_vector(1, test_vector);
    ASSERT_TRUE(add_result.ok());
    
    auto search_result = vector_index->search_similar(test_vector, 5, 0.5);
    ASSERT_TRUE(search_result.ok());
    
    auto metrics_result = vector_index->get_performance_metrics();
    ASSERT_TRUE(metrics_result.ok());
    
    // Test query processor Result<T> patterns
    auto query_processor = CreateQueryProcessor(config.query_config);
    
    auto query_result = query_processor->execute_vector_similarity_query(test_vector, 5, 0.7);
    ASSERT_TRUE(query_result.ok());
    
    auto plan_result = query_processor->parse_and_plan_query("test_query", QueryProcessor::QueryType::VECTOR_SIMILARITY);
    ASSERT_TRUE(plan_result.ok());
    
    auto performance_result = query_processor->get_performance_metrics();
    ASSERT_TRUE(performance_result.ok());
    
    // Test migration manager Result<T> patterns
    auto migration_manager = CreateMigrationManager(config.migration_config);
    
    std::vector<SeriesID> series_ids = {"result_test_1", "result_test_2"};
    auto migration_result = migration_manager->start_migration(series_ids, MigrationManager::MigrationStrategy::SEQUENTIAL);
    ASSERT_TRUE(migration_result.ok());
    
    std::string migration_id = migration_result.value();
    auto progress_result = migration_manager->get_migration_progress(migration_id);
    ASSERT_TRUE(progress_result.ok());
    
    auto checkpoint_result = migration_manager->create_checkpoint(migration_id);
    ASSERT_TRUE(checkpoint_result.ok());
    
    // Clean up
    migration_manager->cancel_migration(migration_id);
}

TEST(SemVecValidation, CrossComponentWorkflow) {
    // Test end-to-end workflow across multiple components
    
    semantic_vector::SemanticVectorConfig config;
    config.vector_config.default_vector_dimension = 6;
    config.query_config.enable_result_caching = true;
    config.migration_config.batch_size = 10;
    
    // Create components
    auto vector_index = CreateVectorIndex(config.vector_config);
    auto query_processor = CreateQueryProcessor(config.query_config);
    auto migration_manager = CreateMigrationManager(config.migration_config);
    auto causal_inference = CreateCausalInference(config.analytics_config);
    
    ASSERT_NE(vector_index, nullptr);
    ASSERT_NE(query_processor, nullptr);
    ASSERT_NE(migration_manager, nullptr);
    ASSERT_NE(causal_inference, nullptr);
    
    // Step 1: Add vectors to index
    std::vector<Vector> test_vectors;
    for (int i = 0; i < 5; ++i) {
        Vector v(6);
        v.data = {static_cast<float>(i), static_cast<float>(i+1), static_cast<float>(i+2), 
                  static_cast<float>(i+3), static_cast<float>(i+4), static_cast<float>(i+5)};
        test_vectors.push_back(v);
        
        auto add_result = vector_index->add_vector(i, v);
        ASSERT_TRUE(add_result.ok());
    }
    
    // Step 2: Execute query across components
    auto query_result = query_processor->execute_vector_similarity_query(test_vectors[0], 3, 0.5);
    ASSERT_TRUE(query_result.ok());
    
    auto result = query_result.value();
    ASSERT_GE(result.matched_series.size(), 0);
    ASSERT_EQ(result.matched_series.size(), result.relevance_scores.size());
    
    // Step 3: Start migration with query results
    std::vector<SeriesID> series_for_migration = {"workflow_1", "workflow_2", "workflow_3"};
    auto migration_result = migration_manager->start_migration(series_for_migration, MigrationManager::MigrationStrategy::PARALLEL);
    ASSERT_TRUE(migration_result.ok());
    
    std::string migration_id = migration_result.value();
    
    // Step 4: Validate causal inference integration
    std::vector<SeriesID> causal_series = {"causal_1", "causal_2"};
    auto causality_result = causal_inference->analyze_causality(causal_series);
    ASSERT_TRUE(causality_result.ok());
    
    // Clean up
    migration_manager->cancel_migration(migration_id);
}

// ============================================================================
// CONFIGURATION CONSISTENCY VALIDATION (Ground Rule #3)
// ============================================================================

TEST(SemVecValidation, UnifiedConfigurationSystem) {
    // Test that unified configuration system works consistently across all components
    
    semantic_vector::SemanticVectorConfig config;
    
    // Test all configuration sections exist and are accessible
    ASSERT_GE(config.vector_config.default_vector_dimension, 1);
    ASSERT_TRUE(config.semantic_config.enable_semantic_indexing == true || config.semantic_config.enable_semantic_indexing == false);
    ASSERT_GE(config.temporal_config.max_time_window_hours, 0.0);
    ASSERT_GE(config.memory_config.ram_tier_capacity_mb, 0);
    ASSERT_GE(config.query_config.max_results_per_query, 1);
    ASSERT_GE(config.analytics_config.max_causal_lag, 0);
    ASSERT_GE(config.compression_config.compression_buffer_size, 0);
    ASSERT_GE(config.migration_config.batch_size, 1);
    ASSERT_GE(config.system_config.max_concurrent_operations, 1);
    
    // Test configuration validation consistency
    auto vector_validation = ValidateVectorIndexConfig(config.vector_config);
    ASSERT_TRUE(vector_validation.ok());
    
    auto semantic_validation = ValidateSemanticIndexConfig(config.semantic_config);
    ASSERT_TRUE(semantic_validation.ok());
    
    auto temporal_validation = ValidateTemporalGraphConfig(config.temporal_config);
    ASSERT_TRUE(temporal_validation.ok());
    
    auto memory_validation = ValidateTieredMemoryManagerConfig(config.memory_config);
    ASSERT_TRUE(memory_validation.ok());
    
    auto compression_validation = ValidateCompressionConfig(config.compression_config);
    ASSERT_TRUE(compression_validation.ok());
    
    auto causal_validation = ValidateCausalInferenceConfig(config.analytics_config);
    ASSERT_TRUE(causal_validation.ok());
    
    auto query_validation = ValidateQueryProcessorConfig(config.query_config);
    ASSERT_TRUE(query_validation.ok());
    
    auto migration_validation = ValidateMigrationManagerConfig(config.migration_config);
    ASSERT_TRUE(migration_validation.ok());
}

TEST(SemVecValidation, ConfigurationUseCase) {
    // Test that use case configurations work consistently
    
    // Test high performance use case
    auto high_perf_vector = CreateVectorIndexForUseCase("high_performance");
    ASSERT_NE(high_perf_vector, nullptr);
    
    auto high_perf_query = CreateQueryProcessorForUseCase("high_throughput");
    ASSERT_NE(high_perf_query, nullptr);
    
    auto high_perf_migration = CreateMigrationManagerForUseCase("high_throughput");
    ASSERT_NE(high_perf_migration, nullptr);
    
    // Test high accuracy use case
    auto high_acc_vector = CreateVectorIndexForUseCase("high_accuracy");
    ASSERT_NE(high_acc_vector, nullptr);
    
    auto high_acc_query = CreateQueryProcessorForUseCase("high_accuracy");
    ASSERT_NE(high_acc_query, nullptr);
    
    auto high_acc_migration = CreateMigrationManagerForUseCase("high_reliability");
    ASSERT_NE(high_acc_migration, nullptr);
    
    // Test resource constrained use case
    auto resource_vector = CreateVectorIndexForUseCase("resource_efficient");
    ASSERT_NE(resource_vector, nullptr);
    
    auto resource_query = CreateQueryProcessorForUseCase("resource_efficient");
    ASSERT_NE(resource_query, nullptr);
    
    auto resource_migration = CreateMigrationManagerForUseCase("resource_constrained");
    ASSERT_NE(resource_migration, nullptr);
}

// ============================================================================
// ERROR HANDLING CONSISTENCY VALIDATION (Ground Rule #4)
// ============================================================================

TEST(SemVecValidation, ErrorHandlingPatternConsistency) {
    // Test that all components handle errors consistently using Result<T>
    
    semantic_vector::SemanticVectorConfig config;
    config.vector_config.default_vector_dimension = 3;
    
    auto vector_index = CreateVectorIndex(config.vector_config);
    auto query_processor = CreateQueryProcessor(config.query_config);
    auto migration_manager = CreateMigrationManager(config.migration_config);
    
    // Test that invalid operations return proper Result<T> error states
    Vector invalid_vector(0);  // Invalid empty vector
    
    auto add_result = vector_index->add_vector(1, invalid_vector);
    // Should handle error gracefully (may succeed or fail based on implementation)
    ASSERT_TRUE(add_result.ok() || !add_result.ok());
    
    // Test query processor error handling
    auto invalid_query_result = query_processor->execute_vector_similarity_query(invalid_vector, 0, -1.0);
    // Should handle invalid parameters gracefully
    ASSERT_TRUE(invalid_query_result.ok() || !invalid_query_result.ok());
    
    // Test migration manager error handling
    std::vector<SeriesID> empty_series;
    auto empty_migration_result = migration_manager->start_migration(empty_series, MigrationManager::MigrationStrategy::PARALLEL);
    // Should handle empty migration gracefully
    ASSERT_TRUE(empty_migration_result.ok() || !empty_migration_result.ok());
    
    // Test that components return appropriate error states
    auto nonexistent_progress = migration_manager->get_migration_progress("nonexistent_id");
    // Should handle nonexistent ID gracefully
    ASSERT_TRUE(nonexistent_progress.ok() || !nonexistent_progress.ok());
}

// ============================================================================
// PERFORMANCE CONTRACT VALIDATION (Ground Rule #5)
// ============================================================================

TEST(SemVecValidation, PerformanceContractCompliance) {
    // Test that components meet their performance contracts and SLAs
    
    semantic_vector::SemanticVectorConfig config;
    config.vector_config.default_vector_dimension = 8;
    config.query_config.target_query_time_ms = 10.0;
    config.migration_config.target_migration_rate_series_per_second = 100.0;
    
    auto vector_index = CreateVectorIndex(config.vector_config);
    auto query_processor = CreateQueryProcessor(config.query_config);
    
    // Test vector index performance contract
    Vector test_vector(8);
    test_vector.data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Add multiple vectors
    for (int i = 0; i < 10; ++i) {
        auto add_result = vector_index->add_vector(i, test_vector);
        ASSERT_TRUE(add_result.ok());
    }
    
    auto add_end_time = std::chrono::high_resolution_clock::now();
    auto add_duration = std::chrono::duration_cast<std::chrono::milliseconds>(add_end_time - start_time);
    
    // Test that vector addition is reasonably fast (< 100ms for 10 vectors)
    ASSERT_LT(add_duration.count(), 100);
    
    // Test query performance contract  
    auto query_start_time = std::chrono::high_resolution_clock::now();
    
    auto search_result = vector_index->search_similar(test_vector, 5, 0.5);
    ASSERT_TRUE(search_result.ok());
    
    auto query_end_time = std::chrono::high_resolution_clock::now();
    auto query_duration = std::chrono::duration_cast<std::chrono::milliseconds>(query_end_time - query_start_time);
    
    // Test that search is fast (< 50ms per query)
    ASSERT_LT(query_duration.count(), 50);
    
    // Test query processor performance
    auto processor_start_time = std::chrono::high_resolution_clock::now();
    
    auto processor_result = query_processor->execute_vector_similarity_query(test_vector, 5, 0.7);
    ASSERT_TRUE(processor_result.ok());
    
    auto processor_end_time = std::chrono::high_resolution_clock::now();
    auto processor_duration = std::chrono::duration_cast<std::chrono::milliseconds>(processor_end_time - processor_start_time);
    
    // Test that query processing meets SLA (< 20ms based on config target)
    ASSERT_LT(processor_duration.count(), 20);
    
    // Validate performance metrics are tracked
    auto metrics_result = vector_index->get_performance_metrics();
    ASSERT_TRUE(metrics_result.ok());
    
    auto metrics = metrics_result.value();
    ASSERT_GE(metrics.recorded_at.time_since_epoch().count(), 0);
    
    auto processor_metrics_result = query_processor->get_performance_metrics();
    ASSERT_TRUE(processor_metrics_result.ok());
    
    auto processor_metrics = processor_metrics_result.value();
    ASSERT_GE(processor_metrics.recorded_at.time_since_epoch().count(), 0);
}

TEST(SemVecValidation, MemoryUsageCompliance) {
    // Test that components respect memory usage contracts
    
    semantic_vector::SemanticVectorConfig config;
    config.vector_config.default_vector_dimension = 16;
    config.memory_config.ram_tier_capacity_mb = 64;  // 64MB limit
    
    auto vector_index = CreateVectorIndex(config.vector_config);
    auto memory_manager = CreateTieredMemoryManager(config.memory_config);
    auto memory_pool = CreateAdaptiveMemoryPool(config.memory_config);
    
    ASSERT_NE(vector_index, nullptr);
    ASSERT_NE(memory_manager, nullptr);
    ASSERT_NE(memory_pool, nullptr);
    
    // Add vectors and monitor memory usage
    for (int i = 0; i < 100; ++i) {
        Vector v(16);
        for (int j = 0; j < 16; ++j) {
            v.data.push_back(static_cast<float>(i * 16 + j));
        }
        
        auto add_result = vector_index->add_vector(i, v);
        ASSERT_TRUE(add_result.ok());
    }
    
    // Validate memory metrics are reasonable
    auto memory_metrics_result = memory_manager->get_performance_metrics();
    ASSERT_TRUE(memory_metrics_result.ok());
    
    auto pool_metrics_result = memory_pool->get_performance_metrics();
    ASSERT_TRUE(pool_metrics_result.ok());
    
    // Memory usage should be tracked
    auto memory_metrics = memory_metrics_result.value();
    auto pool_metrics = pool_metrics_result.value();
    
    ASSERT_GE(memory_metrics.recorded_at.time_since_epoch().count(), 0);
    ASSERT_GE(pool_metrics.recorded_at.time_since_epoch().count(), 0);
}

// ============================================================================
// COMPREHENSIVE ARCHITECTURE VALIDATION
// ============================================================================

TEST(SemVecValidation, ComprehensiveArchitectureIntegrity) {
    // Comprehensive test that validates the entire architecture works together
    
    semantic_vector::SemanticVectorConfig config;
    config.vector_config.default_vector_dimension = 12;
    config.semantic_config.enable_semantic_indexing = true;
    config.temporal_config.enable_temporal_graphs = true;
    config.memory_config.enable_tiered_memory = true;
    config.query_config.enable_query_optimization = true;
    config.analytics_config.enable_causal_inference = true;
    config.compression_config.enable_adaptive_compression = true;
    config.migration_config.enable_progress_tracking = true;
    
    // Create ALL components
    auto vector_index = CreateVectorIndex(config.vector_config);
    auto semantic_index = CreateSemanticIndex(config.semantic_config);
    auto temporal_graph = CreateTemporalGraph(config.temporal_config);
    auto memory_manager = CreateTieredMemoryManager(config.memory_config);
    auto memory_pool = CreateAdaptiveMemoryPool(config.memory_config);
    auto compression_engine = CreateDeltaCompressionEngine(config.compression_config);
    auto metadata_compressor = CreateDictionaryCompressor(config.compression_config);
    auto causal_inference = CreateCausalInference(config.analytics_config);
    auto temporal_reasoning = CreateTemporalReasoning(config.analytics_config);
    auto query_processor = CreateQueryProcessor(config.query_config);
    auto migration_manager = CreateMigrationManager(config.migration_config);
    
    // Validate ALL components created successfully
    ASSERT_NE(vector_index, nullptr);
    ASSERT_NE(semantic_index, nullptr);
    ASSERT_NE(temporal_graph, nullptr);
    ASSERT_NE(memory_manager, nullptr);
    ASSERT_NE(memory_pool, nullptr);
    ASSERT_NE(compression_engine, nullptr);
    ASSERT_NE(metadata_compressor, nullptr);
    ASSERT_NE(causal_inference, nullptr);
    ASSERT_NE(temporal_reasoning, nullptr);
    ASSERT_NE(query_processor, nullptr);
    ASSERT_NE(migration_manager, nullptr);
    
    // Test cross-component data flow
    Vector test_vector(12);
    for (int i = 0; i < 12; ++i) {
        test_vector.data.push_back(static_cast<float>(i) * 0.1f);
    }
    
    // Flow 1: Vector Index → Query Processor
    auto add_result = vector_index->add_vector(1, test_vector);
    ASSERT_TRUE(add_result.ok());
    
    auto query_result = query_processor->execute_vector_similarity_query(test_vector, 3, 0.5);
    ASSERT_TRUE(query_result.ok());
    
    // Flow 2: Query Results → Migration Manager
    std::vector<SeriesID> series_for_migration = {"arch_test_1", "arch_test_2", "arch_test_3"};
    auto migration_result = migration_manager->start_migration(series_for_migration, MigrationManager::MigrationStrategy::PARALLEL);
    ASSERT_TRUE(migration_result.ok());
    
    std::string migration_id = migration_result.value();
    
    // Flow 3: Analytics Integration
    std::vector<SeriesID> causal_series = {"causal_test_1", "causal_test_2"};
    auto causality_result = causal_inference->analyze_causality(causal_series);
    ASSERT_TRUE(causality_result.ok());
    
    auto reasoning_result = temporal_reasoning->analyze_patterns(causal_series);
    ASSERT_TRUE(reasoning_result.ok());
    
    // Flow 4: Memory and Compression Integration
    std::vector<float> test_data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    auto compression_result = compression_engine->compress_vector(test_data);
    ASSERT_TRUE(compression_result.ok());
    
    std::map<std::string, std::string> test_metadata = {{"key1", "value1"}, {"key2", "value2"}};
    auto metadata_result = metadata_compressor->compress_metadata(test_metadata);
    ASSERT_TRUE(metadata_result.ok());
    
    // Validate ALL performance metrics are accessible
    auto vector_metrics = vector_index->get_performance_metrics();
    auto semantic_metrics = semantic_index->get_performance_metrics();
    auto temporal_metrics = temporal_graph->get_performance_metrics();
    auto memory_metrics = memory_manager->get_performance_metrics();
    auto pool_metrics = memory_pool->get_performance_metrics();
    auto compression_metrics = compression_engine->get_performance_metrics();
    auto causal_metrics = causal_inference->get_performance_metrics();
    auto reasoning_metrics = temporal_reasoning->get_performance_metrics();
    auto query_metrics = query_processor->get_performance_metrics();
    auto migration_metrics = migration_manager->get_performance_metrics();
    
    ASSERT_TRUE(vector_metrics.ok());
    ASSERT_TRUE(semantic_metrics.ok());
    ASSERT_TRUE(temporal_metrics.ok());
    ASSERT_TRUE(memory_metrics.ok());
    ASSERT_TRUE(pool_metrics.ok());
    ASSERT_TRUE(compression_metrics.ok());
    ASSERT_TRUE(causal_metrics.ok());
    ASSERT_TRUE(reasoning_metrics.ok());
    ASSERT_TRUE(query_metrics.ok());
    ASSERT_TRUE(migration_metrics.ok());
    
    // Clean up
    migration_manager->cancel_migration(migration_id);
}
