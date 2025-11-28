#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <thread>
#include <random>
#include <map>
#include <iomanip>
#include <sstream>

#include <grpcpp/grpcpp.h>
#include <grpcpp/client_context.h>
#include "opentelemetry/proto/collector/metrics/v1/metrics_service.grpc.pb.h"
#include "opentelemetry/proto/metrics/v1/metrics.pb.h"
#include "opentelemetry/proto/common/v1/common.pb.h"
#include "opentelemetry/proto/resource/v1/resource.pb.h"

using namespace opentelemetry::proto::collector::metrics::v1;
using namespace opentelemetry::proto::metrics::v1;
using namespace opentelemetry::proto::common::v1;
using namespace opentelemetry::proto::resource::v1;

// Configuration
struct Config {
    std::string server_address = "127.0.0.1:9090"; // Default TSDB gRPC port? Check server config.
    int num_nodes = 5;
    int pods_per_node = 10;
    int duration_hours = 1;
    int scrape_interval_seconds = 15;
    bool verbose = false;
};

// Helper to create attributes
void add_attribute(google::protobuf::RepeatedPtrField<KeyValue>* attributes, const std::string& key, const std::string& value) {
    auto* kv = attributes->Add();
    kv->set_key(key);
    kv->mutable_value()->set_string_value(value);
}

void add_attribute(NumberDataPoint* point, const std::string& key, const std::string& value) {
    auto* kv = point->add_attributes();
    kv->set_key(key);
    kv->mutable_value()->set_string_value(value);
}

// Simulation Entities
struct Node {
    std::string name;
    std::string zone;
    std::string region;
    // State
    double cpu_usage = 0.1;
    double memory_usage = 0.2;
};

struct Pod {
    std::string name;
    std::string namespace_;
    std::string service;
    std::string node_name;
    std::string phase = "Running";
    std::string pod_ip;
    // State
    double cpu_usage = 0.0;
    double memory_usage = 0.0;
    double request_rate = 0.0;
    int64_t requests_total = 0;
};

class ClusterSimulator {
public:
    ClusterSimulator(const Config& config) : config_(config), gen_(rd_()) {
        init_cluster();
    }

    void init_cluster() {
        std::cout << "Initializing cluster with " << config_.num_nodes << " nodes..." << std::endl;
        
        // Create Nodes
        for (int i = 0; i < config_.num_nodes; ++i) {
            Node node;
            node.name = "node-" + std::to_string(i);
            node.zone = (i % 2 == 0) ? "us-west-1a" : "us-west-1b";
            node.region = "us-west-1";
            nodes_.push_back(node);
        }

        // Create Pods
        std::vector<std::string> services = {"frontend", "backend", "db", "cache", "auth"};
        std::vector<std::string> namespaces = {"default", "kube-system", "monitoring"};

        for (const auto& node : nodes_) {
            for (int i = 0; i < config_.pods_per_node; ++i) {
                Pod pod;
                pod.service = services[i % services.size()];
                pod.namespace_ = (pod.service == "db" || pod.service == "cache") ? "data" : "default";
                pod.name = pod.service + "-" + std::to_string(std::rand() % 10000) + "-" + std::to_string(i);
                pod.node_name = node.name;
                pod.pod_ip = "10.0." + std::to_string(std::rand() % 255) + "." + std::to_string(std::rand() % 255);
                pods_.push_back(pod);
            }
        }
        std::cout << "Created " << pods_.size() << " pods." << std::endl;
    }

    void step(int64_t timestamp_ns) {
        // Update state
        std::uniform_real_distribution<> cpu_dist(0.0, 1.0);
        std::uniform_real_distribution<> mem_dist(0.0, 1.0);
        std::uniform_real_distribution<> req_dist(0.0, 100.0);

        for (auto& node : nodes_) {
            // Random walk
            node.cpu_usage += (cpu_dist(gen_) - 0.5) * 0.1;
            node.cpu_usage = std::max(0.0, std::min(1.0, node.cpu_usage));
            
            node.memory_usage += (mem_dist(gen_) - 0.5) * 0.05;
            node.memory_usage = std::max(0.0, std::min(1.0, node.memory_usage));
        }

        for (auto& pod : pods_) {
            pod.cpu_usage = std::abs(std::sin(timestamp_ns / 1e9 / 3600.0 * 2 * 3.14159)) * 0.8 + (cpu_dist(gen_) * 0.2); // Daily cycle + noise
            pod.memory_usage += (mem_dist(gen_) - 0.5) * 0.01; // Slow drift
            pod.memory_usage = std::max(0.1, std::min(0.9, pod.memory_usage));
            
            double rate = std::abs(std::sin(timestamp_ns / 1e9 / 3600.0 * 2 * 3.14159)) * 50.0 + (req_dist(gen_));
            pod.request_rate = rate;
            pod.requests_total += static_cast<int64_t>(rate * config_.scrape_interval_seconds);
        }
    }

    ExportMetricsServiceRequest generate_metrics(int64_t timestamp_ns) {
        ExportMetricsServiceRequest request;
        auto* resource_metrics = request.add_resource_metrics();
        
        // Resource: Cluster
        auto* resource = resource_metrics->mutable_resource();
        add_attribute(resource->mutable_attributes(), "cluster", "test-cluster");
        
        auto* scope_metrics = resource_metrics->add_scope_metrics();
        scope_metrics->mutable_scope()->set_name("synthetic-generator");

        // 1. Node Metrics
        for (const auto& node : nodes_) {
            // node_cpu_usage
            {
                auto* metric = scope_metrics->add_metrics();
                metric->set_name("node_cpu_usage_ratio");
                auto* gauge = metric->mutable_gauge();
                auto* point = gauge->add_data_points();
                point->set_time_unix_nano(timestamp_ns);
                point->set_as_double(node.cpu_usage);
                add_attribute(point, "node", node.name);
                add_attribute(point, "zone", node.zone);
                add_attribute(point, "region", node.region);
            }
            // node_memory_usage
            {
                auto* metric = scope_metrics->add_metrics();
                metric->set_name("node_memory_usage_ratio");
                auto* gauge = metric->mutable_gauge();
                auto* point = gauge->add_data_points();
                point->set_time_unix_nano(timestamp_ns);
                point->set_as_double(node.memory_usage);
                add_attribute(point, "node", node.name);
            }
        }

        // 2. Pod Metrics
        for (const auto& pod : pods_) {
            // container_cpu_usage_seconds_total (Counter)
            // We simulate this as a gauge for simplicity in generator, but name it like a counter
            // Actually, let's make it a cumulative counter
            // But for now, let's just use gauge for usage ratio
            {
                auto* metric = scope_metrics->add_metrics();
                metric->set_name("container_cpu_usage_ratio");
                auto* gauge = metric->mutable_gauge();
                auto* point = gauge->add_data_points();
                point->set_time_unix_nano(timestamp_ns);
                point->set_as_double(pod.cpu_usage);
                add_attribute(point, "pod", pod.name);
                add_attribute(point, "namespace", pod.namespace_);
                add_attribute(point, "service", pod.service);
                add_attribute(point, "node", pod.node_name);
            }
            
            // http_requests_total (Counter)
            {
                auto* metric = scope_metrics->add_metrics();
                metric->set_name("http_requests_total");
                auto* sum = metric->mutable_sum();
                sum->set_is_monotonic(true);
                sum->set_aggregation_temporality(AggregationTemporality::AGGREGATION_TEMPORALITY_CUMULATIVE);
                auto* point = sum->add_data_points();
                point->set_time_unix_nano(timestamp_ns);
                point->set_as_double(static_cast<double>(pod.requests_total));
                add_attribute(point, "pod", pod.name);
                add_attribute(point, "service", pod.service);
                add_attribute(point, "method", "GET");
                add_attribute(point, "status", "200");
            }
        }

        return request;
    }

private:
    Config config_;
    std::vector<Node> nodes_;
    std::vector<Pod> pods_;
    std::random_device rd_;
    std::mt19937 gen_;
};

int main(int argc, char** argv) {
    Config config;
    
    // Simple arg parsing
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--address" && i + 1 < argc) config.server_address = argv[++i];
        else if (arg == "--nodes" && i + 1 < argc) config.num_nodes = std::stoi(argv[++i]);
        else if (arg == "--pods" && i + 1 < argc) config.pods_per_node = std::stoi(argv[++i]);
        else if (arg == "--hours" && i + 1 < argc) config.duration_hours = std::stoi(argv[++i]);
        else if (arg == "--verbose") config.verbose = true;
    }

    std::cout << "Starting Synthetic Cluster Generator" << std::endl;
    std::cout << "Target: " << config.server_address << std::endl;
    std::cout << "Simulation: " << config.num_nodes << " nodes, " << config.pods_per_node << " pods/node" << std::endl;
    std::cout << "Duration: " << config.duration_hours << " hours" << std::endl;

    // Connect to gRPC
    auto channel = grpc::CreateChannel(config.server_address, grpc::InsecureChannelCredentials());
    auto stub = MetricsService::NewStub(channel);

    // Wait for connection
    if (!channel->WaitForConnected(std::chrono::system_clock::now() + std::chrono::seconds(5))) {
        std::cerr << "Failed to connect to " << config.server_address << std::endl;
        return 1;
    }
    std::cout << "Connected to gRPC server." << std::endl;

    ClusterSimulator simulator(config);

    auto start_time = std::chrono::system_clock::now();
    int64_t start_ts_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(start_time.time_since_epoch()).count();
    int64_t end_ts_ns = start_ts_ns + static_cast<int64_t>(config.duration_hours) * 3600 * 1000000000LL;
    int64_t step_ns = config.scrape_interval_seconds * 1000000000LL;

    int64_t current_ts_ns = start_ts_ns;
    int steps = 0;
    int total_steps = (config.duration_hours * 3600) / config.scrape_interval_seconds;

    while (current_ts_ns < end_ts_ns) {
        simulator.step(current_ts_ns);
        auto request = simulator.generate_metrics(current_ts_ns);

        grpc::ClientContext context;
        ExportMetricsServiceResponse response;
        grpc::Status status = stub->Export(&context, request, &response);

        if (!status.ok()) {
            std::cerr << "Export failed: " << status.error_message() << std::endl;
            // Continue or exit?
        }

        if (config.verbose || steps % 100 == 0) {
            std::cout << "Step " << steps << "/" << total_steps << " (" << (steps * 100 / total_steps) << "%) - Sent " 
                      << request.resource_metrics(0).scope_metrics(0).metrics_size() << " metrics" << std::endl;
        }

        current_ts_ns += step_ns;
        steps++;
        
        // Optional: sleep to simulate real-time if needed, but for data gen we usually want fast-forward
        // std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "Generation complete." << std::endl;
    return 0;
}
