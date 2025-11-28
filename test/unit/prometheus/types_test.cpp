#include <gtest/gtest.h>
#include <limits>
#include "tsdb/prometheus/model/types.h"

namespace tsdb {
namespace prometheus {
namespace {

class TypesTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up test data
        validLabels = {
            {"job", "prometheus"},
            {"instance", "localhost:9090"}
        };
        
        validMetricName = "http_requests_total";
        validHelp = "Total number of HTTP requests";
    }
    
    LabelSet::LabelMap validLabels;
    std::string validMetricName;
    std::string validHelp;
};

TEST_F(TypesTest, SampleCreation) {
    Sample s(1234567890000, 42.0);
    EXPECT_EQ(s.timestamp(), 1234567890000);
    EXPECT_EQ(s.value(), 42.0);
}

TEST_F(TypesTest, SampleEquality) {
    Sample s1(1234567890000, 42.0);
    Sample s2(1234567890000, 42.0);
    Sample s3(1234567890000, 43.0);
    
    EXPECT_EQ(s1, s2);
    EXPECT_NE(s1, s3);
}

TEST_F(TypesTest, LabelSetCreation) {
    EXPECT_NO_THROW({
        LabelSet labels(validLabels);
        EXPECT_EQ(labels.labels(), validLabels);
    });
}

TEST_F(TypesTest, LabelSetValidation) {
    EXPECT_THROW(([](){
        LabelSet::LabelMap invalidLabels = {{"", "value"}};
        LabelSet labels(invalidLabels);
    }()), InvalidLabelError);
    
    EXPECT_THROW(([](){
        LabelSet::LabelMap invalidLabels = {{"name", ""}};
        LabelSet labels(invalidLabels);
    }()), InvalidLabelError);
    
    EXPECT_THROW(([](){
        LabelSet::LabelMap invalidLabels = {{"123name", "value"}};
        LabelSet labels(invalidLabels);
    }()), InvalidLabelError);
}

TEST_F(TypesTest, LabelSetOperations) {
    LabelSet labels;
    
    // Add label
    EXPECT_NO_THROW(labels.AddLabel("job", "prometheus"));
    EXPECT_TRUE(labels.HasLabel("job"));
    EXPECT_EQ(labels.GetLabelValue("job"), "prometheus");
    
    // Remove label
    labels.RemoveLabel("job");
    EXPECT_FALSE(labels.HasLabel("job"));
    EXPECT_FALSE(labels.GetLabelValue("job").has_value());
}

TEST_F(TypesTest, LabelSetToString) {
    LabelSet labels(validLabels);
    std::string str = labels.ToString();
    EXPECT_TRUE(str.find("job=\"prometheus\"") != std::string::npos);
    EXPECT_TRUE(str.find("instance=\"localhost:9090\"") != std::string::npos);
}

TEST_F(TypesTest, TimeSeriesCreation) {
    LabelSet labels(validLabels);
    TimeSeries ts(labels);
    
    EXPECT_EQ(ts.labels(), labels);
    EXPECT_TRUE(ts.empty());
    EXPECT_EQ(ts.size(), 0);
}

TEST_F(TypesTest, TimeSeriesSamples) {
    LabelSet labels(validLabels);
    TimeSeries ts(labels);
    
    ts.AddSample(1234567890000, 42.0);
    EXPECT_EQ(ts.size(), 1);
    
    const auto& samples = ts.samples();
    EXPECT_EQ(samples[0].timestamp(), 1234567890000);
    EXPECT_EQ(samples[0].value(), 42.0);
}

TEST_F(TypesTest, TimeSeriesValidation) {
    LabelSet labels(validLabels);
    TimeSeries ts(labels);
    
    EXPECT_THROW({
        ts.AddSample(-1, 42.0);  // Invalid timestamp
    }, InvalidTimestampError);
}

TEST_F(TypesTest, MetricFamilyCreation) {
    EXPECT_NO_THROW({
        MetricFamily mf(validMetricName, MetricFamily::Type::COUNTER, validHelp);
        EXPECT_EQ(mf.name(), validMetricName);
        EXPECT_EQ(mf.type(), MetricFamily::Type::COUNTER);
        EXPECT_EQ(mf.help(), validHelp);
    });
}

TEST_F(TypesTest, MetricFamilyValidation) {
    EXPECT_THROW(([](){
        MetricFamily mf("", MetricFamily::Type::COUNTER);
    }()), InvalidMetricError);
    
    EXPECT_THROW(([](){
        MetricFamily mf("123invalid", MetricFamily::Type::COUNTER);
    }()), InvalidMetricError);
}

TEST_F(TypesTest, MetricFamilyTimeSeries) {
    MetricFamily mf(validMetricName, MetricFamily::Type::COUNTER, validHelp);
    
    LabelSet labels(validLabels);
    TimeSeries ts(labels);
    ts.AddSample(1234567890000, 42.0);
    
    mf.AddTimeSeries(ts);
    EXPECT_EQ(mf.series().size(), 1);
    
    mf.RemoveTimeSeries(labels);
    EXPECT_EQ(mf.series().size(), 0);
}

TEST_F(TypesTest, MetricFamilyEquality) {
    MetricFamily mf1(validMetricName, MetricFamily::Type::COUNTER, validHelp);
    MetricFamily mf2(validMetricName, MetricFamily::Type::COUNTER, validHelp);
    MetricFamily mf3(validMetricName, MetricFamily::Type::GAUGE, validHelp);
    
    EXPECT_EQ(mf1, mf2);
    EXPECT_NE(mf1, mf3);
}

// Additional Sample Tests
TEST_F(TypesTest, SampleSpecialValues) {
    // Test NaN
    Sample s1(1234567890000, std::numeric_limits<double>::quiet_NaN());
    EXPECT_TRUE(std::isnan(s1.value()));
    
    // Test Infinity
    Sample s2(1234567890000, std::numeric_limits<double>::infinity());
    EXPECT_TRUE(std::isinf(s2.value()));
    
    // Test -Infinity
    Sample s3(1234567890000, -std::numeric_limits<double>::infinity());
    EXPECT_TRUE(std::isinf(s3.value()));
}

TEST_F(TypesTest, SampleTimestampValidation) {
    // Test minimum timestamp
    EXPECT_NO_THROW({
        Sample s(0, 42.0);
    });
    
    // Test maximum timestamp
    EXPECT_NO_THROW({
        Sample s(253402300799999, 42.0);
    });
    
    // Test invalid timestamps
    EXPECT_THROW((([](){
        TimeSeries ts{LabelSet()};
        ts.AddSample(-1, 42.0);  // Invalid timestamp
    }())), InvalidTimestampError);
    
    EXPECT_THROW((([](){
        TimeSeries ts{LabelSet()};
        ts.AddSample(253402300800000, 42.0);
    }())), InvalidTimestampError);
}

// Additional LabelSet Tests
TEST_F(TypesTest, LabelSetLimits) {
    // Test long label name (1024 chars)
    std::string longName = std::string(1024, 'a');
    EXPECT_NO_THROW({
        LabelSet labels;
        labels.AddLabel(longName, "value");
    });
    
    // Test long label value (4096 chars)
    std::string longValue = std::string(4096, 'x');
    EXPECT_NO_THROW({
        LabelSet labels;
        labels.AddLabel("name", longValue);
    });
    
    // Test special characters in label values
    EXPECT_NO_THROW({
        LabelSet labels;
        labels.AddLabel("label", "value with spaces");
        labels.AddLabel("unicode", "值");
        labels.AddLabel("special", "!@#$%^&*()");
    });
}

TEST_F(TypesTest, LabelSetDuplicates) {
    LabelSet labels;
    
    // Adding same label twice should overwrite
    labels.AddLabel("job", "value1");
    labels.AddLabel("job", "value2");
    EXPECT_EQ(labels.GetLabelValue("job"), "value2");
    
    // Case sensitivity
    labels.AddLabel("Job", "value3");
    EXPECT_NE(labels.GetLabelValue("job"), labels.GetLabelValue("Job"));
}

// Additional TimeSeries Tests
TEST_F(TypesTest, TimeSeriesOrdering) {
    TimeSeries ts{LabelSet()};
    
    // Add samples in random order
    ts.AddSample(1000, 1.0);
    ts.AddSample(500, 2.0);
    ts.AddSample(2000, 3.0);
    
    // Verify samples remain in insertion order
    const auto& samples = ts.samples();
    EXPECT_EQ(samples[0].timestamp(), 1000);
    EXPECT_EQ(samples[1].timestamp(), 500);
    EXPECT_EQ(samples[2].timestamp(), 2000);
}

TEST_F(TypesTest, TimeSeriesDuplicateTimestamps) {
    TimeSeries ts{LabelSet()};
    
    // Adding samples with same timestamp should be allowed
    ts.AddSample(1000, 1.0);
    ts.AddSample(1000, 2.0);
    
    EXPECT_EQ(ts.size(), 2);
    const auto& samples = ts.samples();
    EXPECT_EQ(samples[0].timestamp(), 1000);
    EXPECT_EQ(samples[0].value(), 1.0);
    EXPECT_EQ(samples[1].timestamp(), 1000);
    EXPECT_EQ(samples[1].value(), 2.0);
}

// Additional MetricFamily Tests
TEST_F(TypesTest, MetricFamilySeriesUniqueness) {
    MetricFamily mf(validMetricName, MetricFamily::Type::COUNTER);
    
    // Create two series with same labels
    LabelSet labels(validLabels);
    TimeSeries ts1(labels);
    TimeSeries ts2(labels);
    
    ts1.AddSample(1000, 1.0);
    ts2.AddSample(2000, 2.0);
    
    // Adding both series should be allowed
    mf.AddTimeSeries(ts1);
    mf.AddTimeSeries(ts2);
    
    EXPECT_EQ(mf.series().size(), 2);
}

TEST_F(TypesTest, MetricFamilyNameValidation) {
    // Test valid metric names
    EXPECT_NO_THROW({
        MetricFamily mf1("valid_name", MetricFamily::Type::COUNTER);
        MetricFamily mf2("valid:name:with:colons", MetricFamily::Type::COUNTER);
        MetricFamily mf3("_name_starting_with_underscore", MetricFamily::Type::COUNTER);
    });
    
    // Test invalid metric names
    EXPECT_THROW(([](){
        MetricFamily mf("123invalid", MetricFamily::Type::COUNTER);
    }()), InvalidMetricError);
    
    EXPECT_THROW(([](){
        MetricFamily mf("-invalid", MetricFamily::Type::COUNTER);
    }()), InvalidMetricError);
    
    EXPECT_THROW(([](){
        MetricFamily mf("invalid-name", MetricFamily::Type::COUNTER);
    }()), InvalidMetricError);
}

TEST_F(TypesTest, MetricFamilyHelpText) {
    // Test empty help text
    EXPECT_NO_THROW({
        MetricFamily mf(validMetricName, MetricFamily::Type::COUNTER, "");
    });
    
    // Test multi-line help text
    EXPECT_NO_THROW({
        MetricFamily mf(validMetricName, MetricFamily::Type::COUNTER,
            "First line\nSecond line\nThird line");
    });
    
    // Test help text with special characters
    EXPECT_NO_THROW({
        MetricFamily mf(validMetricName, MetricFamily::Type::COUNTER,
            "Help text with unicode: 值 and symbols: !@#$%^&*()");
    });
}

} // namespace
} // namespace prometheus
} // namespace tsdb 