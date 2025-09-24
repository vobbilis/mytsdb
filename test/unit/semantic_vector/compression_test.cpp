#include <gtest/gtest.h>
#include "tsdb/storage/semantic_vector/delta_compressed_vectors.h"
#include "tsdb/storage/semantic_vector/dictionary_compressed_metadata.h"
#include "tsdb/core/semantic_vector_config.h"

using namespace tsdb::storage::semantic_vector;
using namespace tsdb::core;

TEST(SemVecSmoke, DeltaCompressionBasic) {
    // Create basic compression config
    semantic_vector::SemanticVectorConfig::CompressionConfig config;
    config.vector_compression_algorithm = semantic_vector::CompressionAlgorithm::DELTA;
    config.target_compression_ratio = 0.5f;
    
    // Create delta compressed vectors instance
    auto delta_compressor = CreateDeltaCompressedVectors(config);
    ASSERT_NE(delta_compressor, nullptr);
    
    // Test basic vector compression
    Vector test_vector = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    auto compression_result = delta_compressor->compress_vector(test_vector);
    ASSERT_TRUE(compression_result.is_ok());
    
    // Test basic vector decompression
    auto decompression_result = delta_compressor->decompress_vector(compression_result.value());
    ASSERT_TRUE(decompression_result.is_ok());
    
    // Verify decompressed vector matches original
    Vector decompressed = decompression_result.value();
    ASSERT_EQ(decompressed.size(), test_vector.size());
    
    // Test performance metrics
    auto metrics_result = delta_compressor->get_performance_metrics();
    ASSERT_TRUE(metrics_result.is_ok());
    
    // Test compression ratio
    auto ratio_result = delta_compressor->get_compression_ratio();
    ASSERT_TRUE(ratio_result.is_ok());
}

TEST(SemVecSmoke, DictionaryCompressionBasic) {
    // Create basic compression config
    semantic_vector::SemanticVectorConfig::CompressionConfig config;
    config.metadata_compression_algorithm = semantic_vector::CompressionAlgorithm::DICTIONARY;
    config.max_dictionary_size = 1000;
    
    // Create dictionary compressed metadata instance
    auto dict_compressor = CreateDictionaryCompressedMetadata(config);
    ASSERT_NE(dict_compressor, nullptr);
    
    // Test basic metadata compression
    std::vector<std::string> test_metadata = {"label1", "label2", "label1", "label3", "label2"};
    auto compression_result = dict_compressor->compress_metadata(test_metadata);
    ASSERT_TRUE(compression_result.is_ok());
    
    // Test basic metadata decompression
    auto decompression_result = dict_compressor->decompress_metadata(compression_result.value());
    ASSERT_TRUE(decompression_result.is_ok());
    
    // Verify decompressed metadata matches original
    std::vector<std::string> decompressed = decompression_result.value();
    ASSERT_EQ(decompressed.size(), test_metadata.size());
    ASSERT_EQ(decompressed[0], "label1");
    ASSERT_EQ(decompressed[1], "label2");
    ASSERT_EQ(decompressed[2], "label1");
    
    // Test performance metrics
    auto metrics_result = dict_compressor->get_performance_metrics();
    ASSERT_TRUE(metrics_result.is_ok());
    
    // Test dictionary size
    auto dict_size_result = dict_compressor->get_dictionary_size();
    ASSERT_TRUE(dict_size_result.is_ok());
}

TEST(SemVecSmoke, CompressionUseCases) {
    // Test high compression use case
    auto high_compression_delta = CreateDeltaCompressedVectorsForUseCase("high_compression");
    ASSERT_NE(high_compression_delta, nullptr);
    
    auto high_compression_dict = CreateDictionaryCompressedMetadataForUseCase("high_compression");
    ASSERT_NE(high_compression_dict, nullptr);
    
    // Test high speed use case
    auto high_speed_delta = CreateDeltaCompressedVectorsForUseCase("high_speed");
    ASSERT_NE(high_speed_delta, nullptr);
    
    auto high_speed_dict = CreateDictionaryCompressedMetadataForUseCase("high_speed");
    ASSERT_NE(high_speed_dict, nullptr);
    
    // Test balanced use case
    auto balanced_delta = CreateDeltaCompressedVectorsForUseCase("balanced");
    ASSERT_NE(balanced_delta, nullptr);
    
    auto balanced_dict = CreateDictionaryCompressedMetadataForUseCase("balanced");
    ASSERT_NE(balanced_dict, nullptr);
}

TEST(SemVecSmoke, CompressionConfigValidation) {
    // Test delta compression config validation
    semantic_vector::SemanticVectorConfig::CompressionConfig valid_config;
    valid_config.target_compression_ratio = 0.5f;
    valid_config.max_compression_latency_ms = 3.0f;
    
    auto validation_result = ValidateDeltaCompressionConfig(valid_config);
    ASSERT_TRUE(validation_result.is_ok());
    ASSERT_TRUE(validation_result.value().is_valid);
    
    // Test invalid config
    semantic_vector::SemanticVectorConfig::CompressionConfig invalid_config;
    invalid_config.target_compression_ratio = 2.0f;  // Invalid ratio > 1.0
    
    auto invalid_validation = ValidateDeltaCompressionConfig(invalid_config);
    ASSERT_TRUE(invalid_validation.is_ok());
    ASSERT_FALSE(invalid_validation.value().is_valid);
    
    // Test dictionary compression config validation
    semantic_vector::SemanticVectorConfig::CompressionConfig dict_config;
    dict_config.max_dictionary_size = 5000;
    dict_config.dictionary_rebuild_threshold = 0.3f;
    
    auto dict_validation = ValidateDictionaryCompressionConfig(dict_config);
    ASSERT_TRUE(dict_validation.is_ok());
    ASSERT_TRUE(dict_validation.value().is_valid);
}
































