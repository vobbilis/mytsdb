#include <gtest/gtest.h>
#include "tsdb/storage/semantic_vector/quantized_vector_index.h"
#include "tsdb/core/semantic_vector_config.h"

using namespace tsdb::storage::semantic_vector;
namespace core = ::tsdb::core;

TEST(SemVecSmoke, AddSearchBasic)
{
    core::semantic_vector::SemanticVectorConfig::VectorConfig cfg;
    cfg.default_vector_dimension = 4;
    cfg.default_index_type = core::semantic_vector::VectorIndex::IndexType::HNSW;
    auto idx = CreateVectorIndex(cfg);

    core::Vector v1(4); v1.data = {1.0f, 0.0f, 0.0f, 0.0f};
    core::Vector v2(4); v2.data = {0.0f, 1.0f, 0.0f, 0.0f};
    core::Vector vq(4); vq.data = {0.9f, 0.1f, 0.0f, 0.0f};

    ASSERT_TRUE(idx->add_vector(1, v1).ok());
    ASSERT_TRUE(idx->add_vector(2, v2).ok());

    auto res = idx->search_similar(vq, 1, 0.0);
    ASSERT_TRUE(res.ok());
    ASSERT_FALSE(res.value().empty());
    EXPECT_EQ(res.value()[0].first, 1u);
}


