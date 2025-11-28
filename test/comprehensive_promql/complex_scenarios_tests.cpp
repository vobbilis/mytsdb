#include "test_fixture.h"
#include <gtest/gtest.h>
#include <cmath>
#include "tsdb/core/types.h"

namespace tsdb {
namespace comprehensive {

class ComplexScenariosTest : public ComprehensivePromQLTest {
protected:
    void SetUp() override {
        ComprehensivePromQLTest::SetUp();
        
        // Scenario 1: HTTP Requests (Counter)
        // metric: complex_http_requests_total
        // labels: job=api, instance=inst-1, method=get/post, status=200/500
        // Data: Increasing counters
        
        GenerateCounterSeries("complex_http_requests_total", {{"job", "api"}, {"instance", "inst-1"}, {"method", "get"}, {"status", "200"}}, 1.0); // 1 req/s
        GenerateCounterSeries("complex_http_requests_total", {{"job", "api"}, {"instance", "inst-1"}, {"method", "post"}, {"status", "200"}}, 0.5); // 0.5 req/s
        GenerateCounterSeries("complex_http_requests_total", {{"job", "api"}, {"instance", "inst-1"}, {"method", "get"}, {"status", "500"}}, 0.1); // 0.1 req/s
        
        GenerateCounterSeries("complex_http_requests_total", {{"job", "worker"}, {"instance", "inst-2"}, {"method", "get"}, {"status", "200"}}, 2.0); // 2 req/s
        
        // Scenario 2: CPU Usage (Gauge) for Binary Ops
        // metric: complex_node_memory_usage_bytes
        GenerateGaugeSeries("complex_node_memory_usage_bytes", {{"instance", "inst-1"}}, 1024 * 1024 * 100); // 100MB
        GenerateGaugeSeries("complex_node_memory_total_bytes", {{"instance", "inst-1"}}, 1024 * 1024 * 1024); // 1GB
    }

    void GenerateCounterSeries(const std::string& name, 
                              const std::map<std::string, std::string>& labels_map,
                              double rate_per_sec) {
        tsdb::core::Labels::Map map = labels_map;
        map["__name__"] = name;
        tsdb::core::Labels labels(map);
        
        tsdb::core::TimeSeries series(labels);
        int64_t now = GetQueryTime();
        int count = 300; // 50 minutes
        int64_t start_ts = now - ((count - 1) * 10000); // 10s step
        
        double current_val = 0;
        for (int i = 0; i < count; ++i) {
            series.add_sample(start_ts + i * 10000, current_val);
            current_val += rate_per_sec * 10.0; // Add rate * 10s
        }
        
        auto result = storage_->write(series);
        if (!result.ok()) {
            std::cerr << "Failed to write series: " << result.error() << std::endl;
        }
    }
    
    void GenerateGaugeSeries(const std::string& name, 
                            const std::map<std::string, std::string>& labels_map,
                            double value) {
        tsdb::core::Labels::Map map = labels_map;
        map["__name__"] = name;
        tsdb::core::Labels labels(map);
        
        tsdb::core::TimeSeries series(labels);
        int64_t now = GetQueryTime();
        int count = 60; // 10 minutes
        int64_t start_ts = now - ((count - 1) * 10000);
        
        for (int i = 0; i < count; ++i) {
            series.add_sample(start_ts + i * 10000, value);
        }
        
        storage_->write(series);
    }
};

TEST_F(ComplexScenariosTest, RateAndSumBy) {
    // sum by (job) (rate(complex_http_requests_total[5m]))
    // job=api: 
    //   get/200: 1.0
    //   post/200: 0.5
    //   get/500: 0.1
    //   Total: 1.6
    // job=worker:
    //   get/200: 2.0
    //   Total: 2.0
    
    int64_t timestamp = GetQueryTime();
    auto result = ExecuteQuery("sum by (job) (rate(complex_http_requests_total[5m]))", timestamp);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto& vector = result.getVector();
    ASSERT_EQ(vector.size(), 2);
    
    // Sort by value for easier checking or check map
    std::map<std::string, double> job_rates;
    for (const auto& sample : vector) {
        auto job_opt = sample.metric.GetLabelValue("job");
        std::string job = job_opt.value_or("");
        job_rates[job] = sample.value;
    }
    
    EXPECT_NEAR(job_rates["api"], 1.6, 0.1);
    EXPECT_NEAR(job_rates["worker"], 2.0, 0.1);
}

TEST_F(ComplexScenariosTest, BinaryOpWithVectorMatching) {
    // complex_node_memory_usage_bytes / complex_node_memory_total_bytes
    // 100MB / 1GB = 0.0976... (~0.1)
    
    int64_t timestamp = GetQueryTime();
    auto result = ExecuteQuery("complex_node_memory_usage_bytes / on(instance) complex_node_memory_total_bytes", timestamp);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto& vector = result.getVector();
    ASSERT_EQ(vector.size(), 1);
    
    EXPECT_NEAR(vector[0].value, 100.0 / 1024.0, 0.001);
    EXPECT_EQ(vector[0].metric.GetLabelValue("instance").value_or(""), "inst-1");
}

TEST_F(ComplexScenariosTest, FilterByRate) {
    // rate(complex_http_requests_total[5m]) > 0.8
    // Should return:
    //   get/200 (api): 1.0
    //   get/200 (worker): 2.0
    // Should filter out:
    //   post/200 (api): 0.5
    //   get/500 (api): 0.1
    
    int64_t timestamp = GetQueryTime();
    auto result = ExecuteQuery("rate(complex_http_requests_total[5m]) > 0.8", timestamp);
    
    ASSERT_EQ(result.type, tsdb::prometheus::promql::ValueType::VECTOR);
    auto& vector = result.getVector();
    ASSERT_EQ(vector.size(), 2);
    
    for (const auto& sample : vector) {
        EXPECT_GT(sample.value, 0.8);
    }
}

} // namespace comprehensive
} // namespace tsdb
