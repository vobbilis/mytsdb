#ifndef TSDB_STORAGE_SEMANTIC_VECTOR_TEMPORAL_GRAPH_H_
#define TSDB_STORAGE_SEMANTIC_VECTOR_TEMPORAL_GRAPH_H_

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
// TEMPORAL GRAPH COMPONENT HEADER
// ============================================================================

/**
 * @brief Temporal Graph Component for Correlation Analysis
 * 
 * This component provides temporal correlation analysis, causal inference,
 * and temporal reasoning capabilities for time series data using graph-based
 * representations and advanced analytics algorithms.
 * 
 * DESIGN HINTS:
 * - Use sparse graph representation for memory efficiency
 * - Implement hierarchical graph compression for scalability
 * - Support multiple correlation algorithms (Pearson, Spearman, Granger)
 * - Enable causal inference using Granger causality and PC algorithm
 * - Use temporal pattern recognition for anomaly detection
 * - Implement graph-based community detection for clustering
 * - Support incremental graph updates for real-time analysis
 * 
 * PERFORMANCE EXPECTATIONS:
 * - Correlation computation latency: < 20ms per pair
 * - Causal inference latency: < 50ms per analysis
 * - Memory usage: 70% reduction with sparse representation
 * - Throughput: > 100 correlation computations/second
 * - Graph construction: < 1 second per 1,000 time series
 * - Community detection: < 5 seconds for 10,000 nodes
 * 
 * IMPLEMENTATION GUIDANCE:
 * - Use unified TemporalGraph types from TASK-1 for consistency
 * - Implement memory optimization using unified memory types
 * - Follow performance contracts from TASK-4 interfaces
 * - Use configuration from TASK-2 for flexible deployment
 * - Integrate with correlation algorithms for accurate analysis
 * - Implement circuit breaker patterns for error handling
 * - Add comprehensive performance monitoring and metrics
 * 
 * INTEGRATION NOTES:
 * - Integrates with ITemporalGraph interface from TASK-4
 * - Uses unified TemporalGraph, TemporalNode, CorrelationMatrix types from TASK-1
 * - Follows memory optimization strategies from TASK-2
 * - Supports AdvancedStorage operations from TASK-5
 * - Enables seamless integration with existing storage operations
 */

/**
 * @brief Temporal Graph Implementation
 * 
 * Implements the ITemporalGraph interface with graph-based correlation analysis,
 * causal inference, and temporal reasoning capabilities.
 */
class TemporalGraphImpl : public ITemporalGraph {
public:
    /**
     * @brief Constructor with configuration
     * 
     * DESIGN HINT: Initialize graph structures and correlation algorithms
     * to support real-time temporal analysis and causal inference.
     * 
     * PERFORMANCE HINT: Pre-allocate memory pools for graph operations
     * to avoid frequent memory allocations during correlation computations.
     */
    explicit TemporalGraphImpl(const core::semantic_vector::SemanticVectorConfig::TemporalConfig& config);
    
    virtual ~TemporalGraphImpl() = default;
    
    // ============================================================================
    // GRAPH CONSTRUCTION OPERATIONS
    // ============================================================================
    
    /**
     * @brief Add time series to temporal graph
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Create temporal node with series data and features
     * - Extract temporal features for correlation analysis
     * - Initialize node with empty neighbor list
     * - Update graph statistics and metrics
     * - Handle concurrent access safely
     * 
     * PERFORMANCE TARGETS:
     * - Add latency: < 1ms per time series
     * - Memory overhead: < 5KB per temporal node
     * - Throughput: > 10,000 time series/second for batch operations
     * - Feature extraction: < 0.5ms per time series
     */
    core::Result<void> add_series(const core::SeriesID& series_id) override;
    
    /**
     * @brief Remove time series from temporal graph
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Remove node and all associated edges
     * - Update neighbor lists for affected nodes
     * - Reclaim memory efficiently
     * - Update graph statistics
     * - Handle cascading updates
     */
    core::Result<void> remove_series(const core::SeriesID& series_id) override;
    
    /**
     * @brief Add correlation between time series
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Compute correlation using multiple algorithms
     * - Validate correlation strength and significance
     * - Add edge to graph if correlation exceeds threshold
     * - Update node neighbor lists and weights
     * - Maintain graph consistency
     * 
     * PERFORMANCE TARGETS:
     * - Correlation computation: < 20ms per pair
     * - Edge addition: < 0.1ms per correlation
     * - Memory overhead: < 1KB per edge
     * - Correlation accuracy: > 95% for significant correlations
     */
    core::Result<void> add_correlation(const core::SeriesID& source, const core::SeriesID& target, double correlation) override;
    
    /**
     * @brief Remove correlation between time series
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Remove edge from graph efficiently
     * - Update node neighbor lists
     * - Maintain graph consistency
     * - Update graph statistics
     */
    core::Result<void> remove_correlation(const core::SeriesID& source, const core::SeriesID& target) override;
    
    // ============================================================================
    // GRAPH QUERY OPERATIONS
    // ============================================================================
    
    /**
     * @brief Get neighbors of time series
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Retrieve neighbor list efficiently
     * - Support neighbor ranking by correlation strength
     * - Handle sparse graph representation
     * - Cache frequently accessed neighbor lists
     * 
     * PERFORMANCE TARGETS:
     * - Neighbor lookup: < 0.1ms per query
     * - Neighbor ranking: < 0.5ms per query
     * - Memory usage: < 1KB per neighbor list
     */
    core::Result<std::vector<core::SeriesID>> get_neighbors(const core::SeriesID& series_id) const override;
    
    /**
     * @brief Get correlation between time series
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Efficient correlation lookup in graph
     * - Support both directed and undirected correlations
     * - Handle missing correlations gracefully
     * - Cache frequently accessed correlations
     * 
     * PERFORMANCE TARGETS:
     * - Correlation lookup: < 0.1ms per query
     * - Correlation accuracy: > 99% for stored correlations
     */
    core::Result<double> get_correlation(const core::SeriesID& source, const core::SeriesID& target) const override;
    
    /**
     * @brief Get top correlations for time series
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Efficiently retrieve top-k correlations
     * - Support correlation ranking and filtering
     * - Handle large neighbor lists efficiently
     * - Use priority queues for top-k selection
     * 
     * PERFORMANCE TARGETS:
     * - Top-k retrieval: < 1ms for k=10
     * - Ranking accuracy: > 99% for top correlations
     */
    core::Result<std::vector<std::pair<core::SeriesID, double>>> get_top_correlations(
        const core::SeriesID& series_id, size_t k) const override;
    
    // ============================================================================
    // GRAPH ANALYSIS OPERATIONS
    // ============================================================================
    
    /**
     * @brief Get graph statistics
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Collect comprehensive graph statistics
     * - Include node and edge counts, average degree
     * - Provide correlation distribution statistics
     * - Support real-time statistics updates
     * 
     * PERFORMANCE TARGETS:
     * - Statistics computation: < 1ms for small graphs
     * - Statistics accuracy: > 99% for all metrics
     */
    core::Result<core::TemporalGraph> get_graph_stats() const override;
    
    /**
     * @brief Find communities in temporal graph
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Implement Louvain algorithm for community detection
     * - Use modularity optimization for community quality
     * - Support hierarchical community structure
     * - Handle large graphs efficiently
     * 
     * PERFORMANCE TARGETS:
     * - Community detection: < 5 seconds for 10,000 nodes
     * - Community quality: > 0.7 modularity score
     * - Memory usage: < 100MB for 10,000 nodes
     */
    core::Result<std::vector<core::SeriesID>> find_communities() const override;
    
    /**
     * @brief Find influential nodes in temporal graph
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Implement PageRank algorithm for influence ranking
     * - Use betweenness centrality for bridge detection
     * - Support multiple influence metrics
     * - Handle large graphs efficiently
     * 
     * PERFORMANCE TARGETS:
     * - Influence computation: < 10 seconds for 10,000 nodes
     * - Influence accuracy: > 90% for top influential nodes
     */
    core::Result<std::vector<core::SeriesID>> find_influential_nodes(size_t k) const override;
    
    // ============================================================================
    // SPARSE GRAPH OPERATIONS
    // ============================================================================
    
    /**
     * @brief Enable sparse graph representation
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Convert dense graph to sparse representation
     * - Use CSR (Compressed Sparse Row) format for efficiency
     * - Optimize memory usage for large graphs
     * - Maintain graph functionality in sparse format
     * 
     * PERFORMANCE TARGETS:
     * - Conversion time: < 1 second for 10,000 nodes
     * - Memory reduction: 70% compared to dense representation
     * - Query performance: < 10% degradation
     */
    core::Result<void> enable_sparse_representation() override;
    
    /**
     * @brief Disable sparse graph representation
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Convert sparse graph back to dense representation
     * - Restore full graph functionality
     * - Handle memory allocation efficiently
     * - Maintain data integrity during conversion
     */
    core::Result<void> disable_sparse_representation() override;
    
    /**
     * @brief Check if sparse representation is enabled
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Return current representation state
     * - Support runtime representation switching
     * - Provide representation statistics
     */
    core::Result<bool> is_sparse_enabled() const override;
    
    // ============================================================================
    // GRAPH COMPRESSION OPERATIONS
    // ============================================================================
    
    /**
     * @brief Compress temporal graph
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Implement hierarchical graph compression
     * - Use graph clustering for compression
     * - Maintain correlation accuracy within bounds
     * - Support multiple compression levels
     * 
     * PERFORMANCE TARGETS:
     * - Compression time: < 10 seconds for 10,000 nodes
     * - Compression ratio: 50% reduction in graph size
     * - Accuracy preservation: > 90% of original correlations
     */
    core::Result<void> compress_graph() override;
    
    /**
     * @brief Decompress temporal graph
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Restore full graph from compressed format
     * - Maintain data integrity during decompression
     * - Handle memory allocation efficiently
     * - Support partial decompression
     */
    core::Result<void> decompress_graph() override;
    
    /**
     * @brief Get graph compression ratio
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Calculate compression ratio accurately
     * - Include all graph components in calculation
     * - Support real-time compression statistics
     */
    core::Result<double> get_compression_ratio() const override;
    
    // ============================================================================
    // PERFORMANCE MONITORING
    // ============================================================================
    
    /**
     * @brief Get performance metrics
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Track all performance metrics defined in unified types
     * - Include graph construction, query, and analysis metrics
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
     * - Rebuild affected graph structures if necessary
     * - Maintain backward compatibility
     * - Log configuration changes for audit
     */
    void update_config(const core::semantic_vector::SemanticVectorConfig::TemporalConfig& config) override;
    
    /**
     * @brief Get current configuration
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Return current configuration state
     * - Include runtime modifications
     * - Provide configuration validation status
     * - Support configuration export for backup
     */
    core::semantic_vector::SemanticVectorConfig::TemporalConfig get_config() const override;

private:
    // ============================================================================
    // PRIVATE IMPLEMENTATION DETAILS
    // ============================================================================
    
    /**
     * @brief Internal graph structures
     * 
     * DESIGN HINT: Use efficient data structures for temporal graph operations:
     * - Sparse graph representation for memory efficiency
     * - Hierarchical compression for scalability
     * - Correlation algorithms for accurate analysis
     * - Community detection for clustering
     */
    struct GraphStructures {
        // Sparse graph representation
        std::unique_ptr<class SparseTemporalGraph> sparse_graph;
        
        // Dense graph representation
        std::unique_ptr<class DenseTemporalGraph> dense_graph;
        
        // Correlation computation engine
        std::unique_ptr<class CorrelationEngine> correlation_engine;
        
        // Community detection algorithm
        std::unique_ptr<class CommunityDetector> community_detector;
        
        // Influence computation engine
        std::unique_ptr<class InfluenceEngine> influence_engine;
        
        // Graph compression engine
        std::unique_ptr<class GraphCompressor> graph_compressor;
        
        // Temporal feature extractor
        std::unique_ptr<class TemporalFeatureExtractor> feature_extractor;
    };
    
    /**
     * @brief Performance monitoring structures
     * 
     * DESIGN HINT: Track comprehensive performance metrics for all graph operations
     * to enable optimization and monitoring.
     */
    struct PerformanceMonitoring {
        // Graph construction metrics
        std::atomic<double> average_node_creation_time_ms{0.0};
        std::atomic<double> average_edge_creation_time_ms{0.0};
        std::atomic<size_t> total_nodes_created{0};
        std::atomic<size_t> total_edges_created{0};
        
        // Graph query metrics
        std::atomic<double> average_neighbor_lookup_time_ms{0.0};
        std::atomic<double> average_correlation_lookup_time_ms{0.0};
        std::atomic<size_t> total_neighbor_queries{0};
        std::atomic<size_t> total_correlation_queries{0};
        
        // Graph analysis metrics
        std::atomic<double> average_community_detection_time_ms{0.0};
        std::atomic<double> average_influence_computation_time_ms{0.0};
        std::atomic<size_t> total_community_analyses{0};
        std::atomic<size_t> total_influence_analyses{0};
        
        // Memory usage metrics
        std::atomic<size_t> total_graph_memory_usage_bytes{0};
        std::atomic<double> graph_memory_compression_ratio{1.0};
        std::atomic<size_t> total_nodes_stored{0};
        std::atomic<size_t> total_edges_stored{0};
        
        // Error tracking
        std::atomic<size_t> graph_construction_errors{0};
        std::atomic<size_t> correlation_computation_errors{0};
        std::atomic<size_t> analysis_errors{0};
    };
    
    // Configuration and state
    core::semantic_vector::SemanticVectorConfig::TemporalConfig config_;
    GraphStructures graph_structures_;
    PerformanceMonitoring performance_monitoring_;
    mutable std::shared_mutex mutex_;
    
    // Internal helper methods
    core::Result<void> initialize_graph_structures();
    core::Result<void> validate_series_id(const core::SeriesID& series_id) const;
    core::Result<core::TemporalNode> create_temporal_node(const core::SeriesID& series_id) const;
    core::Result<double> compute_correlation(const core::SeriesID& source, const core::SeriesID& target) const;
    core::Result<void> update_performance_metrics(const std::string& operation, double latency, bool success);
    core::Result<void> handle_memory_pressure();
    core::Result<void> optimize_graph_structures();
};

// ============================================================================
// FACTORY AND UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Create temporal graph instance
 * 
 * IMPLEMENTATION GUIDANCE:
 * - Use factory pattern for flexible instantiation
 * - Support different graph representations for different use cases
 * - Validate configuration before creation
 * - Provide default configurations for common scenarios
 */
std::unique_ptr<ITemporalGraph> CreateTemporalGraph(
    const core::semantic_vector::SemanticVectorConfig::TemporalConfig& config);

/**
 * @brief Create temporal graph for specific use case
 * 
 * IMPLEMENTATION GUIDANCE:
 * - Provide optimized configurations for common use cases
 * - Support high-performance, memory-efficient, and high-accuracy modes
 * - Validate use case requirements
 * - Provide configuration recommendations
 */
std::unique_ptr<ITemporalGraph> CreateTemporalGraphForUseCase(
    const std::string& use_case,  // "high_performance", "memory_efficient", "high_accuracy"
    const core::semantic_vector::SemanticVectorConfig::TemporalConfig& base_config = core::semantic_vector::SemanticVectorConfig::TemporalConfig());

/**
 * @brief Validate temporal graph configuration
 * 
 * IMPLEMENTATION GUIDANCE:
 * - Validate all configuration parameters
 * - Check for conflicts and inconsistencies
 * - Provide detailed error messages and suggestions
 * - Support configuration optimization recommendations
 */
core::Result<core::semantic_vector::ConfigValidationResult> ValidateTemporalGraphConfig(
    const core::semantic_vector::SemanticVectorConfig::TemporalConfig& config);

} // namespace semantic_vector
} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_SEMANTIC_VECTOR_TEMPORAL_GRAPH_H_ 