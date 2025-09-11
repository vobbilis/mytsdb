#ifndef TSDB_STORAGE_ADVANCED_STORAGE_H_
#define TSDB_STORAGE_ADVANCED_STORAGE_H_

#include <memory>
#include <string>
#include <vector>
#include <optional>

#include "tsdb/core/types.h"
#include "tsdb/core/config.h"
#include "tsdb/core/error.h"
#include "tsdb/core/result.h"
#include "tsdb/storage/storage.h"
#include "tsdb/core/semantic_vector_types.h"
#include "tsdb/core/semantic_vector_config.h"
#include "tsdb/storage/semantic_vector_architecture.h"

namespace tsdb {
namespace storage {

// ============================================================================
// EXTENDED STORAGE INTERFACE
// ============================================================================

/**
 * @brief Extended storage interface with semantic vector capabilities
 * 
 * This interface extends the base Storage interface with advanced
 * semantic vector features while maintaining full backward compatibility.
 * All semantic vector operations are optional and only available when
 * semantic vector features are enabled in the configuration.
 */
class AdvancedStorage : public Storage {
public:
    virtual ~AdvancedStorage() = default;
    
    // ============================================================================
    // SEMANTIC VECTOR FEATURE MANAGEMENT
    // ============================================================================
    
    /**
     * @brief Check if semantic vector features are enabled
     */
    virtual bool semantic_vector_enabled() const = 0;
    
    /**
     * @brief Enable semantic vector features
     */
    virtual core::Result<void> enable_semantic_vector_features(
        const core::semantic_vector::SemanticVectorConfig& config = core::semantic_vector::SemanticVectorConfig::balanced_config()) = 0;
    
    /**
     * @brief Disable semantic vector features
     */
    virtual core::Result<void> disable_semantic_vector_features() = 0;
    
    /**
     * @brief Get semantic vector configuration
     */
    virtual core::Result<core::semantic_vector::SemanticVectorConfig> get_semantic_vector_config() const = 0;
    
    /**
     * @brief Update semantic vector configuration
     */
    virtual core::Result<void> update_semantic_vector_config(
        const core::semantic_vector::SemanticVectorConfig& config) = 0;
    
    // ============================================================================
    // VECTOR SIMILARITY SEARCH OPERATIONS
    // ============================================================================
    
    /**
     * @brief Write time series with vector embeddings
     * 
     * Stores time series data along with vector embeddings for similarity search.
     * The vector embedding is automatically generated from the time series data
     * if not provided.
     */
    virtual core::Result<void> write_with_vector(
        const core::TimeSeries& series,
        const std::optional<core::Vector>& vector_embedding = std::nullopt) = 0;
    
    /**
     * @brief Search for similar time series using vector similarity
     * 
     * Finds time series that are similar to the query vector based on
     * vector similarity metrics (cosine, euclidean, etc.).
     */
    virtual core::Result<std::vector<std::pair<core::SeriesID, double>>> vector_similarity_search(
        const core::Vector& query_vector,
        size_t k_nearest = 10,
        double similarity_threshold = 0.7) = 0;
    
    /**
     * @brief Search for similar time series using quantized vectors
     * 
     * Performs similarity search using quantized vectors for memory efficiency.
     */
    virtual core::Result<std::vector<std::pair<core::SeriesID, double>>> quantized_vector_search(
        const core::QuantizedVector& query_vector,
        size_t k_nearest = 10) = 0;
    
    /**
     * @brief Search for similar time series using binary vectors
     * 
     * Performs ultra-fast similarity search using binary vectors and
     * Hamming distance.
     */
    virtual core::Result<std::vector<std::pair<core::SeriesID, uint32_t>>> binary_vector_search(
        const core::BinaryVector& query_vector,
        size_t k_nearest = 10,
        uint32_t max_hamming_distance = 10) = 0;
    
    // ============================================================================
    // SEMANTIC SEARCH OPERATIONS
    // ============================================================================
    
    /**
     * @brief Write time series with semantic embeddings
     * 
     * Stores time series data along with semantic embeddings for natural
     * language search. The semantic embedding is automatically generated
     * from the time series metadata if not provided.
     */
    virtual core::Result<void> write_with_semantic_embedding(
        const core::TimeSeries& series,
        const std::optional<core::Vector>& semantic_embedding = std::nullopt) = 0;
    
    /**
     * @brief Search for time series using natural language queries
     * 
     * Finds time series that match natural language queries using
     * semantic similarity and entity/concept matching.
     */
    virtual core::Result<std::vector<std::pair<core::SeriesID, double>>> semantic_search(
        const core::SemanticQuery& query) = 0;
    
    /**
     * @brief Search for time series by entity
     * 
     * Finds time series that contain specific entities (e.g., "CPU", "memory", "network").
     */
    virtual core::Result<std::vector<core::SeriesID>> search_by_entity(
        const std::string& entity) = 0;
    
    /**
     * @brief Search for time series by concept
     * 
     * Finds time series that match specific concepts (e.g., "performance", "error", "latency").
     */
    virtual core::Result<std::vector<core::SeriesID>> search_by_concept(
        const std::string& concept) = 0;
    
    /**
     * @brief Process natural language query
     * 
     * Converts natural language queries into structured semantic queries
     * for processing.
     */
    virtual core::Result<core::SemanticQuery> process_natural_language_query(
        const std::string& natural_language_query) = 0;
    
    // ============================================================================
    // TEMPORAL CORRELATION OPERATIONS
    // ============================================================================
    
    /**
     * @brief Write time series with temporal correlation data
     * 
     * Stores time series data and automatically computes temporal correlations
     * with existing time series for causal inference and temporal reasoning.
     */
    virtual core::Result<void> write_with_temporal_correlation(
        const core::TimeSeries& series) = 0;
    
    /**
     * @brief Find correlated time series
     * 
     * Finds time series that are temporally correlated with the given series
     * based on correlation analysis.
     */
    virtual core::Result<std::vector<std::pair<core::SeriesID, double>>> find_correlated_series(
        const core::SeriesID& series_id,
        size_t k_nearest = 10,
        double correlation_threshold = 0.7) = 0;
    
    /**
     * @brief Perform causal inference analysis
     * 
     * Performs causal inference analysis to identify causal relationships
     * between time series.
     */
    virtual core::Result<std::vector<core::CausalInference::CausalRelationship>> causal_inference(
        const std::vector<core::SeriesID>& series_ids) = 0;
    
    /**
     * @brief Recognize temporal patterns
     * 
     * Identifies temporal patterns in time series data including seasonal,
     * trend, cyclic, and anomaly patterns.
     */
    virtual core::Result<std::vector<core::TemporalReasoning::TemporalPattern>> recognize_temporal_patterns(
        const core::SeriesID& series_id) = 0;
    
    // ============================================================================
    // ADVANCED QUERY OPERATIONS
    // ============================================================================
    
    /**
     * @brief Advanced query with semantic vector capabilities
     * 
     * Performs advanced queries that combine traditional time series queries
     * with semantic vector features like similarity search and correlation analysis.
     */
    virtual core::Result<core::QueryResult> advanced_query(
        const std::string& query_string,
        const core::semantic_vector::SemanticVectorConfig::QueryConfig& config = core::semantic_vector::SemanticVectorConfig::QueryConfig()) = 0;
    
    /**
     * @brief Multi-modal query processing
     * 
     * Processes queries that combine multiple modalities including vector similarity,
     * semantic search, correlation analysis, and causal inference.
     */
    virtual core::Result<core::QueryResult> multi_modal_query(
        const std::vector<std::string>& query_modalities,
        const core::semantic_vector::SemanticVectorConfig::QueryConfig& config = core::semantic_vector::SemanticVectorConfig::QueryConfig()) = 0;
    
    /**
     * @brief Query optimization
     * 
     * Optimizes query execution plans for better performance using
     * cost-based optimization and semantic vector features.
     */
    virtual core::Result<core::QueryPlan> optimize_query(
        const std::string& query_string) = 0;
    
    // ============================================================================
    // MEMORY OPTIMIZATION OPERATIONS
    // ============================================================================
    
    /**
     * @brief Optimize memory usage
     * 
     * Performs memory optimization operations including vector quantization,
     * embedding pruning, and graph compression.
     */
    virtual core::Result<void> optimize_memory_usage() = 0;
    
    /**
     * @brief Get memory usage statistics
     * 
     * Returns detailed memory usage statistics including vector storage,
     * semantic embeddings, and temporal graph memory usage.
     */
    virtual core::Result<core::PerformanceMetrics> get_memory_usage_stats() const = 0;
    
    /**
     * @brief Compress semantic vector data
     * 
     * Compresses semantic vector data using various compression techniques
     * to reduce memory usage while maintaining accuracy.
     */
    virtual core::Result<void> compress_semantic_vector_data() = 0;
    
    // ============================================================================
    // PERFORMANCE MONITORING OPERATIONS
    // ============================================================================
    
    /**
     * @brief Get semantic vector performance metrics
     * 
     * Returns comprehensive performance metrics for all semantic vector
     * operations including search latency, accuracy, and throughput.
     */
    virtual core::Result<core::PerformanceMetrics> get_semantic_vector_performance_metrics() const = 0;
    
    /**
     * @brief Reset performance metrics
     * 
     * Resets all performance metrics for semantic vector operations.
     */
    virtual core::Result<void> reset_semantic_vector_performance_metrics() = 0;
    
    /**
     * @brief Get component-specific performance metrics
     * 
     * Returns performance metrics for specific semantic vector components.
     */
    virtual core::Result<core::PerformanceMetrics> get_component_performance_metrics(
        const std::string& component_name) const = 0;
    
    // ============================================================================
    // CONFIGURATION AND MANAGEMENT OPERATIONS
    // ============================================================================
    
    /**
     * @brief Get semantic vector component status
     * 
     * Returns the status of all semantic vector components including
     * whether they are initialized, running, and healthy.
     */
    virtual core::Result<std::map<std::string, std::string>> get_semantic_vector_component_status() const = 0;
    
    /**
     * @brief Validate semantic vector configuration
     * 
     * Validates the current semantic vector configuration and returns
     * any validation errors or warnings.
     */
    virtual core::Result<core::semantic_vector::ConfigValidationResult> validate_semantic_vector_config() const = 0;
    
    /**
     * @brief Migrate semantic vector data
     * 
     * Migrates semantic vector data from one configuration to another
     * while maintaining data integrity and performance.
     */
    virtual core::Result<void> migrate_semantic_vector_data(
        const core::semantic_vector::SemanticVectorConfig& new_config) = 0;
    
    // ============================================================================
    // BACKWARD COMPATIBILITY OPERATIONS
    // ============================================================================
    
    /**
     * @brief Check backward compatibility
     * 
     * Checks if the current semantic vector configuration is compatible
     * with existing data and operations.
     */
    virtual core::Result<bool> check_backward_compatibility() const = 0;
    
    /**
     * @brief Export to legacy format
     * 
     * Exports semantic vector data to legacy format for backward compatibility.
     */
    virtual core::Result<std::string> export_to_legacy_format() const = 0;
    
    /**
     * @brief Import from legacy format
     * 
     * Imports semantic vector data from legacy format while maintaining
     * backward compatibility.
     */
    virtual core::Result<void> import_from_legacy_format(const std::string& legacy_data) = 0;
};

// ============================================================================
// EXTENDED STORAGE FACTORY
// ============================================================================

/**
 * @brief Extended storage factory with semantic vector capabilities
 * 
 * Factory for creating advanced storage instances with semantic vector
 * features while maintaining backward compatibility with existing storage.
 */
class AdvancedStorageFactory : public StorageFactory {
public:
    virtual ~AdvancedStorageFactory() = default;
    
    /**
     * @brief Create advanced storage instance with semantic vector features
     */
    virtual std::unique_ptr<AdvancedStorage> create_advanced_storage(
        const core::StorageConfig& config) = 0;
    
    /**
     * @brief Create advanced storage instance with specific semantic vector configuration
     */
    virtual std::unique_ptr<AdvancedStorage> create_advanced_storage_with_semantic_vector(
        const core::StorageConfig& config,
        const core::semantic_vector::SemanticVectorConfig& semantic_vector_config) = 0;
    
    /**
     * @brief Create advanced storage instance for specific use case
     */
    virtual std::unique_ptr<AdvancedStorage> create_advanced_storage_for_use_case(
        const core::StorageConfig& config,
        const std::string& use_case) = 0;  // "high_performance", "memory_efficient", "high_accuracy", etc.
};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Create advanced storage instance
 * 
 * Convenience function for creating advanced storage instances with
 * semantic vector capabilities.
 */
std::shared_ptr<AdvancedStorage> CreateAdvancedStorage(
    const StorageOptions& options,
    const std::optional<core::semantic_vector::SemanticVectorConfig>& semantic_vector_config = std::nullopt);

/**
 * @brief Create advanced storage instance for specific use case
 * 
 * Creates advanced storage instances optimized for specific use cases
 * like high performance, memory efficiency, or high accuracy.
 */
std::shared_ptr<AdvancedStorage> CreateAdvancedStorageForUseCase(
    const StorageOptions& options,
    const std::string& use_case);

/**
 * @brief Check if storage supports semantic vector features
 * 
 * Checks if a storage instance supports semantic vector features
 * and returns the level of support.
 */
core::Result<std::string> CheckSemanticVectorSupport(const std::shared_ptr<Storage>& storage);

/**
 * @brief Upgrade storage to advanced storage
 * 
 * Upgrades a basic storage instance to advanced storage with
 * semantic vector capabilities.
 */
core::Result<std::shared_ptr<AdvancedStorage>> UpgradeToAdvancedStorage(
    const std::shared_ptr<Storage>& storage,
    const core::semantic_vector::SemanticVectorConfig& semantic_vector_config = core::semantic_vector::SemanticVectorConfig::balanced_config());

// ============================================================================
// PERFORMANCE GUARANTEES
// ============================================================================

/**
 * @brief Performance guarantees for advanced storage operations
 * 
 * Defines performance guarantees for all advanced storage operations
 * to ensure consistent performance across different implementations.
 */
struct AdvancedStoragePerformanceGuarantees {
    // Vector similarity search guarantees
    double max_vector_search_latency_ms = 1.0;
    double min_vector_search_accuracy = 0.95;
    size_t max_vectors_per_second = 10000;
    
    // Semantic search guarantees
    double max_semantic_search_latency_ms = 5.0;
    double min_semantic_search_accuracy = 0.9;
    size_t max_semantic_queries_per_second = 1000;
    
    // Temporal correlation guarantees
    double max_correlation_computation_latency_ms = 20.0;
    double min_correlation_accuracy = 0.9;
    size_t max_correlations_per_second = 100;
    
    // Memory optimization guarantees
    double target_memory_reduction_ratio = 0.8;  // 80% memory reduction
    double max_latency_impact = 0.05;            // 5% max latency increase
    double min_accuracy_preservation = 0.95;     // 95% accuracy preservation
    
    // Query processing guarantees
    double max_advanced_query_latency_ms = 10.0;
    double min_query_accuracy = 0.95;
    size_t max_queries_per_second = 100;
    
    // Analytics guarantees
    double max_causal_inference_latency_ms = 50.0;
    double min_inference_accuracy = 0.9;
    size_t max_inferences_per_second = 10;
};

} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_ADVANCED_STORAGE_H_ 