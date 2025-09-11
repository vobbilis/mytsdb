#ifndef TSDB_STORAGE_SEMANTIC_VECTOR_DELTA_COMPRESSED_VECTORS_H_
#define TSDB_STORAGE_SEMANTIC_VECTOR_DELTA_COMPRESSED_VECTORS_H_

#include <memory>
#include <vector>
#include <string>
#include <map>
#include <optional>
#include <shared_mutex>

#include "tsdb/core/semantic_vector_types.h"
#include "tsdb/core/semantic_vector_config.h"
#include "tsdb/core/result.h"
#include "tsdb/core/error.h"

namespace tsdb {
namespace storage {
namespace semantic_vector {

/**
 * @brief Delta Compressed Vectors Implementation
 * 
 * Provides delta encoding compression for vector sequences, achieving
 * significant compression ratios while maintaining fast access patterns.
 * Updated for linter cache refresh and architecture unity.
 */
class DeltaCompressedVectorsImpl {
public:
    explicit DeltaCompressedVectorsImpl(const core::semantic_vector::SemanticVectorConfig::CompressionConfig& config);
    virtual ~DeltaCompressedVectorsImpl() = default;
    
    // Vector compression operations
    core::Result<core::semantic_vector::DeltaCompression> compress_vector(const core::Vector& vector);
    core::Result<core::Vector> decompress_vector(const core::semantic_vector::DeltaCompression& compressed);
    core::Result<void> compress_vector_sequence(const std::vector<core::Vector>& vectors, std::vector<core::semantic_vector::DeltaCompression>& compressed_vectors);
    core::Result<void> decompress_vector_sequence(const std::vector<core::semantic_vector::DeltaCompression>& compressed_vectors, std::vector<core::Vector>& vectors);
    
    // Compression optimization
    core::Result<void> optimize_reference_vector(const std::vector<core::Vector>& training_vectors);
    core::Result<void> update_compression_parameters(const core::semantic_vector::SemanticVectorConfig::CompressionConfig& new_config);
    core::Result<double> get_compression_ratio() const;
    core::Result<double> get_compression_effectiveness() const;
    
    // Performance monitoring  
    core::Result<core::PerformanceMetrics> get_performance_metrics() const;
    core::Result<void> reset_performance_metrics();
    
    // Configuration management
    void update_config(const core::semantic_vector::SemanticVectorConfig::CompressionConfig& config);
    core::semantic_vector::SemanticVectorConfig::CompressionConfig get_config() const;

private:
    // Performance monitoring structures
    struct PerformanceMonitoring {
        std::atomic<double> average_compression_latency_ms{0.0};
        std::atomic<double> average_decompression_latency_ms{0.0};
        std::atomic<size_t> total_compressions{0};
        std::atomic<size_t> total_decompressions{0};
        std::atomic<size_t> total_compressed_bytes{0};
        std::atomic<size_t> total_original_bytes{0};
        std::atomic<double> average_compression_ratio{0.0};
        std::atomic<size_t> compression_errors{0};
        std::atomic<size_t> decompression_errors{0};
    };
    
    // Configuration and state
    core::semantic_vector::SemanticVectorConfig::CompressionConfig config_;
    PerformanceMonitoring performance_monitoring_;
    mutable std::shared_mutex mutex_;
    
    // Internal compression structures
    std::unique_ptr<class DeltaEncoder> delta_encoder_;
    std::unique_ptr<class ReferenceVectorManager> reference_manager_;
    std::unique_ptr<class CompressionOptimizer> compression_optimizer_;
    
    // Internal helper methods
    core::Result<void> initialize_delta_compression_structures();
    core::Result<void> update_performance_metrics(const std::string& operation, double latency, bool success) const;
};

// Factory functions
std::unique_ptr<DeltaCompressedVectorsImpl> CreateDeltaCompressedVectors(
    const core::semantic_vector::SemanticVectorConfig::CompressionConfig& config);

std::unique_ptr<DeltaCompressedVectorsImpl> CreateDeltaCompressedVectorsForUseCase(
    const std::string& use_case,
    const core::semantic_vector::SemanticVectorConfig::CompressionConfig& base_config = core::semantic_vector::SemanticVectorConfig::CompressionConfig());

core::Result<core::semantic_vector::ConfigValidationResult> ValidateDeltaCompressionConfig(
    const core::semantic_vector::SemanticVectorConfig::CompressionConfig& config);

} // namespace semantic_vector
} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_SEMANTIC_VECTOR_DELTA_COMPRESSED_VECTORS_H_