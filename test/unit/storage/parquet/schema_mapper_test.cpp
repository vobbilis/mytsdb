#include <gtest/gtest.h>
#include "tsdb/storage/parquet/schema_mapper.hpp"
#include "tsdb/core/types.h"

using namespace tsdb::storage::parquet;
using namespace tsdb;

TEST(SchemaMapperTest, TestRoundTrip) {
    // 1. Create Samples and Tags
    std::vector<tsdb::core::Sample> samples = {
        tsdb::core::Sample(1000, 10.0),
        tsdb::core::Sample(2000, 20.0),
        tsdb::core::Sample(3000, 30.0)
    };
    std::map<std::string, std::string> tags = {
        {"metric", "cpu"},
        {"host", "server1"}
    };

    // 2. Convert to RecordBatch
    auto batch = SchemaMapper::ToRecordBatch(samples, tags);
    ASSERT_NE(batch, nullptr);
    ASSERT_EQ(batch->num_rows(), 3);

    // 3. Convert back to Samples
    auto samples_result = SchemaMapper::ToSamples(batch);
    ASSERT_TRUE(samples_result.ok());
    auto decoded_samples = samples_result.value();

    ASSERT_EQ(decoded_samples.size(), 3);
    for (size_t i = 0; i < 3; ++i) {
        EXPECT_EQ(decoded_samples[i].timestamp(), samples[i].timestamp());
        EXPECT_EQ(decoded_samples[i].value(), samples[i].value());
    }

    // 4. Extract Tags
    auto tags_result = SchemaMapper::ExtractTags(batch);
    ASSERT_TRUE(tags_result.ok());
    auto decoded_tags = tags_result.value();

    ASSERT_EQ(decoded_tags.size(), 2);
    EXPECT_EQ(decoded_tags["metric"], "cpu");
    EXPECT_EQ(decoded_tags["host"], "server1");
}

TEST(SchemaMapperTest, TestEmptyTags) {
    std::vector<tsdb::core::Sample> samples = {tsdb::core::Sample(1000, 10.0)};
    std::map<std::string, std::string> tags = {};

    auto batch = SchemaMapper::ToRecordBatch(samples, tags);
    ASSERT_NE(batch, nullptr);

    auto tags_result = SchemaMapper::ExtractTags(batch);
    ASSERT_TRUE(tags_result.ok());
    EXPECT_TRUE(tags_result.value().empty());
}
