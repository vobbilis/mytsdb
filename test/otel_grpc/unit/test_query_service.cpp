#include <gtest/gtest.h>
#include "tsdb/otel/query_service.h"
#include "tsdb/storage/storage.h"
#include "tsdb/core/types.h"
#include "tsdb/proto/gen/tsdb.pb.h"
#include "tsdb/proto/gen/tsdb.grpc.pb.h"
#include <memory>
#include <vector>
#include <map>
#include <set>
#include <chrono>

using namespace tsdb;
using namespace tsdb::otel;
using namespace tsdb::storage;
using namespace tsdb::core;
using namespace tsdb::proto;

// Use fully qualified names to avoid collision between core::TimeSeries and proto::TimeSeries
using CoreTimeSeries = tsdb::core::TimeSeries;
using CoreLabels = tsdb::core::Labels;
using CoreSample = tsdb::core::Sample;

// Mock Storage implementation for testing
class MockStorage : public Storage {
public:
    MockStorage() = default;
    ~MockStorage() override = default;
    
    // Store test data
    std::vector<CoreTimeSeries> test_series_;
    
    // Storage interface implementation
    core::Result<void> init(const core::StorageConfig& /*config*/) override {
        return core::Result<void>();
    }
    
    core::Result<void> write(const CoreTimeSeries& series) override {
        test_series_.push_back(series);
        return core::Result<void>();
    }
    
    core::Result<CoreTimeSeries> read(
        const CoreLabels& /*labels*/,
        int64_t /*start_time*/,
        int64_t /*end_time*/) override {
        return core::Result<CoreTimeSeries>::error("Not implemented");
    }
    
#include "tsdb/core/matcher.h"
// ...

    core::Result<std::vector<CoreTimeSeries>> query(
        const std::vector<core::LabelMatcher>& matchers,
        int64_t start_time,
        int64_t end_time) override {
        
        std::vector<CoreTimeSeries> results;
        
        for (const auto& series : test_series_) {
            bool matches = true;
            
            // Check if series matches all matchers
            for (const auto& matcher : matchers) {
                const auto& labels = series.labels();
                auto it = labels.map().find(matcher.name);
                
                if (matcher.type == core::MatcherType::Equal) {
                    if (it == labels.map().end() || it->second != matcher.value) {
                        matches = false;
                        break;
                    }
                } else if (matcher.type == core::MatcherType::NotEqual) {
                    if (it != labels.map().end() && it->second == matcher.value) {
                        matches = false;
                        break;
                    }
                    // If label doesn't exist, it's not equal, so it matches (unless we require existence?)
                    // Prometheus semantics: label must exist for != "" ? No, != "" matches empty or missing.
                    // For simplicity in mock: strict equality check.
                }
                // Implement other matchers if needed for tests
            }
            
            if (matches) {
                // Filter samples by time range
                CoreTimeSeries filtered_series(series.labels());
                
                for (const auto& sample : series.samples()) {
                    if (sample.timestamp() >= start_time && sample.timestamp() <= end_time) {
                        filtered_series.add_sample(sample);
                    }
                }
                
                if (!filtered_series.samples().empty()) {
                    results.push_back(filtered_series);
                }
            }
        }
        
        return core::Result<std::vector<CoreTimeSeries>>(results);
    }
    
    core::Result<std::vector<std::string>> label_names() override {
        std::set<std::string> names;
        for (const auto& series : test_series_) {
            for (const auto& [key, value] : series.labels().map()) {
                names.insert(key);
            }
        }
        return core::Result<std::vector<std::string>>(std::vector<std::string>(names.begin(), names.end()));
    }
    
    core::Result<std::vector<std::string>> label_values(const std::string& label_name) override {
        std::set<std::string> values;
        for (const auto& series : test_series_) {
            const auto& labels = series.labels();
            auto it = labels.map().find(label_name);
            if (it != labels.map().end()) {
                values.insert(it->second);
            }
        }
        return core::Result<std::vector<std::string>>(std::vector<std::string>(values.begin(), values.end()));
    }
    
    core::Result<void> delete_series(
        const std::vector<core::LabelMatcher>& /*matchers*/) override {
        return core::Result<void>::error("Not implemented");
    }
    
    core::Result<void> compact() override {
        return core::Result<void>();
    }
    
    core::Result<void> flush() override {
        return core::Result<void>();
    }
    
    core::Result<void> close() override {
        return core::Result<void>();
    }
    
    std::string stats() const override {
        return "MockStorage stats";
    }
};

class QueryServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_storage_ = std::make_shared<MockStorage>();
        query_service_ = std::make_unique<QueryService>(mock_storage_);
    }
    
    void TearDown() override {
        query_service_.reset();
        mock_storage_.reset();
    }
    
    std::shared_ptr<MockStorage> mock_storage_;
    std::unique_ptr<QueryService> query_service_;
    
    // Helper to create a test TimeSeries
    CoreTimeSeries createTestSeries(const std::string& metric_name, 
                                const std::map<std::string, std::string>& labels,
                                int64_t timestamp, double value) {
        CoreLabels series_labels;
        
        // Add labels
        for (const auto& [key, val] : labels) {
            series_labels.add(key, val);
        }
        series_labels.add("__name__", metric_name);
        
        CoreTimeSeries series(series_labels);
        series.add_sample(timestamp, value);
        
        return series;
    }
};

TEST_F(QueryServiceTest, BasicQueryWorks) {
    // Write test data
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    std::map<std::string, std::string> labels = {
        {"instance", "test-instance-1"},
        {"job", "test-job"}
    };
    
    auto series = createTestSeries("test_metric", labels, now, 42.5);
    auto result = mock_storage_->write(series);
    ASSERT_TRUE(result.ok()) << "Failed to write test series: " << result.error();
    
    // Query via QueryService
    QueryParams request;
    auto* name_matcher = request.add_matchers();
    name_matcher->set_type(LabelMatcher_Type_EQ);
    name_matcher->set_name("__name__");
    name_matcher->set_value("test_metric");
    
    auto* time_range = request.mutable_time_range();
    time_range->set_start_time(now - 1000);
    time_range->set_end_time(now + 1000);
    
    SeriesResponse response;
    grpc::ServerContext context;
    
    grpc::Status grpc_status = query_service_->GetSeries(&context, &request, &response);
    
    ASSERT_TRUE(grpc_status.ok()) << "GetSeries failed: " << grpc_status.error_message();
    ASSERT_EQ(response.series_size(), 1) << "Expected 1 series, got " << response.series_size();
    
    const auto& proto_series = response.series(0);
    ASSERT_EQ(proto_series.labels_size(), 3) << "Expected 3 labels (__name__, instance, job)";
    ASSERT_EQ(proto_series.samples_size(), 1) << "Expected 1 sample";
    
    // Verify sample value
    const auto& sample = proto_series.samples(0);
    ASSERT_DOUBLE_EQ(sample.value(), 42.5) << "Sample value mismatch";
}

TEST_F(QueryServiceTest, QueryWithMultipleMatchers) {
    // Write multiple series
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Series 1
    std::map<std::string, std::string> labels1 = {
        {"instance", "test-instance-1"},
        {"job", "test-job"}
    };
    auto series1 = createTestSeries("test_metric", labels1, now, 10.0);
    mock_storage_->write(series1);
    
    // Series 2 - same metric name, different instance
    std::map<std::string, std::string> labels2 = {
        {"instance", "test-instance-2"},
        {"job", "test-job"}
    };
    auto series2 = createTestSeries("test_metric", labels2, now, 20.0);
    mock_storage_->write(series2);
    
    // Query for specific instance
    QueryParams request;
    auto* name_matcher = request.add_matchers();
    name_matcher->set_type(LabelMatcher_Type_EQ);
    name_matcher->set_name("__name__");
    name_matcher->set_value("test_metric");
    
    auto* instance_matcher = request.add_matchers();
    instance_matcher->set_type(LabelMatcher_Type_EQ);
    instance_matcher->set_name("instance");
    instance_matcher->set_value("test-instance-1");
    
    auto* time_range = request.mutable_time_range();
    time_range->set_start_time(now - 1000);
    time_range->set_end_time(now + 1000);
    
    SeriesResponse response;
    grpc::ServerContext context;
    
    grpc::Status grpc_status = query_service_->GetSeries(&context, &request, &response);
    
    ASSERT_TRUE(grpc_status.ok()) << "GetSeries failed: " << grpc_status.error_message();
    ASSERT_EQ(response.series_size(), 1) << "Expected 1 series matching instance filter";
    
    const auto& proto_series = response.series(0);
    ASSERT_EQ(proto_series.samples_size(), 1);
    ASSERT_DOUBLE_EQ(proto_series.samples(0).value(), 10.0);
}

TEST_F(QueryServiceTest, QueryWithNoResults) {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Query for non-existent metric
    QueryParams request;
    auto* name_matcher = request.add_matchers();
    name_matcher->set_type(LabelMatcher_Type_EQ);
    name_matcher->set_name("__name__");
    name_matcher->set_value("non_existent_metric");
    
    auto* time_range = request.mutable_time_range();
    time_range->set_start_time(now - 1000);
    time_range->set_end_time(now + 1000);
    
    SeriesResponse response;
    grpc::ServerContext context;
    
    grpc::Status grpc_status = query_service_->GetSeries(&context, &request, &response);
    
    ASSERT_TRUE(grpc_status.ok()) << "GetSeries should succeed even with no results";
    ASSERT_EQ(response.series_size(), 0) << "Expected 0 series for non-existent metric";
}

TEST_F(QueryServiceTest, GetLabelNames) {
    // Write test data with various labels
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    std::map<std::string, std::string> labels = {
        {"instance", "test-instance"},
        {"job", "test-job"},
        {"env", "production"}
    };
    auto series = createTestSeries("test_metric", labels, now, 1.0);
    mock_storage_->write(series);
    
    // Query label names
    QueryParams request;
    LabelNamesResponse response;
    grpc::ServerContext context;
    
    grpc::Status grpc_status = query_service_->GetLabelNames(&context, &request, &response);
    
    ASSERT_TRUE(grpc_status.ok()) << "GetLabelNames failed: " << grpc_status.error_message();
    ASSERT_GE(response.names_size(), 4) << "Expected at least 4 label names (__name__, instance, job, env)";
    
    // Verify specific labels exist
    std::set<std::string> label_names;
    for (int i = 0; i < response.names_size(); ++i) {
        label_names.insert(response.names(i));
    }
    
    ASSERT_TRUE(label_names.count("__name__")) << "__name__ label not found";
    ASSERT_TRUE(label_names.count("instance")) << "instance label not found";
    ASSERT_TRUE(label_names.count("job")) << "job label not found";
    ASSERT_TRUE(label_names.count("env")) << "env label not found";
}

TEST_F(QueryServiceTest, GetLabelValues) {
    // Write test data with multiple values for a label
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    std::map<std::string, std::string> labels1 = {{"instance", "instance-1"}};
    auto series1 = createTestSeries("metric1", labels1, now, 1.0);
    mock_storage_->write(series1);
    
    std::map<std::string, std::string> labels2 = {{"instance", "instance-2"}};
    auto series2 = createTestSeries("metric2", labels2, now, 2.0);
    mock_storage_->write(series2);
    
    // Query label values
    LabelValuesRequest request;
    request.set_label_name("instance");
    LabelValuesResponse response;
    grpc::ServerContext context;
    
    grpc::Status grpc_status = query_service_->GetLabelValues(&context, &request, &response);
    
    ASSERT_TRUE(grpc_status.ok()) << "GetLabelValues failed: " << grpc_status.error_message();
    ASSERT_GE(response.values_size(), 2) << "Expected at least 2 instance values";
    
    // Verify values
    std::set<std::string> values;
    for (int i = 0; i < response.values_size(); ++i) {
        values.insert(response.values(i));
    }
    
    ASSERT_TRUE(values.count("instance-1")) << "instance-1 not found";
    ASSERT_TRUE(values.count("instance-2")) << "instance-2 not found";
}
