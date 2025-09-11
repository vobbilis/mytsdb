#ifndef TSDB_STORAGE_SEMANTIC_VECTOR_DICTIONARY_COMPRESSED_METADATA_H_
#define TSDB_STORAGE_SEMANTIC_VECTOR_DICTIONARY_COMPRESSED_METADATA_H_

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
 * @brief Dictionary Compressed Metadata Implementation
 * 
 * Provides dictionary-based compression for metadata strings, achieving
 * high compression ratios for repetitive metadata patterns.
 * Updated for linter cache refresh and architecture unity.
 */
class DictionaryCompressedMetadataImpl {
public:
    explicit DictionaryCompressedMetadataImpl(const core::semantic_vector::SemanticVectorConfig::CompressionConfig& config);
    virtual ~DictionaryCompressedMetadataImpl() = default;
    
    // Metadata compression operations
    core::Result<core::semantic_vector::DictionaryCompression> compress_metadata(const std::vector<std::string>& metadata);
    core::Result<std::vector<std::string>> decompress_metadata(const core::semantic_vector::DictionaryCompression& compressed);
    core::Result<void> compress_metadata_batch(const std::vector<std::vector<std::string>>& metadata_batch, std::vector<core::semantic_vector::DictionaryCompression>& compressed_batch);
    core::Result<void> decompress_metadata_batch(const std::vector<core::semantic_vector::DictionaryCompression>& compressed_batch, std::vector<std::vector<std::string>>& metadata_batch);
    
    // Dictionary management
    core::Result<void> optimize_dictionary(const std::vector<std::vector<std::string>>& training_metadata);
    core::Result<void> rebuild_dictionary();
    core::Result<size_t> get_dictionary_size() const;
    core::Result<double> get_dictionary_efficiency() const;
    
    // Compression optimization
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
        std::atomic<size_t> dictionary_rebuilds{0};
        std::atomic<size_t> compression_errors{0};
        std::atomic<size_t> decompression_errors{0};
    };
    
    // Configuration and state
    core::semantic_vector::SemanticVectorConfig::CompressionConfig config_;
    PerformanceMonitoring performance_monitoring_;
    mutable std::shared_mutex mutex_;
    
    // Internal compression structures
    std::unique_ptr<class StringDictionary> string_dictionary_;
    std::unique_ptr<class MetadataEncoder> metadata_encoder_;
    std::unique_ptr<class DictionaryOptimizer> dictionary_optimizer_;
    
    // Internal helper methods
    core::Result<void> initialize_dictionary_compression_structures();
    core::Result<void> update_performance_metrics(const std::string& operation, double latency, bool success) const;
};

// Factory functions
std::unique_ptr<DictionaryCompressedMetadataImpl> CreateDictionaryCompressedMetadata(
    const core::semantic_vector::SemanticVectorConfig::CompressionConfig& config);

std::unique_ptr<DictionaryCompressedMetadataImpl> CreateDictionaryCompressedMetadataForUseCase(
    const std::string& use_case,
    const core::semantic_vector::SemanticVectorConfig::CompressionConfig& base_config = core::semantic_vector::SemanticVectorConfig::CompressionConfig());

core::Result<core::semantic_vector::ConfigValidationResult> ValidateDictionaryCompressionConfig(
    const core::semantic_vector::SemanticVectorConfig::CompressionConfig& config);

} // namespace semantic_vector
} // namespace storage
} // namespace tsdb

#endif // TSDB_STORAGE_SEMANTIC_VECTOR_DICTIONARY_COMPRESSED_METADATA_H_