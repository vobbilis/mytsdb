#ifndef TSDB_STORAGE_SEMANTIC_VECTOR_VECTOR_INDEX_H_
#define TSDB_STORAGE_SEMANTIC_VECTOR_VECTOR_INDEX_H_

#include <memory>
#include <vector>
#include <string>
#include <map>
#include <optional>
#include <unordered_map>
#include <shared_mutex>
#include <atomic>

#include "tsdb/core/semantic_vector_types.h"
#include "tsdb/core/semantic_vector_config.h"
#include "tsdb/storage/semantic_vector_architecture.h"
#include "tsdb/core/result.h"
#include "tsdb/core/error.h"

// Local aliases to ensure core::Vector and related names are visible to this header
namespace tsdb { namespace core {
using Vector = semantic_vector::Vector;
using QuantizedVector = semantic_vector::QuantizedVector;
using BinaryVector = semantic_vector::BinaryVector;
using VectorIndex = semantic_vector::VectorIndex;
using PerformanceMetrics = semantic_vector::PerformanceMetrics;
} }

namespace tsdb {
namespace storage {
namespace semantic_vector {

// Bring tsdb::core into this namespace for unqualified 'core::' usages
namespace core = ::tsdb::core;

// ============================================================================
// VECTOR INDEX COMPONENT HEADER
// ============================================================================

/**
 * @brief Vector Index Component for Similarity Search
 * 
 * This component provides high-performance vector similarity search capabilities
 * using multiple indexing strategies optimized for different use cases and
 * performance requirements.
 * 
 * DESIGN HINTS:
 * - Use HNSW (Hierarchical Navigable Small World) for fast approximate search
 * - Use IVF (Inverted File Index) for large-scale datasets
 * - Use Binary codes for ultra-fast search with memory efficiency
 * - Implement Product Quantization (PQ) for memory optimization
 * - Support multiple similarity metrics (cosine, euclidean, dot product)
 * - Enable parallel search for high throughput
 * 
 * PERFORMANCE EXPECTATIONS:
 * - Vector search latency: < 1ms for HNSW, < 5ms for IVF
 * - Search accuracy: > 95% for HNSW, > 90% for IVF
 * - Memory usage: 80% reduction with PQ, 96% reduction with binary codes
 * - Throughput: > 10,000 vectors/second for similarity search
 * - Index construction: < 1 second per 10,000 vectors
 * 
 * IMPLEMENTATION GUIDANCE:
 * - Use unified Vector types from TASK-1 for consistency
 * - Implement memory optimization using unified memory types
 * - Follow performance contracts from TASK-4 interfaces
 * - Use configuration from TASK-2 for flexible deployment
 * - Integrate with tiered memory management for optimal performance
 * - Implement circuit breaker patterns for error handling
 * - Add comprehensive performance monitoring and metrics
 * 
 * INTEGRATION NOTES:
 * - Integrates with IVectorIndex interface from TASK-4
 * - Uses unified Vector, QuantizedVector, BinaryVector types from TASK-1
 * - Follows memory optimization strategies from TASK-2
 * - Supports AdvancedStorage operations from TASK-5
 * - Enables seamless integration with existing storage operations
 */

/**
 * @brief Vector Index Implementation
 * 
 * Implements the IVectorIndex interface with multiple indexing strategies
 * for optimal performance across different use cases and dataset sizes.
 */
class VectorIndexImpl : public IVectorIndex {
public:
    /**
     * @brief Constructor with configuration
     * 
     * DESIGN HINT: Initialize all indexing strategies based on configuration
     * to support runtime switching between different search algorithms.
     * 
     * PERFORMANCE HINT: Pre-allocate memory pools for vector storage
     * to avoid frequent memory allocations during search operations.
     */
    explicit VectorIndexImpl(const core::semantic_vector::SemanticVectorConfig::VectorConfig& config);
    
    virtual ~VectorIndexImpl();
    
    // ============================================================================
    // VECTOR MANAGEMENT OPERATIONS
    // ============================================================================
    
    /**
     * @brief Add vector to index
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Validate vector using unified type validation from TASK-1
     * - Apply memory optimization (quantization/pruning) based on config
     * - Add to all relevant indexing strategies (HNSW, IVF, Binary)
     * - Update performance metrics for monitoring
     * - Handle memory pressure with tiered memory management
     * 
     * PERFORMANCE TARGETS:
     * - Add latency: < 0.1ms per vector
     * - Memory overhead: < 10% of vector size
     * - Throughput: > 100,000 vectors/second for batch operations
     */
    core::Result<void> add_vector(const core::SeriesID& series_id, const core::Vector& vector) override;
    
    /**
     * @brief Update existing vector
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Efficiently update all indexing structures
     * - Maintain consistency across different index types
     * - Optimize for incremental updates
     * - Handle concurrent access safely
     */
    core::Result<void> update_vector(const core::SeriesID& series_id, const core::Vector& vector) override;
    
    /**
     * @brief Remove vector from index
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Remove from all indexing structures
     * - Reclaim memory efficiently
     * - Update performance metrics
     * - Handle cascading updates in graph-based indices
     */
    core::Result<void> remove_vector(const core::SeriesID& series_id) override;
    
    /**
     * @brief Get vector by series ID
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Support both original and quantized vector retrieval
     * - Use tiered memory for optimal access patterns
     * - Implement caching for frequently accessed vectors
     * - Handle vector reconstruction from quantized format
     */
    core::Result<core::Vector> get_vector(const core::SeriesID& series_id) const override;
    
    // ============================================================================
    // SIMILARITY SEARCH OPERATIONS
    // ============================================================================
    
    /**
     * @brief Search for similar vectors
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Choose optimal indexing strategy based on query characteristics
     * - Use HNSW for fast approximate search with high accuracy
     * - Use IVF for large-scale datasets with moderate accuracy
     * - Implement early termination for performance optimization
     * - Support parallel search for high throughput
     * - Use unified similarity metrics from TASK-1
     * 
     * PERFORMANCE TARGETS:
     * - Search latency: < 1ms for HNSW, < 5ms for IVF
     * - Search accuracy: > 95% for HNSW, > 90% for IVF
     * - Throughput: > 10,000 queries/second
     * - Memory usage: < 1GB for 1M vectors with PQ
     */
    core::Result<std::vector<std::pair<core::SeriesID, double>>> search_similar(
        const core::Vector& query_vector,
        size_t k_nearest,
        double similarity_threshold = 0.7) const override;
    
    // ============================================================================
    // QUANTIZED VECTOR OPERATIONS
    // ============================================================================
    
    /**
     * @brief Quantize vector using Product Quantization
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Implement PQ algorithm with configurable subvectors and bits
     * - Use k-means clustering for codebook generation
     * - Optimize for memory efficiency (80% reduction target)
     * - Maintain search accuracy within acceptable bounds
     * - Support incremental codebook updates
     * 
     * PERFORMANCE TARGETS:
     * - Quantization time: < 0.1ms per vector
     * - Memory reduction: 80-90% compared to original vectors
     * - Accuracy preservation: > 95% of original search accuracy
     */
    core::Result<core::QuantizedVector> quantize_vector(const core::Vector& vector) const override;
    
    /**
     * @brief Dequantize vector from PQ format
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Efficient reconstruction using pre-computed codebooks
     * - Support partial reconstruction for performance optimization
     * - Handle quantization errors gracefully
     * - Cache frequently used reconstruction patterns
     */
    core::Result<core::Vector> dequantize_vector(const core::QuantizedVector& qvector) const override;
    
    /**
     * @brief Search using quantized vectors
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Implement asymmetric distance computation for efficiency
     * - Use pre-computed lookup tables for fast distance calculation
     * - Support approximate search with configurable accuracy trade-offs
     * - Optimize for batch processing of multiple queries
     */
    core::Result<std::vector<std::pair<core::SeriesID, double>>> search_quantized(
        const core::QuantizedVector& query_vector,
        size_t k_nearest) const override;
    
    // ============================================================================
    // BINARY VECTOR OPERATIONS
    // ============================================================================
    
    /**
     * @brief Convert vector to binary code
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Implement ITQ (Iterative Quantization) for binary encoding
     * - Use LSH (Locality Sensitive Hashing) as alternative
     * - Optimize for ultra-fast Hamming distance computation
     * - Support 64-bit binary codes for maximum efficiency
     * 
     * PERFORMANCE TARGETS:
     * - Binarization time: < 0.01ms per vector
     * - Memory reduction: 96% compared to original vectors
     * - Hamming distance computation: < 0.001ms per comparison
     */
    core::Result<core::BinaryVector> binarize_vector(const core::Vector& vector) const override;
    
    /**
     * @brief Search using binary vectors
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Use bit-level operations for ultra-fast Hamming distance
     * - Implement early termination based on distance bounds
     * - Support SIMD instructions for parallel distance computation
     * - Use lookup tables for fast population count
     * 
     * PERFORMANCE TARGETS:
     * - Search latency: < 0.1ms for binary search
     * - Throughput: > 100,000 binary queries/second
     * - Memory usage: < 100MB for 1M binary vectors
     */
    core::Result<std::vector<std::pair<core::SeriesID, uint32_t>>> search_binary(
        const core::BinaryVector& query_vector,
        size_t k_nearest,
        uint32_t max_hamming_distance = 10) const override;
    
    // ============================================================================
    // INDEX MANAGEMENT OPERATIONS
    // ============================================================================
    
    /**
     * @brief Build index from existing vectors
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Build all indexing structures efficiently
     * - Use parallel construction for large datasets
     * - Optimize memory usage during construction
     * - Support incremental index building
     * - Handle construction failures gracefully
     * 
     * PERFORMANCE TARGETS:
     * - Construction time: < 1 second per 10,000 vectors
     * - Memory usage: < 2x original vector memory during construction
     * - Support for datasets up to 100M vectors
     */
    core::Result<void> build_index() override;
    
    /**
     * @brief Optimize index for better performance
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Rebalance indexing structures for optimal performance
     * - Update codebooks for quantized vectors
     * - Optimize graph connections for HNSW
     * - Defragment memory for better cache locality
     * - Update performance statistics
     */
    core::Result<void> optimize_index() override;
    
    /**
     * @brief Get index statistics
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Collect comprehensive statistics from all index types
     * - Include performance metrics and memory usage
     * - Provide accuracy measurements for different search strategies
     * - Support real-time statistics updates
     */
    core::Result<core::VectorIndex> get_index_stats() const override;
    
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
     * - Rebuild affected indexing structures if necessary
     * - Maintain backward compatibility
     * - Log configuration changes for audit
     */
    void update_config(const core::semantic_vector::SemanticVectorConfig::VectorConfig& config) override;
    
    /**
     * @brief Get current configuration
     * 
     * IMPLEMENTATION GUIDANCE:
     * - Return current configuration state
     * - Include runtime modifications
     * - Provide configuration validation status
     * - Support configuration export for backup
     */
    core::semantic_vector::SemanticVectorConfig::VectorConfig get_config() const override;

private:
    // ============================================================================
    // PRIVATE IMPLEMENTATION DETAILS
    // ============================================================================
    
    /**
     * @brief Internal indexing structures
     * 
     * DESIGN HINT: Use multiple indexing strategies for different use cases:
     * - HNSW for fast approximate search
     * - IVF for large-scale datasets
     * - Binary codes for ultra-fast search
     * - PQ for memory optimization
     */
    struct IndexStructures {
        // HNSW-like index for fast approximate search
        std::unique_ptr<class SimpleHNSWIndex> hnsw_index;
        
        // IVF-like index for large-scale datasets
        std::unique_ptr<class SimpleIVFIndex> ivf_index;
        
        // Binary index for ultra-fast search
        std::unique_ptr<class BinaryIndex> binary_index;
        
        // PQ codebooks for memory optimization
        std::vector<std::vector<std::vector<float>>> pq_codebooks;
        
        // Vector storage with tiered memory management
        std::unique_ptr<class TieredVectorStorage> vector_storage;
    };
    
    /**
     * @brief Performance monitoring structures
     * 
     * DESIGN HINT: Track comprehensive performance metrics for all operations
     * to enable optimization and monitoring.
     */
    struct PerformanceMonitoring {
        // Search performance metrics
        std::atomic<double> average_search_latency_ms{0.0};
        std::atomic<double> average_search_accuracy{0.0};
        std::atomic<size_t> total_searches{0};
        
        // Memory usage metrics
        std::atomic<size_t> total_memory_usage_bytes{0};
        std::atomic<double> memory_compression_ratio{1.0};
        std::atomic<size_t> vectors_stored{0};
        
        // Index construction metrics
        std::atomic<double> index_construction_time_ms{0.0};
        std::atomic<size_t> index_optimization_count{0};
        
        // Error tracking
        std::atomic<size_t> search_errors{0};
        std::atomic<size_t> construction_errors{0};
    };
    
    // Configuration and state
    core::semantic_vector::SemanticVectorConfig::VectorConfig config_;
    IndexStructures index_structures_;
    PerformanceMonitoring performance_monitoring_;

    // Fallback exact-search storage and caches
    mutable std::shared_mutex mutex_;
    std::unordered_map<core::SeriesID, core::Vector> raw_vectors_;
    std::unordered_map<core::SeriesID, core::QuantizedVector> quantized_cache_;
    std::unordered_map<core::SeriesID, core::BinaryVector> binary_cache_;
    
    // Internal helper methods
    core::Result<void> initialize_indexing_structures();
    core::Result<void> validate_vector(const core::Vector& vector) const;
    core::Result<double> compute_similarity(const core::Vector& v1, const core::Vector& v2) const;
    core::Result<void> update_performance_metrics(const std::string& operation, double latency, bool success) const;
    core::Result<void> handle_memory_pressure();
    core::Result<void> optimize_indexing_strategy();
};

// ============================================================================
// FACTORY AND UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Create vector index instance
 * 
 * IMPLEMENTATION GUIDANCE:
 * - Use factory pattern for flexible instantiation
 * - Support different configurations for different use cases
 * - Validate configuration before creation
 * - Provide default configurations for common scenarios
 */
std::unique_ptr<IVectorIndex> CreateVectorIndex(
    const core::semantic_vector::SemanticVectorConfig::VectorConfig& config);

/**
 * @brief Create vector index for specific use case
 * 
 * IMPLEMENTATION GUIDANCE:
 * - Provide optimized configurations for common use cases
 * - Support high-performance, memory-efficient, and high-accuracy modes
 * - Validate use case requirements
 * - Provide configuration recommendations
 */
std::unique_ptr<IVectorIndex> CreateVectorIndexForUseCase(
    const std::string& use_case,  // "high_performance", "memory_efficient", "high_accuracy"
    const core::semantic_vector::SemanticVectorConfig::VectorConfig& base_config = core::semantic_vector::SemanticVectorConfig::VectorConfig());

/**
 * @brief Validate vector index configuration
 * 
 * IMPLEMENTATION GUIDANCE:
 * - Validate all configuration parameters
 * - Check for conflicts and inconsistencies
 * - Provide detailed error messages and suggestions
 * - Support configuration optimization recommendations
 */
core::Result<core::semantic_vector::ConfigValidationResult> ValidateVectorIndexConfig(
    const core::semantic_vector::SemanticVectorConfig::VectorConfig& config);

} // namespace semantic_vector
} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_SEMANTIC_VECTOR_VECTOR_INDEX_H_ 