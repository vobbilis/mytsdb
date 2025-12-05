#include <gtest/gtest.h>
#include "tsdb/storage/storage_impl.h"
#include "tsdb/prometheus/remote/write_handler.h"
#include "tsdb/prometheus/remote/read_handler.h"
#include "tsdb/prometheus/remote/converter.h"
#include "remote.pb.h"
#include <filesystem>
#include <chrono>

using namespace tsdb;
using namespace tsdb::prometheus::remote;

// Alias to avoid namespace conflict
namespace prom_proto = ::prometheus;
namespace prom_http = tsdb::prometheus;

class PrometheusRemoteStorageIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for test data
        test_dir_ = std::filesystem::temp_directory_path() / "prometheus_remote_test";
        std::filesystem::remove_all(test_dir_);
        std::filesystem::create_directories(test_dir_);
        
        // Create storage
        core::StorageConfig config = core::StorageConfig::Default();
        config.data_dir = test_dir_.string();
        
        storage_ = std::make_shared<storage::StorageImpl>(config);
        auto init_result = storage_->init(config);
        ASSERT_TRUE(init_result.ok()) << "Failed to initialize storage: " << init_result.error();
        
        // Create handlers
        write_handler_ = std::make_shared<WriteHandler>(storage_);
        read_handler_ = std::make_shared<ReadHandler>(storage_);
    }
    
    void TearDown() override {
        if (storage_) {
            storage_->close();
        }
        std::filesystem::remove_all(test_dir_);
    }
    
    // Helper to create a WriteRequest with real-world data
    prom_proto::WriteRequest CreateCPUMetricsWriteRequest() {
        prom_proto::WriteRequest request;
        
        // Simulate CPU usage metrics from 3 hosts over 5 minutes
        std::vector<std::string> hosts = {"server1", "server2", "server3"};
        auto now = std::chrono::system_clock::now();
        auto base_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        
        for (const auto& host : hosts) {
            auto* ts = request.add_timeseries();
            
            // Add labels
            auto* name_label = ts->add_labels();
            name_label->set_name("__name__");
            name_label->set_value("cpu_usage_percent");
            
            auto* host_label = ts->add_labels();
            host_label->set_name("host");
            host_label->set_value(host);
            
            auto* job_label = ts->add_labels();
            job_label->set_name("job");
            job_label->set_value("node_exporter");
            
            auto* env_label = ts->add_labels();
            env_label->set_name("environment");
            env_label->set_value("production");
            
            // Add samples (5 minutes of data at 15s intervals = 20 samples)
            for (int i = 0; i < 20; i++) {
                auto* sample = ts->add_samples();
                sample->set_timestamp(base_time - (19 - i) * 15000); // 15s intervals
                // Simulate realistic CPU usage (20-80%)
                double cpu_usage = 30.0 + (std::hash<std::string>{}(host + std::to_string(i)) % 50);
                sample->set_value(cpu_usage);
            }
        }
        
        return request;
    }
    
    // Helper to create a ReadRequest
    prom_proto::ReadRequest CreateReadRequest(
        const std::string& metric_name,
        const std::string& host_filter,
        int64_t start_time,
        int64_t end_time) {
        
        prom_proto::ReadRequest request;
        auto* query = request.add_queries();
        
        query->set_start_timestamp_ms(start_time);
        query->set_end_timestamp_ms(end_time);
        
        // Add metric name matcher
        auto* name_matcher = query->add_matchers();
        name_matcher->set_type(prom_proto::LabelMatcher::EQ);
        name_matcher->set_name("__name__");
        name_matcher->set_value(metric_name);
        
        // Add host matcher if specified
        if (!host_filter.empty()) {
            auto* host_matcher = query->add_matchers();
            host_matcher->set_type(prom_proto::LabelMatcher::EQ);
            host_matcher->set_name("host");
            host_matcher->set_value(host_filter);
        }
        
        return request;
    }
    
    std::filesystem::path test_dir_;
    std::shared_ptr<storage::StorageImpl> storage_;
    std::shared_ptr<WriteHandler> write_handler_;
    std::shared_ptr<ReadHandler> read_handler_;
};

TEST_F(PrometheusRemoteStorageIntegrationTest, WriteAndReadCPUMetrics) {
    // 1. Create write request with CPU metrics
    auto write_req = CreateCPUMetricsWriteRequest();
    
    // Serialize to protobuf
    std::string write_body;
    ASSERT_TRUE(write_req.SerializeToString(&write_body));
    
    // 2. Send write request
    prom_http::Request write_http_req;
    write_http_req.method = "POST";
    write_http_req.path = "/api/v1/write";
    write_http_req.body = write_body;
    write_http_req.headers["Content-Type"] = "application/x-protobuf";
    
    std::string write_response;
    write_handler_->Handle(write_http_req, write_response);
    
    // Verify write succeeded
    EXPECT_EQ(write_response, "{}") << "Write failed: " << write_response;
    
    // 3. Create read request for all CPU metrics
    auto now = std::chrono::system_clock::now();
    auto base_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    auto read_req = CreateReadRequest(
        "cpu_usage_percent",
        "",  // No host filter - get all hosts
        base_time - 6 * 60 * 1000,  // 6 minutes ago
        base_time
    );
    
    // Serialize read request
    std::string read_body;
    ASSERT_TRUE(read_req.SerializeToString(&read_body));
    
    // 4. Send read request
    prom_http::Request read_http_req;
    read_http_req.method = "POST";
    read_http_req.path = "/api/v1/read";
    read_http_req.body = read_body;
    read_http_req.headers["Content-Type"] = "application/x-protobuf";
    
    std::string read_response;
    read_handler_->Handle(read_http_req, read_response);
    
    // 5. Parse read response
    prom_proto::ReadResponse read_resp;
    ASSERT_TRUE(read_resp.ParseFromString(read_response)) 
        << "Failed to parse read response";
    
    // 6. Verify results
    ASSERT_EQ(read_resp.results_size(), 1);
    const auto& result = read_resp.results(0);
    
    // Should have 3 time series (one per host)
    EXPECT_EQ(result.timeseries_size(), 3) 
        << "Expected 3 time series (one per host)";
    
    // Verify each time series
    std::set<std::string> found_hosts;
    for (const auto& ts : result.timeseries()) {
        // Check labels
        EXPECT_GE(ts.labels_size(), 4) << "Expected at least 4 labels";
        
        std::string host;
        std::string metric_name;
        for (const auto& label : ts.labels()) {
            if (label.name() == "host") {
                host = label.value();
            } else if (label.name() == "__name__") {
                metric_name = label.value();
            }
        }
        
        EXPECT_EQ(metric_name, "cpu_usage_percent");
        EXPECT_FALSE(host.empty());
        found_hosts.insert(host);
        
        // Check samples (should have 20 samples)
        EXPECT_EQ(ts.samples_size(), 20) 
            << "Expected 20 samples for host " << host;
        
        // Verify sample values are reasonable (20-80% CPU)
        for (const auto& sample : ts.samples()) {
            EXPECT_GE(sample.value(), 20.0);
            EXPECT_LE(sample.value(), 80.0);
            EXPECT_GT(sample.timestamp(), 0);
        }
        
        // Verify samples are in chronological order
        for (int i = 1; i < ts.samples_size(); i++) {
            EXPECT_GT(ts.samples(i).timestamp(), ts.samples(i-1).timestamp())
                << "Samples should be in chronological order";
        }
    }
    
    // Verify we got all 3 hosts
    EXPECT_EQ(found_hosts.size(), 3);
    EXPECT_TRUE(found_hosts.count("server1") > 0);
    EXPECT_TRUE(found_hosts.count("server2") > 0);
    EXPECT_TRUE(found_hosts.count("server3") > 0);
}

TEST_F(PrometheusRemoteStorageIntegrationTest, WriteAndReadWithHostFilter) {
    // 1. Write CPU metrics
    auto write_req = CreateCPUMetricsWriteRequest();
    std::string write_body;
    ASSERT_TRUE(write_req.SerializeToString(&write_body));
    
    prom_http::Request write_http_req;
    write_http_req.method = "POST";
    write_http_req.path = "/api/v1/write";
    write_http_req.body = write_body;
    
    std::string write_response;
    write_handler_->Handle(write_http_req, write_response);
    EXPECT_EQ(write_response, "{}");
    
    // 2. Read with host filter (only server1)
    auto now = std::chrono::system_clock::now();
    auto base_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    auto read_req = CreateReadRequest(
        "cpu_usage_percent",
        "server1",  // Filter for server1 only
        base_time - 6 * 60 * 1000,
        base_time
    );
    
    std::string read_body;
    ASSERT_TRUE(read_req.SerializeToString(&read_body));
    
    prom_http::Request read_http_req;
    read_http_req.method = "POST";
    read_http_req.path = "/api/v1/read";
    read_http_req.body = read_body;
    
    std::string read_response;
    read_handler_->Handle(read_http_req, read_response);
    
    // 3. Parse and verify
    prom_proto::ReadResponse read_resp;
    ASSERT_TRUE(read_resp.ParseFromString(read_response));
    
    ASSERT_EQ(read_resp.results_size(), 1);
    const auto& result = read_resp.results(0);
    
    // Should have only 1 time series (server1)
    EXPECT_EQ(result.timeseries_size(), 1);
    
    if (result.timeseries_size() > 0) {
        const auto& ts = result.timeseries(0);
        
        // Verify it's server1
        bool found_server1 = false;
        for (const auto& label : ts.labels()) {
            if (label.name() == "host" && label.value() == "server1") {
                found_server1 = true;
                break;
            }
        }
        EXPECT_TRUE(found_server1);
        
        // Verify samples
        EXPECT_EQ(ts.samples_size(), 20);
    }
}

TEST_F(PrometheusRemoteStorageIntegrationTest, MultipleMetricTypes) {
    prom_proto::WriteRequest request;
    auto now = std::chrono::system_clock::now();
    auto base_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    // Add different metric types
    std::vector<std::pair<std::string, double>> metrics = {
        {"memory_usage_bytes", 1024 * 1024 * 512},  // 512 MB
        {"disk_io_bytes_total", 1024 * 1024 * 1024}, // 1 GB
        {"network_rx_bytes_total", 1024 * 1024 * 100}, // 100 MB
        {"http_requests_total", 12345}
    };
    
    for (const auto& [metric_name, value] : metrics) {
        auto* ts = request.add_timeseries();
        
        auto* name_label = ts->add_labels();
        name_label->set_name("__name__");
        name_label->set_value(metric_name);
        
        auto* host_label = ts->add_labels();
        host_label->set_name("host");
        host_label->set_value("server1");
        
        // Add 10 samples
        for (int i = 0; i < 10; i++) {
            auto* sample = ts->add_samples();
            sample->set_timestamp(base_time - (9 - i) * 60000); // 1 minute intervals
            sample->set_value(value + i * 100);
        }
    }
    
    // Write
    std::string write_body;
    ASSERT_TRUE(request.SerializeToString(&write_body));
    
    prom_http::Request write_http_req;
    write_http_req.method = "POST";
    write_http_req.body = write_body;
    
    std::string write_response;
    write_handler_->Handle(write_http_req, write_response);
    EXPECT_EQ(write_response, "{}");
    
    // Read each metric type
    for (const auto& [metric_name, expected_value] : metrics) {
        auto read_req = CreateReadRequest(
            metric_name,
            "server1",
            base_time - 15 * 60 * 1000,
            base_time
        );
        
        std::string read_body;
        ASSERT_TRUE(read_req.SerializeToString(&read_body));
        
        prom_http::Request read_http_req;
        read_http_req.method = "POST";
        read_http_req.body = read_body;
        
        std::string read_response;
        read_handler_->Handle(read_http_req, read_response);
        
        prom_proto::ReadResponse read_resp;
        ASSERT_TRUE(read_resp.ParseFromString(read_response));
        
        ASSERT_EQ(read_resp.results_size(), 1);
        ASSERT_EQ(read_resp.results(0).timeseries_size(), 1) 
            << "Failed to read metric: " << metric_name;
        
        const auto& ts = read_resp.results(0).timeseries(0);
        EXPECT_EQ(ts.samples_size(), 10) 
            << "Expected 10 samples for " << metric_name;
    }
}

TEST_F(PrometheusRemoteStorageIntegrationTest, EmptyReadRequest) {
    // Try to read before writing anything
    auto now = std::chrono::system_clock::now();
    auto base_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    auto read_req = CreateReadRequest(
        "nonexistent_metric",
        "",
        base_time - 60 * 60 * 1000,
        base_time
    );
    
    std::string read_body;
    ASSERT_TRUE(read_req.SerializeToString(&read_body));
    
    prom_http::Request read_http_req;
    read_http_req.method = "POST";
    read_http_req.body = read_body;
    
    std::string read_response;
    read_handler_->Handle(read_http_req, read_response);
    
    prom_proto::ReadResponse read_resp;
    ASSERT_TRUE(read_resp.ParseFromString(read_response));
    
    // Should return empty results, not an error
    ASSERT_EQ(read_resp.results_size(), 1);
    EXPECT_EQ(read_resp.results(0).timeseries_size(), 0);
}

TEST_F(PrometheusRemoteStorageIntegrationTest, InvalidWriteRequest) {
    prom_http::Request write_http_req;
    write_http_req.method = "POST";
    write_http_req.body = "invalid protobuf data";
    
    std::string write_response;
    write_handler_->Handle(write_http_req, write_response);
    
    // Should return error
    EXPECT_NE(write_response, "{}");
    EXPECT_TRUE(write_response.find("error") != std::string::npos);
}

TEST_F(PrometheusRemoteStorageIntegrationTest, InvalidReadRequest) {
    prom_http::Request read_http_req;
    read_http_req.method = "POST";
    read_http_req.body = "invalid protobuf data";
    
    std::string read_response;
    read_handler_->Handle(read_http_req, read_response);
    
    // Should return error
    EXPECT_TRUE(read_response.find("error") != std::string::npos);
}
