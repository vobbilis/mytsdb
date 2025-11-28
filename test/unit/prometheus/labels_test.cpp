#include <gtest/gtest.h>
#include <memory>
#include "tsdb/prometheus/api/labels.h"
#include <rapidjson/document.h>
#include "tsdb/storage/storage.h"
#include "tsdb/core/result.h"
#include "tsdb/core/types.h"

namespace tsdb {
namespace prometheus {
using namespace api;
namespace {

class MockStorage : public storage::Storage {
public:
    // Required Storage interface methods
    core::Result<void> init(const core::StorageConfig& config) override {
        return core::Result<void>();
    }
    
    core::Result<void> write(const core::TimeSeries& series) override {
        return core::Result<void>();
    }
    
    core::Result<core::TimeSeries> read(
        const core::Labels& labels,
        int64_t start_time,
        int64_t end_time) override {
        return core::Result<core::TimeSeries>::error("Not implemented");
    }
    
#include "tsdb/core/matcher.h"
// ...

    core::Result<std::vector<core::TimeSeries>> query(
        const std::vector<core::LabelMatcher>& matchers,
        int64_t start_time,
        int64_t end_time) override {
        std::vector<core::TimeSeries> result;
        
        // Create a mock time series
        core::Labels::Map label_map = {{"job", "prometheus"}, {"instance", "localhost:9090"}};
        core::Labels labels(label_map);
        core::TimeSeries ts(labels);
        ts.add_sample(core::Sample(1234567890000, 42.0));
        result.push_back(std::move(ts));
        
        return core::Result<std::vector<core::TimeSeries>>(std::move(result));
    }
    
    core::Result<std::vector<std::string>> label_names() override {
        return core::Result<std::vector<std::string>>(
            std::vector<std::string>{"job", "instance", "service", "env"}
        );
    }
    
    core::Result<std::vector<std::string>> label_values(const std::string& label_name) override {
        if (label_name == "job") {
            std::vector<std::string> values = {"prometheus", "node_exporter", "mysql"};
            return core::Result<std::vector<std::string>>(std::move(values));
        }
        if (label_name == "instance") {
            std::vector<std::string> values = {"localhost:9090", "localhost:9100"};
            return core::Result<std::vector<std::string>>(std::move(values));
        }
        return core::Result<std::vector<std::string>>(std::vector<std::string>{});
    }
    
    core::Result<void> delete_series(
        const std::vector<core::LabelMatcher>& matchers) override {
        return core::Result<void>();
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
        return "Mock storage stats";
    }
};

class LabelsTest : public ::testing::Test {
protected:
    void SetUp() override {
        storage = std::make_shared<MockStorage>();
        handler = std::make_unique<LabelsHandler>(storage);
    }
    
    std::shared_ptr<MockStorage> storage;
    std::unique_ptr<LabelsHandler> handler;
    
    bool ValidateJSON(const std::string& json) {
        rapidjson::Document doc;
        return !doc.Parse(json.c_str()).HasParseError();
    }
    
    void ValidateSuccessResponse(const LabelQueryResult& result,
                               const std::vector<std::string>& expected) {
        EXPECT_EQ(result.status, "success");
        EXPECT_TRUE(result.error_type.empty());
        EXPECT_TRUE(result.error.empty());
        EXPECT_EQ(result.values, expected);
        
        // Validate JSON format
        EXPECT_TRUE(ValidateJSON(result.ToJSON()));
    }
    
    void ValidateErrorResponse(const LabelQueryResult& result,
                             const std::string& expected_type,
                             const std::string& expected_error) {
        EXPECT_EQ(result.status, "error");
        EXPECT_EQ(result.error_type, expected_type);
        EXPECT_EQ(result.error, expected_error);
        EXPECT_TRUE(result.values.empty());
        
        // Validate JSON format
        EXPECT_TRUE(ValidateJSON(result.ToJSON()));
    }
};

TEST_F(LabelsTest, DirectStorageCall) {
    std::cout << "Starting DirectStorageCall test" << std::endl;
    auto result = storage->label_names();
    std::cout << "storage->label_names() returned" << std::endl;
    EXPECT_TRUE(result.ok());
    std::cout << "result.ok() passed" << std::endl;
    EXPECT_EQ(result.value().size(), 4);
    std::cout << "DirectStorageCall complete" << std::endl;
}

TEST_F(LabelsTest, GetLabels) {
    std::cout << "Starting GetLabels test" << std::endl;
    // Test basic label retrieval
    std::cout << "Calling handler->GetLabels()" << std::endl;
    auto result = handler->GetLabels();
    std::cout << "GetLabels returned" << std::endl;
    ValidateSuccessResponse(result, {"job", "instance", "service", "env"});
    std::cout << "Validation complete" << std::endl;
}

TEST_F(LabelsTest, GetLabelValues) {
    // Test valid label
    auto result = handler->GetLabelValues("job");
    ValidateSuccessResponse(result, {"prometheus", "node_exporter", "mysql"});
    
    // Test another valid label
    result = handler->GetLabelValues("instance");
    ValidateSuccessResponse(result, {"localhost:9090", "localhost:9100"});
    
    // Test non-existent label
    result = handler->GetLabelValues("nonexistent");
    ValidateSuccessResponse(result, {});
}

TEST_F(LabelsTest, GetLabelValuesInvalidName) {
    // Test invalid label name
    auto result = handler->GetLabelValues("123invalid");
    ValidateErrorResponse(result, "invalid_parameter",
                         "Invalid label name: 123invalid");
}

TEST_F(LabelsTest, TimeRangeValidation) {
    LabelQueryParams params;
    params.start_time = 1000;
    params.end_time = 500;  // Invalid: end before start
    
    auto result = handler->GetLabels(params);
    ValidateErrorResponse(result, "invalid_parameters",
                         "Invalid query parameters");
}

TEST_F(LabelsTest, MatcherValidation) {
    LabelQueryParams params;
    params.matchers = {"invalid{matcher"};  // Invalid matcher syntax
    
    auto result = handler->GetLabels(params);
    ValidateErrorResponse(result, "invalid_parameters",
                         "Invalid query parameters");
}

TEST_F(LabelsTest, GetSeries) {
    // Test basic series retrieval
    auto result = handler->GetSeries({"{job=\"prometheus\"}"});
    EXPECT_EQ(result.status, "success");
    EXPECT_EQ(result.values.size(), 1);
    EXPECT_TRUE(result.values[0].find("job=\"prometheus\"") != std::string::npos);
    
    // Test invalid matcher
    result = handler->GetSeries({"invalid{matcher"});
    ValidateErrorResponse(result, "invalid_parameter",
                         "Invalid label matchers");
}

TEST_F(LabelsTest, ResponseFormat) {
    // Test success response format
    auto result = handler->GetLabels();
    auto json = result.ToJSON();
    EXPECT_TRUE(ValidateJSON(json));
    EXPECT_TRUE(json.find("\"status\":\"success\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"data\":[") != std::string::npos);
    
    // Test error response format
    LabelQueryParams params;
    params.start_time = 1000;
    params.end_time = 500;
    result = handler->GetLabels(params);
    json = result.ToJSON();
    EXPECT_TRUE(ValidateJSON(json));
    EXPECT_TRUE(json.find("\"status\":\"error\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"errorType\":") != std::string::npos);
    EXPECT_TRUE(json.find("\"error\":") != std::string::npos);
}

} // namespace
} // namespace prometheus
} // namespace tsdb 