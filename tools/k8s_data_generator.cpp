/**
 * Large-Scale K8s Metrics Data Generator
 * 
 * Generates metrics simulating a production Kubernetes cluster:
 * - 9,000+ pods across multiple regions/zones/clusters
 * - 100 metric types per container
 * - 12 label dimensions (region, zone, cluster, namespace, app, service, deployment, pod, container, node, instance, job)
 * - Historical data with realistic patterns
 */

#include <arrow/api.h>
#include <arrow/flight/api.h>
#include <iostream>
#include <chrono>
#include <random>
#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <iomanip>
#include <sstream>

struct ClusterConfig {
    // Geographic distribution
    std::vector<std::string> regions = {"us-east-1", "us-west-2", "eu-west-1"};
    int zones_per_region = 3;   // a, b, c
    int clusters_per_zone = 1;
    
    // Cluster topology
    int namespaces_per_cluster = 10;
    int services_per_namespace = 20;
    int pods_per_service = 5;  // replicas
    int containers_per_pod = 2;
    int nodes_per_cluster = 20;
    
    // Data generation
    int days_of_data = 7;
    int scrape_interval_sec = 15;
    int batch_size = 5000;
    int workers = 4;
    
    // Metrics
    int metrics_per_container = 100;
    
    int total_pods() const {
        return regions.size() * zones_per_region * clusters_per_zone *
               namespaces_per_cluster * services_per_namespace * pods_per_service;
    }
    
    int64_t estimated_samples() const {
        int64_t intervals = (int64_t)days_of_data * 24 * 3600 / scrape_interval_sec;
        return (int64_t)total_pods() * containers_per_pod * metrics_per_container * intervals;
    }
};

// 100 Metric Types (realistic K8s/Prometheus metrics)
const std::vector<std::pair<std::string, std::string>> METRICS = {
    // Container metrics (20)
    {"container_cpu_usage_seconds_total", "counter"},
    {"container_cpu_user_seconds_total", "counter"},
    {"container_cpu_system_seconds_total", "counter"},
    {"container_memory_usage_bytes", "gauge"},
    {"container_memory_working_set_bytes", "gauge"},
    {"container_memory_rss", "gauge"},
    {"container_memory_cache", "gauge"},
    {"container_memory_swap", "gauge"},
    {"container_memory_max_usage_bytes", "gauge"},
    {"container_network_receive_bytes_total", "counter"},
    {"container_network_transmit_bytes_total", "counter"},
    {"container_network_receive_packets_total", "counter"},
    {"container_network_transmit_packets_total", "counter"},
    {"container_network_receive_errors_total", "counter"},
    {"container_network_transmit_errors_total", "counter"},
    {"container_fs_reads_bytes_total", "counter"},
    {"container_fs_writes_bytes_total", "counter"},
    {"container_fs_read_seconds_total", "counter"},
    {"container_fs_write_seconds_total", "counter"},
    {"container_fs_usage_bytes", "gauge"},
    
    // kube-state-metrics (20)
    {"kube_pod_container_resource_requests_cpu_cores", "gauge"},
    {"kube_pod_container_resource_requests_memory_bytes", "gauge"},
    {"kube_pod_container_resource_limits_cpu_cores", "gauge"},
    {"kube_pod_container_resource_limits_memory_bytes", "gauge"},
    {"kube_pod_status_phase", "gauge"},
    {"kube_pod_status_ready", "gauge"},
    {"kube_pod_container_status_running", "gauge"},
    {"kube_pod_container_status_waiting", "gauge"},
    {"kube_pod_container_status_terminated", "gauge"},
    {"kube_pod_container_status_restarts_total", "counter"},
    {"kube_deployment_status_replicas", "gauge"},
    {"kube_deployment_status_replicas_available", "gauge"},
    {"kube_deployment_status_replicas_updated", "gauge"},
    {"kube_deployment_spec_replicas", "gauge"},
    {"kube_replicaset_status_ready_replicas", "gauge"},
    {"kube_service_info", "gauge"},
    {"kube_endpoint_info", "gauge"},
    {"kube_namespace_status_phase", "gauge"},
    {"kube_node_status_condition", "gauge"},
    {"kube_node_status_allocatable", "gauge"},
    
    // Node metrics (20)
    {"node_cpu_seconds_total", "counter"},
    {"node_memory_MemTotal_bytes", "gauge"},
    {"node_memory_MemFree_bytes", "gauge"},
    {"node_memory_MemAvailable_bytes", "gauge"},
    {"node_memory_Buffers_bytes", "gauge"},
    {"node_memory_Cached_bytes", "gauge"},
    {"node_memory_SwapTotal_bytes", "gauge"},
    {"node_memory_SwapFree_bytes", "gauge"},
    {"node_disk_read_bytes_total", "counter"},
    {"node_disk_written_bytes_total", "counter"},
    {"node_disk_reads_completed_total", "counter"},
    {"node_disk_writes_completed_total", "counter"},
    {"node_disk_io_time_seconds_total", "counter"},
    {"node_network_receive_bytes_total", "counter"},
    {"node_network_transmit_bytes_total", "counter"},
    {"node_network_receive_packets_total", "counter"},
    {"node_network_transmit_packets_total", "counter"},
    {"node_load1", "gauge"},
    {"node_load5", "gauge"},
    {"node_load15", "gauge"},
    
    // HTTP/service metrics (20)
    {"http_requests_total", "counter"},
    {"http_request_duration_seconds_bucket", "histogram"},
    {"http_request_duration_seconds_count", "counter"},
    {"http_request_duration_seconds_sum", "counter"},
    {"http_request_size_bytes_bucket", "histogram"},
    {"http_request_size_bytes_count", "counter"},
    {"http_request_size_bytes_sum", "counter"},
    {"http_response_size_bytes_bucket", "histogram"},
    {"http_response_size_bytes_count", "counter"},
    {"http_response_size_bytes_sum", "counter"},
    {"http_requests_in_flight", "gauge"},
    {"grpc_server_handled_total", "counter"},
    {"grpc_server_started_total", "counter"},
    {"grpc_server_msg_received_total", "counter"},
    {"grpc_server_msg_sent_total", "counter"},
    {"grpc_server_handling_seconds_bucket", "histogram"},
    {"grpc_server_handling_seconds_count", "counter"},
    {"grpc_server_handling_seconds_sum", "counter"},
    {"grpc_client_handled_total", "counter"},
    {"grpc_client_started_total", "counter"},
    
    // Application metrics (20)
    {"up", "gauge"},
    {"process_cpu_seconds_total", "counter"},
    {"process_resident_memory_bytes", "gauge"},
    {"process_virtual_memory_bytes", "gauge"},
    {"process_open_fds", "gauge"},
    {"process_max_fds", "gauge"},
    {"process_start_time_seconds", "gauge"},
    {"go_goroutines", "gauge"},
    {"go_threads", "gauge"},
    {"go_gc_duration_seconds_count", "counter"},
    {"go_gc_duration_seconds_sum", "counter"},
    {"go_memstats_alloc_bytes", "gauge"},
    {"go_memstats_heap_alloc_bytes", "gauge"},
    {"go_memstats_heap_objects", "gauge"},
    {"promhttp_metric_handler_requests_total", "counter"},
    {"promhttp_metric_handler_requests_in_flight", "gauge"},
    {"scrape_duration_seconds", "gauge"},
    {"scrape_samples_scraped", "gauge"},
    {"scrape_series_added", "gauge"},
    {"scrape_samples_post_metric_relabeling", "gauge"},
};

class K8sDataGenerator {
public:
    K8sDataGenerator(const ClusterConfig& config, std::shared_ptr<arrow::flight::FlightClient> client)
        : config_(config), client_(std::move(client)), gen_(std::random_device{}()) {}

    void Generate() {
        PrintConfig();
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Calculate time range
        auto now = std::chrono::system_clock::now();
        int64_t end_time = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        int64_t start_time = end_time - (int64_t)config_.days_of_data * 86400 * 1000;
        
        // Generate data for each region
        std::vector<std::thread> workers;
        std::atomic<int64_t> total_samples{0};
        std::atomic<int> completed_regions{0};
        
        for (const auto& region : config_.regions) {
            workers.emplace_back([this, &region, start_time, end_time, &total_samples, &completed_regions]() {
                GenerateRegion(region, start_time, end_time, total_samples);
                completed_regions++;
                std::cout << "Region " << region << " complete (" << completed_regions << "/" << config_.regions.size() << ")" << std::endl;
            });
        }
        
        for (auto& w : workers) w.join();
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
        
        std::cout << "\n=== Generation Complete ===" << std::endl;
        std::cout << "Total Samples: " << total_samples << std::endl;
        std::cout << "Time: " << duration << " seconds" << std::endl;
        std::cout << "Rate: " << (duration > 0 ? total_samples / duration : 0) << " samples/sec" << std::endl;
    }

private:
    void PrintConfig() {
        std::cout << "=== Large-Scale K8s Data Generator ===" << std::endl;
        std::cout << "Regions: " << config_.regions.size() << " (";
        for (size_t i = 0; i < config_.regions.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << config_.regions[i];
        }
        std::cout << ")" << std::endl;
        std::cout << "Zones/Region: " << config_.zones_per_region << std::endl;
        std::cout << "Clusters/Zone: " << config_.clusters_per_zone << std::endl;
        std::cout << "Namespaces/Cluster: " << config_.namespaces_per_cluster << std::endl;
        std::cout << "Services/Namespace: " << config_.services_per_namespace << std::endl;
        std::cout << "Pods/Service: " << config_.pods_per_service << std::endl;
        std::cout << "Total Pods: " << config_.total_pods() << std::endl;
        std::cout << "Metrics/Container: " << config_.metrics_per_container << std::endl;
        std::cout << "Days of Data: " << config_.days_of_data << std::endl;
        std::cout << "Estimated Samples: " << config_.estimated_samples() << " (~" 
                  << config_.estimated_samples()/1000000 << "M)" << std::endl;
        std::cout << std::endl;
    }

    void GenerateRegion(const std::string& region, int64_t start_time, int64_t end_time, std::atomic<int64_t>& total) {
        for (int zone_idx = 0; zone_idx < config_.zones_per_region; ++zone_idx) {
            std::string zone = region + static_cast<char>('a' + zone_idx);  // e.g., us-east-1a
            
            for (int cluster_idx = 0; cluster_idx < config_.clusters_per_zone; ++cluster_idx) {
                std::string cluster = region + "-cluster-" + std::to_string(cluster_idx);
                GenerateCluster(region, zone, cluster, start_time, end_time, total);
            }
        }
    }

    void GenerateCluster(const std::string& region, const std::string& zone, const std::string& cluster,
                         int64_t start_time, int64_t end_time, std::atomic<int64_t>& total) {
        // Generate node pool for this cluster
        std::vector<std::string> nodes;
        for (int i = 0; i < config_.nodes_per_cluster; ++i) {
            nodes.push_back(cluster + "-node-" + std::to_string(i));
        }
        
        std::uniform_int_distribution<> node_dist(0, nodes.size() - 1);
        
        for (int ns_idx = 0; ns_idx < config_.namespaces_per_cluster; ++ns_idx) {
            std::string ns = "ns-" + std::to_string(ns_idx);
            
            // Generate services in this namespace
            for (int svc_idx = 0; svc_idx < config_.services_per_namespace; ++svc_idx) {
                std::string app = "app-" + std::to_string(svc_idx % 10);
                std::string service = app + "-svc";
                std::string deployment = app + "-deploy";
                
                // Generate pods for this service (replicas)
                for (int pod_idx = 0; pod_idx < config_.pods_per_service; ++pod_idx) {
                    std::string pod = deployment + "-" + GeneratePodSuffix();
                    std::string node = nodes[node_dist(gen_)];
                    std::string instance = GenerateInstanceIP() + ":9090";
                    
                    // Generate containers for this pod
                    for (int cont_idx = 0; cont_idx < config_.containers_per_pod; ++cont_idx) {
                        std::string container = (cont_idx == 0) ? "main" : "sidecar";
                        std::string job = "kubernetes-pods";
                        
                        // Generate metrics for this container
                        int64_t samples = GenerateContainerMetrics(
                            region, zone, cluster, ns, app, service, deployment,
                            pod, container, node, instance, job,
                            start_time, end_time);
                        total += samples;
                    }
                }
            }
        }
    }

    std::string GeneratePodSuffix() {
        std::uniform_int_distribution<> dist(0, 35);
        std::string suffix;
        for (int i = 0; i < 5; ++i) {
            int c = dist(gen_);
            suffix += (c < 26) ? ('a' + c) : ('0' + c - 26);
        }
        return suffix;
    }

    std::string GenerateInstanceIP() {
        std::uniform_int_distribution<> dist(1, 254);
        return "10." + std::to_string(dist(gen_)) + "." + 
               std::to_string(dist(gen_)) + "." + std::to_string(dist(gen_));
    }

    int64_t GenerateContainerMetrics(
        const std::string& region, const std::string& zone, const std::string& cluster,
        const std::string& ns, const std::string& app, const std::string& service,
        const std::string& deployment, const std::string& pod, const std::string& container,
        const std::string& node, const std::string& instance, const std::string& job,
        int64_t start_time, int64_t end_time) {
        
        int64_t total_written = 0;
        std::uniform_real_distribution<> val_dist(0.0, 100.0);
        
        // Generate metrics (limited by config)
        int metrics_to_generate = std::min((int)METRICS.size(), config_.metrics_per_container);
        
        for (int m = 0; m < metrics_to_generate; ++m) {
            const auto& [metric_name, metric_type] = METRICS[m];
            
            // Build Arrow schema with 12 dimensions
            auto schema = arrow::schema({
                arrow::field("timestamp", arrow::int64()),
                arrow::field("value", arrow::float64()),
                arrow::field("region", arrow::utf8()),
                arrow::field("zone", arrow::utf8()),
                arrow::field("cluster", arrow::utf8()),
                arrow::field("namespace", arrow::utf8()),
                arrow::field("app", arrow::utf8()),
                arrow::field("service", arrow::utf8()),
                arrow::field("deployment", arrow::utf8()),
                arrow::field("pod", arrow::utf8()),
                arrow::field("container", arrow::utf8()),
                arrow::field("node", arrow::utf8()),
                arrow::field("instance", arrow::utf8()),
                arrow::field("job", arrow::utf8()),
            });
            
            // Calculate samples
            int64_t interval_ms = config_.scrape_interval_sec * 1000;
            int64_t samples = (end_time - start_time) / interval_ms;
            
            // Generate in batches
            for (int64_t batch_start = 0; batch_start < samples; batch_start += config_.batch_size) {
                int64_t batch_end = std::min(batch_start + (int64_t)config_.batch_size, samples);
                int64_t batch_samples = batch_end - batch_start;
                
                arrow::Int64Builder ts_builder;
                arrow::DoubleBuilder val_builder;
                arrow::StringBuilder region_b, zone_b, cluster_b, ns_b, app_b, svc_b, deploy_b, pod_b, cont_b, node_b, inst_b, job_b;
                
                double base_value = val_dist(gen_);
                
                for (int64_t i = 0; i < batch_samples; ++i) {
                    int64_t ts = start_time + (batch_start + i) * interval_ms;
                    
                    double value = base_value;
                    if (metric_type == "counter") {
                        value = base_value * (batch_start + i);
                    } else {
                        value = base_value + (val_dist(gen_) - 50.0) * 0.1;
                    }
                    
                    ts_builder.Append(ts);
                    val_builder.Append(value);
                    region_b.Append(region);
                    zone_b.Append(zone);
                    cluster_b.Append(cluster);
                    ns_b.Append(ns);
                    app_b.Append(app);
                    svc_b.Append(service);
                    deploy_b.Append(deployment);
                    pod_b.Append(pod);
                    cont_b.Append(container);
                    node_b.Append(node);
                    inst_b.Append(instance);
                    job_b.Append(job);
                }
                
                std::shared_ptr<arrow::Int64Array> ts_arr;
                std::shared_ptr<arrow::DoubleArray> val_arr;
                std::shared_ptr<arrow::StringArray> region_arr, zone_arr, cluster_arr, ns_arr, app_arr, svc_arr, deploy_arr, pod_arr, cont_arr, node_arr, inst_arr, job_arr;
                
                ts_builder.Finish(&ts_arr);
                val_builder.Finish(&val_arr);
                region_b.Finish(&region_arr);
                zone_b.Finish(&zone_arr);
                cluster_b.Finish(&cluster_arr);
                ns_b.Finish(&ns_arr);
                app_b.Finish(&app_arr);
                svc_b.Finish(&svc_arr);
                deploy_b.Finish(&deploy_arr);
                pod_b.Finish(&pod_arr);
                cont_b.Finish(&cont_arr);
                node_b.Finish(&node_arr);
                inst_b.Finish(&inst_arr);
                job_b.Finish(&job_arr);
                
                auto batch = arrow::RecordBatch::Make(schema, batch_samples,
                    {ts_arr, val_arr, region_arr, zone_arr, cluster_arr, ns_arr, 
                     app_arr, svc_arr, deploy_arr, pod_arr, cont_arr, node_arr, inst_arr, job_arr});
                
                // Send via Flight
                auto descriptor = arrow::flight::FlightDescriptor::Path({metric_name});
                arrow::flight::FlightCallOptions call_options;
                
                auto put_result = client_->DoPut(call_options, descriptor, batch->schema());
                if (put_result.ok()) {
                    (void)put_result->writer->WriteRecordBatch(*batch);
                    (void)put_result->writer->DoneWriting();
                    total_written += batch_samples;
                }
            }
        }
        
        return total_written;
    }

    ClusterConfig config_;
    std::shared_ptr<arrow::flight::FlightClient> client_;
    std::mt19937 gen_;
};

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n"
              << "\nScale Options:\n"
              << "  --quick          Quick test: 1 region, 1 day, 10 metrics\n"
              << "  --small          Small scale: 1 region, 3 days, 50 metrics\n"
              << "  --medium         Medium scale: 2 regions, 7 days, 100 metrics (default)\n"
              << "  --large          Large scale: 3 regions, 14 days, 100 metrics\n"
              << "  --seed-20m       Seed ~20M samples with 7d time range (local-kind friendly)\n"
              << "\nCustom Options:\n"
              << "  --host HOST      Flight server host (default: localhost)\n"
              << "  --port PORT      Flight server port (default: 8815)\n"
              << "  --days N         Days of data (default: 7)\n"
              << "  --scrape-interval-sec N   Scrape interval in seconds (default: 15)\n"
              << "  --target-samples N        Approx target samples; adjusts pods_per_service to fit\n"
              << "  --namespaces N            Namespaces per cluster\n"
              << "  --services-per-namespace N Services per namespace\n"
              << "  --pods-per-service N      Pods per service\n"
              << "  --containers-per-pod N    Containers per pod\n"
              << "  --metrics-per-container N Metrics per container\n"
              << "  --nodes-per-cluster N     Nodes per cluster\n"
              << "  --help           Show this help\n";
}

int main(int argc, char* argv[]) {
    std::string host = "localhost";
    int port = 8815;
    ClusterConfig config;
    std::string preset = "medium";
    bool seed_20m = false;
    int64_t target_samples = 0;
    
    // Pass 1: detect preset/scale choice
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--quick") preset = "quick";
        else if (arg == "--small") preset = "small";
        else if (arg == "--medium") preset = "medium";
        else if (arg == "--large") preset = "large";
        else if (arg == "--seed-20m") seed_20m = true;
        else if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
    }
    
    // Apply preset
    if (preset == "quick") {
        config.regions = {"us-east-1"};
        config.zones_per_region = 1;
        config.clusters_per_zone = 1;
        config.namespaces_per_cluster = 2;
        config.services_per_namespace = 3;
        config.pods_per_service = 2;
        config.containers_per_pod = 1;
        config.metrics_per_container = 10;
        config.days_of_data = 1;
    } else if (preset == "small") {
        config.regions = {"us-east-1"};
        config.zones_per_region = 2;
        config.clusters_per_zone = 1;
        config.namespaces_per_cluster = 5;
        config.services_per_namespace = 5;
        config.pods_per_service = 3;
        config.containers_per_pod = 2;
        config.metrics_per_container = 50;
        config.days_of_data = 3;
    } else if (preset == "medium") {
        config.regions = {"us-east-1", "us-west-2"};
        config.zones_per_region = 3;
        config.clusters_per_zone = 1;
        config.namespaces_per_cluster = 10;
        config.services_per_namespace = 10;
        config.pods_per_service = 3;
        config.containers_per_pod = 2;
        config.metrics_per_container = 100;
        config.days_of_data = 7;
    } else if (preset == "large") {
        config.regions = {"us-east-1", "us-west-2", "eu-west-1"};
        config.zones_per_region = 3;
        config.clusters_per_zone = 1;
        config.namespaces_per_cluster = 10;
        config.services_per_namespace = 20;
        config.pods_per_service = 5;
        config.containers_per_pod = 2;
        config.metrics_per_container = 100;
        config.days_of_data = 14;
    }

    // Seed preset: ~20M samples while keeping a 7-day time range by coarsening scrape interval.
    // Target: ~50 pods, 2 containers, 100 metrics, 7 days, 5m scrape => ~20.16M samples.
    if (seed_20m) {
        config.regions = {"us-east-1"};
        config.zones_per_region = 1;
        config.clusters_per_zone = 1;
        config.namespaces_per_cluster = 5;
        config.services_per_namespace = 5;
        config.pods_per_service = 2;        // 5*5*2 = 50 pods
        config.containers_per_pod = 2;
        config.metrics_per_container = 100;
        config.days_of_data = 7;
        config.scrape_interval_sec = 300;   // 5 minutes
        config.nodes_per_cluster = 20;
        config.workers = 4;
        config.batch_size = 5000;
    }

    // Pass 2: apply overrides
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) host = argv[++i];
        else if (arg == "--port" && i + 1 < argc) port = std::stoi(argv[++i]);
        else if (arg == "--days" && i + 1 < argc) config.days_of_data = std::stoi(argv[++i]);
        else if (arg == "--scrape-interval-sec" && i + 1 < argc) config.scrape_interval_sec = std::stoi(argv[++i]);
        else if (arg == "--target-samples" && i + 1 < argc) target_samples = std::stoll(argv[++i]);
        else if (arg == "--namespaces" && i + 1 < argc) config.namespaces_per_cluster = std::stoi(argv[++i]);
        else if (arg == "--services-per-namespace" && i + 1 < argc) config.services_per_namespace = std::stoi(argv[++i]);
        else if (arg == "--pods-per-service" && i + 1 < argc) config.pods_per_service = std::stoi(argv[++i]);
        else if (arg == "--containers-per-pod" && i + 1 < argc) config.containers_per_pod = std::stoi(argv[++i]);
        else if (arg == "--metrics-per-container" && i + 1 < argc) config.metrics_per_container = std::stoi(argv[++i]);
        else if (arg == "--nodes-per-cluster" && i + 1 < argc) config.nodes_per_cluster = std::stoi(argv[++i]);
    }

    // If requested, approximate target samples by adjusting pods_per_service while keeping other knobs stable.
    if (target_samples > 0) {
        const int64_t intervals = (int64_t)config.days_of_data * 24 * 3600 / std::max(1, config.scrape_interval_sec);
        const int64_t series_per_pod = (int64_t)std::max(1, config.containers_per_pod) * std::max(1, config.metrics_per_container);
        const int64_t pods_needed = (intervals > 0 && series_per_pod > 0)
            ? (target_samples + (intervals * series_per_pod) - 1) / (intervals * series_per_pod)
            : 1;

        const int64_t denom = (int64_t)std::max(1, (int)config.regions.size()) *
                              std::max(1, config.zones_per_region) *
                              std::max(1, config.clusters_per_zone) *
                              std::max(1, config.namespaces_per_cluster) *
                              std::max(1, config.services_per_namespace);
        const int64_t pods_per_service = (pods_needed + denom - 1) / denom;
        config.pods_per_service = std::max<int>(1, (int)pods_per_service);
    }
    
    // Connect to server
    auto location_result = arrow::flight::Location::ForGrpcTcp(host, port);
    if (!location_result.ok()) {
        std::cerr << "Failed to create location: " << location_result.status().ToString() << "\n";
        return 1;
    }
    
    arrow::flight::FlightClientOptions client_options;
    auto client_result = arrow::flight::FlightClient::Connect(*location_result, client_options);
    if (!client_result.ok()) {
        std::cerr << "Failed to connect: " << client_result.status().ToString() << "\n";
        return 1;
    }
    
    std::cout << "Connected to Arrow Flight server at " << host << ":" << port << std::endl << std::endl;
    
    K8sDataGenerator generator(config, std::move(*client_result));
    generator.Generate();
    
    return 0;
}
