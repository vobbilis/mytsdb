#ifndef TSDB_CORE_SEMANTIC_VECTOR_CONFIG_H_
#define TSDB_CORE_SEMANTIC_VECTOR_CONFIG_H_

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <memory>
#include <functional>

#include "tsdb/core/semantic_vector_types.h"

namespace tsdb {
namespace core {
namespace semantic_vector {

// ============================================================================
// UNIFIED CONFIGURATION ARCHITECTURE
// ============================================================================

/**
 * @brief Unified configuration for all semantic vector features
 * 
 * This is the central configuration hub that defines all settings
 * for the semantic vector storage system, ensuring consistency
 * across all components and providing comprehensive configuration management.
 */
struct SemanticVectorConfig {
    // ============================================================================
    // VECTOR CONFIGURATION
    // ============================================================================
    
    /**
     * @brief Vector processing configuration
     * 
     * Controls vector creation, storage, and similarity search operations.
     */
    struct VectorConfig {
        // Vector dimensions and types
        size_t default_vector_dimension = 768;  // Default BERT embedding dimension
        size_t max_vector_dimension = 4096;     // Maximum supported dimension
        std::string default_metric = "cosine";  // Default similarity metric
        
        // Vector indexing configuration
        VectorIndex::IndexType default_index_type = VectorIndex::IndexType::HNSW;
        size_t hnsw_max_connections = 16;       // HNSW max connections per node
        size_t hnsw_ef_construction = 200;      // HNSW construction parameter
        size_t hnsw_ef_search = 50;             // HNSW search parameter
        size_t ivf_num_lists = 100;             // IVF number of lists
        
        // Vector search configuration
        size_t default_k_nearest = 10;          // Default k for k-NN search
        double default_similarity_threshold = 0.7; // Default similarity threshold
        bool enable_parallel_search = true;     // Enable parallel search
        size_t max_search_threads = 8;          // Maximum search threads
        
        // Vector validation
        bool validate_vectors_on_write = true;  // Validate vectors before storage
        bool normalize_vectors = true;          // Normalize vectors for cosine similarity
        double min_vector_magnitude = 1e-6;     // Minimum vector magnitude
        
        // Performance targets
        double target_search_latency_ms = 1.0;  // Target search latency
        double target_search_accuracy = 0.95;   // Target search accuracy
        size_t target_vectors_per_second = 10000; // Target throughput
        
        // Validation method
        bool is_valid() const;
        
        // Configuration examples
        static VectorConfig high_performance() {
            VectorConfig config;
            config.default_index_type = VectorIndex::IndexType::HNSW;
            config.hnsw_max_connections = 32;
            config.hnsw_ef_construction = 400;
            config.hnsw_ef_search = 100;
            config.enable_parallel_search = true;
            config.max_search_threads = 16;
            config.target_search_latency_ms = 0.5;
            return config;
        }
        
        static VectorConfig high_accuracy() {
            VectorConfig config;
            config.default_index_type = VectorIndex::IndexType::EXACT;
            config.default_similarity_threshold = 0.9;
            config.target_search_accuracy = 0.99;
            config.validate_vectors_on_write = true;
            return config;
        }
        
        static VectorConfig memory_efficient() {
            VectorConfig config;
            config.default_index_type = VectorIndex::IndexType::BINARY;
            config.default_vector_dimension = 64;
            config.target_search_latency_ms = 5.0;
            config.target_search_accuracy = 0.85;
            return config;
        }
    } vector_config;
    
    // ============================================================================
    // SEMANTIC CONFIGURATION
    // ============================================================================
    
    /**
     * @brief Semantic processing configuration
     * 
     * Controls natural language processing, semantic search, and
     * embedding generation operations.
     */
    struct SemanticConfig {
        // Embedding model configuration
        std::string embedding_model = "bert-base-uncased";  // Default BERT model
        size_t embedding_dimension = 768;                   // Model embedding dimension
        size_t max_sequence_length = 512;                   // Maximum sequence length
        bool enable_model_caching = true;                   // Cache loaded models
        
        // Semantic search configuration
        size_t default_semantic_results = 20;               // Default results per query
        double default_semantic_threshold = 0.7;            // Default semantic threshold
        bool enable_entity_extraction = true;               // Enable entity extraction
        bool enable_concept_extraction = true;              // Enable concept extraction
        
        // Entity and concept configuration
        size_t max_entities_per_series = 10;                // Maximum entities per series
        size_t max_concepts_per_series = 5;                 // Maximum concepts per series
        double entity_confidence_threshold = 0.8;           // Entity confidence threshold
        double concept_confidence_threshold = 0.7;          // Concept confidence threshold
        
        // Query processing configuration
        bool enable_query_expansion = true;                 // Enable query expansion
        bool enable_synonym_matching = true;                // Enable synonym matching
        size_t max_query_expansion_terms = 5;               // Maximum expansion terms
        double query_expansion_threshold = 0.6;             // Expansion similarity threshold
        
        // Performance targets
        double target_embedding_time_ms = 10.0;             // Target embedding generation time
        double target_semantic_search_time_ms = 5.0;        // Target semantic search time
        double target_semantic_accuracy = 0.9;              // Target semantic accuracy
        
        // Validation method
        bool is_valid() const;
        
        // Configuration examples
        static SemanticConfig high_accuracy() {
            SemanticConfig config;
            config.embedding_model = "bert-large-uncased";
            config.embedding_dimension = 1024;
            config.default_semantic_threshold = 0.8;
            config.entity_confidence_threshold = 0.9;
            config.concept_confidence_threshold = 0.8;
            config.target_semantic_accuracy = 0.95;
            return config;
        }
        
        static SemanticConfig fast_processing() {
            SemanticConfig config;
            config.embedding_model = "distilbert-base-uncased";
            config.embedding_dimension = 768;
            config.enable_model_caching = true;
            config.target_embedding_time_ms = 5.0;
            config.target_semantic_search_time_ms = 2.0;
            return config;
        }
        
        static SemanticConfig lightweight() {
            SemanticConfig config;
            config.embedding_model = "sentence-transformers/all-MiniLM-L6-v2";
            config.embedding_dimension = 384;
            config.max_entities_per_series = 5;
            config.max_concepts_per_series = 3;
            config.enable_query_expansion = false;
            return config;
        }
    } semantic_config;
    
    // ============================================================================
    // TEMPORAL CONFIGURATION
    // ============================================================================
    
    /**
     * @brief Temporal processing configuration
     * 
     * Controls temporal graph construction, correlation analysis,
     * and temporal reasoning operations.
     */
    struct TemporalConfig {
        // Graph construction configuration
        double correlation_threshold = 0.7;                 // Minimum correlation for edges
        size_t max_neighbors_per_node = 50;                 // Maximum neighbors per node
        bool enable_hierarchical_compression = true;        // Enable graph compression
        size_t compression_levels = 4;                      // Number of compression levels
        
        // Correlation analysis configuration
        size_t min_correlation_samples = 100;               // Minimum samples for correlation
        double correlation_confidence_level = 0.95;         // Statistical confidence level
        bool enable_multiple_testing_correction = true;     // Enable p-value correction
        std::string correlation_method = "pearson";         // Correlation method
        
        // Temporal reasoning configuration
        bool enable_pattern_recognition = true;             // Enable pattern recognition
        bool enable_anomaly_detection = true;               // Enable anomaly detection
        bool enable_forecasting = true;                     // Enable time series forecasting
        size_t min_pattern_length = 10;                     // Minimum pattern length
        
        // Causal inference configuration
        bool enable_causal_inference = true;                // Enable causal inference
        CausalInference::Algorithm causal_algorithm = CausalInference::Algorithm::GRANGER_CAUSALITY;
        double causal_significance_threshold = 0.05;        // Statistical significance
        size_t max_causal_lag = 10;                         // Maximum causal lag
        
        // Performance targets
        double target_correlation_time_ms = 20.0;           // Target correlation computation time
        double target_inference_time_ms = 50.0;             // Target causal inference time
        double target_correlation_accuracy = 0.9;           // Target correlation accuracy
        
        // Validation method
        bool is_valid() const;
        
        // Configuration examples
        static TemporalConfig high_precision() {
            TemporalConfig config;
            config.correlation_threshold = 0.8;
            config.correlation_confidence_level = 0.99;
            config.causal_significance_threshold = 0.01;
            config.target_correlation_accuracy = 0.95;
            return config;
        }
        
        static TemporalConfig fast_analysis() {
            TemporalConfig config;
            config.correlation_threshold = 0.6;
            config.max_neighbors_per_node = 25;
            config.enable_hierarchical_compression = true;
            config.target_correlation_time_ms = 10.0;
            config.target_inference_time_ms = 25.0;
            return config;
        }
        
        static TemporalConfig lightweight() {
            TemporalConfig config;
            config.correlation_threshold = 0.8;
            config.max_neighbors_per_node = 20;
            config.enable_causal_inference = false;
            config.enable_forecasting = false;
            return config;
        }
    } temporal_config;
    
    // ============================================================================
    // MEMORY OPTIMIZATION CONFIGURATION
    // ============================================================================
    
    /**
     * @brief Memory optimization configuration
     * 
     * Controls all memory optimization features including quantization,
     * pruning, compression, and tiered memory management.
     */
    struct MemoryConfig {
        // Quantization settings
        bool enable_product_quantization = true;            // Enable Product Quantization
        bool enable_binary_quantization = false;            // Enable Binary Quantization
        size_t pq_subvectors = 8;                           // PQ subvectors (8-16)
        size_t pq_bits_per_subvector = 8;                   // PQ bits per subvector (6-10)
        
        // Pruning settings
        bool enable_embedding_pruning = true;               // Enable embedding pruning
        float sparsity_threshold = 0.1f;                    // Sparsity threshold (0.05-0.2)
        bool enable_knowledge_distillation = true;          // Enable knowledge distillation
        std::string distillation_model = "distilbert-base-uncased"; // Distillation model
        
        // Graph compression settings
        bool enable_sparse_graph = true;                    // Enable sparse graph representation
        double graph_correlation_threshold = 0.7f;          // Graph correlation threshold
        size_t max_graph_levels = 4;                        // Maximum graph compression levels
        bool enable_hierarchical_compression = true;        // Enable hierarchical compression
        
        // Memory management settings
        bool enable_tiered_memory = true;                   // Enable tiered memory management
        size_t ram_tier_capacity_mb = 1024;                 // RAM tier capacity (1GB)
        size_t ssd_tier_capacity_mb = 10240;                // SSD tier capacity (10GB)
        size_t hdd_tier_capacity_mb = 102400;               // HDD tier capacity (100GB)
        
        // Compression settings
        bool enable_delta_compression = true;               // Enable delta compression
        bool enable_dictionary_compression = true;          // Enable dictionary compression
        size_t compression_level = 6;                       // Compression level (0-9)
        
        // Performance targets
        double target_memory_reduction = 0.8;               // Target memory reduction (80%)
        double max_latency_impact = 0.05;                   // Max latency impact (5%)
        double min_accuracy_preservation = 0.95;            // Min accuracy preservation (95%)
        
        // Memory monitoring
        bool enable_memory_monitoring = true;               // Enable memory usage monitoring
        size_t memory_check_interval_seconds = 60;          // Memory check interval
        double memory_warning_threshold = 0.8;              // Memory warning threshold (80%)
        double memory_critical_threshold = 0.95;            // Memory critical threshold (95%)
        
        // Validation method
        bool is_valid() const;
        
        // Configuration examples
        static MemoryConfig high_efficiency() {
            MemoryConfig config;
            config.enable_product_quantization = true;
            config.pq_subvectors = 16;
            config.pq_bits_per_subvector = 6;
            config.enable_embedding_pruning = true;
            config.sparsity_threshold = 0.05f;
            config.target_memory_reduction = 0.9;
            return config;
        }
        
        static MemoryConfig balanced() {
            MemoryConfig config;
            config.enable_product_quantization = true;
            config.pq_subvectors = 8;
            config.pq_bits_per_subvector = 8;
            config.enable_embedding_pruning = true;
            config.sparsity_threshold = 0.1f;
            config.target_memory_reduction = 0.8;
            return config;
        }
        
        static MemoryConfig high_performance() {
            MemoryConfig config;
            config.enable_product_quantization = false;
            config.enable_embedding_pruning = false;
            config.enable_tiered_memory = true;
            config.ram_tier_capacity_mb = 2048;
            config.target_memory_reduction = 0.5;
            config.max_latency_impact = 0.02;
            return config;
        }
    } memory_config;
    
    // ============================================================================
    // QUERY PROCESSING CONFIGURATION
    // ============================================================================
    
    /**
     * @brief Query processing configuration
     * 
     * Controls query parsing, optimization, execution, and result processing.
     */
    struct QueryConfig {
        // Query execution configuration
        size_t max_results_per_query = 100;                 // Maximum results per query
        double query_timeout_seconds = 30.0;                // Query timeout
        bool enable_parallel_execution = true;              // Enable parallel execution
        size_t max_parallel_threads = 8;                    // Maximum parallel threads
        
        // Query optimization configuration
        bool enable_query_optimization = true;              // Enable query optimization
        bool enable_cost_based_optimization = true;         // Enable cost-based optimization
        size_t max_optimization_iterations = 10;            // Maximum optimization iterations
        double optimization_timeout_seconds = 5.0;          // Optimization timeout
        
        // Result caching configuration
        bool enable_result_caching = true;                  // Enable result caching
        size_t cache_size = 10000;                          // Cache size in entries
        double cache_ttl_seconds = 3600;                    // Cache TTL (1 hour)
        bool enable_cache_compression = true;               // Enable cache compression
        
        // Query validation configuration
        bool validate_queries = true;                       // Validate queries before execution
        size_t max_query_complexity = 1000;                 // Maximum query complexity
        bool enable_query_logging = true;                   // Enable query logging
        std::string log_level = "info";                     // Query log level
        
        // Performance targets
        double target_query_time_ms = 10.0;                 // Target query execution time
        double target_optimization_time_ms = 1.0;           // Target optimization time
        double target_cache_hit_ratio = 0.8;                // Target cache hit ratio
        
        // Validation method
        bool is_valid() const;
        
        // Configuration examples
        static QueryConfig high_throughput() {
            QueryConfig config;
            config.enable_parallel_execution = true;
            config.max_parallel_threads = 16;
            config.enable_result_caching = true;
            config.cache_size = 50000;
            config.target_query_time_ms = 5.0;
            return config;
        }
        
        static QueryConfig high_accuracy() {
            QueryConfig config;
            config.enable_query_optimization = true;
            config.enable_cost_based_optimization = true;
            config.max_optimization_iterations = 20;
            config.validate_queries = true;
            config.target_query_time_ms = 20.0;
            return config;
        }
        
        static QueryConfig resource_efficient() {
            QueryConfig config;
            config.max_parallel_threads = 4;
            config.cache_size = 5000;
            config.enable_cache_compression = true;
            config.target_query_time_ms = 15.0;
            return config;
        }
    } query_config;
    
    // ============================================================================
    // ADVANCED ANALYTICS CONFIGURATION
    // ============================================================================
    
    /**
     * @brief Advanced analytics configuration
     * 
     * Controls causal inference, temporal reasoning, and advanced
     * analytical operations.
     */
    struct AnalyticsConfig {
        // Causal inference configuration
        bool enable_causal_inference = true;                // Enable causal inference
        CausalInference::Algorithm causal_algorithm = CausalInference::Algorithm::GRANGER_CAUSALITY;
        double causal_significance_threshold = 0.05;        // Statistical significance
        size_t max_causal_lag = 10;                         // Maximum causal lag
        bool enable_multiple_testing_correction = true;     // Enable p-value correction
        
        // Temporal reasoning configuration
        bool enable_temporal_reasoning = true;              // Enable temporal reasoning
        TemporalReasoning::ReasoningType reasoning_type = TemporalReasoning::ReasoningType::PATTERN_RECOGNITION;
        double pattern_threshold = 0.7;                     // Pattern significance threshold
        size_t min_pattern_length = 10;                     // Minimum pattern length
        bool enable_multi_modal_reasoning = true;           // Enable multi-modal reasoning
        
        // Anomaly detection configuration
        bool enable_anomaly_detection = true;               // Enable anomaly detection
        double anomaly_threshold = 3.0;                     // Anomaly detection threshold (sigma)
        size_t anomaly_window_size = 100;                   // Anomaly detection window
        bool enable_adaptive_thresholds = true;             // Enable adaptive thresholds
        
        // Forecasting configuration
        bool enable_forecasting = true;                     // Enable time series forecasting
        size_t max_forecast_horizon = 100;                  // Maximum forecast horizon
        size_t min_training_samples = 1000;                 // Minimum training samples
        std::string forecasting_model = "prophet";          // Default forecasting model
        
        // Performance targets
        double target_inference_time_ms = 50.0;             // Target causal inference time
        double target_reasoning_time_ms = 30.0;             // Target temporal reasoning time
        double target_analytics_accuracy = 0.9;             // Target analytics accuracy
        
        // Validation method
        bool is_valid() const;
        
        // Configuration examples
        static AnalyticsConfig comprehensive() {
            AnalyticsConfig config;
            config.enable_causal_inference = true;
            config.enable_temporal_reasoning = true;
            config.enable_anomaly_detection = true;
            config.enable_forecasting = true;
            config.enable_multi_modal_reasoning = true;
            config.target_analytics_accuracy = 0.95;
            return config;
        }
        
        static AnalyticsConfig fast_analysis() {
            AnalyticsConfig config;
            config.causal_algorithm = CausalInference::Algorithm::GRANGER_CAUSALITY;
            config.reasoning_type = TemporalReasoning::ReasoningType::PATTERN_RECOGNITION;
            config.target_inference_time_ms = 25.0;
            config.target_reasoning_time_ms = 15.0;
            return config;
        }
        
        static AnalyticsConfig lightweight() {
            AnalyticsConfig config;
            config.enable_causal_inference = false;
            config.enable_forecasting = false;
            config.enable_multi_modal_reasoning = false;
            config.target_analytics_accuracy = 0.8;
            return config;
        }
    } analytics_config;
    
    // ============================================================================
    // COMPRESSION CONFIGURATION
    // ============================================================================
    
    /**
     * @brief Compression configuration
     * 
     * Controls vector and metadata compression settings, algorithms,
     * and optimization strategies.
     */
    struct CompressionConfig {
        // Compression algorithm configuration
        CompressionAlgorithm vector_compression_algorithm = CompressionAlgorithm::DELTA;       // Vector compression method
        CompressionAlgorithm metadata_compression_algorithm = CompressionAlgorithm::DICTIONARY; // Metadata compression method
        bool enable_adaptive_compression = true;                                               // Adapt compression based on data patterns
        bool enable_hybrid_compression = false;                                                // Use combined compression methods
        
        // Delta compression configuration
        float delta_compression_threshold = 0.01f;         // Minimum delta for storage
        size_t delta_reference_window = 100;               // Size of reference window
        bool enable_delta_optimization = true;             // Optimize delta encoding
        
        // Dictionary compression configuration
        size_t max_dictionary_size = 10000;                // Maximum dictionary entries
        float dictionary_rebuild_threshold = 0.3f;         // When to rebuild dictionary (fragmentation ratio)
        bool enable_dictionary_optimization = true;        // Optimize dictionary encoding
        
        // Performance configuration
        size_t compression_buffer_size = 1024 * 1024;      // Compression buffer size (1MB)
        size_t compression_thread_count = 4;               // Number of compression threads
        bool enable_parallel_compression = true;           // Enable parallel compression
        
        // Quality configuration
        float target_compression_ratio = 0.6f;             // Target compression ratio (40% reduction)
        float max_compression_latency_ms = 5.0f;           // Maximum acceptable compression latency
        float max_decompression_latency_ms = 2.0f;         // Maximum acceptable decompression latency
        
        // Monitoring and optimization
        bool enable_compression_monitoring = true;         // Monitor compression effectiveness
        bool enable_compression_tuning = true;             // Auto-tune compression parameters
        size_t compression_stats_window = 1000;            // Window size for compression statistics
        
        // Validation method
        bool is_valid() const;
        
        // Configuration examples
        static CompressionConfig high_compression() {
            CompressionConfig config;
            config.vector_compression_algorithm = CompressionAlgorithm::HYBRID;
            config.metadata_compression_algorithm = CompressionAlgorithm::DICTIONARY;
            config.enable_adaptive_compression = true;
            config.enable_hybrid_compression = true;
            config.target_compression_ratio = 0.3f;  // 70% reduction
            config.max_compression_latency_ms = 10.0f;
            return config;
        }
        
        static CompressionConfig high_speed() {
            CompressionConfig config;
            config.vector_compression_algorithm = CompressionAlgorithm::DELTA;
            config.metadata_compression_algorithm = CompressionAlgorithm::DICTIONARY;
            config.enable_parallel_compression = true;
            config.compression_thread_count = 8;
            config.target_compression_ratio = 0.7f;  // 30% reduction
            config.max_compression_latency_ms = 1.0f;
            config.max_decompression_latency_ms = 0.5f;
            return config;
        }
        
        static CompressionConfig balanced() {
            CompressionConfig config;
            config.vector_compression_algorithm = CompressionAlgorithm::DELTA;
            config.metadata_compression_algorithm = CompressionAlgorithm::DICTIONARY;
            config.enable_adaptive_compression = true;
            config.target_compression_ratio = 0.5f;  // 50% reduction
            config.max_compression_latency_ms = 3.0f;
            return config;
        }
    } compression_config;
    
    // ============================================================================
    // MIGRATION CONFIGURATION
    // ============================================================================
    
    /**
     * @brief Migration configuration
     * 
     * Controls data migration, rollback mechanisms, and transition management
     * for semantic vector storage deployment and system upgrades.
     */
    struct MigrationConfig {
        // Migration strategy configuration
        MigrationManager::MigrationStrategy default_strategy = MigrationManager::MigrationStrategy::PARALLEL;
        MigrationManager::RollbackStrategy rollback_strategy = MigrationManager::RollbackStrategy::CHECKPOINT;
        bool enable_dual_write = true;                      // Enable dual-write during migration
        bool enable_gradual_migration = true;               // Enable gradual migration
        
        // Batch processing configuration
        size_t batch_size = 1000;                          // Number of series per batch
        size_t max_parallel_batches = 8;                   // Maximum parallel batches
        size_t batch_retry_limit = 3;                      // Maximum retries per batch
        double batch_timeout_seconds = 300.0;              // Timeout per batch (5 minutes)
        
        // Progress tracking configuration
        bool enable_progress_tracking = true;              // Enable detailed progress tracking
        double progress_report_interval_seconds = 30.0;    // Progress report frequency
        bool enable_performance_monitoring = true;         // Monitor migration performance
        size_t max_recent_batches = 100;                   // Keep N recent batches in memory
        
        // Checkpoint configuration
        bool enable_checkpoints = true;                    // Enable migration checkpoints
        size_t checkpoint_interval_batches = 50;           // Create checkpoint every N batches
        size_t max_checkpoints = 10;                       // Maximum checkpoints to keep
        bool enable_checkpoint_verification = true;        // Verify checkpoint integrity
        
        // Rollback configuration
        bool enable_automatic_rollback = true;             // Auto-rollback on critical errors
        double rollback_trigger_error_rate = 0.05;         // Rollback if >5% error rate
        size_t rollback_verification_batches = 10;         // Verify N batches after rollback
        bool enable_rollback_to_checkpoint = true;         // Enable checkpoint-based rollback
        
        // Data consistency configuration
        bool enable_data_validation = true;                // Validate data during migration
        double consistency_check_threshold = 0.95;         // Minimum consistency score
        bool enable_integrity_checks = true;               // Enable integrity checking
        size_t validation_sample_rate = 100;               // Validate every Nth series
        
        // Performance optimization
        size_t migration_thread_pool_size = 16;            // Migration worker threads
        size_t io_buffer_size_mb = 64;                     // I/O buffer size (64MB)
        bool enable_compression_during_migration = true;   // Compress data during migration
        bool enable_parallel_validation = true;            // Parallel validation
        
        // System resource limits
        double max_cpu_usage = 0.8;                        // Maximum CPU usage (80%)
        double max_memory_usage = 0.7;                     // Maximum memory usage (70%)
        double max_disk_io_mbps = 500.0;                   // Maximum disk I/O (500 MB/s)
        double max_network_io_mbps = 200.0;                // Maximum network I/O (200 MB/s)
        
        // Quality and reliability
        double target_migration_rate_series_per_second = 100.0; // Target migration rate
        double target_data_accuracy = 0.999;               // Target data accuracy (99.9%)
        double max_acceptable_downtime_minutes = 5.0;      // Maximum downtime during migration
        bool enable_zero_downtime_migration = true;        // Attempt zero-downtime migration
        
        // Monitoring and alerting
        bool enable_migration_logging = true;              // Enable migration logging
        std::string log_level = "info";                    // Migration log level
        bool enable_alerting = true;                       // Enable migration alerts
        std::vector<std::string> alert_endpoints;          // Alert notification endpoints
        
        // Validation method
        bool is_valid() const;
        
        // Configuration examples
        static MigrationConfig high_throughput() {
            MigrationConfig config;
            config.default_strategy = MigrationManager::MigrationStrategy::PARALLEL;
            config.batch_size = 5000;
            config.max_parallel_batches = 16;
            config.migration_thread_pool_size = 32;
            config.target_migration_rate_series_per_second = 500.0;
            config.enable_compression_during_migration = false; // Speed over space
            return config;
        }
        
        static MigrationConfig high_reliability() {
            MigrationConfig config;
            config.default_strategy = MigrationManager::MigrationStrategy::SEQUENTIAL;
            config.enable_checkpoints = true;
            config.checkpoint_interval_batches = 10;
            config.enable_data_validation = true;
            config.validation_sample_rate = 10; // Validate 10% of data
            config.enable_automatic_rollback = true;
            config.rollback_trigger_error_rate = 0.01; // 1% error rate triggers rollback
            return config;
        }
        
        static MigrationConfig zero_downtime() {
            MigrationConfig config;
            config.default_strategy = MigrationManager::MigrationStrategy::INCREMENTAL;
            config.enable_dual_write = true;
            config.enable_gradual_migration = true;
            config.enable_zero_downtime_migration = true;
            config.max_acceptable_downtime_minutes = 0.0;
            config.batch_size = 100; // Smaller batches for smoother operation
            return config;
        }
        
        static MigrationConfig resource_constrained() {
            MigrationConfig config;
            config.default_strategy = MigrationManager::MigrationStrategy::SEQUENTIAL;
            config.batch_size = 500;
            config.max_parallel_batches = 2;
            config.migration_thread_pool_size = 4;
            config.max_cpu_usage = 0.5;
            config.max_memory_usage = 0.4;
            config.target_migration_rate_series_per_second = 50.0;
            return config;
        }
    } migration_config;
    
    // ============================================================================
    // SYSTEM CONFIGURATION
    // ============================================================================
    
    /**
     * @brief System-wide configuration
     * 
     * Controls system-wide settings, monitoring, logging, and
     * performance management.
     */
    struct SystemConfig {
        // Performance monitoring
        bool enable_performance_monitoring = true;          // Enable performance monitoring
        size_t metrics_collection_interval_seconds = 60;    // Metrics collection interval
        bool enable_performance_alerts = true;              // Enable performance alerts
        double performance_alert_threshold = 0.8;           // Performance alert threshold
        
        // Logging configuration
        bool enable_logging = true;                         // Enable system logging
        std::string log_level = "info";                     // Log level
        std::string log_file_path = "/var/log/tsdb_semantic_vector.log"; // Log file path
        size_t max_log_file_size_mb = 100;                  // Maximum log file size
        size_t max_log_files = 10;                          // Maximum number of log files
        
        // Error handling configuration
        bool enable_error_recovery = true;                  // Enable automatic error recovery
        size_t max_retry_attempts = 3;                      // Maximum retry attempts
        double retry_backoff_seconds = 1.0;                 // Retry backoff time
        bool enable_circuit_breaker = true;                 // Enable circuit breaker pattern
        
        // Security configuration
        bool enable_authentication = false;                 // Enable authentication
        bool enable_authorization = false;                  // Enable authorization
        std::string encryption_algorithm = "AES-256";       // Encryption algorithm
        bool enable_audit_logging = false;                  // Enable audit logging
        
        // Validation method
        bool is_valid() const;
        
        // Configuration examples
        static SystemConfig production() {
            SystemConfig config;
            config.enable_performance_monitoring = true;
            config.enable_logging = true;
            config.log_level = "warn";
            config.enable_error_recovery = true;
            config.enable_circuit_breaker = true;
            config.enable_authentication = true;
            config.enable_authorization = true;
            config.enable_audit_logging = true;
            return config;
        }
        
        static SystemConfig development() {
            SystemConfig config;
            config.enable_performance_monitoring = true;
            config.enable_logging = true;
            config.log_level = "debug";
            config.enable_error_recovery = true;
            config.enable_circuit_breaker = false;
            return config;
        }
        
        static SystemConfig minimal() {
            SystemConfig config;
            config.enable_performance_monitoring = false;
            config.enable_logging = false;
            config.enable_error_recovery = false;
            config.enable_circuit_breaker = false;
            return config;
        }
    } system_config;
    
    // ============================================================================
    // CONFIGURATION MANAGEMENT
    // ============================================================================
    
    /**
     * @brief Configuration validation and management
     */
    
    // Validation methods
    bool is_valid() const;
    ConfigValidationResult validate() const;
    
    // Configuration inheritance and override
    void inherit_from(const SemanticVectorConfig& parent);
    void override_with(const SemanticVectorConfig& overrides);
    
    // Configuration serialization
    std::string to_json() const;
    static SemanticVectorConfig from_json(const std::string& json);
    
    // Configuration examples for different use cases
    static SemanticVectorConfig high_performance_config();
    static SemanticVectorConfig high_accuracy_config();
    static SemanticVectorConfig memory_efficient_config();
    static SemanticVectorConfig balanced_config();
    static SemanticVectorConfig development_config();
    static SemanticVectorConfig production_config();
    
    // Configuration comparison
    bool operator==(const SemanticVectorConfig& other) const;
    bool operator!=(const SemanticVectorConfig& other) const;
    
    // Configuration diff
    struct ConfigDiff {
        std::vector<std::string> changed_fields;
        std::map<std::string, std::string> old_values;
        std::map<std::string, std::string> new_values;
    };
    
    ConfigDiff diff(const SemanticVectorConfig& other) const;
    
    // Configuration migration
    bool is_compatible_with(const SemanticVectorConfig& other) const;
    SemanticVectorConfig migrate_from(const SemanticVectorConfig& old_config) const;
};

// ============================================================================
// CONFIGURATION VALIDATION AND UTILITIES
// ============================================================================

/**
 * @brief Configuration validator
 * 
 * Provides comprehensive validation for all configuration options
 * and ensures consistency across different configuration sections.
 */
class ConfigValidator {
public:
    // Individual section validation
    static bool validate_vector_config(const SemanticVectorConfig::VectorConfig& config);
    static bool validate_semantic_config(const SemanticVectorConfig::SemanticConfig& config);
    static bool validate_temporal_config(const SemanticVectorConfig::TemporalConfig& config);
    static bool validate_memory_config(const SemanticVectorConfig::MemoryConfig& config);
    static bool validate_query_config(const SemanticVectorConfig::QueryConfig& config);
    static bool validate_analytics_config(const SemanticVectorConfig::AnalyticsConfig& config);
    static bool validate_migration_config(const SemanticVectorConfig::MigrationConfig& config);
    static bool validate_system_config(const SemanticVectorConfig::SystemConfig& config);
    
    // Cross-section validation
    static bool validate_cross_section_consistency(const SemanticVectorConfig& config);
    static bool validate_performance_targets(const SemanticVectorConfig& config);
    static bool validate_resource_requirements(const SemanticVectorConfig& config);
    
    // Comprehensive validation
    static ConfigValidationResult validate_config(const SemanticVectorConfig& config);
    
    // Configuration optimization
    static SemanticVectorConfig optimize_for_performance(const SemanticVectorConfig& config);
    static SemanticVectorConfig optimize_for_memory(const SemanticVectorConfig& config);
    static SemanticVectorConfig optimize_for_accuracy(const SemanticVectorConfig& config);
};

/**
 * @brief Configuration manager
 * 
 * Manages configuration loading, saving, validation, and updates
 * with support for hot-reloading and configuration versioning.
 */
class ConfigManager {
public:
    // Configuration loading and saving
    static Result<SemanticVectorConfig> load_from_file(const std::string& file_path);
    static Result<void> save_to_file(const SemanticVectorConfig& config, const std::string& file_path);
    
    // Configuration hot-reloading
    static Result<void> enable_hot_reload(const std::string& file_path, 
                                        std::function<void(const SemanticVectorConfig&)> callback);
    static void disable_hot_reload();
    
    // Configuration versioning
    static std::string get_config_version(const SemanticVectorConfig& config);
    static bool is_config_compatible(const std::string& version1, const std::string& version2);
    
    // Configuration backup and restore
    static Result<void> backup_config(const SemanticVectorConfig& config, const std::string& backup_path);
    static Result<SemanticVectorConfig> restore_config(const std::string& backup_path);
    
    // Configuration monitoring
    static void start_config_monitoring();
    static void stop_config_monitoring();
    static std::vector<std::string> get_config_changes();
};

} // namespace semantic_vector
} // namespace core
} // namespace tsdb

#endif // TSDB_CORE_SEMANTIC_VECTOR_CONFIG_H_ 