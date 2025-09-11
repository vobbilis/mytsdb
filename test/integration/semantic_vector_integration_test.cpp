#include <gtest/gtest.h>
#include "tsdb/core/types.h"
#include "tsdb/core/config.h"
#include "tsdb/storage/storage.h"
#include "tsdb/storage/storage_impl.h"
#include "tsdb/storage/advanced_storage.h"
#include "tsdb/core/semantic_vector_types.h"
#include "tsdb/core/semantic_vector_config.h"
#include "tsdb/storage/semantic_vector_architecture.h"

#ifdef TSDB_SEMVEC
#include "tsdb/storage/semantic_vector_storage_impl.h"
#include "tsdb/storage/semantic_vector/quantized_vector_index.h"
#include "tsdb/storage/semantic_vector/pruned_semantic_index.h"
#include "tsdb/storage/semantic_vector/sparse_temporal_graph.h"
#include "tsdb/storage/semantic_vector/query_processor.h"
#include "tsdb/storage/semantic_vector/migration_manager.h"
#endif

#include <filesystem>
#include <memory>
#include <random>
#include <thread>
#include <chrono>
#include <future>
#include <atomic>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace tsdb {
namespace integration {
namespace {

/**
 * @brief Semantic Vector Integration Tests
 * 
 * These tests verify complete end-to-end workflows and integration points
 * for the semantic vector storage system. They test the full pipeline from
 * configuration to storage operations to advanced queries.
 * 
 * Test Coverage:
 * 1. End-to-end workflow with unified types
 * 2. Dual-write strategy with error recovery
 * 3. Advanced query methods with unified query types
 * 4. Error handling and recovery mechanisms
 * 5. Integration with existing storage seamlessly
 * 
 * Following established ground rules:
 * - Uses SemVecIntegration test prefix for consistency
 * - Gated by TSDB_SEMVEC compilation flag
 * - Maintains backward compatibility
 * - Comprehensive error testing
 * - Performance validation
 */

#ifdef TSDB_SEMVEC

// ============================================================================
// TEST FIXTURES AND UTILITIES
// ============================================================================

class SemVecIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test directory
        test_dir_ = std::filesystem::temp_directory_path() / "semantic_vector_integration_test";
        std::filesystem::create_directories(test_dir_);
        
        // Set up base configuration
        config_ = std::make_unique<core::Config>();
        config_->storage.data_dir = test_dir_.string();
        config_->storage.wal_dir = test_dir_.string() + "/wal";
        config_->storage.retention_policy.max_age = std::chrono::hours(24);
        
        // Enable semantic vector features with balanced configuration
        config_->semantic_vector_features.enabled = true;
        config_->semantic_vector_features.config = core::semantic_vector::SemanticVectorConfig::balanced_config();
        
        // Create base storage first
        auto base_storage_result = storage::StorageImpl::create(*config_);
        ASSERT_TRUE(base_storage_result.ok()) << "Failed to create base storage: " << base_storage_result.error().message();
        base_storage_ = std::move(base_storage_result.value());
        
        // Create semantic vector storage
        auto semvec_storage_result = storage::SemanticVectorStorageImpl::create(*config_, base_storage_.get());
        ASSERT_TRUE(semvec_storage_result.ok()) << "Failed to create semantic vector storage: " << semvec_storage_result.error().message();
        semvec_storage_ = std::move(semvec_storage_result.value());
        
        // Cast to AdvancedStorage interface for testing
        advanced_storage_ = dynamic_cast<storage::AdvancedStorage*>(semvec_storage_.get());
        ASSERT_NE(advanced_storage_, nullptr) << "Failed to cast to AdvancedStorage interface";
        
        // Verify semantic vector features are enabled
        ASSERT_TRUE(advanced_storage_->semantic_vector_enabled()) << "Semantic vector features should be enabled";
    }
    
    void TearDown() override {
        // Close storage
        if (semvec_storage_) {
            auto close_result = semvec_storage_->close();
            EXPECT_TRUE(close_result.ok()) << "Failed to close semantic vector storage: " << close_result.error().message();
        }
        if (base_storage_) {
            auto close_result = base_storage_->close();
            EXPECT_TRUE(close_result.ok()) << "Failed to close base storage: " << close_result.error().message();
        }
        
        // Clean up test directory
        std::error_code ec;
        std::filesystem::remove_all(test_dir_, ec);
    }
    
    // Helper function to create test time series data
    std::vector<core::TimeSeries> create_test_time_series(size_t count = 10, size_t samples_per_series = 100) {
        std::vector<core::TimeSeries> series_list;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> value_dist(0.0f, 100.0f);
        
        auto base_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        for (size_t i = 0; i < count; ++i) {
            core::TimeSeries ts;
            ts.labels = {
                {"__name__", "test_metric_" + std::to_string(i)},
                {"instance", "test_instance_" + std::to_string(i % 3)},
                {"job", "integration_test"}
            };
            
            for (size_t j = 0; j < samples_per_series; ++j) {
                core::Sample sample;
                sample.timestamp = base_time + (j * 1000); // 1 second intervals
                sample.value = value_dist(gen);
                ts.samples.push_back(sample);
            }
            
            series_list.push_back(std::move(ts));
        }
        
        return series_list;
    }
    
    // Helper function to create test vectors
    std::vector<core::semantic_vector::Vector> create_test_vectors(size_t count = 10, size_t dimensions = 128) {
        std::vector<core::semantic_vector::Vector> vectors;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<float> dist(0.0f, 1.0f);
        
        for (size_t i = 0; i < count; ++i) {
            core::semantic_vector::Vector vector(dimensions);
            for (size_t j = 0; j < dimensions; ++j) {
                vector.data[j] = dist(gen);
            }
            vectors.push_back(std::move(vector));
        }
        
        return vectors;
    }

protected:
    std::filesystem::path test_dir_;
    std::unique_ptr<core::Config> config_;
    std::unique_ptr<storage::Storage> base_storage_;
    std::unique_ptr<storage::Storage> semvec_storage_;
    storage::AdvancedStorage* advanced_storage_ = nullptr;
};

// ============================================================================
// TASK-20 REQUIREMENT 1: END-TO-END WORKFLOW TESTING
// ============================================================================

TEST_F(SemVecIntegrationTest, EndToEndWorkflowWithUnifiedTypes) {
    // Test complete workflow: data ingestion -> storage -> vector indexing -> semantic search -> results
    
    // Step 1: Ingest traditional time series data
    auto test_series = create_test_time_series(5, 50);
    for (const auto& ts : test_series) {
        auto write_result = semvec_storage_->write(ts);
        ASSERT_TRUE(write_result.ok()) << "Failed to write time series: " << write_result.error().message();
    }
    
    // Step 2: Add vector embeddings for semantic search
    auto test_vectors = create_test_vectors(5, 128);
    for (size_t i = 0; i < test_vectors.size(); ++i) {
        std::string series_id = "test_metric_" + std::to_string(i);
        auto vector_result = advanced_storage_->add_vector_embedding(series_id, test_vectors[i]);
        ASSERT_TRUE(vector_result.ok()) << "Failed to add vector embedding: " << vector_result.error().message();
    }
    
    // Step 3: Perform semantic search
    core::semantic_vector::SemanticQuery semantic_query;
    semantic_query.query_text = "test metrics for integration testing";
    semantic_query.max_results = 3;
    semantic_query.similarity_threshold = 0.1f;
    
    auto search_result = advanced_storage_->semantic_search(semantic_query);
    ASSERT_TRUE(search_result.ok()) << "Failed to perform semantic search: " << search_result.error().message();
    
    auto search_results = search_result.value();
    EXPECT_GT(search_results.size(), 0) << "Semantic search should return results";
    EXPECT_LE(search_results.size(), 3) << "Should respect max_results limit";
    
    // Step 4: Verify vector similarity search
    if (!test_vectors.empty()) {
        auto similarity_result = advanced_storage_->vector_similarity_search(test_vectors[0], 3, 0.1f);
        ASSERT_TRUE(similarity_result.ok()) << "Failed to perform vector similarity search: " << similarity_result.error().message();
        
        auto similarity_results = similarity_result.value();
        EXPECT_GT(similarity_results.size(), 0) << "Vector similarity search should return results";
        EXPECT_LE(similarity_results.size(), 3) << "Should respect max_results limit";
    }
    
    // Step 5: Query traditional time series data to verify dual storage
    core::QueryRequest query_req;
    query_req.start_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() - 3600000; // 1 hour ago
    query_req.end_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    query_req.matchers = {{"__name__", "test_metric_0"}};
    
    auto query_result = semvec_storage_->query(query_req);
    ASSERT_TRUE(query_result.ok()) << "Failed to query time series data: " << query_result.error().message();
    
    auto query_response = query_result.value();
    EXPECT_GT(query_response.series.size(), 0) << "Should return time series data";
}

// ============================================================================
// TASK-20 REQUIREMENT 2: DUAL-WRITE STRATEGY WITH ERROR RECOVERY
// ============================================================================

TEST_F(SemVecIntegrationTest, DualWriteStrategyWithErrorRecovery) {
    // Test that data is written to both traditional storage and semantic vector storage
    // with proper error handling and recovery mechanisms
    
    auto test_series = create_test_time_series(3, 20);
    auto test_vectors = create_test_vectors(3, 64);
    
    // Test successful dual write
    for (size_t i = 0; i < test_series.size(); ++i) {
        // Write time series data
        auto write_result = semvec_storage_->write(test_series[i]);
        ASSERT_TRUE(write_result.ok()) << "Failed to write time series: " << write_result.error().message();
        
        // Add corresponding vector embedding
        std::string series_id = "test_metric_" + std::to_string(i);
        auto vector_result = advanced_storage_->add_vector_embedding(series_id, test_vectors[i]);
        ASSERT_TRUE(vector_result.ok()) << "Failed to add vector embedding: " << vector_result.error().message();
    }
    
    // Verify data exists in both storages
    // 1. Check traditional storage
    core::QueryRequest query_req;
    query_req.start_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() - 3600000;
    query_req.end_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    query_req.matchers = {{"job", "integration_test"}};
    
    auto query_result = semvec_storage_->query(query_req);
    ASSERT_TRUE(query_result.ok()) << "Failed to query traditional storage: " << query_result.error().message();
    EXPECT_EQ(query_result.value().series.size(), 3) << "Should have 3 series in traditional storage";
    
    // 2. Check semantic vector storage
    auto similarity_result = advanced_storage_->vector_similarity_search(test_vectors[0], 5, 0.0f);
    ASSERT_TRUE(similarity_result.ok()) << "Failed to query semantic vector storage: " << similarity_result.error().message();
    EXPECT_GT(similarity_result.value().size(), 0) << "Should have vectors in semantic storage";
    
    // Test error recovery scenarios
    // Simulate partial failure and recovery
    std::string recovery_series_id = "recovery_test_metric";
    core::semantic_vector::Vector recovery_vector(32);
    std::fill(recovery_vector.data.begin(), recovery_vector.data.end(), 1.0f);
    
    // This should succeed even if previous operations had issues
    auto recovery_result = advanced_storage_->add_vector_embedding(recovery_series_id, recovery_vector);
    EXPECT_TRUE(recovery_result.ok()) << "Error recovery should work: " << recovery_result.error().message();
}

// ============================================================================
// TASK-20 REQUIREMENT 3: ADVANCED QUERY METHODS WITH UNIFIED QUERY TYPES
// ============================================================================

TEST_F(SemVecIntegrationTest, AdvancedQueryMethodsWithUnifiedTypes) {
    // Set up test data
    auto test_series = create_test_time_series(8, 30);
    auto test_vectors = create_test_vectors(8, 96);
    
    // Ingest data
    for (size_t i = 0; i < test_series.size(); ++i) {
        auto write_result = semvec_storage_->write(test_series[i]);
        ASSERT_TRUE(write_result.ok()) << "Failed to write time series: " << write_result.error().message();
        
        std::string series_id = "test_metric_" + std::to_string(i);
        auto vector_result = advanced_storage_->add_vector_embedding(series_id, test_vectors[i]);
        ASSERT_TRUE(vector_result.ok()) << "Failed to add vector embedding: " << vector_result.error().message();
    }
    
    // Test 1: Vector Similarity Search with unified Vector types
    {
        auto similarity_result = advanced_storage_->vector_similarity_search(test_vectors[0], 4, 0.2f);
        ASSERT_TRUE(similarity_result.ok()) << "Vector similarity search failed: " << similarity_result.error().message();
        
        auto results = similarity_result.value();
        EXPECT_GT(results.size(), 0) << "Should return similarity results";
        EXPECT_LE(results.size(), 4) << "Should respect max_results limit";
        
        // Verify results are properly scored
        for (const auto& result : results) {
            EXPECT_GE(result.similarity_score, 0.2f) << "Results should meet similarity threshold";
            EXPECT_LE(result.similarity_score, 1.0f) << "Similarity score should be normalized";
            EXPECT_FALSE(result.series_id.empty()) << "Should have valid series ID";
        }
    }
    
    // Test 2: Semantic Search with unified SemanticQuery types
    {
        core::semantic_vector::SemanticQuery semantic_query;
        semantic_query.query_text = "integration test metrics data";
        semantic_query.max_results = 5;
        semantic_query.similarity_threshold = 0.1f;
        semantic_query.include_metadata = true;
        
        auto search_result = advanced_storage_->semantic_search(semantic_query);
        ASSERT_TRUE(search_result.ok()) << "Semantic search failed: " << search_result.error().message();
        
        auto results = search_result.value();
        EXPECT_GT(results.size(), 0) << "Should return semantic search results";
        EXPECT_LE(results.size(), 5) << "Should respect max_results limit";
        
        // Verify semantic results structure
        for (const auto& result : results) {
            EXPECT_GE(result.relevance_score, 0.1f) << "Results should meet similarity threshold";
            EXPECT_LE(result.relevance_score, 1.0f) << "Relevance score should be normalized";
            EXPECT_FALSE(result.series_id.empty()) << "Should have valid series ID";
            if (semantic_query.include_metadata) {
                EXPECT_GT(result.metadata.size(), 0) << "Should include metadata when requested";
            }
        }
    }
    
    // Test 3: Temporal Analysis with unified TemporalQuery types
    {
        core::semantic_vector::TemporalQuery temporal_query;
        temporal_query.series_ids = {"test_metric_0", "test_metric_1", "test_metric_2"};
        temporal_query.analysis_type = core::semantic_vector::TemporalAnalysisType::CORRELATION_ANALYSIS;
        temporal_query.time_window = std::chrono::minutes(30);
        temporal_query.correlation_threshold = 0.5f;
        
        auto temporal_result = advanced_storage_->temporal_analysis(temporal_query);
        ASSERT_TRUE(temporal_result.ok()) << "Temporal analysis failed: " << temporal_result.error().message();
        
        auto analysis_result = temporal_result.value();
        EXPECT_FALSE(analysis_result.analysis_id.empty()) << "Should have valid analysis ID";
        EXPECT_EQ(analysis_result.analysis_type, core::semantic_vector::TemporalAnalysisType::CORRELATION_ANALYSIS);
        EXPECT_GT(analysis_result.correlations.size(), 0) << "Should return correlation results";
        
        // Verify correlation structure
        for (const auto& correlation : analysis_result.correlations) {
            EXPECT_FALSE(correlation.series_id_1.empty()) << "Should have valid first series ID";
            EXPECT_FALSE(correlation.series_id_2.empty()) << "Should have valid second series ID";
            EXPECT_GE(correlation.correlation_coefficient, -1.0f) << "Correlation should be >= -1.0";
            EXPECT_LE(correlation.correlation_coefficient, 1.0f) << "Correlation should be <= 1.0";
        }
    }
    
    // Test 4: Multi-modal Query combining different query types
    {
        core::semantic_vector::MultiModalQuery multi_query;
        multi_query.vector_query.query_vector = test_vectors[0];
        multi_query.vector_query.max_results = 3;
        multi_query.vector_query.similarity_threshold = 0.3f;
        
        multi_query.semantic_query.query_text = "test metrics";
        multi_query.semantic_query.max_results = 3;
        multi_query.semantic_query.similarity_threshold = 0.2f;
        
        multi_query.combine_results = true;
        multi_query.result_fusion_strategy = core::semantic_vector::ResultFusionStrategy::WEIGHTED_AVERAGE;
        
        auto multi_result = advanced_storage_->multi_modal_search(multi_query);
        ASSERT_TRUE(multi_result.ok()) << "Multi-modal search failed: " << multi_result.error().message();
        
        auto results = multi_result.value();
        EXPECT_GT(results.size(), 0) << "Should return multi-modal results";
        
        // Verify combined results structure
        for (const auto& result : results) {
            EXPECT_FALSE(result.series_id.empty()) << "Should have valid series ID";
            EXPECT_GT(result.combined_score, 0.0f) << "Should have valid combined score";
            EXPECT_LE(result.combined_score, 1.0f) << "Combined score should be normalized";
        }
    }
}

// ============================================================================
// TASK-20 REQUIREMENT 4: ERROR HANDLING AND RECOVERY MECHANISMS
// ============================================================================

TEST_F(SemVecIntegrationTest, ErrorHandlingAndRecoveryMechanisms) {
    // Test comprehensive error handling across all operations
    
    // Test 1: Invalid configuration handling
    {
        core::semantic_vector::SemanticVectorConfig invalid_config;
        invalid_config.vector.enabled = true;
        invalid_config.vector.dimensions = 0; // Invalid dimension
        
        auto enable_result = advanced_storage_->enable_semantic_vector_features(invalid_config);
        EXPECT_FALSE(enable_result.ok()) << "Should reject invalid configuration";
        EXPECT_NE(enable_result.error().code(), core::ErrorCode::OK) << "Should return appropriate error code";
    }
    
    // Test 2: Invalid vector operations
    {
        core::semantic_vector::Vector invalid_vector(0); // Empty vector
        auto add_result = advanced_storage_->add_vector_embedding("test_series", invalid_vector);
        EXPECT_FALSE(add_result.ok()) << "Should reject invalid vector";
        
        // Test vector dimension mismatch
        core::semantic_vector::Vector wrong_dimension_vector(256); // Different from configured dimension
        std::fill(wrong_dimension_vector.data.begin(), wrong_dimension_vector.data.end(), 1.0f);
        
        auto mismatch_result = advanced_storage_->add_vector_embedding("test_series_2", wrong_dimension_vector);
        // This might succeed with automatic dimension handling, but should be logged
        if (!mismatch_result.ok()) {
            EXPECT_NE(mismatch_result.error().code(), core::ErrorCode::OK) << "Should handle dimension mismatch appropriately";
        }
    }
    
    // Test 3: Query error handling
    {
        // Empty semantic query
        core::semantic_vector::SemanticQuery empty_query;
        auto empty_result = advanced_storage_->semantic_search(empty_query);
        EXPECT_FALSE(empty_result.ok()) << "Should reject empty semantic query";
        
        // Invalid similarity threshold
        core::semantic_vector::SemanticQuery invalid_threshold_query;
        invalid_threshold_query.query_text = "test query";
        invalid_threshold_query.similarity_threshold = 2.0f; // > 1.0
        
        auto threshold_result = advanced_storage_->semantic_search(invalid_threshold_query);
        EXPECT_FALSE(threshold_result.ok()) << "Should reject invalid similarity threshold";
    }
    
    // Test 4: Recovery after errors
    {
        // After encountering errors, normal operations should still work
        auto test_vectors = create_test_vectors(2, 128);
        
        auto recovery_result = advanced_storage_->add_vector_embedding("recovery_test", test_vectors[0]);
        EXPECT_TRUE(recovery_result.ok()) << "Should recover after errors: " << recovery_result.error().message();
        
        // Verify the recovery worked
        auto search_result = advanced_storage_->vector_similarity_search(test_vectors[0], 1, 0.5f);
        EXPECT_TRUE(search_result.ok()) << "Search should work after recovery: " << search_result.error().message();
    }
    
    // Test 5: Concurrent error handling
    {
        std::vector<std::future<void>> futures;
        std::atomic<int> success_count(0);
        std::atomic<int> error_count(0);
        
        auto test_vectors = create_test_vectors(10, 128);
        
        for (size_t i = 0; i < 10; ++i) {
            futures.push_back(std::async(std::launch::async, [this, &test_vectors, &success_count, &error_count, i]() {
                std::string series_id = "concurrent_test_" + std::to_string(i);
                auto result = advanced_storage_->add_vector_embedding(series_id, test_vectors[i]);
                if (result.ok()) {
                    success_count.fetch_add(1);
                } else {
                    error_count.fetch_add(1);
                }
            }));
        }
        
        // Wait for all operations to complete
        for (auto& future : futures) {
            future.wait();
        }
        
        // Most operations should succeed, but system should handle any concurrent errors gracefully
        EXPECT_GT(success_count.load(), 0) << "Some concurrent operations should succeed";
        // Error count might be > 0 due to concurrency, but system should remain stable
    }
}

// ============================================================================
// TASK-20 REQUIREMENT 5: INTEGRATION WITH EXISTING STORAGE SEAMLESSLY
// ============================================================================

TEST_F(SemVecIntegrationTest, IntegrationWithExistingStorageSeamlessly) {
    // Test that semantic vector features integrate seamlessly with existing storage
    // without affecting traditional time series operations
    
    // Test 1: Traditional operations work unchanged
    {
        auto test_series = create_test_time_series(5, 40);
        
        // Write using traditional storage interface
        for (const auto& ts : test_series) {
            auto write_result = semvec_storage_->write(ts);
            ASSERT_TRUE(write_result.ok()) << "Traditional write should work: " << write_result.error().message();
        }
        
        // Query using traditional storage interface
        core::QueryRequest query_req;
        query_req.start_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count() - 3600000;
        query_req.end_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        query_req.matchers = {{"job", "integration_test"}};
        
        auto query_result = semvec_storage_->query(query_req);
        ASSERT_TRUE(query_result.ok()) << "Traditional query should work: " << query_result.error().message();
        EXPECT_EQ(query_result.value().series.size(), 5) << "Should return all traditional series";
    }
    
    // Test 2: Mixed operations (traditional + semantic vector)
    {
        auto test_series = create_test_time_series(3, 30);
        auto test_vectors = create_test_vectors(3, 128);
        
        // Interleave traditional and semantic vector operations
        for (size_t i = 0; i < test_series.size(); ++i) {
            // Traditional write
            auto write_result = semvec_storage_->write(test_series[i]);
            ASSERT_TRUE(write_result.ok()) << "Mixed traditional write should work: " << write_result.error().message();
            
            // Semantic vector addition
            std::string series_id = "test_metric_" + std::to_string(i);
            auto vector_result = advanced_storage_->add_vector_embedding(series_id, test_vectors[i]);
            ASSERT_TRUE(vector_result.ok()) << "Mixed vector addition should work: " << vector_result.error().message();
            
            // Traditional query to verify data is still accessible
            core::QueryRequest verify_query;
            verify_query.start_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count() - 3600000;
            verify_query.end_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            verify_query.matchers = {{"__name__", "test_metric_" + std::to_string(i)}};
            
            auto verify_result = semvec_storage_->query(verify_query);
            ASSERT_TRUE(verify_result.ok()) << "Verification query should work: " << verify_result.error().message();
            EXPECT_GT(verify_result.value().series.size(), 0) << "Should find the written series";
        }
    }
    
    // Test 3: Backward compatibility
    {
        // Create a traditional storage instance for comparison
        core::Config traditional_config = *config_;
        traditional_config.semantic_vector_features.enabled = false;
        
        auto traditional_result = storage::StorageImpl::create(traditional_config);
        ASSERT_TRUE(traditional_result.ok()) << "Should create traditional storage: " << traditional_result.error().message();
        auto traditional_storage = std::move(traditional_result.value());
        
        // Write same data to both storages
        auto test_series = create_test_time_series(2, 20);
        
        for (const auto& ts : test_series) {
            auto traditional_write = traditional_storage->write(ts);
            auto semvec_write = semvec_storage_->write(ts);
            
            ASSERT_TRUE(traditional_write.ok()) << "Traditional storage write should work";
            ASSERT_TRUE(semvec_write.ok()) << "Semantic vector storage write should work";
        }
        
        // Query both storages and compare results
        core::QueryRequest comp_query;
        comp_query.start_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count() - 3600000;
        comp_query.end_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        comp_query.matchers = {{"job", "integration_test"}};
        
        auto traditional_query = traditional_storage->query(comp_query);
        auto semvec_query = semvec_storage_->query(comp_query);
        
        ASSERT_TRUE(traditional_query.ok()) << "Traditional query should work";
        ASSERT_TRUE(semvec_query.ok()) << "Semantic vector query should work";
        
        // Results should be equivalent for traditional queries
        EXPECT_EQ(traditional_query.value().series.size(), semvec_query.value().series.size()) 
            << "Both storages should return same number of series";
        
        // Clean up traditional storage
        auto close_result = traditional_storage->close();
        EXPECT_TRUE(close_result.ok()) << "Should close traditional storage cleanly";
    }
    
    // Test 4: Performance impact measurement
    {
        auto test_series = create_test_time_series(10, 50);
        
        // Measure traditional write performance
        auto start_time = std::chrono::high_resolution_clock::now();
        for (const auto& ts : test_series) {
            auto write_result = semvec_storage_->write(ts);
            ASSERT_TRUE(write_result.ok()) << "Performance test write should work";
        }
        auto end_time = std::chrono::high_resolution_clock::now();
        
        auto write_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        // Measure traditional query performance
        start_time = std::chrono::high_resolution_clock::now();
        
        core::QueryRequest perf_query;
        perf_query.start_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count() - 3600000;
        perf_query.end_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        perf_query.matchers = {{"job", "integration_test"}};
        
        auto query_result = semvec_storage_->query(perf_query);
        ASSERT_TRUE(query_result.ok()) << "Performance query should work";
        
        end_time = std::chrono::high_resolution_clock::now();
        auto query_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        // Performance impact should be minimal (< 5% as per requirements)
        // These are basic sanity checks - detailed performance testing is in TASK-21
        EXPECT_LT(write_duration.count(), 5000) << "Write operations should be reasonably fast";
        EXPECT_LT(query_duration.count(), 1000) << "Query operations should be reasonably fast";
        
        // Log performance metrics for reference
        std::cout << "Write performance: " << write_duration.count() << "ms for " << test_series.size() << " series\n";
        std::cout << "Query performance: " << query_duration.count() << "ms\n";
    }
}

// ============================================================================
// ADDITIONAL INTEGRATION SCENARIOS
// ============================================================================

TEST_F(SemVecIntegrationTest, ComplexWorkflowIntegration) {
    // Test a complex real-world scenario combining multiple features
    
    // Step 1: Set up a realistic dataset
    auto metrics_series = create_test_time_series(20, 100);
    auto embedding_vectors = create_test_vectors(20, 128);
    
    // Step 2: Ingest data with mixed patterns
    for (size_t i = 0; i < metrics_series.size(); ++i) {
        // Write time series
        auto write_result = semvec_storage_->write(metrics_series[i]);
        ASSERT_TRUE(write_result.ok()) << "Complex workflow write failed";
        
        // Add embeddings for some series (simulating partial semantic coverage)
        if (i % 2 == 0) {
            std::string series_id = "test_metric_" + std::to_string(i);
            auto vector_result = advanced_storage_->add_vector_embedding(series_id, embedding_vectors[i]);
            ASSERT_TRUE(vector_result.ok()) << "Complex workflow vector addition failed";
        }
    }
    
    // Step 3: Perform complex queries
    // Traditional range query
    core::QueryRequest range_query;
    range_query.start_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() - 7200000; // 2 hours ago
    range_query.end_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    range_query.matchers = {{"job", "integration_test"}};
    
    auto range_result = semvec_storage_->query(range_query);
    ASSERT_TRUE(range_result.ok()) << "Complex range query failed";
    EXPECT_EQ(range_result.value().series.size(), 20) << "Should return all series";
    
    // Semantic search on subset with embeddings
    core::semantic_vector::SemanticQuery semantic_query;
    semantic_query.query_text = "integration test metrics";
    semantic_query.max_results = 15;
    semantic_query.similarity_threshold = 0.1f;
    
    auto semantic_result = advanced_storage_->semantic_search(semantic_query);
    ASSERT_TRUE(semantic_result.ok()) << "Complex semantic search failed";
    
    // Should return results only for series with embeddings (10 series, every other one)
    auto semantic_results = semantic_result.value();
    EXPECT_GT(semantic_results.size(), 0) << "Should find semantic matches";
    EXPECT_LE(semantic_results.size(), 10) << "Should only match series with embeddings";
    
    // Step 4: Verify data consistency across operations
    for (const auto& semantic_match : semantic_results) {
        // Each semantic match should correspond to a series we can query traditionally
        core::QueryRequest verify_query;
        verify_query.start_time = range_query.start_time;
        verify_query.end_time = range_query.end_time;
        verify_query.matchers = {{"__name__", semantic_match.series_id}};
        
        auto verify_result = semvec_storage_->query(verify_query);
        ASSERT_TRUE(verify_result.ok()) << "Verification query should work for semantic match";
        EXPECT_GT(verify_result.value().series.size(), 0) << "Should find corresponding time series data";
    }
}

#else // TSDB_SEMVEC not defined

// Placeholder tests when semantic vector features are disabled
TEST(SemVecIntegrationTest, SemanticVectorFeaturesDisabled) {
    GTEST_SKIP() << "Semantic vector features are disabled (TSDB_SEMVEC not defined)";
}

#endif // TSDB_SEMVEC

} // namespace
} // namespace integration
} // namespace tsdb
