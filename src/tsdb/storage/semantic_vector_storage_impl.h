#ifndef TSDB_STORAGE_SEMANTIC_VECTOR_STORAGE_IMPL_H_
#define TSDB_STORAGE_SEMANTIC_VECTOR_STORAGE_IMPL_H_

#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <unordered_map>
#include <queue>
#include <chrono>

#include "tsdb/core/types.h"
#include "tsdb/core/config.h"
#include "tsdb/core/error.h"
#include "tsdb/core/result.h"
#include "tsdb/storage/storage.h"
#include "tsdb/core/semantic_vector_types.h"
#include "tsdb/core/semantic_vector_config.h"
#include "tsdb/storage/advanced_storage.h"
#include "tsdb/storage/semantic_vector_architecture.h"

// Component implementation headers (TASK-6)
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

namespace tsdb {
namespace storage {

// ============================================================================
// SEMANTIC VECTOR STORAGE IMPLEMENTATION
// ============================================================================

/**
 * @brief Main implementation of semantic vector storage
 * 
 * This is the core implementation class that integrates all semantic vector
 * components with consistent interfaces. It uses unified types from TASK-1,
 * configuration from TASK-2, interfaces from TASK-4, and extends the storage
 * interface from TASK-5.
 * 
 * Key Features:
 * - Dual-write strategy with proper error handling
 * - Memory management integration using unified memory types
 * - Performance monitoring and metrics collection
 * - Backward compatibility with existing storage
 * - Component integration with consistent interfaces
 */
class SemanticVectorStorageImpl : public AdvancedStorage {
public:
    // ============================================================================
    // CONSTRUCTOR AND DESTRUCTOR
    // ============================================================================
    
    /**
     * @brief Constructor with unified configuration
     * 
     * Initializes the semantic vector storage with unified configuration
     * from TASK-2, ensuring all components are properly configured.
     */
    explicit SemanticVectorStorageImpl(
        const core::StorageConfig& storage_config,
        const core::semantic_vector::SemanticVectorConfig& semantic_vector_config = core::semantic_vector::SemanticVectorConfig::balanced_config());
    
    /**
     * @brief Destructor with proper cleanup
     */
    virtual ~SemanticVectorStorageImpl();
    
    // ============================================================================
    // SEMANTIC VECTOR FEATURE MANAGEMENT (AdvancedStorage Interface)
    // ============================================================================
    
    /**
     * @brief Check if semantic vector features are enabled
     */
    bool semantic_vector_enabled() const override;
    
    /**
     * @brief Enable semantic vector features
     */
    core::Result<void> enable_semantic_vector_features(
        const core::semantic_vector::SemanticVectorConfig& config = core::semantic_vector::SemanticVectorConfig::balanced_config()) override;
    
    /**
     * @brief Disable semantic vector features
     */
    core::Result<void> disable_semantic_vector_features() override;
    
    /**
     * @brief Get semantic vector configuration
     */
    core::Result<core::semantic_vector::SemanticVectorConfig> get_semantic_vector_config() const override;
    
    /**
     * @brief Update semantic vector configuration
     */
    core::Result<void> update_semantic_vector_config(
        const core::semantic_vector::SemanticVectorConfig& config) override;
    
    // ============================================================================
    // VECTOR SIMILARITY SEARCH OPERATIONS (AdvancedStorage Interface)
    // ============================================================================
    
    /**
     * @brief Write time series with vector embeddings
     * 
     * Implements dual-write strategy: writes to base storage and vector index
     * with proper error handling and rollback support.
     */
    core::Result<void> write_with_vector(
        const core::TimeSeries& series,
        const std::optional<core::Vector>& vector_embedding = std::nullopt) override;
    
    /**
     * @brief Search for similar time series using vector similarity
     */
    core::Result<std::vector<std::pair<core::SeriesID, double>>> vector_similarity_search(
        const core::Vector& query_vector,
        size_t k_nearest = 10,
        double similarity_threshold = 0.7) override;
    
    /**
     * @brief Search for similar time series using quantized vectors
     */
    core::Result<std::vector<std::pair<core::SeriesID, double>>> quantized_vector_search(
        const core::QuantizedVector& query_vector,
        size_t k_nearest = 10) override;
    
    /**
     * @brief Search for similar time series using binary vectors
     */
    core::Result<std::vector<std::pair<core::SeriesID, uint32_t>>> binary_vector_search(
        const core::BinaryVector& query_vector,
        size_t k_nearest = 10,
        uint32_t max_hamming_distance = 10) override;
    
    // ============================================================================
    // SEMANTIC SEARCH OPERATIONS (AdvancedStorage Interface)
    // ============================================================================
    
    /**
     * @brief Write time series with semantic embeddings
     */
    core::Result<void> write_with_semantic_embedding(
        const core::TimeSeries& series,
        const std::optional<core::Vector>& semantic_embedding = std::nullopt) override;
    
    /**
     * @brief Search for time series using natural language queries
     */
    core::Result<std::vector<std::pair<core::SeriesID, double>>> semantic_search(
        const core::SemanticQuery& query) override;
    
    /**
     * @brief Search for time series by entity
     */
    core::Result<std::vector<core::SeriesID>> search_by_entity(
        const std::string& entity) override;
    
    /**
     * @brief Search for time series by concept
     */
    core::Result<std::vector<core::SeriesID>> search_by_concept(
        const std::string& concept) override;
    
    /**
     * @brief Process natural language query
     */
    core::Result<core::SemanticQuery> process_natural_language_query(
        const std::string& natural_language_query) override;
    
    // ============================================================================
    // TEMPORAL CORRELATION OPERATIONS (AdvancedStorage Interface)
    // ============================================================================
    
    /**
     * @brief Write time series with temporal correlation data
     */
    core::Result<void> write_with_temporal_correlation(
        const core::TimeSeries& series) override;
    
    /**
     * @brief Find correlated time series
     */
    core::Result<std::vector<std::pair<core::SeriesID, double>>> find_correlated_series(
        const core::SeriesID& series_id,
        size_t k_nearest = 10,
        double correlation_threshold = 0.7) override;
    
    /**
     * @brief Perform causal inference analysis
     */
    core::Result<std::vector<core::CausalInference::CausalRelationship>> causal_inference(
        const std::vector<core::SeriesID>& series_ids) override;
    
    /**
     * @brief Recognize temporal patterns
     */
    core::Result<std::vector<core::TemporalReasoning::TemporalPattern>> recognize_temporal_patterns(
        const core::SeriesID& series_id) override;
    
    // ============================================================================
    // ADVANCED QUERY OPERATIONS (AdvancedStorage Interface)
    // ============================================================================
    
    /**
     * @brief Advanced query with semantic vector capabilities
     */
    core::Result<core::QueryResult> advanced_query(
        const std::string& query_string,
        const core::semantic_vector::SemanticVectorConfig::QueryConfig& config = core::semantic_vector::SemanticVectorConfig::QueryConfig()) override;
    
    /**
     * @brief Multi-modal query processing
     */
    core::Result<core::QueryResult> multi_modal_query(
        const std::vector<std::string>& query_modalities,
        const core::semantic_vector::SemanticVectorConfig::QueryConfig& config = core::semantic_vector::SemanticVectorConfig::QueryConfig()) override;
    
    /**
     * @brief Query optimization
     */
    core::Result<core::QueryPlan> optimize_query(
        const std::string& query_string) override;
    
    // ============================================================================
    // MEMORY OPTIMIZATION OPERATIONS (AdvancedStorage Interface)
    // ============================================================================
    
    /**
     * @brief Optimize memory usage
     */
    core::Result<void> optimize_memory_usage() override;
    
    /**
     * @brief Get memory usage statistics
     */
    core::Result<core::PerformanceMetrics> get_memory_usage_stats() const override;
    
    /**
     * @brief Compress semantic vector data
     */
    core::Result<void> compress_semantic_vector_data() override;
    
    // ============================================================================
    // PERFORMANCE MONITORING OPERATIONS (AdvancedStorage Interface)
    // ============================================================================
    
    /**
     * @brief Get semantic vector performance metrics
     */
    core::Result<core::PerformanceMetrics> get_semantic_vector_performance_metrics() const override;
    
    /**
     * @brief Reset performance metrics
     */
    core::Result<void> reset_semantic_vector_performance_metrics() override;
    
    /**
     * @brief Get component-specific performance metrics
     */
    core::Result<core::PerformanceMetrics> get_component_performance_metrics(
        const std::string& component_name) const override;
    
    // ============================================================================
    // CONFIGURATION AND MANAGEMENT OPERATIONS (AdvancedStorage Interface)
    // ============================================================================
    
    /**
     * @brief Get semantic vector component status
     */
    core::Result<std::map<std::string, std::string>> get_semantic_vector_component_status() const override;
    
    /**
     * @brief Validate semantic vector configuration
     */
    core::Result<core::semantic_vector::ConfigValidationResult> validate_semantic_vector_config() const override;
    
    /**
     * @brief Migrate semantic vector data
     */
    core::Result<void> migrate_semantic_vector_data(
        const core::semantic_vector::SemanticVectorConfig& new_config) override;
    
    // ============================================================================
    // BACKWARD COMPATIBILITY OPERATIONS (AdvancedStorage Interface)
    // ============================================================================
    
    /**
     * @brief Check backward compatibility
     */
    core::Result<bool> check_backward_compatibility() const override;
    
    /**
     * @brief Export to legacy format
     */
    core::Result<std::string> export_to_legacy_format() const override;
    
    /**
     * @brief Import from legacy format
     */
    core::Result<void> import_from_legacy_format(const std::string& legacy_data) override;
    
    // ============================================================================
    // BASE STORAGE OPERATIONS (Storage Interface - Backward Compatibility)
    // ============================================================================
    
    /**
     * @brief Write time series data (base storage operation)
     * 
     * Implements dual-write strategy: writes to base storage and optionally
     * to semantic vector components if enabled.
     */
    core::Result<void> write(const core::TimeSeries& series) override;
    
    /**
     * @brief Read time series data (base storage operation)
     */
    core::Result<core::TimeSeries> read(const core::SeriesID& series_id) override;
    
    /**
     * @brief Delete time series data (base storage operation)
     * 
     * Implements dual-delete strategy: removes from base storage and
     * from all semantic vector components.
     */
    core::Result<void> delete_series(const core::SeriesID& series_id) override;
    
    /**
     * @brief Query time series data (base storage operation)
     */
    core::Result<std::vector<core::TimeSeries>> query(const core::Query& query) override;
    
    /**
     * @brief Get storage statistics (base storage operation)
     */
    core::Result<core::StorageStats> get_stats() const override;
    
    /**
     * @brief Close storage (base storage operation)
     */
    core::Result<void> close() override;

private:
    // ============================================================================
    // INTERNAL COMPONENT MANAGEMENT
    // ============================================================================
    
    /**
     * @brief Initialize all semantic vector components
     */
    core::Result<void> initialize_components();
    
    /**
     * @brief Initialize vector processing components
     */
    core::Result<void> initialize_vector_components();
    
    /**
     * @brief Initialize semantic processing components
     */
    core::Result<void> initialize_semantic_components();
    
    /**
     * @brief Initialize temporal processing components
     */
    core::Result<void> initialize_temporal_components();
    
    /**
     * @brief Initialize memory management components
     */
    core::Result<void> initialize_memory_components();
    
    /**
     * @brief Initialize query processing components
     */
    core::Result<void> initialize_query_components();
    
    /**
     * @brief Initialize analytics components
     */
    core::Result<void> initialize_analytics_components();
    
    /**
     * @brief Initialize migration components
     */
    core::Result<void> initialize_migration_components();
    
    // ============================================================================
    // DUAL-WRITE STRATEGY IMPLEMENTATION
    // ============================================================================
    
    /**
     * @brief Execute dual-write with proper error handling
     * 
     * Implements the dual-write strategy ensuring data consistency
     * between base storage and semantic vector components.
     */
    core::Result<void> execute_dual_write(
        const core::TimeSeries& series,
        const std::optional<core::Vector>& vector_embedding = std::nullopt,
        const std::optional<core::Vector>& semantic_embedding = std::nullopt);
    
    /**
     * @brief Rollback dual-write on error
     */
    core::Result<void> rollback_dual_write(
        const core::TimeSeries& series,
        const std::vector<std::string>& failed_components);
    
    /**
     * @brief Validate dual-write consistency
     */
    core::Result<bool> validate_dual_write_consistency(
        const core::SeriesID& series_id);
    
    // ============================================================================
    // MEMORY MANAGEMENT INTEGRATION
    // ============================================================================
    
    /**
     * @brief Integrate memory management with all components
     */
    core::Result<void> integrate_memory_management();
    
    /**
     * @brief Monitor memory usage across all components
     */
    core::Result<core::PerformanceMetrics> monitor_memory_usage() const;
    
    /**
     * @brief Optimize memory usage across all components
     */
    core::Result<void> optimize_component_memory_usage();
    
    // ============================================================================
    // PERFORMANCE MONITORING INTEGRATION
    // ============================================================================
    
    /**
     * @brief Collect performance metrics from all components
     */
    core::Result<core::PerformanceMetrics> collect_component_metrics() const;
    
    /**
     * @brief Aggregate performance metrics
     */
    core::Result<core::PerformanceMetrics> aggregate_metrics(
        const std::vector<core::PerformanceMetrics>& component_metrics) const;
    
    /**
     * @brief Monitor component health
     */
    core::Result<std::map<std::string, std::string>> monitor_component_health() const;
    
    // ============================================================================
    // ERROR HANDLING AND RECOVERY
    // ============================================================================
    
    /**
     * @brief Handle component errors with circuit breaker pattern
     */
    core::Result<void> handle_component_error(
        const std::string& component_name,
        const core::Error& error);
    
    /**
     * @brief Recover from component failures
     */
    core::Result<void> recover_component(
        const std::string& component_name);
    
    /**
     * @brief Validate component consistency
     */
    core::Result<bool> validate_component_consistency() const;
    
    // ============================================================================
    // CONFIGURATION MANAGEMENT
    // ============================================================================
    
    /**
     * @brief Apply configuration to all components
     */
    core::Result<void> apply_configuration_to_components();
    
    /**
     * @brief Validate configuration consistency
     */
    core::Result<core::semantic_vector::ConfigValidationResult> validate_configuration_consistency() const;
    
    /**
     * @brief Update component configurations
     */
    core::Result<void> update_component_configurations();
    
    // ============================================================================
    // INTERNAL DATA STRUCTURES
    // ============================================================================
    
    // Configuration
    core::StorageConfig storage_config_;
    core::semantic_vector::SemanticVectorConfig semantic_vector_config_;
    
    // Component instances (using unified interfaces from TASK-4)
    std::shared_ptr<semantic_vector::IVectorIndex> vector_index_;
    std::shared_ptr<semantic_vector::ISemanticIndex> semantic_index_;
    std::shared_ptr<semantic_vector::ITemporalGraph> temporal_graph_;
    std::shared_ptr<semantic_vector::ITieredMemoryManager> tiered_memory_manager_;
    std::shared_ptr<semantic_vector::IAdaptiveMemoryPool> adaptive_memory_pool_;
    std::shared_ptr<semantic_vector::IAdvancedQueryProcessor> query_processor_;
    std::shared_ptr<semantic_vector::ICausalInference> causal_inference_;
    std::shared_ptr<semantic_vector::ITemporalReasoning> temporal_reasoning_;
    
    // Compression components
    std::unique_ptr<semantic_vector::DeltaCompressedVectorsImpl> delta_compressed_vectors_;
    std::unique_ptr<semantic_vector::DictionaryCompressedMetadataImpl> dictionary_compressed_metadata_;
    
    // Migration component
    std::unique_ptr<semantic_vector::MigrationManagerImpl> migration_manager_;
    
    // Integration contracts (from TASK-4)
    semantic_vector::IntegrationContracts integration_contracts_;
    
    // State management
    std::atomic<bool> semantic_vector_enabled_;
    std::atomic<bool> initialized_;
    std::atomic<bool> shutting_down_;
    
    // Thread safety
    mutable std::mutex config_mutex_;
    mutable std::mutex component_mutex_;
    mutable std::mutex metrics_mutex_;
    
    // Performance monitoring
    mutable std::mutex performance_mutex_;
    core::PerformanceMetrics aggregated_metrics_;
    std::chrono::system_clock::time_point last_metrics_update_;
    
    // Error handling
    std::map<std::string, std::atomic<uint32_t>> component_error_counts_;
    std::map<std::string, std::chrono::system_clock::time_point> component_last_error_;
    std::map<std::string, bool> component_circuit_breaker_state_;
    
    // Background processing
    std::thread background_thread_;
    std::condition_variable background_cv_;
    std::queue<std::function<void()>> background_tasks_;
    mutable std::mutex background_mutex_;
    
    // Memory management
    std::atomic<size_t> total_memory_usage_;
    std::atomic<size_t> optimized_memory_usage_;
    std::chrono::system_clock::time_point last_memory_optimization_;
    
    // ============================================================================
    // INTERNAL HELPER METHODS
    // ============================================================================
    
    /**
     * @brief Start background processing thread
     */
    void start_background_thread();
    
    /**
     * @brief Stop background processing thread
     */
    void stop_background_thread();
    
    /**
     * @brief Background processing loop
     */
    void background_processing_loop();
    
    /**
     * @brief Schedule background task
     */
    void schedule_background_task(std::function<void()> task);
    
    /**
     * @brief Update performance metrics
     */
    void update_performance_metrics();
    
    /**
     * @brief Check component health
     */
    bool is_component_healthy(const std::string& component_name) const;
    
    /**
     * @brief Reset component error count
     */
    void reset_component_error_count(const std::string& component_name);
    
    /**
     * @brief Check circuit breaker state
     */
    bool is_circuit_breaker_open(const std::string& component_name) const;
    
    /**
     * @brief Open circuit breaker for component
     */
    void open_circuit_breaker(const std::string& component_name);
    
    /**
     * @brief Close circuit breaker for component
     */
    void close_circuit_breaker(const std::string& component_name);
    
    /**
     * @brief Generate vector embedding from time series
     */
    core::Result<core::Vector> generate_vector_embedding(const core::TimeSeries& series);
    
    /**
     * @brief Generate semantic embedding from time series
     */
    core::Result<core::Vector> generate_semantic_embedding(const core::TimeSeries& series);
    
    /**
     * @brief Validate time series data
     */
    core::Result<bool> validate_time_series_data(const core::TimeSeries& series) const;
    
    /**
     * @brief Log operation for debugging
     */
    void log_operation(const std::string& operation, const std::string& details) const;
    
    /**
     * @brief Log error for debugging
     */
    void log_error(const std::string& operation, const core::Error& error) const;
};

// ============================================================================
// FACTORY FUNCTIONS
// ============================================================================

/**
 * @brief Create semantic vector storage instance
 * 
 * Factory function for creating SemanticVectorStorageImpl instances
 * with proper configuration and initialization.
 */
std::shared_ptr<SemanticVectorStorageImpl> create_semantic_vector_storage(
    const core::StorageConfig& storage_config,
    const core::semantic_vector::SemanticVectorConfig& semantic_vector_config = core::semantic_vector::SemanticVectorConfig::balanced_config());

/**
 * @brief Create semantic vector storage for specific use case
 * 
 * Factory function for creating SemanticVectorStorageImpl instances
 * optimized for specific use cases (high performance, memory efficient, etc.).
 */
std::shared_ptr<SemanticVectorStorageImpl> create_semantic_vector_storage_for_use_case(
    const core::StorageConfig& storage_config,
    const std::string& use_case);  // "high_performance", "memory_efficient", "high_accuracy", etc.

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Validate semantic vector storage configuration
 * 
 * Validates that the storage configuration is compatible with
 * semantic vector features.
 */
core::Result<core::semantic_vector::ConfigValidationResult> validate_semantic_vector_storage_config(
    const core::StorageConfig& storage_config,
    const core::semantic_vector::SemanticVectorConfig& semantic_vector_config);

/**
 * @brief Get semantic vector storage performance guarantees
 * 
 * Returns performance guarantees for the semantic vector storage
 * based on the current configuration.
 */
core::semantic_vector::PerformanceMetrics get_semantic_vector_storage_performance_guarantees(
    const core::semantic_vector::SemanticVectorConfig& config);

} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_SEMANTIC_VECTOR_STORAGE_IMPL_H_ 