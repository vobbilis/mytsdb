#include <gtest/gtest.h>
#include "tsdb/storage/internal/block_impl.h"
#include "tsdb/core/types.h"

using namespace tsdb::storage::internal;
using namespace tsdb;

TEST(BlockImplTest, TestSerializeDeserialize) {
    // 1. Create a block and add data
    BlockHeader header;
    header.start_time = 1000;
    header.end_time = 3000;
    header.magic = BlockHeader::MAGIC;
    header.version = BlockHeader::VERSION;

    auto block = std::make_shared<BlockImpl>(
        header,
        std::make_unique<SimpleTimestampCompressor>(),
        std::make_unique<SimpleValueCompressor>(),
        std::make_unique<SimpleLabelCompressor>()
    );

    core::Labels labels({{"metric", "cpu"}, {"host", "server1"}});
    block->append(labels, core::Sample(1000, 10.0));
    block->append(labels, core::Sample(2000, 20.0));
    block->append(labels, core::Sample(3000, 30.0));

    // 2. Seal and Serialize
    block->seal();
    auto data = block->serialize();
    ASSERT_FALSE(data.empty());

    // 3. Deserialize
    auto deserialized_block = BlockImpl::deserialize(data);
    ASSERT_NE(deserialized_block, nullptr);

    // 4. Verify content
    EXPECT_EQ(deserialized_block->num_series(), 1);
    EXPECT_EQ(deserialized_block->num_samples(), 3);
    
    auto series = deserialized_block->read(labels);
    ASSERT_EQ(series.samples().size(), 3);
    EXPECT_EQ(series.samples()[0].timestamp(), 1000);
    EXPECT_EQ(series.samples()[0].value(), 10.0);
}
