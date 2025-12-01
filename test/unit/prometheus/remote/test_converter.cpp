#include <gtest/gtest.h>
#include "tsdb/prometheus/remote/converter.h"
#include "remote.pb.h"

using namespace tsdb;
using namespace tsdb::prometheus::remote;

class ConverterTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ConverterTest, FromWriteRequest_EmptyRequest) {
    ::prometheus::WriteRequest request;
    auto result = Converter::FromWriteRequest(request);
    EXPECT_TRUE(result.empty());
}

TEST_F(ConverterTest, FromWriteRequest_SingleSeries) {
    ::prometheus::WriteRequest request;
    auto* ts = request.add_timeseries();
    
    // Add labels
    auto* label1 = ts->add_labels();
    label1->set_name("__name__");
    label1->set_value("cpu_usage");
    
    auto* label2 = ts->add_labels();
    label2->set_name("host");
    label2->set_value("server1");
    
    // Add samples
    auto* sample1 = ts->add_samples();
    sample1->set_timestamp(1000);
    sample1->set_value(0.75);
    
    auto* sample2 = ts->add_samples();
    sample2->set_timestamp(2000);
    sample2->set_value(0.80);
    
    // Convert
    auto result = Converter::FromWriteRequest(request);
    
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].labels().size(), 2);
    EXPECT_EQ(result[0].labels().get("__name__").value(), "cpu_usage");
    EXPECT_EQ(result[0].labels().get("host").value(), "server1");
    EXPECT_EQ(result[0].samples().size(), 2);
    EXPECT_EQ(result[0].samples()[0].timestamp(), 1000);
    EXPECT_DOUBLE_EQ(result[0].samples()[0].value(), 0.75);
    EXPECT_EQ(result[0].samples()[1].timestamp(), 2000);
    EXPECT_DOUBLE_EQ(result[0].samples()[1].value(), 0.80);
}

TEST_F(ConverterTest, FromWriteRequest_MultipleSeries) {
    ::prometheus::WriteRequest request;
    
    // First series
    auto* ts1 = request.add_timeseries();
    auto* label1 = ts1->add_labels();
    label1->set_name("__name__");
    label1->set_value("metric1");
    auto* sample1 = ts1->add_samples();
    sample1->set_timestamp(1000);
    sample1->set_value(1.0);
    
    // Second series
    auto* ts2 = request.add_timeseries();
    auto* label2 = ts2->add_labels();
    label2->set_name("__name__");
    label2->set_value("metric2");
    auto* sample2 = ts2->add_samples();
    sample2->set_timestamp(2000);
    sample2->set_value(2.0);
    
    // Convert
    auto result = Converter::FromWriteRequest(request);
    
    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].labels().get("__name__").value(), "metric1");
    EXPECT_EQ(result[1].labels().get("__name__").value(), "metric2");
}

TEST_F(ConverterTest, ToReadResponse_EmptySeries) {
    std::vector<core::TimeSeries> series;
    auto response = Converter::ToReadResponse(series);
    
    EXPECT_EQ(response.results_size(), 1);
    EXPECT_EQ(response.results(0).timeseries_size(), 0);
}

TEST_F(ConverterTest, ToReadResponse_SingleSeries) {
    core::Labels labels;
    labels.add("__name__", "cpu_usage");
    labels.add("host", "server1");
    
    core::TimeSeries ts(labels);
    ts.add_sample(1000, 0.75);
    ts.add_sample(2000, 0.80);
    
    std::vector<core::TimeSeries> series = {ts};
    auto response = Converter::ToReadResponse(series);
    
    ASSERT_EQ(response.results_size(), 1);
    ASSERT_EQ(response.results(0).timeseries_size(), 1);
    
    const auto& proto_ts = response.results(0).timeseries(0);
    EXPECT_EQ(proto_ts.labels_size(), 2);
    EXPECT_EQ(proto_ts.samples_size(), 2);
    
    // Check labels
    bool found_name = false, found_host = false;
    for (const auto& label : proto_ts.labels()) {
        if (label.name() == "__name__") {
            EXPECT_EQ(label.value(), "cpu_usage");
            found_name = true;
        } else if (label.name() == "host") {
            EXPECT_EQ(label.value(), "server1");
            found_host = true;
        }
    }
    EXPECT_TRUE(found_name && found_host);
    
    // Check samples
    EXPECT_EQ(proto_ts.samples(0).timestamp(), 1000);
    EXPECT_DOUBLE_EQ(proto_ts.samples(0).value(), 0.75);
    EXPECT_EQ(proto_ts.samples(1).timestamp(), 2000);
    EXPECT_DOUBLE_EQ(proto_ts.samples(1).value(), 0.80);
}

TEST_F(ConverterTest, FromProtoMatcher_Equal) {
    ::prometheus::LabelMatcher proto_matcher;
    proto_matcher.set_type(::prometheus::LabelMatcher::EQ);
    proto_matcher.set_name("job");
    proto_matcher.set_value("prometheus");
    
    auto matcher = Converter::FromProtoMatcher(proto_matcher);
    
    EXPECT_EQ(matcher.type, core::MatcherType::Equal);
    EXPECT_EQ(matcher.name, "job");
    EXPECT_EQ(matcher.value, "prometheus");
}

TEST_F(ConverterTest, FromProtoMatcher_NotEqual) {
    ::prometheus::LabelMatcher proto_matcher;
    proto_matcher.set_type(::prometheus::LabelMatcher::NEQ);
    proto_matcher.set_name("instance");
    proto_matcher.set_value("localhost");
    
    auto matcher = Converter::FromProtoMatcher(proto_matcher);
    
    EXPECT_EQ(matcher.type, core::MatcherType::NotEqual);
}

TEST_F(ConverterTest, FromProtoMatcher_RegexMatch) {
    ::prometheus::LabelMatcher proto_matcher;
    proto_matcher.set_type(::prometheus::LabelMatcher::RE);
    proto_matcher.set_name("host");
    proto_matcher.set_value("server.*");
    
    auto matcher = Converter::FromProtoMatcher(proto_matcher);
    
    EXPECT_EQ(matcher.type, core::MatcherType::RegexMatch);
}

TEST_F(ConverterTest, FromProtoMatcher_RegexNoMatch) {
    ::prometheus::LabelMatcher proto_matcher;
    proto_matcher.set_type(::prometheus::LabelMatcher::NRE);
    proto_matcher.set_name("env");
    proto_matcher.set_value("prod.*");
    
    auto matcher = Converter::FromProtoMatcher(proto_matcher);
    
    EXPECT_EQ(matcher.type, core::MatcherType::RegexNoMatch);
}

TEST_F(ConverterTest, FromProtoSample) {
    ::prometheus::Sample proto_sample;
    proto_sample.set_timestamp(123456);
    proto_sample.set_value(42.5);
    
    auto sample = Converter::FromProtoSample(proto_sample);
    
    EXPECT_EQ(sample.timestamp(), 123456);
    EXPECT_DOUBLE_EQ(sample.value(), 42.5);
}

TEST_F(ConverterTest, ToProtoSample) {
    core::Sample sample(123456, 42.5);
    
    auto proto_sample = Converter::ToProtoSample(sample);
    
    EXPECT_EQ(proto_sample.timestamp(), 123456);
    EXPECT_DOUBLE_EQ(proto_sample.value(), 42.5);
}

TEST_F(ConverterTest, RoundTrip_WriteAndRead) {
    // Create a WriteRequest
    ::prometheus::WriteRequest write_req;
    auto* ts = write_req.add_timeseries();
    
    auto* label = ts->add_labels();
    label->set_name("__name__");
    label->set_value("test_metric");
    
    auto* sample = ts->add_samples();
    sample->set_timestamp(1000);
    sample->set_value(123.45);
    
    // Convert to internal format
    auto internal_series = Converter::FromWriteRequest(write_req);
    
    // Convert back to ReadResponse
    auto read_resp = Converter::ToReadResponse(internal_series);
    
    // Verify
    ASSERT_EQ(read_resp.results_size(), 1);
    ASSERT_EQ(read_resp.results(0).timeseries_size(), 1);
    
    const auto& result_ts = read_resp.results(0).timeseries(0);
    EXPECT_EQ(result_ts.labels_size(), 1);
    EXPECT_EQ(result_ts.labels(0).name(), "__name__");
    EXPECT_EQ(result_ts.labels(0).value(), "test_metric");
    EXPECT_EQ(result_ts.samples_size(), 1);
    EXPECT_EQ(result_ts.samples(0).timestamp(), 1000);
    EXPECT_DOUBLE_EQ(result_ts.samples(0).value(), 123.45);
}
