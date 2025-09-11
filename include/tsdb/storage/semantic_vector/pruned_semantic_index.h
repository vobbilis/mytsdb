#ifndef TSDB_STORAGE_SEMANTIC_VECTOR_SEMANTIC_INDEX_H_
#define TSDB_STORAGE_SEMANTIC_VECTOR_SEMANTIC_INDEX_H_

#include <memory>
#include <vector>
#include <string>
#include <map>
#include <optional>
#include <shared_mutex>

#include "tsdb/core/semantic_vector_types.h"
#include "tsdb/core/semantic_vector_config.h"
#include "tsdb/storage/semantic_vector_architecture.h"
#include "tsdb/core/result.h"
#include "tsdb/core/error.h"

namespace tsdb {
namespace storage {
namespace semantic_vector {

// ============================================================================
// SEMANTIC INDEX COMPONENT HEADER
// ============================================================================

/**
 * @brief Semantic Index Component for Natural Language Search
 * 
 * This component provides natural language search capabilities using semantic
 * embeddings, entity extraction, and concept matching for time series data.
 * 
 * DESIGN HINTS:
 * - Use BERT-based embeddings for semantic understanding
 * - Implement entity extraction for specific time series identification
 * - Use concept matching for abstract query processing
 * - Support embedding pruning for memory optimization
 * - Enable knowledge distillation for smaller, faster models
 * - Implement sparse indexing for efficient storage
 * - Support multi-language queries and embeddings
 * 
 * PERFORMANCE EXPECTATIONS:
 * - Semantic search latency: < 5ms for processed queries
 * - Search accuracy: > 90% for natural language queries
 * - Memory usage: 80% reduction with embedding pruning
 * - Throughput: > 1,000 semantic queries/second
 * - Entity extraction: < 1ms per query
 * - Concept matching: < 2ms per query
 * 
 * IMPLEMENTATION GUIDANCE:
 * - Use unified SemanticQuery types from TASK-1 for consistency
 * - Implement memory optimization using unified memory types
 * - Follow performance contracts from TASK-4 interfaces
 * - Use configuration from TASK-2 for flexible deployment
 * - Integrate with BERT models for embedding generation
 * - Implement circuit breaker patterns for error handling
 * - Add comprehensive performance monitoring and metrics
 * 
 * INTEGRATION NOTES:
 * - Integrates with ISemanticIndex interface from TASK-4
 * - Uses unified SemanticQuery, SemanticIndex, PrunedEmbedding types from TASK-1
 * - Follows memory optimization strategies from TASK-2
 * - Supports AdvancedStorage operations from TASK-5
 * - Enables seamless integration with existing storage operations
 */

/**
 * @brief Semantic Index Implementation
 * 
 * Implements the ISemanticIndex interface with BERT-based semantic processing,
 * entity extraction, and concept matching for natural language search.
 */
class SemanticIndexImpl : public ISemanticIndex {
public:
    /**
     * @brief Constructor with configuration
     * 
     * DESIGN HINT: Initialize BERT model and embedding generation pipeline
     * to support real-time semantic processing of queries and time series data.
     * 
     * PERFORMANCE HINT: Pre-load BERT model and warm up embedding generation
     * to minimize latency during search operations.
     */
    explicit SemanticIndexImpl(const core::semantic_vector::SemanticVectorConfig::SemanticConfig& config);
    
    virtual ~SemanticIndexImpl() = default;
    
    // ============================================================================
    // SEMANTIC EMBEDDING MANAGEMENT
    // ============================================================================
    
    /**
     * @brief Add semantic embedding for time series
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Generate BERT embedding from time series metadata if not provided
     * - Apply embedding pruning for memory optimization
     * - Extract entities and concepts for indexing
     * - Store in sparse format for efficient retrieval
     * - Update entity and concept indices
     * - Handle concurrent access safely
     * 
     * PERFORMANCE TARGETS:
     * - Add latency: < 10ms per embedding
     * - Memory overhead: < 20% of original embedding size
     * - Throughput: > 10,000 embeddings/second for batch operations
     * - Pruning accuracy: > 95% of original semantic accuracy
     */
    core::Result<void> add_semantic_embedding(const core::SeriesID& series_id, const core::Vector& embedding) override;
    
    /**
     * @brief Update semantic embedding
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Efficiently update embedding and related indices
     * - Maintain consistency across entity and concept mappings
     * - Optimize for incremental updates
     * - Handle embedding regeneration if needed
     */
    core::Result<void> update_semantic_embedding(const core::SeriesID& series_id, const core::Vector& embedding) override;
    
    /**
     * @brief Remove semantic embedding
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Remove from all semantic indices
     * - Clean up entity and concept mappings
     * - Reclaim memory efficiently
     * - Update performance metrics
     */
    core::Result<void> remove_semantic_embedding(const core::SeriesID& series_id) override;
    
    /**
     * @brief Get semantic embedding by series ID
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Support both original and pruned embedding retrieval
     * - Use tiered memory for optimal access patterns
     * - Implement caching for frequently accessed embeddings
     * - Handle embedding reconstruction from pruned format
     */
    core::Result<core::Vector> get_semantic_embedding(const core::SeriesID& series_id) const override;
    
    // ============================================================================
    // SEMANTIC SEARCH OPERATIONS
    // ============================================================================
    
    /**
     * @brief Search using semantic query
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Process natural language query into structured format
     * - Generate query embedding using BERT model
     * - Perform semantic similarity search
     * - Apply entity and concept filtering
     * - Rank results by semantic relevance
     * - Support query expansion for better recall
     * 
     * PERFORMANCE TARGETS:
     * - Search latency: < 5ms for processed queries
     * - Search accuracy: > 90% for natural language queries
     * - Throughput: > 1,000 semantic queries/second
     * - Query processing: < 2ms for natural language parsing
     */
    core::Result<std::vector<std::pair<core::SeriesID, double>>> semantic_search(
        const core::semantic_vector::SemanticQuery& query) const override;
    
    // ============================================================================
    // ENTITY AND CONCEPT MANAGEMENT
    // ============================================================================
    
    /**
     * @brief Add entities for time series
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Extract entities from time series metadata
     * - Use NER (Named Entity Recognition) for entity extraction
     * - Store entity mappings efficiently
     * - Support entity normalization and disambiguation
     * - Handle entity relationships and hierarchies
     * 
     * PERFORMANCE TARGETS:
     * - Entity extraction: < 1ms per time series
     * - Entity storage: < 1KB per entity mapping
     * - Entity lookup: < 0.1ms per entity query
     */
    core::Result<void> add_entities(const core::SeriesID& series_id, const std::vector<std::string>& entities) override;
    
    /**
     * @brief Add concepts for time series
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Extract concepts from time series metadata
     * - Use concept extraction models for abstract concepts
     * - Store concept mappings efficiently
     * - Support concept hierarchies and relationships
     * - Handle concept disambiguation
     * 
     * PERFORMANCE TARGETS:
     * - Concept extraction: < 2ms per time series
     * - Concept storage: < 2KB per concept mapping
     * - Concept lookup: < 0.2ms per concept query
     */
    core::Result<void> add_concepts(const core::SeriesID& series_id, const std::vector<std::string>& concepts) override;
    
    /**
     * @brief Get entities for time series
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Retrieve entities efficiently from storage
     * - Support entity ranking by relevance
     * - Handle entity relationships
     * - Cache frequently accessed entities
     */
    core::Result<std::vector<std::string>> get_entities(const core::SeriesID& series_id) const override;
    
    /**
     * @brief Get concepts for time series
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Retrieve concepts efficiently from storage
     * - Support concept ranking by relevance
     * - Handle concept hierarchies
     * - Cache frequently accessed concepts
     */
    core::Result<std::vector<std::string>> get_concepts(const core::SeriesID& series_id) const override;
    
    // ============================================================================
    // ENTITY AND CONCEPT SEARCH
    // ============================================================================
    
    /**
     * @brief Search by entity
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Use efficient entity indexing for fast lookup
     * - Support entity normalization and fuzzy matching
     * - Handle entity relationships and hierarchies
     * - Rank results by entity relevance
     * - Support entity-based query expansion
     * 
     * PERFORMANCE TARGETS:
     * - Entity search latency: < 1ms per entity query
     * - Entity search accuracy: > 95% for exact matches
     * - Entity search throughput: > 10,000 entity queries/second
     */
    core::Result<std::vector<core::SeriesID>> search_by_entity(const std::string& entity) const override;
    
    /**
     * @brief Search by concept
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Use efficient concept indexing for fast lookup
     * - Support concept normalization and fuzzy matching
     * - Handle concept hierarchies and relationships
     * - Rank results by concept relevance
     * - Support concept-based query expansion
     * 
     * PERFORMANCE TARGETS:
     * - Concept search latency: < 2ms per concept query
     * - Concept search accuracy: > 90% for exact matches
     * - Concept search throughput: > 5,000 concept queries/second
     */
    core::Result<std::vector<core::SeriesID>> search_by_concept(const std::string& concept) const override;
    
    // ============================================================================
    // PRUNED EMBEDDING OPERATIONS
    // ============================================================================
    
    /**
     * @brief Prune embedding for memory optimization
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Implement magnitude-based pruning for embedding weights
     * - Use knowledge distillation for smaller models
     * - Maintain semantic accuracy within acceptable bounds
     * - Support configurable sparsity levels
     * - Optimize for fast reconstruction
     * 
     * PERFORMANCE TARGETS:
     * - Pruning time: < 1ms per embedding
     * - Memory reduction: 80-90% compared to original embeddings
     * - Accuracy preservation: > 95% of original semantic accuracy
     * - Reconstruction time: < 0.1ms per embedding
     */
    core::Result<core::semantic_vector::PrunedEmbedding> prune_embedding(const core::Vector& embedding) override;
    
    /**
     * @brief Reconstruct embedding from pruned format
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Efficient reconstruction using sparse format
     * - Support partial reconstruction for performance optimization
     * - Handle pruning errors gracefully
     * - Cache frequently used reconstruction patterns
     */
    core::Result<core::Vector> reconstruct_embedding(const core::semantic_vector::PrunedEmbedding& pruned) override;
    
    /**
     * @brief Get pruning accuracy metrics
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Track pruning accuracy over time
     * - Compare pruned vs original embedding performance
     * - Provide accuracy statistics for different sparsity levels
     * - Support accuracy optimization recommendations
     */
    core::Result<double> get_pruning_accuracy() const override;
    
    // ============================================================================
    // PERFORMANCE MONITORING
    // ============================================================================
    
    /**
     * @brief Get performance metrics
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Track all performance metrics defined in unified types
     * - Include search latency, accuracy, and throughput
     * - Monitor memory usage and optimization ratios
     * - Provide component-specific metrics
     * - Support real-time monitoring and alerting
     */
    core::Result<core::PerformanceMetrics> get_performance_metrics() const override;
    
    /**
     * @brief Reset performance metrics
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Reset all performance counters
     * - Clear historical metrics
     * - Maintain configuration and settings
     * - Log reset events for monitoring
     */
    core::Result<void> reset_performance_metrics() override;
    
    // ============================================================================
    // CONFIGURATION MANAGEMENT
    // ============================================================================
    
    /**
     * @brief Update configuration
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Validate new configuration using unified validation
     * - Apply configuration changes safely
     * - Rebuild affected semantic structures if necessary
     * - Maintain backward compatibility
     * - Log configuration changes for audit
     */
    void update_config(const core::semantic_vector::SemanticVectorConfig::SemanticConfig& config) override;
    
    /**
     * @brief Get current configuration
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Return current configuration state
     * - Include runtime modifications
     * - Provide configuration validation status
     * - Support configuration export for backup
     */
    core::semantic_vector::SemanticVectorConfig::SemanticConfig get_config() const override;

private:
    // ============================================================================
    // PRIVATE IMPLEMENTATION DETAILS
    // ============================================================================
    
    /**
     * @brief Internal semantic structures
     * 
     * DESIGN HINT: Use efficient data structures for semantic processing:
     * - BERT model for embedding generation
     * - Sparse embeddings for memory efficiency
     * - Entity and concept indices for fast lookup
     * - Pruned embeddings for memory optimization
     */
    struct SemanticStructures {
        // BERT model for embedding generation
        std::unique_ptr<class BERTModel> bert_model;
        
        // Sparse semantic embeddings
        std::unique_ptr<class SparseSemanticStorage> semantic_storage;
        
        // Entity index for fast entity lookup
        std::unique_ptr<class EntityIndex> entity_index;
        
        // Concept index for fast concept lookup
        std::unique_ptr<class ConceptIndex> concept_index;
        
        // Pruned embedding storage
        std::unique_ptr<class PrunedEmbeddingStorage> pruned_storage;
        
        // Query processing pipeline
        std::unique_ptr<class SemanticQueryProcessor> query_processor;
    };
    
    /**
     * @brief Performance monitoring structures
     * 
     * DESIGN HINT: Track comprehensive performance metrics for all semantic operations
     * to enable optimization and monitoring.
     */
    struct PerformanceMonitoring {
        // Search performance metrics
        std::atomic<double> average_semantic_search_latency_ms{0.0};
        std::atomic<double> average_semantic_search_accuracy{0.0};
        std::atomic<size_t> total_semantic_searches{0};
        
        // Entity and concept metrics
        std::atomic<double> average_entity_search_latency_ms{0.0};
        std::atomic<double> average_concept_search_latency_ms{0.0};
        std::atomic<size_t> total_entity_searches{0};
        std::atomic<size_t> total_concept_searches{0};
        
        // Memory usage metrics
        std::atomic<size_t> total_semantic_memory_usage_bytes{0};
        std::atomic<double> semantic_memory_compression_ratio{1.0};
        std::atomic<size_t> semantic_embeddings_stored{0};
        
        // Pruning metrics
        std::atomic<double> average_pruning_accuracy{0.0};
        std::atomic<double> average_pruning_time_ms{0.0};
        std::atomic<size_t> total_pruned_embeddings{0};
        
        // Error tracking
        std::atomic<size_t> semantic_search_errors{0};
        std::atomic<size_t> embedding_generation_errors{0};
        std::atomic<size_t> entity_extraction_errors{0};
    };
    
    // Configuration and state
    core::semantic_vector::SemanticVectorConfig::SemanticConfig config_;
    SemanticStructures semantic_structures_;
    PerformanceMonitoring performance_monitoring_;
    mutable std::shared_mutex mutex_;
    
    // Internal helper methods
    core::Result<void> initialize_semantic_structures();
    core::Result<void> validate_embedding(const core::Vector& embedding) const;
    core::Result<core::Vector> generate_embedding_from_metadata(const core::SeriesID& series_id) const;
    core::Result<std::vector<std::string>> extract_entities_from_metadata(const core::SeriesID& series_id) const;
    core::Result<std::vector<std::string>> extract_concepts_from_metadata(const core::SeriesID& series_id) const;
    core::Result<double> compute_semantic_similarity(const core::Vector& v1, const core::Vector& v2) const;
    core::Result<void> update_performance_metrics(const std::string& operation, double latency, bool success) const;
    core::Result<void> handle_memory_pressure();
    core::Result<void> optimize_semantic_structures();
};

// ============================================================================
// FACTORY AND UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Create semantic index instance
 * 
 * IMPLEMENTATION GUIDANCE:
 * - Use factory pattern for flexible instantiation
 * - Support different BERT models for different use cases
 * - Validate configuration before creation
 * - Provide default configurations for common scenarios
 */
std::unique_ptr<ISemanticIndex> CreateSemanticIndex(
    const core::semantic_vector::SemanticVectorConfig::SemanticConfig& config);

/**
 * @brief Create semantic index for specific use case
 * 
 * IMPLEMENTATION GUIDANCE:
 * - Provide optimized configurations for common use cases
 * - Support high-performance, memory-efficient, and high-accuracy modes
 * - Validate use case requirements
 * - Provide configuration recommendations
 */
std::unique_ptr<ISemanticIndex> CreateSemanticIndexForUseCase(
    const std::string& use_case,  // "high_performance", "memory_efficient", "high_accuracy"
    const core::semantic_vector::SemanticVectorConfig::SemanticConfig& base_config = core::semantic_vector::SemanticVectorConfig::SemanticConfig());

/**
 * @brief Validate semantic index configuration
 * 
 * IMPLEMENTATION GUIDANCE:
 * - Validate all configuration parameters
 * - Check for conflicts and inconsistencies
 * - Provide detailed error messages and suggestions
 * - Support configuration optimization recommendations
 */
core::Result<core::semantic_vector::ConfigValidationResult> ValidateSemanticIndexConfig(
    const core::semantic_vector::SemanticVectorConfig::SemanticConfig& config);

} // namespace semantic_vector
} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_SEMANTIC_VECTOR_SEMANTIC_INDEX_H_ 