#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <random>
#include <iomanip>
#include <mutex>

#include <grpcpp/grpcpp.h>
#include <httplib.h>

#include "opentelemetry/proto/collector/metrics/v1/metrics_service.grpc.pb.h"
#include "opentelemetry/proto/metrics/v1/metrics.pb.h"
#include "opentelemetry/proto/common/v1/common.pb.h"
#include "opentelemetry/proto/resource/v1/resource.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using opentelemetry::proto::collector::metrics::v1::MetricsService;
using opentelemetry::proto::collector::metrics::v1::ExportMetricsServiceRequest;
using opentelemetry::proto::collector::metrics::v1::ExportMetricsServiceResponse;

class BenchmarkTool {
public:
    BenchmarkTool(const std::string& grpc_address, const std::string& http_address)
        : grpc_address_(grpc_address), http_address_(http_address) {
        
        // Setup gRPC channel
        grpc_channel_ = grpc::CreateChannel(grpc_address_, grpc::InsecureChannelCredentials());
        // metrics_stub_ removed, created per thread
    }

    void run_write_benchmark(int num_series, int samples_per_series) {
        std::cout << "Starting Realistic K8s Write Benchmark..." << std::endl;
        std::cout << "  Target Series: " << num_series << std::endl;
        std::cout << "  Samples/Series: " << samples_per_series << std::endl;

        auto start = std::chrono::high_resolution_clock::now();
        
        std::atomic<int> success_count{0};
        std::atomic<int> fail_count{0};
        
        // Configuration for simulation
        const int num_nodes = 50;
        const int pods_per_node = num_series / (num_nodes * 3); // Exact 3 metrics per pod
        const std::vector<std::string> namespaces = {"default", "kube-system", "monitoring", "payment", "inventory"};
        const std::vector<std::string> services = {"api-server", "db-proxy", "cache", "worker"};
        
        // Use multiple threads for writing
        int num_threads = 10;
        std::vector<std::thread> threads;
        int nodes_per_thread = std::max(1, num_nodes / num_threads);
        
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&, t]() {
                auto stub = MetricsService::NewStub(grpc_channel_);
                int start_node = t * nodes_per_thread;
                int end_node = std::min((t + 1) * nodes_per_thread, num_nodes);
                
                auto now = std::chrono::system_clock::now();
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_real_distribution<> cpu_dist(0.1, 4.0);
                std::uniform_int_distribution<> mem_dist(100 * 1024 * 1024, 1024 * 1024 * 1024);
                
                for (int n = start_node; n < end_node; ++n) {
                    std::string node_name = "node-" + std::to_string(n);
                    std::string zone = "us-west-1" + std::string(1, 'a' + (n % 3));
                    
                    // Generate Pods for this node
                    for (int p = 0; p < pods_per_node; ++p) {
                        std::string ns = namespaces[p % namespaces.size()];
                        std::string svc = services[p % services.size()];
                        std::string pod_name = svc + "-" + std::to_string(n) + "-" + std::to_string(p);
                        
                        // 1. container_cpu_usage_seconds_total
                        {
                            ExportMetricsServiceRequest request;
                            auto* rm = request.add_resource_metrics();
                            auto* sm = rm->add_scope_metrics();
                            auto* metric = sm->add_metrics();
                            metric->set_name("container_cpu_usage_seconds_total");
                            auto* gauge = metric->mutable_gauge();
                            
                            for (int j = 0; j < samples_per_series; ++j) {
                                auto* point = gauge->add_data_points();
                                auto sample_time = now - std::chrono::minutes(samples_per_series - j);
                                point->set_time_unix_nano(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                    sample_time.time_since_epoch()).count());
                                point->set_as_double(cpu_dist(gen) + (j * 0.01)); // Monotonic-ish
                                
                                auto* a1 = point->add_attributes(); a1->set_key("pod"); a1->mutable_value()->set_string_value(pod_name);
                                auto* a2 = point->add_attributes(); a2->set_key("namespace"); a2->mutable_value()->set_string_value(ns);
                                auto* a3 = point->add_attributes(); a3->set_key("node"); a3->mutable_value()->set_string_value(node_name);
                                auto* a4 = point->add_attributes(); a4->set_key("container"); a4->mutable_value()->set_string_value("main");
                            }
                            
                            ClientContext context;
                            ExportMetricsServiceResponse response;
                            if (stub->Export(&context, request, &response).ok()) success_count++; else fail_count++;
                        }
                        
                        // 2. container_memory_usage_bytes
                        {
                            ExportMetricsServiceRequest request;
                            auto* rm = request.add_resource_metrics();
                            auto* sm = rm->add_scope_metrics();
                            auto* metric = sm->add_metrics();
                            metric->set_name("container_memory_usage_bytes");
                            auto* gauge = metric->mutable_gauge();
                            
                            for (int j = 0; j < samples_per_series; ++j) {
                                auto* point = gauge->add_data_points();
                                auto sample_time = now - std::chrono::minutes(samples_per_series - j);
                                point->set_time_unix_nano(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                    sample_time.time_since_epoch()).count());
                                point->set_as_double(mem_dist(gen));
                                
                                auto* a1 = point->add_attributes(); a1->set_key("pod"); a1->mutable_value()->set_string_value(pod_name);
                                auto* a2 = point->add_attributes(); a2->set_key("namespace"); a2->mutable_value()->set_string_value(ns);
                                auto* a3 = point->add_attributes(); a3->set_key("node"); a3->mutable_value()->set_string_value(node_name);
                            }
                             ClientContext context;
                            ExportMetricsServiceResponse response;
                            if (stub->Export(&context, request, &response).ok()) success_count++; else fail_count++;
                        }
                        
                        // 3. http_requests_total
                        {
                            ExportMetricsServiceRequest request;
                            auto* rm = request.add_resource_metrics();
                            auto* sm = rm->add_scope_metrics();
                            auto* metric = sm->add_metrics();
                            metric->set_name("http_requests_total");
                            auto* gauge = metric->mutable_gauge();
                            
                            for (int j = 0; j < samples_per_series; ++j) {
                                auto* point = gauge->add_data_points();
                                auto sample_time = now - std::chrono::minutes(samples_per_series - j);
                                point->set_time_unix_nano(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                    sample_time.time_since_epoch()).count());
                                point->set_as_double(j * 10.0);
                                
                                auto* a1 = point->add_attributes(); a1->set_key("service"); a1->mutable_value()->set_string_value(svc);
                                auto* a2 = point->add_attributes(); a2->set_key("status"); a2->mutable_value()->set_string_value("200");
                                auto* a3 = point->add_attributes(); a3->set_key("method"); a3->mutable_value()->set_string_value("GET");
                                auto* a4 = point->add_attributes(); a4->set_key("pod"); a4->mutable_value()->set_string_value(pod_name);
                            }
                            ClientContext context;
                            ExportMetricsServiceResponse response;
                            if (stub->Export(&context, request, &response).ok()) success_count++; else fail_count++;
                        }
                    }
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "Write Benchmark Completed:" << std::endl;
        std::cout << "  Time: " << duration.count() << " ms" << std::endl;
        std::cout << "  Success: " << success_count << " requests (series)" << std::endl;
        std::cout << "  Failed: " << fail_count << " requests" << std::endl;
        double rate = (double)(success_count * samples_per_series) / (duration.count() / 1000.0);
        std::cout << "  Rate: " << rate << " samples/sec" << std::endl;
    }

    void run_read_benchmark(int num_queries, int concurrency, int num_series) {
        std::cout << "Starting Read Benchmark..." << std::endl;
        std::cout << "  Queries: " << num_queries << std::endl;
        std::cout << "  Concurrency: " << concurrency << std::endl;

        std::atomic<int> completed{0};
        std::atomic<int> errors{0};
        std::vector<double> latencies;
        std::mutex latencies_mutex;

        auto worker = [&]() {
            httplib::Client cli(http_address_);
            // Increase timeout for stress tests
            cli.set_read_timeout(30, 0); // 30 seconds
            
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, num_series - 1);
            
            while (true) {
                int current = completed.fetch_add(1);
                if (current >= num_queries) break;

                // Query random metrics to defeat simple caching
                int metric_id = dis(gen);
                std::string query = "benchmark_metric_" + std::to_string(metric_id);
                
                auto q_start = std::chrono::high_resolution_clock::now();
                
                auto res = cli.Get(("/api/v1/query?query=" + query).c_str());
                
                auto q_end = std::chrono::high_resolution_clock::now();
                double lat_ms = std::chrono::duration<double, std::milli>(q_end - q_start).count();

                if (res && res->status == 200) {
                    std::lock_guard<std::mutex> lock(latencies_mutex);
                    latencies.push_back(lat_ms);
                } else {
                    errors++;
                    if (errors <= 5) {
                        std::cerr << "Query failed: " << (res ? std::to_string(res->status) : "connection error") << std::endl;
                    }
                }
            }
        };

        auto start = std::chrono::high_resolution_clock::now();

        std::vector<std::thread> threads;
        for (int i = 0; i < concurrency; ++i) {
            threads.emplace_back(worker);
        }

        for (auto& t : threads) t.join();

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "Read Benchmark Completed:" << std::endl;
        std::cout << "  Time: " << duration.count() << " ms" << std::endl;
        std::cout << "  Errors: " << errors << std::endl;
        
        if (!latencies.empty()) {
            std::sort(latencies.begin(), latencies.end());
            std::cout << "  P50: " << latencies[latencies.size() * 0.50] << " ms" << std::endl;
            std::cout << "  P90: " << latencies[latencies.size() * 0.90] << " ms" << std::endl;
            std::cout << "  P99: " << latencies[latencies.size() * 0.99] << " ms" << std::endl;
            std::cout << "  Max: " << latencies.back() << " ms" << std::endl;
        }
    }

private:
    std::string grpc_address_;
    std::string http_address_;
    std::shared_ptr<Channel> grpc_channel_;
};

void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [options]\n"
              << "Options:\n"
              << "  --grpc-addr ADDR    gRPC server address (default: localhost:50051)\n"
              << "  --http-addr ADDR    HTTP server address (default: localhost:8080)\n"
              << "  --series N          Number of time series (default: 1000)\n"
              << "  --samples N         Samples per series (default: 10)\n"
              << "  --queries N         Number of queries (default: 1000)\n"
              << "  --concurrency N     Query concurrency (default: 10)\n"
              << "  --help              Show this help\n";
}

int main(int argc, char** argv) {
    std::string grpc_addr = "localhost:50051";
    std::string http_addr = "localhost:8080";
    int num_series = 1000;
    int samples_per_series = 10;
    int num_queries = 1000;
    int concurrency = 10;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--grpc-addr" && i + 1 < argc) grpc_addr = argv[++i];
        else if (arg == "--http-addr" && i + 1 < argc) http_addr = argv[++i];
        else if (arg == "--series" && i + 1 < argc) num_series = std::stoi(argv[++i]);
        else if (arg == "--samples" && i + 1 < argc) samples_per_series = std::stoi(argv[++i]);
        else if (arg == "--queries" && i + 1 < argc) num_queries = std::stoi(argv[++i]);
        else if (arg == "--concurrency" && i + 1 < argc) concurrency = std::stoi(argv[++i]);
        else if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
    }

    BenchmarkTool tool(grpc_addr, http_addr);
    
    tool.run_write_benchmark(num_series, samples_per_series);
    
    // Wait a bit for data to be indexed/processed
    std::cout << "Waiting 2 seconds for processing..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    tool.run_read_benchmark(num_queries, concurrency, num_series);

    return 0;
}
