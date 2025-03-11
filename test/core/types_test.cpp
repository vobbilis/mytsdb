#include <gtest/gtest.h>
#include "tsdb/core/types.h"
#include "tsdb/core/error.h"

namespace tsdb {
namespace core {
namespace {

TEST(LabelsTest, EmptyLabels) {
    Labels labels;
    EXPECT_TRUE(labels.empty());
    EXPECT_EQ(labels.size(), 0);
    EXPECT_FALSE(labels.has("name"));
    EXPECT_FALSE(labels.get("name").has_value());
}

TEST(LabelsTest, AddAndGet) {
    Labels labels;
    labels.add("name", "test");
    labels.add("env", "prod");
    
    EXPECT_FALSE(labels.empty());
    EXPECT_EQ(labels.size(), 2);
    EXPECT_TRUE(labels.has("name"));
    EXPECT_TRUE(labels.has("env"));
    EXPECT_FALSE(labels.has("missing"));
    
    auto name = labels.get("name");
    EXPECT_TRUE(name.has_value());
    EXPECT_EQ(*name, "test");
    
    auto env = labels.get("env");
    EXPECT_TRUE(env.has_value());
    EXPECT_EQ(*env, "prod");
    
    auto missing = labels.get("missing");
    EXPECT_FALSE(missing.has_value());
}

TEST(LabelsTest, Remove) {
    Labels labels;
    labels.add("name", "test");
    labels.add("env", "prod");
    
    EXPECT_EQ(labels.size(), 2);
    labels.remove("name");
    EXPECT_EQ(labels.size(), 1);
    EXPECT_FALSE(labels.has("name"));
    EXPECT_TRUE(labels.has("env"));
}

TEST(LabelsTest, EmptyName) {
    Labels labels;
    EXPECT_THROW(labels.add("", "value"), InvalidArgumentError);
}

TEST(LabelsTest, Comparison) {
    Labels l1;
    l1.add("name", "test");
    l1.add("env", "prod");
    
    Labels l2;
    l2.add("name", "test");
    l2.add("env", "prod");
    
    Labels l3;
    l3.add("name", "test");
    l3.add("env", "dev");
    
    EXPECT_EQ(l1, l2);
    EXPECT_NE(l1, l3);
    EXPECT_LT(l3, l1);  // "dev" < "prod"
}

TEST(LabelsTest, ToString) {
    Labels labels;
    labels.add("name", "test");
    labels.add("env", "prod");
    
    std::string str = labels.to_string();
    EXPECT_TRUE(str.find("name=\"test\"") != std::string::npos);
    EXPECT_TRUE(str.find("env=\"prod\"") != std::string::npos);
}

TEST(SampleTest, Construction) {
    Sample s(1234, 5.67);
    EXPECT_EQ(s.timestamp(), 1234);
    EXPECT_EQ(s.value(), 5.67);
}

TEST(SampleTest, Comparison) {
    Sample s1(1234, 5.67);
    Sample s2(1234, 5.67);
    Sample s3(1234, 5.68);
    Sample s4(1235, 5.67);
    
    EXPECT_EQ(s1, s2);
    EXPECT_NE(s1, s3);
    EXPECT_NE(s1, s4);
}

TEST(TimeSeriesTest, EmptyTimeSeries) {
    TimeSeries ts;
    EXPECT_TRUE(ts.empty());
    EXPECT_EQ(ts.size(), 0);
}

TEST(TimeSeriesTest, AddSamples) {
    Labels labels;
    labels.add("name", "test");
    
    TimeSeries ts(labels);
    EXPECT_EQ(ts.labels(), labels);
    
    ts.add_sample(1234, 5.67);
    ts.add_sample(1235, 6.78);
    
    EXPECT_FALSE(ts.empty());
    EXPECT_EQ(ts.size(), 2);
    
    const auto& samples = ts.samples();
    EXPECT_EQ(samples[0].timestamp(), 1234);
    EXPECT_EQ(samples[0].value(), 5.67);
    EXPECT_EQ(samples[1].timestamp(), 1235);
    EXPECT_EQ(samples[1].value(), 6.78);
}

TEST(TimeSeriesTest, ChronologicalOrder) {
    TimeSeries ts;
    ts.add_sample(1234, 5.67);
    EXPECT_THROW(ts.add_sample(1233, 6.78), InvalidArgumentError);
}

TEST(TimeSeriesTest, Clear) {
    TimeSeries ts;
    ts.add_sample(1234, 5.67);
    ts.add_sample(1235, 6.78);
    
    EXPECT_EQ(ts.size(), 2);
    ts.clear();
    EXPECT_TRUE(ts.empty());
    EXPECT_EQ(ts.size(), 0);
}

TEST(TimeSeriesTest, Comparison) {
    Labels l1;
    l1.add("name", "test1");
    
    Labels l2;
    l2.add("name", "test2");
    
    TimeSeries ts1(l1);
    ts1.add_sample(1234, 5.67);
    
    TimeSeries ts2(l1);
    ts2.add_sample(1234, 5.67);
    
    TimeSeries ts3(l2);
    ts3.add_sample(1234, 5.67);
    
    EXPECT_EQ(ts1, ts2);
    EXPECT_NE(ts1, ts3);
}

} // namespace
} // namespace core
} // namespace tsdb 