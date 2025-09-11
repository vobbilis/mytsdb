#include <gtest/gtest.h>
#include "tsdb/storage/semantic_vector/sparse_temporal_graph.h"
#include "tsdb/core/semantic_vector_config.h"

using namespace tsdb::storage::semantic_vector;
namespace core = ::tsdb::core;

TEST(SemVecSmoke, TemporalGraphBasic)
{
    core::semantic_vector::SemanticVectorConfig::TemporalConfig cfg;
    cfg.correlation_threshold = 0.5;
    cfg.max_graph_nodes = 1000;
    cfg.enable_dense_representation = false;
    auto graph = CreateTemporalGraph(cfg);

    // Test node operations
    ASSERT_TRUE(graph->add_series(12345).ok());
    ASSERT_TRUE(graph->add_series(12346).ok());
    ASSERT_TRUE(graph->add_series(12347).ok());

    // Test correlation operations
    ASSERT_TRUE(graph->add_correlation(12345, 12346, 0.8).ok());
    ASSERT_TRUE(graph->add_correlation(12345, 12347, 0.6).ok());
    ASSERT_TRUE(graph->add_correlation(12346, 12347, 0.9).ok());

    // Test neighbor queries
    auto neighbors = graph->get_neighbors(12345);
    ASSERT_TRUE(neighbors.ok());
    ASSERT_GE(neighbors.value().size(), 2);

    // Test correlation queries
    auto correlation = graph->get_correlation(12345, 12346);
    ASSERT_TRUE(correlation.ok());
    ASSERT_NEAR(correlation.value(), 0.8, 0.001);

    // Test top correlations
    auto top_correlations = graph->get_top_correlations(12345, 5);
    ASSERT_TRUE(top_correlations.ok());
    ASSERT_GE(top_correlations.value().size(), 2);

    // Test graph statistics
    auto stats = graph->get_graph_stats();
    ASSERT_TRUE(stats.ok());
    ASSERT_EQ(stats.value().node_count, 3);
    ASSERT_GE(stats.value().edge_count, 3);

    // Test cleanup
    ASSERT_TRUE(graph->remove_series(12345).ok());
    ASSERT_TRUE(graph->remove_correlation(12346, 12347).ok());
}

TEST(SemVecSmoke, TemporalGraphAnalysis)
{
    core::semantic_vector::SemanticVectorConfig::TemporalConfig cfg;
    cfg.correlation_threshold = 0.3;
    cfg.max_graph_nodes = 100;
    auto graph = CreateTemporalGraph(cfg);

    // Add multiple nodes
    for (core::SeriesID i = 1; i <= 10; ++i) {
        ASSERT_TRUE(graph->add_series(i).ok());
    }

    // Add correlations to create communities
    ASSERT_TRUE(graph->add_correlation(1, 2, 0.9).ok());
    ASSERT_TRUE(graph->add_correlation(2, 3, 0.8).ok());
    ASSERT_TRUE(graph->add_correlation(4, 5, 0.9).ok());
    ASSERT_TRUE(graph->add_correlation(5, 6, 0.8).ok());

    // Test community detection
    auto communities = graph->find_communities();
    ASSERT_TRUE(communities.ok());
    ASSERT_GT(communities.value().size(), 0);

    // Test influential node detection
    auto influential = graph->find_influential_nodes(3);
    ASSERT_TRUE(influential.ok());
    ASSERT_LE(influential.value().size(), 3);

    // Test performance metrics
    auto metrics = graph->get_performance_metrics();
    ASSERT_TRUE(metrics.ok());
}

TEST(SemVecSmoke, TemporalGraphSparse)
{
    core::semantic_vector::SemanticVectorConfig::TemporalConfig cfg;
    cfg.correlation_threshold = 0.5;
    auto graph = CreateTemporalGraph(cfg);

    // Test sparse representation queries
    auto sparse_enabled = graph->is_sparse_enabled();
    ASSERT_TRUE(sparse_enabled.ok());
    ASSERT_TRUE(sparse_enabled.value()); // Should be sparse by default

    // Test sparse operations
    ASSERT_TRUE(graph->enable_sparse_representation().ok());
    ASSERT_TRUE(graph->disable_sparse_representation().ok());
}

TEST(SemVecSmoke, TemporalGraphCompression)
{
    core::semantic_vector::SemanticVectorConfig::TemporalConfig cfg;
    cfg.correlation_threshold = 0.2;
    cfg.enable_graph_compression = true;
    auto graph = CreateTemporalGraph(cfg);

    // Add nodes and edges
    for (core::SeriesID i = 1; i <= 5; ++i) {
        ASSERT_TRUE(graph->add_series(i).ok());
    }

    // Add weak and strong correlations
    ASSERT_TRUE(graph->add_correlation(1, 2, 0.9).ok()); // Strong
    ASSERT_TRUE(graph->add_correlation(1, 3, 0.2).ok()); // Weak
    ASSERT_TRUE(graph->add_correlation(2, 3, 0.1).ok()); // Very weak

    // Test compression
    ASSERT_TRUE(graph->compress_graph().ok());
    
    auto compression_ratio = graph->get_compression_ratio();
    ASSERT_TRUE(compression_ratio.ok());
    ASSERT_LE(compression_ratio.value(), 1.0);

    // Test decompression
    ASSERT_TRUE(graph->decompress_graph().ok());
}

TEST(SemVecSmoke, ConfigValidation)
{
    core::semantic_vector::SemanticVectorConfig::TemporalConfig cfg;
    cfg.correlation_threshold = 0.5;
    cfg.max_graph_nodes = 1000;

    auto validation = ValidateTemporalGraphConfig(cfg);
    ASSERT_TRUE(validation.ok());
    ASSERT_TRUE(validation.value().is_valid);

    // Test invalid config
    cfg.correlation_threshold = 1.5; // Invalid

    auto invalid_validation = ValidateTemporalGraphConfig(cfg);
    ASSERT_TRUE(invalid_validation.ok());
    ASSERT_FALSE(invalid_validation.value().is_valid);
    ASSERT_GT(invalid_validation.value().errors.size(), 0);
}

TEST(SemVecSmoke, UseCaseFactories)
{
    core::semantic_vector::SemanticVectorConfig::TemporalConfig base_cfg;
    
    // Test use case factories
    auto hp_graph = CreateTemporalGraphForUseCase("high_performance", base_cfg);
    ASSERT_NE(hp_graph, nullptr);
    ASSERT_FALSE(hp_graph->get_config().enable_dense_representation);
    
    auto me_graph = CreateTemporalGraphForUseCase("memory_efficient", base_cfg);
    ASSERT_NE(me_graph, nullptr);
    ASSERT_TRUE(me_graph->get_config().enable_graph_compression);
    
    auto ha_graph = CreateTemporalGraphForUseCase("high_accuracy", base_cfg);
    ASSERT_NE(ha_graph, nullptr);
    ASSERT_TRUE(ha_graph->get_config().enable_dense_representation);
}
