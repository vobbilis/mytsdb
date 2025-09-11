#ifndef TSDB_STORAGE_SEMANTIC_VECTOR_ARCHITECTURE_H_
#define TSDB_STORAGE_SEMANTIC_VECTOR_ARCHITECTURE_H_

#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <chrono>

#include "tsdb/core/semantic_vector_types.h"
#include "tsdb/core/semantic_vector_config.h"
#include "tsdb/core/result.h"
#include "tsdb/core/error.h"

namespace tsdb {
namespace storage {
namespace semantic_vector {

// ============================================================================
// COMPONENT INTERFACE ARCHITECTURE
// ============================================================================

/**
 * @brief Complete interface architecture for semantic vector storage
 * 
 * This file defines all component interfaces and their relationships
 * in one place, ensuring consistency across the entire semantic vector
 * storage system. All interfaces use unified types from TASK-1 and
 * configuration from TASK-2.
 */

// ============================================================================
// VECTOR PROCESSING INTERFACES
// ============================================================================

/**
 * @brief Interface for vector indexing and similarity search
 * 
 * Provides vector storage, indexing, and similarity search capabilities
 * using unified Vector types and optimized indexing strategies.
 */
class IVectorIndex {
public:
    virtual ~IVectorIndex() = default;
    
    // Vector management
    virtual core::Result<void> add_vector(const core::SeriesID& series_id, const core::Vector& vector) = 0;
    virtual core::Result<void> update_vector(const core::SeriesID& series_id, const core::Vector& vector) = 0;
    virtual core::Result<void> remove_vector(const core::SeriesID& series_id) = 0;
    virtual core::Result<core::Vector> get_vector(const core::SeriesID& series_id) const = 0;
    
    // Similarity search
    virtual core::Result<std::vector<std::pair<core::SeriesID, double>>> search_similar(
        const core::Vector& query_vector, 
        size_t k_nearest,
        double similarity_threshold = 0.7) const = 0;
    
    // Quantized vector operations
    virtual core::Result<core::QuantizedVector> quantize_vector(const core::Vector& vector) const = 0;
    virtual core::Result<core::Vector> dequantize_vector(const core::QuantizedVector& qvector) const = 0;
    virtual core::Result<std::vector<std::pair<core::SeriesID, double>>> search_quantized(
        const core::QuantizedVector& query_vector,
        size_t k_nearest) const = 0;
    
    // Binary vector operations
    virtual core::Result<core::BinaryVector> binarize_vector(const core::Vector& vector) const = 0;
    virtual core::Result<std::vector<std::pair<core::SeriesID, uint32_t>>> search_binary(
        const core::BinaryVector& query_vector,
        size_t k_nearest,
        uint32_t max_hamming_distance = 10) const = 0;
    
    // Index management
    virtual core::Result<void> build_index() = 0;
    virtual core::Result<void> optimize_index() = 0;
    virtual core::Result<core::VectorIndex> get_index_stats() const = 0;
    
    // Performance monitoring
    virtual core::Result<core::PerformanceMetrics> get_performance_metrics() const = 0;
    virtual core::Result<void> reset_performance_metrics() = 0;
    
    // Configuration
    virtual void update_config(const core::semantic_vector::SemanticVectorConfig::VectorConfig& config) = 0;
    virtual core::semantic_vector::SemanticVectorConfig::VectorConfig get_config() const = 0;
};

/**
 * @brief Interface for vector compression and optimization
 * 
 * Provides vector compression, quantization, and memory optimization
 * capabilities using unified memory optimization types.
 */
class IVectorCompressor {
public:
    virtual ~IVectorCompressor() = default;
    
    // Compression operations
    virtual core::Result<std::vector<uint8_t>> compress_vector(const core::Vector& vector) = 0;
    virtual core::Result<core::Vector> decompress_vector(const std::vector<uint8_t>& compressed_data) = 0;
    virtual core::Result<double> get_compression_ratio() const = 0;
    
    // Quantization operations
    virtual core::Result<core::QuantizedVector> quantize_vector(const core::Vector& vector) = 0;
    virtual core::Result<core::Vector> dequantize_vector(const core::QuantizedVector& qvector) = 0;
    virtual core::Result<double> get_quantization_error() const = 0;
    
    // Binary operations
    virtual core::Result<core::BinaryVector> binarize_vector(const core::Vector& vector) = 0;
    virtual core::Result<core::Vector> debinarize_vector(const core::BinaryVector& bvector) = 0;
    virtual core::Result<double> get_binarization_error() const = 0;
    
    // Memory optimization
    virtual core::Result<size_t> get_memory_usage() const = 0;
    virtual core::Result<size_t> get_optimized_memory_usage() const = 0;
    virtual core::Result<double> get_memory_reduction_ratio() const = 0;
    
    // Configuration
    virtual void update_config(const core::semantic_vector::SemanticVectorConfig::MemoryConfig& config) = 0;
    virtual core::semantic_vector::SemanticVectorConfig::MemoryConfig get_config() const = 0;
};

// ============================================================================
// SEMANTIC PROCESSING INTERFACES
// ============================================================================

/**
 * @brief Interface for semantic indexing and natural language search
 * 
 * Provides semantic embedding generation, storage, and natural language
 * search capabilities using unified SemanticQuery types.
 */
class ISemanticIndex {
public:
    virtual ~ISemanticIndex() = default;
    
    // Semantic embedding management
    virtual core::Result<void> add_semantic_embedding(const core::SeriesID& series_id, const core::Vector& embedding) = 0;
    virtual core::Result<void> update_semantic_embedding(const core::SeriesID& series_id, const core::Vector& embedding) = 0;
    virtual core::Result<void> remove_semantic_embedding(const core::SeriesID& series_id) = 0;
    virtual core::Result<core::Vector> get_semantic_embedding(const core::SeriesID& series_id) const = 0;
    
    // Natural language search
    virtual core::Result<std::vector<std::pair<core::SeriesID, double>>> semantic_search(
        const core::semantic_vector::SemanticQuery& query) const = 0;
    
    // Entity and concept management
    virtual core::Result<void> add_entities(const core::SeriesID& series_id, const std::vector<std::string>& entities) = 0;
    virtual core::Result<void> add_concepts(const core::SeriesID& series_id, const std::vector<std::string>& concepts) = 0;
    virtual core::Result<std::vector<std::string>> get_entities(const core::SeriesID& series_id) const = 0;
    virtual core::Result<std::vector<std::string>> get_concepts(const core::SeriesID& series_id) const = 0;
    
    // Entity and concept search
    virtual core::Result<std::vector<core::SeriesID>> search_by_entity(const std::string& entity) const = 0;
    virtual core::Result<std::vector<core::SeriesID>> search_by_concept(const std::string& concept) const = 0;
    
    // Pruned embedding operations
    virtual core::Result<core::semantic_vector::PrunedEmbedding> prune_embedding(const core::Vector& embedding) = 0;
    virtual core::Result<core::Vector> reconstruct_embedding(const core::semantic_vector::PrunedEmbedding& pruned) = 0;
    virtual core::Result<double> get_pruning_accuracy() const = 0;
    
    // Performance monitoring
    virtual core::Result<core::PerformanceMetrics> get_performance_metrics() const = 0;
    virtual core::Result<void> reset_performance_metrics() = 0;
    
    // Configuration
    virtual void update_config(const core::semantic_vector::SemanticVectorConfig::SemanticConfig& config) = 0;
    virtual core::semantic_vector::SemanticVectorConfig::SemanticConfig get_config() const = 0;
};

/**
 * @brief Interface for semantic embedding generation
 * 
 * Provides BERT-based semantic embedding generation and natural language
 * processing capabilities.
 */
class ISemanticEmbeddingGenerator {
public:
    virtual ~ISemanticEmbeddingGenerator() = default;
    
    // Embedding generation
    virtual core::Result<core::Vector> generate_embedding(const std::string& text) = 0;
    virtual core::Result<core::Vector> generate_embedding_for_series(const core::SeriesID& series_id) = 0;
    virtual core::Result<std::vector<core::Vector>> generate_batch_embeddings(const std::vector<std::string>& texts) = 0;
    
    // Entity and concept extraction
    virtual core::Result<std::vector<std::string>> extract_entities(const std::string& text) = 0;
    virtual core::Result<std::vector<std::string>> extract_concepts(const std::string& text) = 0;
    
    // Query processing
    virtual core::Result<core::semantic_vector::SemanticQuery> process_natural_language_query(const std::string& query) = 0;
    virtual core::Result<std::vector<core::semantic_vector::SemanticQuery>> expand_query(const core::semantic_vector::SemanticQuery& query) = 0;
    
    // Model management
    virtual core::Result<void> load_model(const std::string& model_path) = 0;
    virtual core::Result<void> unload_model() = 0;
    virtual core::Result<bool> is_model_loaded() const = 0;
    
    // Performance monitoring
    virtual core::Result<core::PerformanceMetrics> get_performance_metrics() const = 0;
    virtual core::Result<void> reset_performance_metrics() = 0;
    
    // Configuration
    virtual void update_config(const core::semantic_vector::SemanticVectorConfig::SemanticConfig& config) = 0;
    virtual core::semantic_vector::SemanticVectorConfig::SemanticConfig get_config() const = 0;
};

// ============================================================================
// TEMPORAL PROCESSING INTERFACES
// ============================================================================

/**
 * @brief Interface for temporal graph construction and correlation analysis
 * 
 * Provides temporal graph construction, correlation analysis, and
 * temporal reasoning capabilities using unified TemporalGraph types.
 */
class ITemporalGraph {
public:
    virtual ~ITemporalGraph() = default;
    
    // Graph construction
    virtual core::Result<void> add_series(const core::SeriesID& series_id) = 0;
    virtual core::Result<void> remove_series(const core::SeriesID& series_id) = 0;
    virtual core::Result<void> add_correlation(const core::SeriesID& source, const core::SeriesID& target, double correlation) = 0;
    virtual core::Result<void> remove_correlation(const core::SeriesID& source, const core::SeriesID& target) = 0;
    
    // Graph queries
    virtual core::Result<std::vector<core::SeriesID>> get_neighbors(const core::SeriesID& series_id) const = 0;
    virtual core::Result<double> get_correlation(const core::SeriesID& source, const core::SeriesID& target) const = 0;
    virtual core::Result<std::vector<std::pair<core::SeriesID, double>>> get_top_correlations(const core::SeriesID& series_id, size_t k) const = 0;
    
    // Graph analysis
    virtual core::Result<core::semantic_vector::TemporalGraph> get_graph_stats() const = 0;
    virtual core::Result<std::vector<core::SeriesID>> find_communities() const = 0;
    virtual core::Result<std::vector<core::SeriesID>> find_influential_nodes(size_t k) const = 0;
    
    // Sparse graph operations
    virtual core::Result<void> enable_sparse_representation() = 0;
    virtual core::Result<void> disable_sparse_representation() = 0;
    virtual core::Result<bool> is_sparse_enabled() const = 0;
    
    // Graph compression
    virtual core::Result<void> compress_graph() = 0;
    virtual core::Result<void> decompress_graph() = 0;
    virtual core::Result<double> get_compression_ratio() const = 0;
    
    // Performance monitoring
    virtual core::Result<core::PerformanceMetrics> get_performance_metrics() const = 0;
    virtual core::Result<void> reset_performance_metrics() = 0;
    
    // Configuration
    virtual void update_config(const core::semantic_vector::SemanticVectorConfig::TemporalConfig& config) = 0;
    virtual core::semantic_vector::SemanticVectorConfig::TemporalConfig get_config() const = 0;
};

/**
 * @brief Interface for correlation matrix operations
 * 
 * Provides correlation matrix construction, storage, and analysis
 * capabilities using unified CorrelationMatrix types.
 */
class ICorrelationMatrix {
public:
    virtual ~ICorrelationMatrix() = default;
    
    // Matrix operations
    virtual core::Result<void> set_correlation(const core::SeriesID& i, const core::SeriesID& j, double correlation) = 0;
    virtual core::Result<double> get_correlation(const core::SeriesID& i, const core::SeriesID& j) const = 0;
    virtual core::Result<void> remove_correlation(const core::SeriesID& i, const core::SeriesID& j) = 0;
    
    // Matrix queries
    virtual core::Result<std::vector<std::pair<core::SeriesID, double>>> get_top_correlations(
        const core::SeriesID& series_id, size_t k) const = 0;
    virtual core::Result<std::vector<core::SeriesID>> get_highly_correlated_series(
        const core::SeriesID& series_id, double threshold) const = 0;
    
    // Matrix analysis
    virtual core::Result<core::semantic_vector::CorrelationMatrix> get_matrix_stats() const = 0;
    virtual core::Result<std::vector<double>> get_eigenvalues() const = 0;
    virtual core::Result<std::vector<std::vector<double>>> get_eigenvectors() const = 0;
    
    // Sparse matrix operations
    virtual core::Result<void> enable_sparse_storage() = 0;
    virtual core::Result<void> disable_sparse_storage() = 0;
    virtual core::Result<bool> is_sparse_enabled() const = 0;
    
    // Matrix compression
    virtual core::Result<void> compress_matrix() = 0;
    virtual core::Result<void> decompress_matrix() = 0;
    virtual core::Result<double> get_compression_ratio() const = 0;
    
    // Performance monitoring
    virtual core::Result<core::PerformanceMetrics> get_performance_metrics() const = 0;
    virtual core::Result<void> reset_performance_metrics() = 0;
    
    // Configuration
    virtual void update_config(const core::semantic_vector::SemanticVectorConfig::TemporalConfig& config) = 0;
    virtual core::semantic_vector::SemanticVectorConfig::TemporalConfig get_config() const = 0;
};

// ============================================================================
// MEMORY MANAGEMENT INTERFACES
// ============================================================================

/**
 * @brief Interface for tiered memory management
 * 
 * Provides tiered memory management capabilities using unified
 * memory optimization types and tiered memory policies.
 */
class ITieredMemoryManager {
public:
    virtual ~ITieredMemoryManager() = default;
    
    // Memory tier management
    virtual core::Result<void> add_to_tier(const core::SeriesID& series_id, core::semantic_vector::TieredMemoryPolicy::Tier tier) = 0;
    virtual core::Result<void> promote_to_tier(const core::SeriesID& series_id, core::semantic_vector::TieredMemoryPolicy::Tier tier) = 0;
    virtual core::Result<void> demote_from_tier(const core::SeriesID& series_id) = 0;
    virtual core::Result<core::semantic_vector::TieredMemoryPolicy::Tier> get_tier(const core::SeriesID& series_id) const = 0;
    
    // Memory access
    virtual core::Result<void> access_series(const core::SeriesID& series_id) = 0;
    virtual core::Result<double> get_access_frequency(const core::SeriesID& series_id) const = 0;
    virtual core::Result<std::vector<core::SeriesID>> get_series_in_tier(core::semantic_vector::TieredMemoryPolicy::Tier tier) const = 0;
    
    // Memory optimization
    virtual core::Result<void> optimize_memory_usage() = 0;
    virtual core::Result<void> defragment_memory() = 0;
    virtual core::Result<size_t> get_memory_usage(core::semantic_vector::TieredMemoryPolicy::Tier tier) const = 0;
    virtual core::Result<size_t> get_total_memory_usage() const = 0;
    
    // Policy management
    virtual core::Result<void> update_policy(const core::semantic_vector::TieredMemoryPolicy& policy) = 0;
    virtual core::Result<core::semantic_vector::TieredMemoryPolicy> get_policy() const = 0;
    
    // Performance monitoring
    virtual core::Result<core::semantic_vector::TieredMemoryPolicy::Metrics> get_metrics() const = 0;
    virtual core::Result<void> reset_metrics() = 0;
    
    // Configuration
    virtual void update_config(const core::semantic_vector::SemanticVectorConfig::MemoryConfig& config) = 0;
    virtual core::semantic_vector::SemanticVectorConfig::MemoryConfig get_config() const = 0;
};

/**
 * @brief Interface for adaptive memory pool management
 * 
 * Provides adaptive memory allocation, deallocation, and optimization
 * capabilities using unified memory optimization types.
 */
class IAdaptiveMemoryPool {
public:
    virtual ~IAdaptiveMemoryPool() = default;
    
    // Memory allocation
    virtual core::Result<void*> allocate(size_t size) = 0;
    virtual core::Result<void> deallocate(void* ptr) = 0;
    virtual core::Result<void*> reallocate(void* ptr, size_t new_size) = 0;
    
    // Memory pool management
    virtual core::Result<void> create_pool(size_t initial_size) = 0;
    virtual core::Result<void> destroy_pool() = 0;
    virtual core::Result<size_t> get_pool_size() const = 0;
    virtual core::Result<size_t> get_allocated_size() const = 0;
    virtual core::Result<size_t> get_free_size() const = 0;
    
    // Memory optimization
    virtual core::Result<void> defragment() = 0;
    virtual core::Result<void> compact() = 0;
    virtual core::Result<double> get_fragmentation_ratio() const = 0;
    virtual core::Result<double> get_allocation_efficiency() const = 0;
    
    // Performance monitoring
    virtual core::Result<core::PerformanceMetrics> get_performance_metrics() const = 0;
    virtual core::Result<void> reset_performance_metrics() = 0;
    
    // Configuration
    virtual void update_config(const core::semantic_vector::SemanticVectorConfig::MemoryConfig& config) = 0;
    virtual core::semantic_vector::SemanticVectorConfig::MemoryConfig get_config() const = 0;
};

// ============================================================================
// QUERY PROCESSING INTERFACES
// ============================================================================

/**
 * @brief Interface for advanced query processing
 * 
 * Provides advanced query processing capabilities including vector similarity,
 * semantic search, correlation analysis, and causal inference using unified
 * QueryProcessor types.
 */
class IAdvancedQueryProcessor {
public:
    virtual ~IAdvancedQueryProcessor() = default;
    
        // Vector similarity queries
    virtual core::Result<core::QueryResult> process_vector_query(
        const core::Vector& query_vector, 
        size_t k_nearest,
        const core::semantic_vector::SemanticVectorConfig::QueryConfig& config) = 0;
    
    // Semantic search queries
    virtual core::Result<core::QueryResult> process_semantic_query(
        const core::semantic_vector::SemanticQuery& query,
        const core::semantic_vector::SemanticVectorConfig::QueryConfig& config) = 0;
    
    // Correlation analysis queries
    virtual core::Result<core::QueryResult> process_correlation_query(
        const core::SeriesID& series_id,
        double correlation_threshold,
        const core::semantic_vector::SemanticVectorConfig::QueryConfig& config) = 0;
    
    // Causal inference queries
    virtual core::Result<core::QueryResult> process_causal_query(
        const core::SeriesID& cause_series,
        const core::SeriesID& effect_series,
        const core::semantic_vector::SemanticVectorConfig::QueryConfig& config) = 0;
    
    // Temporal reasoning queries
    virtual Result<core::QueryResult> process_temporal_query(
        const core::SeriesID& series_id,
        const std::string& reasoning_type,
        const core::semantic_vector::SemanticVectorConfig::QueryConfig& config) = 0;
    
    // Query optimization
    virtual Result<core::QueryPlan> optimize_query(const core::QueryProcessor::QueryType& query_type, const std::string& query_string) = 0;
    virtual Result<core::QueryResult> execute_query_plan(const core::QueryPlan& plan) = 0;
    
    // Query caching
    virtual Result<void> cache_result(const std::string& query_key, const core::QueryResult& result) = 0;
    virtual Result<core::QueryResult> get_cached_result(const std::string& query_key) const = 0;
    virtual Result<void> invalidate_cache(const std::string& query_key) = 0;
    virtual Result<void> clear_cache() = 0;
    
    // Performance monitoring
    virtual Result<core::PerformanceMetrics> get_performance_metrics() const = 0;
    virtual Result<void> reset_performance_metrics() = 0;
    
    // Configuration
    virtual void update_config(const core::semantic_vector::SemanticVectorConfig::QueryConfig& config) = 0;
    virtual core::semantic_vector::SemanticVectorConfig::QueryConfig get_config() const = 0;
};

// ============================================================================
// ADVANCED ANALYTICS INTERFACES
// ============================================================================

/**
 * @brief Interface for causal inference operations
 * 
 * Provides causal inference capabilities including Granger causality,
 * PC algorithm, and structural causal models using unified CausalInference types.
 */
class ICausalInference {
public:
    virtual ~ICausalInference() = default;
    
    // Causal inference operations
    virtual Result<std::vector<core::CausalInference::CausalRelationship>> infer_causality(
        const std::vector<core::SeriesID>& series_ids) = 0;
    
    virtual Result<core::CausalInference::CausalGraph> build_causal_graph(
        const std::vector<core::SeriesID>& series_ids) = 0;
    
    virtual Result<double> estimate_causal_effect(
        const core::SeriesID& treatment_series,
        const core::SeriesID& outcome_series) = 0;
    
    // Algorithm-specific operations
    virtual Result<std::vector<core::CausalInference::CausalRelationship>> granger_causality_test(
        const std::vector<core::SeriesID>& series_ids) = 0;
    
    virtual Result<core::CausalInference::CausalGraph> pc_algorithm(
        const std::vector<core::SeriesID>& series_ids) = 0;
    
    virtual Result<core::CausalInference::CausalGraph> structural_causal_model(
        const std::vector<core::SeriesID>& series_ids) = 0;
    
    // Performance monitoring
    virtual Result<core::PerformanceMetrics> get_performance_metrics() const = 0;
    virtual Result<void> reset_performance_metrics() = 0;
    
    // Configuration
    virtual void update_config(const core::semantic_vector::SemanticVectorConfig::AnalyticsConfig& config) = 0;
    virtual core::semantic_vector::SemanticVectorConfig::AnalyticsConfig get_config() const = 0;
};

/**
 * @brief Interface for temporal reasoning operations
 * 
 * Provides temporal reasoning capabilities including pattern recognition,
 * temporal correlation analysis, and multi-modal reasoning using unified
 * TemporalReasoning types.
 */
class ITemporalReasoning {
public:
    virtual ~ITemporalReasoning() = default;
    
    // Temporal reasoning operations
    virtual Result<std::vector<core::TemporalReasoning::TemporalPattern>> recognize_patterns(
        const core::SeriesID& series_id) = 0;
    
    virtual Result<std::vector<core::TemporalReasoning::TemporalInference>> make_inferences(
        const std::vector<core::SeriesID>& series_ids) = 0;
    
    virtual Result<double> analyze_temporal_correlation(
        const core::SeriesID& series1,
        const core::SeriesID& series2) = 0;
    
    // Pattern recognition operations
    virtual Result<std::vector<core::TemporalReasoning::TemporalPattern>> find_seasonal_patterns(
        const core::SeriesID& series_id) = 0;
    
    virtual Result<std::vector<core::TemporalReasoning::TemporalPattern>> find_trend_patterns(
        const core::SeriesID& series_id) = 0;
    
    virtual Result<std::vector<core::TemporalReasoning::TemporalPattern>> find_cyclic_patterns(
        const core::SeriesID& series_id) = 0;
    
    virtual Result<std::vector<core::TemporalReasoning::TemporalPattern>> find_anomaly_patterns(
        const core::SeriesID& series_id) = 0;
    
    // Multi-modal reasoning
    virtual Result<core::TemporalReasoning::TemporalInference> multi_modal_reasoning(
        const std::vector<core::SeriesID>& series_ids,
        const std::string& reasoning_type) = 0;
    
    // Performance monitoring
    virtual Result<core::PerformanceMetrics> get_performance_metrics() const = 0;
    virtual Result<void> reset_performance_metrics() = 0;
    
    // Configuration
    virtual void update_config(const core::semantic_vector::SemanticVectorConfig::AnalyticsConfig& config) = 0;
    virtual core::semantic_vector::SemanticVectorConfig::AnalyticsConfig get_config() const = 0;
};

// ============================================================================
// INTEGRATION CONTRACTS
// ============================================================================

/**
 * @brief Integration contracts between components
 * 
 * Defines the contracts and relationships between different components
 * to ensure consistent integration and proper data flow.
 */
struct IntegrationContracts {
    // Component dependencies
    struct Dependencies {
        std::shared_ptr<IVectorIndex> vector_index;
        std::shared_ptr<ISemanticIndex> semantic_index;
        std::shared_ptr<ITemporalGraph> temporal_graph;
        std::shared_ptr<ITieredMemoryManager> memory_manager;
        std::shared_ptr<IAdvancedQueryProcessor> query_processor;
        std::shared_ptr<ICausalInference> causal_inference;
        std::shared_ptr<ITemporalReasoning> temporal_reasoning;
    };
    
    // Data flow contracts
    struct DataFlow {
        // Vector processing flow
        std::function<Result<core::Vector>(const core::SeriesID&)> get_vector_data;
        std::function<Result<void>(const core::SeriesID&, const core::Vector&)> store_vector_data;
        
        // Semantic processing flow
        std::function<Result<core::Vector>(const core::SeriesID&)> get_semantic_data;
        std::function<Result<void>(const core::SeriesID&, const core::Vector&)> store_semantic_data;
        
        // Temporal processing flow
        std::function<Result<std::vector<double>>(const core::SeriesID&)> get_temporal_data;
        std::function<Result<void>(const core::SeriesID&, const std::vector<double>&)> store_temporal_data;
    };
    
    // Performance contracts
    struct PerformanceContracts {
        // Vector operations
        double max_vector_search_latency_ms = 1.0;
        double min_vector_search_accuracy = 0.95;
        size_t max_vectors_per_second = 10000;
        
        // Semantic operations
        double max_semantic_search_latency_ms = 5.0;
        double min_semantic_search_accuracy = 0.9;
        size_t max_semantic_queries_per_second = 1000;
        
        // Temporal operations
        double max_correlation_computation_latency_ms = 20.0;
        double min_correlation_accuracy = 0.9;
        size_t max_correlations_per_second = 100;
        
        // Memory operations
        double max_memory_allocation_latency_ms = 0.1;
        double min_memory_efficiency = 0.95;
        size_t max_memory_operations_per_second = 100000;
        
        // Query operations
        double max_query_execution_latency_ms = 10.0;
        double min_query_accuracy = 0.95;
        size_t max_queries_per_second = 100;
        
        // Analytics operations
        double max_inference_latency_ms = 50.0;
        double min_inference_accuracy = 0.9;
        size_t max_inferences_per_second = 10;
    };
    
    // Error handling contracts
    struct ErrorHandling {
        // Error recovery strategies
        std::function<Result<void>(const core::Error&)> vector_error_recovery;
        std::function<Result<void>(const core::Error&)> semantic_error_recovery;
        std::function<Result<void>(const core::Error&)> temporal_error_recovery;
        std::function<Result<void>(const core::Error&)> memory_error_recovery;
        std::function<Result<void>(const core::Error&)> query_error_recovery;
        std::function<Result<void>(const core::Error&)> analytics_error_recovery;
        
        // Circuit breaker patterns
        bool enable_circuit_breaker = true;
        size_t circuit_breaker_threshold = 5;
        std::chrono::milliseconds circuit_breaker_timeout = std::chrono::milliseconds(5000);
    };
};

// ============================================================================
// INTERFACE VALIDATION AND UTILITIES
// ============================================================================

/**
 * @brief Interface validator
 * 
 * Validates interface consistency and integration contracts
 * across all components of the semantic vector storage system.
 */
class InterfaceValidator {
public:
    // Interface validation
    static Result<bool> validate_vector_interfaces(const std::shared_ptr<IVectorIndex>& vector_index);
    static Result<bool> validate_semantic_interfaces(const std::shared_ptr<ISemanticIndex>& semantic_index);
    static Result<bool> validate_temporal_interfaces(const std::shared_ptr<ITemporalGraph>& temporal_graph);
    static Result<bool> validate_memory_interfaces(const std::shared_ptr<ITieredMemoryManager>& memory_manager);
    static Result<bool> validate_query_interfaces(const std::shared_ptr<IAdvancedQueryProcessor>& query_processor);
    static Result<bool> validate_analytics_interfaces(const std::shared_ptr<ICausalInference>& causal_inference,
                                                    const std::shared_ptr<ITemporalReasoning>& temporal_reasoning);
    
    // Integration validation
    static Result<bool> validate_integration_contracts(const IntegrationContracts& contracts);
    static Result<bool> validate_performance_contracts(const IntegrationContracts::PerformanceContracts& contracts);
    static Result<bool> validate_error_handling_contracts(const IntegrationContracts::ErrorHandling& contracts);
    
    // Cross-component validation
    static Result<bool> validate_cross_component_consistency(const IntegrationContracts::Dependencies& dependencies);
    static Result<bool> validate_data_flow_consistency(const IntegrationContracts::DataFlow& data_flow);
    
    // Comprehensive validation
    static Result<core::semantic_vector::ConfigValidationResult> validate_all_interfaces(
        const IntegrationContracts& contracts);
};

/**
 * @brief Interface factory
 * 
 * Creates and configures interface implementations with proper
 * dependencies and integration contracts.
 */
class InterfaceFactory {
public:
    // Interface creation
    static Result<std::shared_ptr<IVectorIndex>> create_vector_index(
        const core::semantic_vector::SemanticVectorConfig& config);
    
    static Result<std::shared_ptr<ISemanticIndex>> create_semantic_index(
        const core::semantic_vector::SemanticVectorConfig& config);
    
    static Result<std::shared_ptr<ITemporalGraph>> create_temporal_graph(
        const core::semantic_vector::SemanticVectorConfig& config);
    
    static Result<std::shared_ptr<ITieredMemoryManager>> create_memory_manager(
        const core::semantic_vector::SemanticVectorConfig& config);
    
    static Result<std::shared_ptr<IAdvancedQueryProcessor>> create_query_processor(
        const core::semantic_vector::SemanticVectorConfig& config);
    
    static Result<std::shared_ptr<ICausalInference>> create_causal_inference(
        const core::semantic_vector::SemanticVectorConfig& config);
    
    static Result<std::shared_ptr<ITemporalReasoning>> create_temporal_reasoning(
        const core::semantic_vector::SemanticVectorConfig& config);
    
    // Integration setup
    static Result<IntegrationContracts> setup_integration_contracts(
        const std::shared_ptr<IVectorIndex>& vector_index,
        const std::shared_ptr<ISemanticIndex>& semantic_index,
        const std::shared_ptr<ITemporalGraph>& temporal_graph,
        const std::shared_ptr<ITieredMemoryManager>& memory_manager,
        const std::shared_ptr<IAdvancedQueryProcessor>& query_processor,
        const std::shared_ptr<ICausalInference>& causal_inference,
        const std::shared_ptr<ITemporalReasoning>& temporal_reasoning);
};

} // namespace semantic_vector
} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_SEMANTIC_VECTOR_ARCHITECTURE_H_ 