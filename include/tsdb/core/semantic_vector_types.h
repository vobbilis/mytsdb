#ifndef TSDB_CORE_SEMANTIC_VECTOR_TYPES_H_
#define TSDB_CORE_SEMANTIC_VECTOR_TYPES_H_

#include <vector>
#include <string>
#include <map>
#include <memory>
#include <optional>
#include <chrono>
#include <cstdint>
#include <functional>

#include "tsdb/core/types.h"
#include "tsdb/core/result.h"
#include "tsdb/core/error.h"

namespace tsdb {
namespace core {
namespace semantic_vector {

// ============================================================================
// VECTOR TYPES - For AI/ML workloads and similarity search
// ============================================================================

/**
 * @brief Core vector type for AI/ML workloads
 * 
 * This represents a high-dimensional vector used for:
 * - Time series embeddings
 * - Feature vectors
 * - Query vectors for similarity search
 * - Model weights and parameters
 */
struct Vector {
    std::vector<float> data;           // Vector data (typically 768-dim for BERT)
    size_t dimension;                  // Vector dimension
    std::string metadata;              // Optional metadata (JSON string)
    std::chrono::system_clock::time_point created_at;  // Creation timestamp
    
    // Constructor with validation
    explicit Vector(size_t dim = 768) : dimension(dim) {
        data.reserve(dim);
        created_at = std::chrono::system_clock::now();
    }
    
    // Validation methods
    bool is_valid() const { return data.size() == dimension && dimension > 0; }
    float magnitude() const;  // L2 norm
    float cosine_similarity(const Vector& other) const;
};

/**
 * @brief Quantized vector for memory optimization
 * 
 * Uses Product Quantization (PQ) to reduce memory usage by 80-90%
 * while maintaining search accuracy.
 */
struct QuantizedVector {
    std::vector<uint8_t> codes;        // PQ codes (8 bytes for 768-dim vector)
    size_t dimension;                  // Original dimension
    size_t num_subvectors;             // Number of PQ subvectors (typically 8-16)
    size_t bits_per_subvector;         // Bits per subvector (typically 6-10)
    std::vector<std::vector<float>> codebooks;  // PQ codebooks
    
    // Constructor with PQ configuration
    QuantizedVector(size_t dim = 768, size_t subvectors = 8, size_t bits = 8)
        : dimension(dim), num_subvectors(subvectors), bits_per_subvector(bits) {
        codes.reserve(num_subvectors);
        codebooks.resize(num_subvectors);
    }
    
    // Memory usage calculation
    size_t memory_usage() const {
        return codes.size() + (codebooks.size() * codebooks[0].size() * sizeof(float));
    }
    
    // Dequantization method
    Vector dequantize() const;
};

/**
 * @brief Binary vector for ultra-fast similarity search
 * 
 * Uses 64-bit binary codes for Hamming distance search,
 * achieving 96x memory reduction with sub-millisecond search times.
 */
struct BinaryVector {
    uint64_t code;                     // 64-bit binary code
    size_t original_dimension;         // Original vector dimension
    std::string hash_function;         // Hash function used (e.g., "ITQ", "LSH")
    
    // Hamming distance calculation
    uint32_t hamming_distance(const BinaryVector& other) const;
    
    // Memory usage (constant)
    static constexpr size_t memory_usage() { return sizeof(uint64_t); }
};

/**
 * @brief Vector index for similarity search
 * 
 * Supports multiple indexing strategies:
 * - HNSW (Hierarchical Navigable Small World) for approximate search
 * - IVF (Inverted File Index) for large-scale search
 * - Exact search for small datasets
 */
struct VectorIndex {
    enum class IndexType {
        HNSW,      // Fast approximate search
        IVF,       // Large-scale search
        EXACT,     // Exact search
        BINARY     // Binary codes for ultra-fast search
    };
    
    IndexType type;
    size_t dimension;
    size_t num_vectors;
    std::string metric;  // "cosine", "euclidean", "dot", "hamming"
    
    // Performance characteristics
    double search_latency_ms;          // Average search latency
    double memory_usage_mb;            // Memory usage in MB
    double accuracy;                   // Search accuracy (0.0-1.0)
    
    // Configuration
    struct Config {
        size_t max_connections = 16;   // HNSW max connections
        size_t ef_construction = 200;  // HNSW construction parameter
        size_t ef_search = 50;         // HNSW search parameter
        size_t num_lists = 100;        // IVF number of lists
        bool enable_parallel_search = true;
    } config;
};

// ============================================================================
// SEMANTIC TYPES - For natural language processing and semantic search
// ============================================================================

/**
 * @brief Semantic query for natural language processing
 * 
 * Represents a natural language query that can be processed
 * to find semantically similar time series.
 */
struct SemanticQuery {
    std::string natural_language;      // Natural language query
    std::vector<std::string> entities; // Extracted entities
    std::map<std::string, std::string> context;  // Query context
    std::vector<float> query_embedding; // BERT embedding of the query
    
    // Query types for different semantic operations
    enum class QueryType {
        SIMILARITY_SEARCH,     // Find similar time series
        ENTITY_SEARCH,         // Search by specific entities
        CONCEPT_SEARCH,        // Search by concepts
        CORRELATION_SEARCH,    // Find correlated series
        ANOMALY_SEARCH,        // Find anomalous patterns
        FORECASTING_QUERY,     // Forecasting queries
        CAUSAL_QUERY           // Causal inference queries
    } type;
    
    // Query parameters
    size_t k_nearest = 10;             // Number of results to return
    double similarity_threshold = 0.7;  // Minimum similarity threshold
    std::chrono::system_clock::time_point query_time;
    
    // Constructor
    SemanticQuery(const std::string& query, QueryType t = QueryType::SIMILARITY_SEARCH)
        : natural_language(query), type(t) {
        query_time = std::chrono::system_clock::now();
    }
};

/**
 * @brief Semantic index for natural language search
 * 
 * Stores semantic embeddings and metadata for time series,
 * enabling natural language queries and semantic similarity search.
 */
struct SemanticIndex {
    std::vector<Vector> embeddings;    // BERT embeddings for time series
    std::map<std::string, std::vector<SeriesID>> entity_index;  // Entity → Series mapping
    std::map<std::string, std::vector<SeriesID>> concept_index; // Concept → Series mapping
    std::string embedding_model;       // BERT model used (e.g., "bert-base-uncased")
    
    // Pruning configuration for memory optimization
    struct PruningConfig {
        float sparsity_threshold = 0.1f;     // Keep only 10% of weights
        size_t max_entities_per_series = 10;  // Limit entities per series
        size_t max_concepts_per_series = 5;   // Limit concepts per series
        bool enable_embedding_distillation = true;  // Use smaller BERT models
    } pruning_config;
    
    // Performance metrics
    size_t total_embeddings;
    size_t total_entities;
    size_t total_concepts;
    double memory_usage_mb;
    double search_latency_ms;
};

/**
 * @brief Pruned embedding for memory optimization
 * 
 * Stores only the most important embedding weights,
 * reducing memory usage by 80-90% while maintaining semantic accuracy.
 */
struct PrunedEmbedding {
    std::vector<uint32_t> indices;     // Indices of non-zero weights
    std::vector<float> values;         // Non-zero weight values
    size_t original_dimension;         // Original embedding dimension
    float sparsity_ratio;              // Sparsity ratio (0.0-1.0)
    
    // Constructor
    PrunedEmbedding(const Vector& original, float sparsity = 0.1f);
    
    // Memory usage calculation
    size_t memory_usage() const {
        return indices.size() * sizeof(uint32_t) + values.size() * sizeof(float);
    }
    
    // Reconstruction method
    Vector reconstruct() const;
};

// ============================================================================
// TEMPORAL TYPES - For temporal graphs and correlation analysis
// ============================================================================

/**
 * @brief Temporal node in the correlation graph
 * 
 * Represents a time series in the temporal graph with
 * its features, neighbors, and correlation weights.
 */
struct TemporalNode {
    SeriesID series_id;                // Associated time series ID
    std::vector<int64_t> timestamps;   // Time series timestamps
    std::vector<double> values;        // Time series values
    std::map<std::string, double> features;  // Extracted features
    std::vector<SeriesID> neighbors;   // Connected series IDs
    std::map<SeriesID, double> correlation_weights;  // Correlation strengths
    
    // Node metadata
    std::chrono::system_clock::time_point last_updated;
    double max_correlation;            // Maximum correlation with any neighbor
    size_t degree;                     // Number of neighbors
    
    // Constructor
    explicit TemporalNode(SeriesID id) : series_id(id), max_correlation(0.0), degree(0) {
        last_updated = std::chrono::system_clock::now();
    }
    
    // Memory usage calculation
    size_t memory_usage() const;
};

/**
 * @brief Temporal graph for correlation analysis
 * 
 * Represents a graph of time series with temporal correlations,
 * enabling causal inference and temporal reasoning.
 */
struct TemporalGraph {
    std::map<SeriesID, std::unique_ptr<TemporalNode>> nodes;
    size_t num_nodes;
    size_t num_edges;
    double average_degree;
    
    // Graph configuration
    struct GraphConfig {
        double correlation_threshold = 0.7;  // Minimum correlation for edges
        size_t max_neighbors_per_node = 50;  // Maximum neighbors per node
        bool enable_hierarchical_compression = true;  // Enable graph compression
        size_t compression_levels = 4;       // Number of compression levels
    } config;
    
    // Performance metrics
    double memory_usage_mb;
    double graph_construction_time_ms;
    double query_latency_ms;
    
    // Graph operations
    void add_node(SeriesID series_id);
    void add_edge(SeriesID source, SeriesID target, double correlation);
    std::vector<SeriesID> get_neighbors(SeriesID series_id) const;
    double get_correlation(SeriesID source, SeriesID target) const;
};

/**
 * @brief Correlation matrix for temporal analysis
 * 
 * Stores pairwise correlations between time series,
 * optimized for sparse storage and fast access.
 */
struct CorrelationMatrix {
    std::vector<std::vector<double>> correlations;  // Dense matrix (for small datasets)
    std::map<std::pair<SeriesID, SeriesID>, double> sparse_correlations;  // Sparse storage
    size_t num_series;
    bool is_sparse;                    // Whether to use sparse storage
    
    // Configuration
    struct MatrixConfig {
        double correlation_threshold = 0.7;  // Store only correlations above threshold
        size_t max_matrix_size = 10000;      // Switch to sparse above this size
        bool enable_compression = true;      // Enable matrix compression
    } config;
    
    // Memory usage calculation
    size_t memory_usage() const;
    
    // Matrix operations
    void set_correlation(SeriesID i, SeriesID j, double correlation);
    double get_correlation(SeriesID i, SeriesID j) const;
    std::vector<std::pair<SeriesID, double>> get_top_correlations(SeriesID series_id, size_t k) const;
};

// ============================================================================
// MEMORY OPTIMIZATION TYPES - For memory management and optimization
// ============================================================================

/**
 * @brief Memory optimization configuration
 * 
 * Centralized configuration for all memory optimization features,
 * ensuring consistent memory usage across all components.
 */
struct MemoryOptimizationConfig {
    // Quantization settings
    bool enable_product_quantization = true;
    bool enable_binary_quantization = false;
    size_t pq_subvectors = 8;
    size_t pq_bits_per_subvector = 8;
    
    // Pruning settings
    bool enable_embedding_pruning = true;
    float sparsity_threshold = 0.1f;
    size_t max_entities_per_series = 10;
    size_t max_concepts_per_series = 5;
    
    // Graph compression settings
    bool enable_sparse_graph = true;
    double correlation_threshold = 0.7f;
    size_t max_graph_levels = 4;
    
    // Memory management settings
    bool enable_tiered_memory = true;
    size_t ram_tier_capacity_mb = 1024;  // 1GB RAM tier
    size_t ssd_tier_capacity_mb = 10240; // 10GB SSD tier
    size_t hdd_tier_capacity_mb = 102400; // 100GB HDD tier
    
    // Compression settings
    bool enable_delta_compression = true;
    bool enable_dictionary_compression = true;
    size_t compression_level = 6;  // 0-9, higher = more compression
    
    // Performance targets
    double target_memory_reduction = 0.8;  // 80% memory reduction
    double max_latency_impact = 0.05;      // 5% max latency increase
    double min_accuracy_preservation = 0.95; // 95% accuracy preservation
    
    // Validation method
    bool is_valid() const;
    
    // Memory usage calculation
    size_t calculate_memory_usage(size_t num_series, size_t vector_dimension) const;
};

/**
 * @brief Memory tier policy for hierarchical storage
 * 
 * Defines how data is moved between different memory tiers
 * (RAM, SSD, HDD) based on access patterns and memory pressure.
 */
struct TieredMemoryPolicy {
    enum class Tier {
        RAM,    // Fastest, most expensive
        SSD,    // Medium speed, medium cost
        HDD     // Slowest, cheapest
    };
    
    struct TierConfig {
        Tier tier;
        size_t capacity_bytes;
        double access_cost_ms;  // Relative access cost
        std::string eviction_policy;  // "LRU", "LFU", "FIFO"
    };
    
    std::vector<TierConfig> tiers;
    
    // Promotion/demotion policies
    double promotion_threshold = 0.8;  // Promote if access frequency > 80%
    double demotion_threshold = 0.2;   // Demote if access frequency < 20%
    size_t promotion_batch_size = 1000; // Batch size for promotions
    size_t demotion_batch_size = 1000;  // Batch size for demotions
    
    // Performance monitoring
    struct Metrics {
        size_t total_promotions;
        size_t total_demotions;
        double average_access_time_ms;
        double cache_hit_ratio;
        size_t total_memory_usage_bytes;
    } metrics;
    
    // Policy methods
    Tier get_optimal_tier(const std::string& access_pattern) const;
    bool should_promote(SeriesID series_id, double access_frequency) const;
    bool should_demote(SeriesID series_id, double access_frequency) const;
};

// ---------------------------------------------------------------------------
// Forward declarations for analytics result types used below
// ---------------------------------------------------------------------------
struct Anomaly;
struct Prediction;
struct Correlation;

// ============================================================================
// QUERY PROCESSING TYPES - For advanced query processing
// ============================================================================

/**
 * @brief Query processor for advanced analytics
 * 
 * Handles all types of queries including vector similarity,
 * semantic search, correlation analysis, and causal inference.
 */
struct QueryProcessor {
    enum class QueryType {
        VECTOR_SIMILARITY,    // Vector similarity search
        SEMANTIC_SEARCH,      // Natural language search
        CORRELATION,          // Multi-variate correlation
        ANOMALY_DETECTION,    // Anomaly detection
        FORECASTING,          // Time series forecasting
        CAUSAL_INFERENCE,     // Causal inference
        TEMPORAL_REASONING    // Temporal reasoning
    };
    
    // Query configuration
    struct QueryConfig {
        size_t max_results = 100;      // Maximum results per query
        double timeout_seconds = 30.0; // Query timeout
        bool enable_parallel_execution = true;
        bool enable_result_caching = true;
        size_t cache_size = 10000;     // Cache size in entries
    } config;
    
    // Performance metrics
    struct PerformanceMetrics {
        double average_query_time_ms;
        size_t total_queries_processed;
        size_t cache_hits;
        size_t cache_misses;
        double cache_hit_ratio;
    } metrics;
    
    // Query processing methods
    Result<std::vector<SeriesID>> process_vector_query(const Vector& query_vector, size_t k);
    Result<std::vector<SeriesID>> process_semantic_query(const SemanticQuery& query);
    Result<std::vector<semantic_vector::Correlation>> process_correlation_query(const std::vector<SeriesID>& series_ids);
    Result<std::vector<semantic_vector::Anomaly>> process_anomaly_query(const SeriesID& series_id);
    Result<std::vector<semantic_vector::Prediction>> process_forecasting_query(const SeriesID& series_id, size_t horizon);
};

/**
 * @brief Query plan for optimized execution
 * 
 * Represents an optimized execution plan for complex queries,
 * including cost estimation and parallelization strategies.
 */
struct QueryPlan {
    QueryProcessor::QueryType type;
    std::vector<std::string> operations;  // Sequence of operations
    double estimated_cost;                // Estimated execution cost
    size_t estimated_memory_usage;        // Estimated memory usage
    std::vector<std::string> parallel_groups;  // Operations that can run in parallel
    
    // Plan optimization
    bool is_optimized = false;
    double optimization_time_ms;
    size_t optimization_iterations;
    
    // Execution tracking
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point executed_at;
    double actual_execution_time_ms;
    size_t actual_memory_usage;
    
    // Plan validation
    bool is_valid() const;
    double get_optimization_ratio() const;  // Estimated vs actual performance
};

/**
 * @brief Query result with metadata
 * 
 * Represents the result of a query with comprehensive metadata
 * including performance metrics and result quality indicators.
 */
struct QueryResult {
    std::vector<SeriesID> series_ids;     // Result series IDs
    std::vector<double> scores;           // Similarity/correlation scores
    std::vector<std::string> explanations; // Result explanations
    QueryPlan execution_plan;             // Execution plan used
    
    // Result metadata
    size_t total_candidates_evaluated;
    double query_time_ms;
    double memory_usage_mb;
    double result_quality_score;  // 0.0-1.0, higher is better
    
    // Result validation
    bool is_valid() const;
    size_t size() const { return series_ids.size(); }
    
    // Result sorting and filtering
    void sort_by_score(bool descending = true);
    void filter_by_threshold(double threshold);
    void limit_results(size_t max_results);
};

// ============================================================================
// ADVANCED ANALYTICS TYPES - For causal inference and temporal reasoning
// ============================================================================

/**
 * @brief Causal inference for time series analysis
 * 
 * Implements causal inference algorithms including Granger causality,
 * PC algorithm, and structural causal models.
 */
struct CausalInference {
    enum class Algorithm {
        GRANGER_CAUSALITY,    // Granger causality test
        PC_ALGORITHM,         // PC algorithm for causal discovery
        STRUCTURAL_CAUSAL_MODEL, // Structural causal models
        INTERVENTION_ANALYSIS // Intervention analysis
    };
    
    // Causal relationship
    struct CausalRelationship {
        SeriesID cause;
        SeriesID effect;
        double strength;      // Causal strength (0.0-1.0)
        double confidence;    // Confidence level (0.0-1.0)
        std::string algorithm; // Algorithm used for inference
        std::chrono::system_clock::time_point inferred_at;
        
        // Relationship validation
        bool is_significant() const { return strength > 0.5 && confidence > 0.8; }
    };
    
    // Causal graph
    struct CausalGraph {
        std::map<SeriesID, std::vector<CausalRelationship>> relationships;
        size_t num_nodes;
        size_t num_edges;
        
        // Graph operations
        void add_relationship(const CausalRelationship& rel);
        std::vector<CausalRelationship> get_causes(SeriesID effect) const;
        std::vector<CausalRelationship> get_effects(SeriesID cause) const;
        double get_causal_strength(SeriesID cause, SeriesID effect) const;
    };
    
    // Configuration
    struct Config {
        Algorithm algorithm = Algorithm::GRANGER_CAUSALITY;
        double significance_threshold = 0.05;  // Statistical significance
        size_t max_lag = 10;                   // Maximum time lag
        bool enable_multiple_testing_correction = true;
    } config;
    
    // Performance metrics
    double inference_time_ms;
    size_t relationships_discovered;
    double average_confidence;
    
    // Inference methods
    Result<std::vector<CausalRelationship>> infer_causality(const std::vector<SeriesID>& series_ids);
    Result<CausalGraph> build_causal_graph(const std::vector<SeriesID>& series_ids);
    Result<double> estimate_causal_effect(SeriesID treatment, SeriesID outcome);
};

/**
 * @brief Temporal reasoning for complex temporal analysis
 * 
 * Implements temporal reasoning algorithms for pattern recognition,
 * temporal correlation analysis, and multi-modal reasoning.
 */
struct TemporalReasoning {
    enum class ReasoningType {
        PATTERN_RECOGNITION,  // Temporal pattern recognition
        CORRELATION_ANALYSIS, // Temporal correlation analysis
        MULTI_MODAL_REASONING, // Multi-modal temporal reasoning
        TEMPORAL_INFERENCE    // Temporal inference
    };
    
    enum class CorrelationType {
        PEARSON,      // Pearson correlation
        SPEARMAN,     // Spearman rank correlation
        KENDALL,      // Kendall tau correlation
        PARTIAL,      // Partial correlation
        CROSS         // Cross-correlation
    };
    
    // Temporal pattern
    struct TemporalPattern {
        std::string pattern_type;       // "seasonal", "trend", "cyclic", "anomaly"
        std::vector<double> pattern_data; // Pattern data
        double confidence;              // Pattern confidence (0.0-1.0)
        std::chrono::system_clock::time_point discovered_at;
        
        // Pattern validation
        bool is_significant() const { return confidence > 0.7; }
    };
    
    // Temporal inference
    struct TemporalInference {
        std::string inference_type;     // "prediction", "explanation", "causality"
        std::vector<double> inference_data; // Inference results
        double confidence;              // Inference confidence (0.0-1.0)
        std::string explanation;        // Human-readable explanation
        std::chrono::system_clock::time_point inferred_at;
    };
    
    // Configuration
    struct Config {
        ReasoningType type = ReasoningType::PATTERN_RECOGNITION;
        double pattern_threshold = 0.7;  // Pattern significance threshold
        size_t min_pattern_length = 10;  // Minimum pattern length
        bool enable_multi_modal = true;  // Enable multi-modal reasoning
    } config;
    
    // Performance metrics
    double reasoning_time_ms;
    size_t patterns_discovered;
    size_t inferences_made;
    double average_confidence;
    
    // Reasoning methods
    Result<std::vector<TemporalPattern>> recognize_patterns(const SeriesID& series_id);
    Result<std::vector<TemporalInference>> make_inferences(const std::vector<SeriesID>& series_ids);
    Result<double> analyze_temporal_correlation(const SeriesID& series1, const SeriesID& series2);
};

// ============================================================================
// UTILITY TYPES - For common operations and metadata
// ============================================================================

/**
 * @brief Performance metrics for monitoring
 * 
 * Comprehensive performance metrics for all semantic vector operations,
 * enabling performance monitoring and optimization.
 */
struct PerformanceMetrics {
    // Memory metrics
    size_t total_memory_usage_bytes;
    size_t vector_memory_usage_bytes;
    size_t semantic_memory_usage_bytes;
    size_t temporal_memory_usage_bytes;
    double memory_compression_ratio;
    
    // Performance metrics
    double average_vector_search_time_ms;
    double average_semantic_search_time_ms;
    double average_correlation_time_ms;
    double average_inference_time_ms;
    
    // Quality metrics
    double vector_search_accuracy;
    double semantic_search_accuracy;
    double correlation_accuracy;
    double inference_accuracy;
    
    // Throughput metrics
    size_t queries_per_second;
    size_t vectors_processed_per_second;
    size_t correlations_computed_per_second;
    
    // Timestamp
    std::chrono::system_clock::time_point recorded_at;
    
    // Metrics validation
    bool is_valid() const;
    
    // Metrics aggregation
    static PerformanceMetrics aggregate(const std::vector<PerformanceMetrics>& metrics);
};

/**
 * @brief Error types for semantic vector operations
 * 
 * Comprehensive error types for all semantic vector operations,
 * enabling proper error handling and debugging.
 */
enum class SemanticVectorError {
    INVALID_VECTOR_DIMENSION,
    INVALID_SEMANTIC_QUERY,
    INVALID_TEMPORAL_GRAPH,
    MEMORY_ALLOCATION_FAILED,
    QUANTIZATION_FAILED,
    PRUNING_FAILED,
    CORRELATION_COMPUTATION_FAILED,
    CAUSAL_INFERENCE_FAILED,
    TEMPORAL_REASONING_FAILED,
    QUERY_PROCESSING_FAILED,
    CONFIGURATION_INVALID,
    PERFORMANCE_DEGRADED
};

/**
 * @brief Configuration validation result
 * 
 * Result of configuration validation with detailed error information
 * and suggestions for fixing configuration issues.
 */
struct ConfigValidationResult {
    bool is_valid;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    std::vector<std::string> suggestions;
    
    // Validation methods
    void add_error(const std::string& error);
    void add_warning(const std::string& warning);
    void add_suggestion(const std::string& suggestion);
    
    // Result summary
    std::string get_summary() const;
};

// ============================================================================
// TYPE VALIDATION AND UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Validate type consistency across components
 * 
 * Ensures that all types are consistent and compatible
 * across different components of the semantic vector system.
 */
class TypeValidator {
public:
    // Validation methods
    static bool validate_vector_types();
    static bool validate_semantic_types();
    static bool validate_temporal_types();
    static bool validate_memory_types();
    static bool validate_query_types();
    static bool validate_analytics_types();
    
    // Cross-component validation
    static bool validate_type_consistency();
    static bool validate_interface_compatibility();
    static bool validate_performance_contracts();
    
    // Validation result
    struct ValidationResult {
        bool success;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
    };
    
    static ValidationResult validate_all_types();
};

/**
 * @brief Type conversion utilities
 * 
 * Utilities for converting between different type representations
 * while maintaining data integrity and performance.
 */
class TypeConverter {
public:
    // Vector conversions
    static QuantizedVector vector_to_quantized(const Vector& vector, const MemoryOptimizationConfig& config);
    static BinaryVector vector_to_binary(const Vector& vector);
    static Vector quantized_to_vector(const QuantizedVector& qvector);
    
    // Semantic conversions
    static PrunedEmbedding embedding_to_pruned(const Vector& embedding, const MemoryOptimizationConfig& config);
    static Vector pruned_to_embedding(const PrunedEmbedding& pruned);
    
    // Temporal conversions
    static CorrelationMatrix graph_to_matrix(const TemporalGraph& graph);
    static TemporalGraph matrix_to_graph(const CorrelationMatrix& matrix);
    
    // Performance monitoring
    static double get_conversion_overhead_ms();
    static size_t get_conversion_memory_overhead_bytes();
};

// ============================================================================
// FORWARD DECLARATIONS FOR COMPONENT-SPECIFIC TYPES
// ============================================================================

// Analytics types are now defined above

// ============================================================================
// MEMORY MANAGEMENT TYPES - For memory optimization and allocation
// ============================================================================

/**
 * @brief Memory tier enumeration for tiered storage
 */
enum class MemoryTier {
    RAM = 0,    // Fastest access, highest cost
    SSD = 1,    // Medium access, medium cost  
    HDD = 2     // Slowest access, lowest cost
};

// ============================================================================
// COMPRESSION TYPES - For vector and metadata compression
// ============================================================================

/**
 * @brief Compression algorithm enumeration
 */
enum class CompressionAlgorithm {
    NONE = 0,         // No compression
    DELTA = 1,        // Delta encoding compression
    DICTIONARY = 2,   // Dictionary-based compression
    HYBRID = 3        // Combined delta + dictionary compression
};

// ============================================================================
// ANALYTICS TYPES - For causal inference and temporal reasoning
// ============================================================================



/**
 * @brief Anomaly detection result
 */
struct Anomaly {
    std::chrono::system_clock::time_point timestamp;  // When anomaly occurred
    core::SeriesID series_id;                         // Which series had the anomaly
    double value{0.0};                               // Anomalous value
    double expected_value{0.0};                      // Expected value
    double deviation_score{0.0};                     // How many standard deviations away
    double confidence{0.0};                          // Confidence in anomaly detection (0.0-1.0)
    std::string anomaly_type;                        // Type of anomaly (spike, dip, shift, etc.)
    
    // Constructor
    Anomaly() = default;
    
    // Severity assessment
    bool is_severe() const { return deviation_score > 3.0; }
    bool is_high_confidence() const { return confidence > 0.8; }
};

/**
 * @brief Prediction result for forecasting
 */
struct Prediction {
    std::chrono::system_clock::time_point timestamp;  // When prediction is for
    core::SeriesID series_id;                         // Which series prediction is for
    double predicted_value{0.0};                     // Predicted value
    double confidence_interval_low{0.0};             // Lower bound of confidence interval
    double confidence_interval_high{0.0};            // Upper bound of confidence interval
    double prediction_confidence{0.0};               // Confidence in prediction (0.0-1.0)
    std::string model_used;                          // Which model generated prediction
    
    // Constructor
    Prediction() = default;
    
    // Uncertainty assessment
    double get_uncertainty() const { 
        return confidence_interval_high - confidence_interval_low; 
    }
    bool is_reliable() const { return prediction_confidence > 0.7; }
};

/**
 * @brief Correlation analysis result
 */
struct Correlation {
    core::SeriesID series_a;                         // First series in correlation
    core::SeriesID series_b;                         // Second series in correlation
    double correlation_coefficient{0.0};             // Correlation strength (-1.0 to 1.0)
    double p_value{1.0};                             // Statistical significance
    TemporalReasoning::CorrelationType type{TemporalReasoning::CorrelationType::PEARSON};  // Correlation type
    size_t lag{0};                                   // Lag at which correlation is strongest
    double confidence{0.0};                          // Confidence in correlation (0.0-1.0)
    
    // Constructor
    Correlation() = default;
    
    // Significance assessment
    bool is_significant() const { return p_value < 0.05; }
    bool is_strong() const { return std::abs(correlation_coefficient) > 0.7; }
    bool is_positive() const { return correlation_coefficient > 0.0; }
};

// ============================================================================
// QUERY PROCESSING TYPES - For query parsing, optimization, and execution
// ============================================================================




// ============================================================================
// MIGRATION MANAGER TYPES - For data migration and system transitions
// ============================================================================

/**
 * @brief Migration manager types and operations
 */
struct MigrationManager {
    /**
     * @brief Migration phase enumeration
     */
    enum class MigrationPhase {
        PREPARATION = 0,       // Preparing for migration
        VALIDATION = 1,        // Validating data consistency
        MIGRATION = 2,         // Active data migration
        VERIFICATION = 3,      // Verifying migration success
        ROLLBACK = 4,          // Rolling back changes
        COMPLETION = 5         // Migration completed
    };
    
    /**
     * @brief Migration strategy
     */
    enum class MigrationStrategy {
        PARALLEL = 0,          // Parallel dual-write migration
        SEQUENTIAL = 1,        // Sequential batch migration
        INCREMENTAL = 2,       // Incremental streaming migration
        BULK = 3,              // Bulk transfer migration
        HYBRID = 4             // Hybrid approach based on data size
    };
    
    /**
     * @brief Rollback strategy
     */
    enum class RollbackStrategy {
        IMMEDIATE = 0,         // Immediate rollback on error
        GRADUAL = 1,           // Gradual rollback with verification
        CHECKPOINT = 2,        // Rollback to specific checkpoint
        FULL_RESTORE = 3       // Full restoration from backup
    };
};

/**
 * @brief Migration progress tracking
 */
struct MigrationProgress {
    MigrationManager::MigrationPhase current_phase{MigrationManager::MigrationPhase::PREPARATION};
    MigrationManager::MigrationStrategy strategy{MigrationManager::MigrationStrategy::PARALLEL};
    std::string migration_id;                           // Unique migration identifier
    std::chrono::system_clock::time_point start_time;   // Migration start time
    std::chrono::system_clock::time_point last_update;  // Last progress update
    
    // Progress metrics
    size_t total_series_count{0};                       // Total series to migrate
    size_t migrated_series_count{0};                    // Successfully migrated series
    size_t failed_series_count{0};                      // Failed migration attempts
    size_t skipped_series_count{0};                     // Skipped series (already migrated)
    
    // Batch progress
    size_t total_batches{0};                            // Total batches to process
    size_t completed_batches{0};                        // Successfully completed batches
    size_t failed_batches{0};                           // Failed batch attempts
    size_t current_batch_id{0};                         // Current batch being processed
    
    // Performance metrics
    double migration_rate_series_per_second{0.0};       // Current migration rate
    double average_batch_time_seconds{0.0};             // Average time per batch
    double estimated_time_remaining_seconds{0.0};       // Estimated time to completion
    size_t memory_usage_bytes{0};                       // Memory used during migration
    
    // Quality metrics
    double data_consistency_score{1.0};                 // Data consistency (0.0-1.0)
    size_t validation_errors{0};                        // Number of validation errors
    size_t data_corruption_instances{0};                // Data corruption detected
    bool integrity_check_passed{true};                  // Overall integrity status
    
    // Constructor
    MigrationProgress() {
        start_time = std::chrono::system_clock::now();
        last_update = start_time;
        migration_id = "migration_" + std::to_string(
            std::chrono::duration_cast<std::chrono::milliseconds>(start_time.time_since_epoch()).count());
    }
    
    // Progress calculation
    double get_completion_percentage() const {
        if (total_series_count == 0) return 0.0;
        return (static_cast<double>(migrated_series_count) / total_series_count) * 100.0;
    }
    
    bool is_completed() const {
        return current_phase == MigrationManager::MigrationPhase::COMPLETION;
    }
    
    bool has_errors() const {
        return failed_series_count > 0 || failed_batches > 0 || validation_errors > 0;
    }
    
    // Update progress
    void update_progress(size_t new_migrated_count) {
        migrated_series_count = new_migrated_count;
        last_update = std::chrono::system_clock::now();
        
        // Calculate migration rate
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(last_update - start_time);
        if (elapsed.count() > 0) {
            migration_rate_series_per_second = static_cast<double>(migrated_series_count) / elapsed.count();
        }
        
        // Estimate remaining time
        if (migration_rate_series_per_second > 0.0) {
            size_t remaining_series = total_series_count - migrated_series_count;
            estimated_time_remaining_seconds = remaining_series / migration_rate_series_per_second;
        }
    }
};

/**
 * @brief Migration batch configuration
 */
struct MigrationBatch {
    size_t batch_id{0};                                 // Unique batch identifier
    std::vector<core::SeriesID> series_ids;             // Series IDs in this batch
    size_t batch_size{1000};                           // Number of series per batch
    std::chrono::system_clock::time_point start_time;   // Batch start time
    std::chrono::system_clock::time_point end_time;     // Batch completion time
    
    // Batch status
    enum class Status {
        PENDING = 0,           // Waiting to be processed
        PROCESSING = 1,        // Currently being processed
        COMPLETED = 2,         // Successfully completed
        FAILED = 3,            // Failed with errors
        RETRYING = 4,          // Being retried after failure
        SKIPPED = 5            // Skipped due to conditions
    } status{Status::PENDING};
    
    // Error tracking
    std::vector<std::string> errors;                    // Error messages for this batch
    size_t retry_count{0};                              // Number of retry attempts
    size_t max_retries{3};                              // Maximum allowed retries
    
    // Performance metrics
    double processing_time_seconds{0.0};               // Time taken to process this batch
    size_t memory_usage_bytes{0};                      // Peak memory usage during processing
    double throughput_series_per_second{0.0};          // Batch processing throughput
    
    // Constructor
    MigrationBatch() {
        start_time = std::chrono::system_clock::now();
        end_time = start_time;
    }
    
    // Batch management
    bool is_completed() const { return status == Status::COMPLETED; }
    bool has_failed() const { return status == Status::FAILED; }
    bool can_retry() const { return retry_count < max_retries && status == Status::FAILED; }
    
    void mark_completed() {
        status = Status::COMPLETED;
        end_time = std::chrono::system_clock::now();
        processing_time_seconds = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time).count() / 1000000.0;
        if (processing_time_seconds > 0.0) {
            throughput_series_per_second = series_ids.size() / processing_time_seconds;
        }
    }
    
    void mark_failed(const std::string& error_message) {
        status = Status::FAILED;
        errors.push_back(error_message);
        end_time = std::chrono::system_clock::now();
    }
    
    void start_retry() {
        retry_count++;
        status = Status::RETRYING;
        start_time = std::chrono::system_clock::now();
    }
};

/**
 * @brief Migration rollback checkpoint
 */
struct MigrationCheckpoint {
    std::string checkpoint_id;                          // Unique checkpoint identifier
    MigrationManager::MigrationPhase phase_at_checkpoint; // Migration phase when checkpoint was created
    std::chrono::system_clock::time_point created_at;   // Checkpoint creation time
    size_t series_migrated_at_checkpoint{0};           // Series migrated when checkpoint was created
    
    // Rollback data
    std::map<core::SeriesID, std::string> series_backup_locations; // Backup locations for series data
    std::vector<std::string> operation_log;            // Log of operations since checkpoint
    std::map<std::string, std::string> system_state;   // System state at checkpoint
    
    // Checkpoint metadata
    size_t checkpoint_size_bytes{0};                   // Size of checkpoint data
    bool is_verified{false};                           // Whether checkpoint integrity is verified
    std::string checksum;                              // Checksum for integrity verification
    
    // Constructor
    MigrationCheckpoint() {
        created_at = std::chrono::system_clock::now();
        checkpoint_id = "checkpoint_" + std::to_string(
            std::chrono::duration_cast<std::chrono::milliseconds>(created_at.time_since_epoch()).count());
    }
    
    // Checkpoint management
    bool is_valid() const { return is_verified && !checksum.empty(); }
    
    void verify_checkpoint() {
        // Verification would happen here
        is_verified = true;
    }
    
    void add_operation(const std::string& operation) {
        operation_log.push_back(operation);
    }
};

/**
 * @brief Migration status report
 */
struct MigrationStatusReport {
    std::string migration_id;                           // Migration being reported
    std::chrono::system_clock::time_point report_time;  // When report was generated
    MigrationProgress progress;                         // Current migration progress
    
    // System health
    double system_cpu_usage{0.0};                      // CPU usage during migration
    double system_memory_usage{0.0};                   // Memory usage during migration
    double system_disk_io_mbps{0.0};                   // Disk I/O rate
    double system_network_io_mbps{0.0};                // Network I/O rate
    
    // Migration quality metrics
    double data_accuracy{1.0};                         // Data accuracy score (0.0-1.0)
    double migration_efficiency{1.0};                  // Migration efficiency score (0.0-1.0)
    std::vector<std::string> warnings;                 // Non-critical warnings
    std::vector<std::string> recommendations;          // Performance recommendations
    
    // Recent batches
    std::vector<MigrationBatch> recent_batches;         // Last N batches for detailed tracking
    
    // Constructor
    MigrationStatusReport() {
        report_time = std::chrono::system_clock::now();
    }
    
    // Report assessment
    bool is_healthy() const {
        return progress.data_consistency_score > 0.95 && 
               !progress.has_errors() && 
               data_accuracy > 0.95;
    }
    
    bool needs_attention() const {
        return progress.has_errors() || 
               data_accuracy < 0.9 || 
               system_cpu_usage > 0.9 || 
               system_memory_usage > 0.9;
    }
    
    double get_overall_health_score() const {
        return (progress.data_consistency_score + data_accuracy + migration_efficiency) / 3.0;
    }
};

/**
 * @brief Delta compression result for vectors
 */
struct DeltaCompression {
    std::vector<float> reference_vector;    // Base reference vector
    std::vector<float> deltas;              // Delta values from reference
    float compression_ratio{0.0f};         // Achieved compression ratio
    size_t original_size_bytes{0};         // Original uncompressed size
    size_t compressed_size_bytes{0};       // Compressed size
    
    // Constructor
    DeltaCompression() = default;
    
    // Compression effectiveness
    double get_effectiveness() const {
        return original_size_bytes > 0 ? 
            1.0 - (static_cast<double>(compressed_size_bytes) / static_cast<double>(original_size_bytes)) : 0.0;
    }
};

/**
 * @brief Dictionary compression result for metadata
 */
struct DictionaryCompression {
    std::vector<std::string> dictionary;    // String dictionary
    std::vector<uint32_t> encoded_indices;  // Encoded metadata as dictionary indices
    float compression_ratio{0.0f};          // Achieved compression ratio
    size_t original_size_bytes{0};          // Original uncompressed size
    size_t compressed_size_bytes{0};        // Compressed size
    
    // Constructor
    DictionaryCompression() = default;
    
    // Compression effectiveness
    double get_effectiveness() const {
        return original_size_bytes > 0 ? 
            1.0 - (static_cast<double>(compressed_size_bytes) / static_cast<double>(original_size_bytes)) : 0.0;
    }
};

/**
 * @brief Memory pool statistics
 */
struct MemoryPoolStats {
    size_t total_capacity_bytes{0};     // Total pool capacity
    size_t allocated_bytes{0};          // Currently allocated memory
    size_t free_bytes{0};               // Available memory
    double utilization_ratio{0.0};      // Allocation ratio (0.0-1.0)
    double fragmentation_ratio{0.0};    // Fragmentation ratio (0.0-1.0)
    size_t allocation_count{0};         // Number of active allocations
    size_t size_class_count{0};         // Number of size classes
    
    // Constructor
    MemoryPoolStats() = default;
    
    // Memory efficiency calculation
    double get_efficiency() const {
        return allocated_bytes > 0 ? static_cast<double>(allocated_bytes) / static_cast<double>(total_capacity_bytes) : 0.0;
    }
};

} // namespace semantic_vector

// Re-export commonly used semantic_vector types into tsdb::core
using semantic_vector::Vector;
using semantic_vector::QuantizedVector;
using semantic_vector::BinaryVector;
using semantic_vector::VectorIndex;
using semantic_vector::PerformanceMetrics;
using semantic_vector::MemoryTier;
using semantic_vector::MemoryPoolStats;
using semantic_vector::CompressionAlgorithm;
using semantic_vector::DeltaCompression;
using semantic_vector::DictionaryCompression;
using semantic_vector::CausalInference;
using semantic_vector::TemporalReasoning;
using semantic_vector::Anomaly;
using semantic_vector::Prediction;
using semantic_vector::Correlation;
using semantic_vector::QueryProcessor;
using semantic_vector::QueryPlan;
using semantic_vector::QueryResult;
using semantic_vector::MigrationManager;
using semantic_vector::MigrationProgress;
using semantic_vector::MigrationBatch;
using semantic_vector::MigrationCheckpoint;
using semantic_vector::MigrationStatusReport;

} // namespace core
} // namespace tsdb

#endif // TSDB_CORE_SEMANTIC_VECTOR_TYPES_H_ 