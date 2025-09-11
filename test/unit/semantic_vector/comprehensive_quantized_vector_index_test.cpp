#include <gtest/gtest.h>
#include "tsdb/storage/semantic_vector/quantized_vector_index.h"
#include "tsdb/core/semantic_vector_config.h"
#include "tsdb/core/semantic_vector_types.h"

#include <random>
#include <chrono>
#include <algorithm>
#include <numeric>

using namespace tsdb::storage::semantic_vector;
namespace core = ::tsdb::core;

/**
 * @brief Comprehensive Unit Tests for Quantized Vector Index
 * 
 * These tests expand the basic smoke tests into comprehensive validation
 * covering all aspects of the quantized vector index implementation:
 * - Memory optimization validation
 * - Performance characteristics testing
 * - Error handling verification
 * - Component integration validation
 * 
 * Following established ground rules:
 * - Uses SemVecUnit test prefix for comprehensive unit tests
 * - Gated by TSDB_SEMVEC compilation flag
 * - Comprehensive coverage of all public methods
 * - Performance contract validation
 */

#ifdef TSDB_SEMVEC

// ============================================================================
// TEST FIXTURES AND UTILITIES
// ============================================================================

class SemVecUnitQuantizedVectorIndexTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up various configurations for testing
        basic_config_.default_vector_dimension = 128;
        basic_config_.default_index_type = core::semantic_vector::VectorIndex::IndexType::HNSW;
        basic_config_.enable_quantization = true;
        basic_config_.quantization_bits = 8;
        basic_config_.enable_caching = true;
        basic_config_.cache_size_mb = 64;
        
        high_performance_config_ = basic_config_;
        high_performance_config_.default_index_type = core::semantic_vector::VectorIndex::IndexType::IVF;
        high_performance_config_.enable_parallel_search = true;
        high_performance_config_.max_threads = 4;
        
        memory_efficient_config_ = basic_config_;
        memory_efficient_config_.quantization_bits = 4;
        memory_efficient_config_.enable_compression = true;
        memory_efficient_config_.cache_size_mb = 16;
    }
    
    std::vector<core::Vector> create_test_vectors(size_t count, size_t dimensions = 128) {
        std::vector<core::Vector> vectors;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<float> dist(0.0f, 1.0f);
        
        vectors.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            core::Vector vector(dimensions);
            for (size_t j = 0; j < dimensions; ++j) {
                vector.data[j] = dist(gen);
            }
            
            // Normalize vector
            float norm = 0.0f;
            for (float val : vector.data) {
                norm += val * val;
            }
            norm = std::sqrt(norm);
            
            if (norm > 0.0f) {
                for (float& val : vector.data) {
                    val /= norm;
                }
            }
            
            vectors.push_back(std::move(vector));
        }
        
        return vectors;
    }

protected:
    core::semantic_vector::SemanticVectorConfig::VectorConfig basic_config_;
    core::semantic_vector::SemanticVectorConfig::VectorConfig high_performance_config_;
    core::semantic_vector::SemanticVectorConfig::VectorConfig memory_efficient_config_;
};

// ============================================================================
// COMPREHENSIVE UNIT TESTS
// ============================================================================

TEST_F(SemVecUnitQuantizedVectorIndexTest, VectorAdditionAndRetrieval) {
    auto index = CreateVectorIndex(basic_config_);
    ASSERT_NE(index, nullptr);
    
    auto test_vectors = create_test_vectors(100, 128);
    
    // Test adding vectors
    for (size_t i = 0; i < test_vectors.size(); ++i) {
        auto result = index->add_vector(i, test_vectors[i]);
        ASSERT_TRUE(result.ok()) << "Failed to add vector " << i << ": " << result.error().message();
    }
    
    // Test retrieving vectors
    for (size_t i = 0; i < test_vectors.size(); ++i) {
        auto result = index->get_vector(i);
        ASSERT_TRUE(result.ok()) << "Failed to retrieve vector " << i << ": " << result.error().message();
        
        auto retrieved_vector = result.value();
        EXPECT_EQ(retrieved_vector.size(), test_vectors[i].size());
        
        // Verify vector content (allowing for quantization error)
        float similarity = 0.0f;
        for (size_t j = 0; j < retrieved_vector.size(); ++j) {
            similarity += test_vectors[i].data[j] * retrieved_vector.data[j];
        }
        EXPECT_GT(similarity, 0.8f) << "Quantized vector should maintain high similarity to original";
    }
}

TEST_F(SemVecUnitQuantizedVectorIndexTest, SimilaritySearchAccuracy) {
    auto index = CreateVectorIndex(basic_config_);
    ASSERT_NE(index, nullptr);
    
    auto test_vectors = create_test_vectors(200, 128);
    
    // Add vectors to index
    for (size_t i = 0; i < test_vectors.size(); ++i) {
        auto result = index->add_vector(i, test_vectors[i]);
        ASSERT_TRUE(result.ok()) << "Failed to add vector " << i;
    }
    
    // Test similarity search accuracy
    for (size_t query_idx = 0; query_idx < 10; ++query_idx) {
        const auto& query_vector = test_vectors[query_idx];
        
        auto search_result = index->search_similar(query_vector, 10, 0.1f);
        ASSERT_TRUE(search_result.ok()) << "Search failed for query " << query_idx;
        
        auto results = search_result.value();
        EXPECT_GT(results.size(), 0) << "Should return similar vectors";
        EXPECT_LE(results.size(), 10) << "Should respect max_results limit";
        
        // First result should be the query vector itself (or very similar)
        if (!results.empty()) {
            EXPECT_GE(results[0].second, 0.9f) << "First result should be highly similar";
            
            // Results should be sorted by similarity (descending)
            for (size_t i = 1; i < results.size(); ++i) {
                EXPECT_GE(results[i-1].second, results[i].second) 
                    << "Results should be sorted by similarity";
            }
            
            // All results should meet similarity threshold
            for (const auto& result : results) {
                EXPECT_GE(result.second, 0.1f) << "All results should meet similarity threshold";
                EXPECT_LE(result.second, 1.0f) << "Similarity should be normalized";
            }
        }
    }
}

TEST_F(SemVecUnitQuantizedVectorIndexTest, QuantizationMemoryOptimization) {
    // Test memory optimization through quantization
    
    // Create index without quantization
    auto unquantized_config = basic_config_;
    unquantized_config.enable_quantization = false;
    auto unquantized_index = CreateVectorIndex(unquantized_config);
    
    // Create index with quantization
    auto quantized_config = basic_config_;
    quantized_config.enable_quantization = true;
    quantized_config.quantization_bits = 4; // Aggressive quantization
    auto quantized_index = CreateVectorIndex(quantized_config);
    
    auto test_vectors = create_test_vectors(1000, 256);
    
    // Add vectors to both indices
    for (size_t i = 0; i < test_vectors.size(); ++i) {
        auto unquantized_result = unquantized_index->add_vector(i, test_vectors[i]);
        auto quantized_result = quantized_index->add_vector(i, test_vectors[i]);
        
        ASSERT_TRUE(unquantized_result.ok()) << "Unquantized add failed for vector " << i;
        ASSERT_TRUE(quantized_result.ok()) << "Quantized add failed for vector " << i;
    }
    
    // Get memory usage (simplified - in real implementation would use actual memory metrics)
    auto unquantized_memory = unquantized_index->get_memory_usage();
    auto quantized_memory = quantized_index->get_memory_usage();
    
    ASSERT_TRUE(unquantized_memory.ok()) << "Failed to get unquantized memory usage";
    ASSERT_TRUE(quantized_memory.ok()) << "Failed to get quantized memory usage";
    
    // Quantized index should use significantly less memory
    double memory_reduction = 1.0 - (double)quantized_memory.value() / unquantized_memory.value();
    EXPECT_GT(memory_reduction, 0.3) << "Quantization should reduce memory by >30%, got " << (memory_reduction * 100) << "%";
    
    std::cout << "Memory reduction through quantization: " << (memory_reduction * 100) << "%\n";
    std::cout << "Unquantized memory: " << unquantized_memory.value() << " bytes\n";
    std::cout << "Quantized memory: " << quantized_memory.value() << " bytes\n";
}

TEST_F(SemVecUnitQuantizedVectorIndexTest, PerformanceCharacteristics) {
    auto index = CreateVectorIndex(high_performance_config_);
    ASSERT_NE(index, nullptr);
    
    const size_t num_vectors = 5000;
    const size_t num_queries = 100;
    
    auto test_vectors = create_test_vectors(num_vectors, 128);
    
    // Measure indexing performance
    auto index_start = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < test_vectors.size(); ++i) {
        auto result = index->add_vector(i, test_vectors[i]);
        ASSERT_TRUE(result.ok()) << "Failed to add vector " << i;
    }
    
    auto index_end = std::chrono::high_resolution_clock::now();
    auto index_time = std::chrono::duration_cast<std::chrono::milliseconds>(index_end - index_start);
    
    double indexing_rate = (double)num_vectors / (index_time.count() / 1000.0);
    std::cout << "Indexing performance: " << indexing_rate << " vectors/second\n";
    
    EXPECT_GT(indexing_rate, 100.0) << "Indexing should be >100 vectors/second";
    
    // Measure search performance
    std::vector<double> search_times;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dist(0, test_vectors.size() - 1);
    
    for (size_t i = 0; i < num_queries; ++i) {
        size_t query_idx = dist(gen);
        
        auto search_start = std::chrono::high_resolution_clock::now();
        auto search_result = index->search_similar(test_vectors[query_idx], 10, 0.1f);
        auto search_end = std::chrono::high_resolution_clock::now();
        
        ASSERT_TRUE(search_result.ok()) << "Search failed for query " << i;
        
        auto search_time = std::chrono::duration_cast<std::chrono::microseconds>(search_end - search_start);
        search_times.push_back(search_time.count() / 1000.0); // Convert to milliseconds
    }
    
    // Calculate search performance statistics
    std::sort(search_times.begin(), search_times.end());
    double avg_search_time = std::accumulate(search_times.begin(), search_times.end(), 0.0) / search_times.size();
    double p95_search_time = search_times[search_times.size() * 0.95];
    
    std::cout << "Search performance:\n";
    std::cout << "  Average: " << avg_search_time << " ms\n";
    std::cout << "  P95: " << p95_search_time << " ms\n";
    
    // Performance requirements from TASK-21
    EXPECT_LT(avg_search_time, 1.0) << "Average search time should be <1ms";
    EXPECT_LT(p95_search_time, 5.0) << "P95 search time should be <5ms";
}

TEST_F(SemVecUnitQuantizedVectorIndexTest, ErrorHandlingAndEdgeCases) {
    auto index = CreateVectorIndex(basic_config_);
    ASSERT_NE(index, nullptr);
    
    // Test empty vector handling
    {
        core::Vector empty_vector(0);
        auto result = index->add_vector(0, empty_vector);
        EXPECT_FALSE(result.ok()) << "Should reject empty vectors";
    }
    
    // Test dimension mismatch
    {
        core::Vector wrong_dim_vector(64); // Different from config dimension (128)
        std::fill(wrong_dim_vector.data.begin(), wrong_dim_vector.data.end(), 1.0f);
        
        auto result = index->add_vector(1, wrong_dim_vector);
        // This might succeed with automatic handling or fail - either is acceptable
        if (!result.ok()) {
            std::cout << "Dimension mismatch handled: " << result.error().message() << "\n";
        }
    }
    
    // Test invalid vector ID operations
    {
        auto get_result = index->get_vector(999999);
        EXPECT_FALSE(get_result.ok()) << "Should fail for non-existent vector ID";
        
        auto update_result = index->update_vector(999999, core::Vector(128, 1.0f));
        EXPECT_FALSE(update_result.ok()) << "Should fail to update non-existent vector";
        
        auto remove_result = index->remove_vector(999999);
        EXPECT_FALSE(remove_result.ok()) << "Should fail to remove non-existent vector";
    }
    
    // Test extreme similarity thresholds
    {
        core::Vector test_vector(128, 1.0f);
        index->add_vector(2, test_vector);
        
        // Threshold > 1.0 (invalid)
        auto invalid_result = index->search_similar(test_vector, 10, 2.0f);
        EXPECT_FALSE(invalid_result.ok()) << "Should reject invalid similarity threshold";
        
        // Threshold < 0.0 (invalid)
        auto negative_result = index->search_similar(test_vector, 10, -0.5f);
        EXPECT_FALSE(negative_result.ok()) << "Should reject negative similarity threshold";
        
        // Zero max results
        auto zero_result = index->search_similar(test_vector, 0, 0.5f);
        if (zero_result.ok()) {
            EXPECT_EQ(zero_result.value().size(), 0) << "Zero max results should return empty";
        }
    }
    
    // Test extreme vector values
    {
        core::Vector extreme_vector(128);
        
        // Test with infinity values
        std::fill(extreme_vector.data.begin(), extreme_vector.data.end(), std::numeric_limits<float>::infinity());
        auto inf_result = index->add_vector(3, extreme_vector);
        EXPECT_FALSE(inf_result.ok()) << "Should reject vectors with infinity values";
        
        // Test with NaN values
        std::fill(extreme_vector.data.begin(), extreme_vector.data.end(), std::numeric_limits<float>::quiet_NaN());
        auto nan_result = index->add_vector(4, extreme_vector);
        EXPECT_FALSE(nan_result.ok()) << "Should reject vectors with NaN values";
    }
}

#else // TSDB_SEMVEC not defined

// Placeholder test when semantic vector features are disabled
TEST(SemVecUnit, SemanticVectorFeaturesDisabled) {
    GTEST_SKIP() << "Semantic vector features are disabled (TSDB_SEMVEC not defined)";
}

#endif // TSDB_SEMVEC
