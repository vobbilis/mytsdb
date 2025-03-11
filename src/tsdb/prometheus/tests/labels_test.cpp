#include <gtest/gtest.h>
#include <memory>
#include "../api/labels.h"
#include <rapidjson/document.h>

namespace tsdb {
namespace prometheus {
namespace {

class MockStorage {
public:
    std::vector<std::string> GetLabelNames(
        std::optional<int64_t> start_time,
        std::optional<int64_t> end_time,
        const std::vector<std::string>& matchers) {
        return {"job", "instance", "service", "env"};
    }
    
    std::vector<std::string> GetLabelValues(
        const std::string& label_name,
        std::optional<int64_t> start_time,
        std::optional<int64_t> end_time,
        const std::vector<std::string>& matchers) {
        if (label_name == "job") {
            return {"prometheus", "node_exporter", "mysql"};
        }
        if (label_name == "instance") {
            return {"localhost:9090", "localhost:9100"};
        }
        return {};
    }
    
    std::vector<TimeSeries> GetSeries(
        const std::vector<std::string>& matchers,
        std::optional<int64_t> start_time,
        std::optional<int64_t> end_time) {
        LabelSet labels;
        labels.AddLabel("job", "prometheus");
        labels.AddLabel("instance", "localhost:9090");
        
        TimeSeries ts(labels);
        ts.AddSample(1234567890000, 42.0);
        
        return {ts};
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

TEST_F(LabelsTest, GetLabels) {
    // Test basic label retrieval
    auto result = handler->GetLabels();
    ValidateSuccessResponse(result, {"job", "instance", "service", "env"});
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