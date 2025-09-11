#include <gtest/gtest.h>
#include "tsdb/storage/semantic_vector/pruned_semantic_index.h"
#include "tsdb/core/semantic_vector_config.h"

using namespace tsdb::storage::semantic_vector;
namespace core = ::tsdb::core;

TEST(SemVecSmoke, SemanticIndexBasic)
{
    core::semantic_vector::SemanticVectorConfig::SemanticConfig cfg;
    cfg.default_embedding_dimension = 128;
    cfg.sparsity_threshold = 0.1f;
    cfg.enable_embedding_pruning = true;
    auto idx = CreateSemanticIndex(cfg);

    core::Vector v1(128); 
    v1.data.resize(128);
    for (size_t i = 0; i < 128; ++i) {
        v1.data[i] = static_cast<float>(i % 10) * 0.1f;
    }

    ASSERT_TRUE(idx->add_semantic_embedding(12345, v1).ok());
    
    auto retrieved = idx->get_semantic_embedding(12345);
    ASSERT_TRUE(retrieved.ok());
    ASSERT_EQ(retrieved.value().data.size(), 128);

    // Test entity operations
    std::vector<std::string> entities = {"temperature", "sensor"};
    ASSERT_TRUE(idx->add_entities(12345, entities).ok());
    
    auto entity_search = idx->search_by_entity("temperature");
    ASSERT_TRUE(entity_search.ok());

    // Test concept operations
    std::vector<std::string> concepts = {"performance", "monitoring"};
    ASSERT_TRUE(idx->add_concepts(12345, concepts).ok());
    
    auto concept_search = idx->search_by_concept("performance");
    ASSERT_TRUE(concept_search.ok());

    // Test embedding pruning
    auto pruned = idx->prune_embedding(v1);
    ASSERT_TRUE(pruned.ok());
    ASSERT_LT(pruned.value().sparse_values.size(), v1.data.size()); // Should be pruned

    auto reconstructed = idx->reconstruct_embedding(pruned.value());
    ASSERT_TRUE(reconstructed.ok());
    ASSERT_EQ(reconstructed.value().data.size(), 128);

    // Test performance metrics
    auto metrics = idx->get_performance_metrics();
    ASSERT_TRUE(metrics.ok());

    // Test cleanup
    ASSERT_TRUE(idx->remove_semantic_embedding(12345).ok());
}

TEST(SemVecSmoke, SemanticSearch)
{
    core::semantic_vector::SemanticVectorConfig::SemanticConfig cfg;
    cfg.default_embedding_dimension = 64;
    cfg.sparsity_threshold = 0.05f;
    auto idx = CreateSemanticIndex(cfg);

    // Test semantic search with text query
    core::SemanticQuery query;
    query.text = "temperature sensor performance";
    query.similarity_threshold = 0.7;
    query.max_results = 10;

    auto results = idx->semantic_search(query);
    ASSERT_TRUE(results.ok());

    // Test with vector query
    core::Vector query_vec(64);
    query_vec.data.resize(64, 0.5f);
    query.vector = query_vec;
    query.text = "";

    auto vector_results = idx->semantic_search(query);
    ASSERT_TRUE(vector_results.ok());
}

TEST(SemVecSmoke, ConfigValidation)
{
    core::semantic_vector::SemanticVectorConfig::SemanticConfig cfg;
    cfg.default_embedding_dimension = 256;
    cfg.sparsity_threshold = 0.1f;
    cfg.max_bert_sequence_length = 512;

    auto validation = ValidateSemanticIndexConfig(cfg);
    ASSERT_TRUE(validation.ok());
    ASSERT_TRUE(validation.value().is_valid);

    // Test invalid config
    cfg.default_embedding_dimension = 0;
    cfg.sparsity_threshold = 1.5f; // Invalid

    auto invalid_validation = ValidateSemanticIndexConfig(cfg);
    ASSERT_TRUE(invalid_validation.ok());
    ASSERT_FALSE(invalid_validation.value().is_valid);
    ASSERT_GT(invalid_validation.value().errors.size(), 0);
}

TEST(SemVecSmoke, UseCaseFactories)
{
    core::semantic_vector::SemanticVectorConfig::SemanticConfig base_cfg;
    
    // Test use case factories
    auto hp_idx = CreateSemanticIndexForUseCase("high_performance", base_cfg);
    ASSERT_NE(hp_idx, nullptr);
    ASSERT_EQ(hp_idx->get_config().default_embedding_dimension, 384);
    
    auto me_idx = CreateSemanticIndexForUseCase("memory_efficient", base_cfg);
    ASSERT_NE(me_idx, nullptr);
    ASSERT_EQ(me_idx->get_config().default_embedding_dimension, 256);
    
    auto ha_idx = CreateSemanticIndexForUseCase("high_accuracy", base_cfg);
    ASSERT_NE(ha_idx, nullptr);
    ASSERT_EQ(ha_idx->get_config().default_embedding_dimension, 768);
}
