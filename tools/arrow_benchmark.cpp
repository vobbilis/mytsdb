/**
 * Arrow Flight Benchmark Tool
 * 
 * Tests high-performance metrics ingestion via Arrow Flight.
 * Compares throughput against OTEL/gRPC path.
 */

#include <arrow/api.h>
#include <arrow/flight/api.h>
#include <iostream>
#include <chrono>
#include <random>
#include <thread>
#include <atomic>
#include <vector>

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n"
              << "Options:\n"
              << "  --host HOST         Arrow Flight server host (default: localhost)\n"
              << "  --port PORT         Arrow Flight server port (default: 8815)\n"
              << "  --series N          Number of series (default: 10000)\n"
              << "  --samples N         Samples per series (default: 10)\n"
              << "  --batch-size N      Rows per RecordBatch (default: 1000)\n"
              << "  --workers N         Number of parallel workers (default: 4)\n"
              << "  --help              Show this help\n";
}

arrow::Result<std::shared_ptr<arrow::RecordBatch>> CreateMetricBatch(
    const std::string& metric_name,
    int num_samples,
    int64_t base_timestamp) {
    
    // Schema: timestamp (int64), value (float64), pod (string), namespace (string)
    auto schema = arrow::schema({
        arrow::field("timestamp", arrow::int64()),
        arrow::field("value", arrow::float64()),
        arrow::field("pod", arrow::utf8()),
        arrow::field("namespace", arrow::utf8())
    });
    
    arrow::Int64Builder ts_builder;
    arrow::DoubleBuilder val_builder;
    arrow::StringBuilder pod_builder;
    arrow::StringBuilder ns_builder;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> val_dist(0.0, 100.0);
    
    for (int i = 0; i < num_samples; ++i) {
        ARROW_RETURN_NOT_OK(ts_builder.Append(base_timestamp + i * 60000));  // 1 min apart
        ARROW_RETURN_NOT_OK(val_builder.Append(val_dist(gen)));
        ARROW_RETURN_NOT_OK(pod_builder.Append("pod-" + std::to_string(i % 100)));
        ARROW_RETURN_NOT_OK(ns_builder.Append("default"));
    }
    
    std::shared_ptr<arrow::Int64Array> ts_array;
    std::shared_ptr<arrow::DoubleArray> val_array;
    std::shared_ptr<arrow::StringArray> pod_array;
    std::shared_ptr<arrow::StringArray> ns_array;
    
    ARROW_RETURN_NOT_OK(ts_builder.Finish(&ts_array));
    ARROW_RETURN_NOT_OK(val_builder.Finish(&val_array));
    ARROW_RETURN_NOT_OK(pod_builder.Finish(&pod_array));
    ARROW_RETURN_NOT_OK(ns_builder.Finish(&ns_array));
    
    return arrow::RecordBatch::Make(schema, num_samples, 
        {ts_array, val_array, pod_array, ns_array});
}

int main(int argc, char* argv[]) {
    std::string host = "localhost";
    int port = 8815;
    int num_series = 10000;
    int samples_per_series = 10;
    int batch_size = 1000;
    int num_workers = 4;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) host = argv[++i];
        else if (arg == "--port" && i + 1 < argc) port = std::stoi(argv[++i]);
        else if (arg == "--series" && i + 1 < argc) num_series = std::stoi(argv[++i]);
        else if (arg == "--samples" && i + 1 < argc) samples_per_series = std::stoi(argv[++i]);
        else if (arg == "--batch-size" && i + 1 < argc) batch_size = std::stoi(argv[++i]);
        else if (arg == "--workers" && i + 1 < argc) num_workers = std::stoi(argv[++i]);
        else if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
    }
    
    std::cout << "Arrow Flight Benchmark\n"
              << "  Host: " << host << ":" << port << "\n"
              << "  Series: " << num_series << "\n"
              << "  Samples/Series: " << samples_per_series << "\n"
              << "  Batch Size: " << batch_size << "\n"
              << "  Workers: " << num_workers << "\n";
    
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
    auto client = std::move(*client_result);
    
    std::cout << "Connected to Arrow Flight server\n";
    
    auto start_time = std::chrono::high_resolution_clock::now();
    std::atomic<int64_t> total_samples{0};
    std::atomic<int> errors{0};
    
    // Create worker threads
    std::vector<std::thread> workers;
    int series_per_worker = num_series / num_workers;
    
    for (int w = 0; w < num_workers; ++w) {
        workers.emplace_back([&, w]() {
            int start_series = w * series_per_worker;
            int end_series = (w == num_workers - 1) ? num_series : (w + 1) * series_per_worker;
            
            auto base_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            for (int s = start_series; s < end_series; s += batch_size / samples_per_series) {
                std::string metric_name = "benchmark_metric_" + std::to_string(s);
                
                int samples_in_batch = std::min(batch_size, 
                    (end_series - s) * samples_per_series);
                
                auto batch_result = CreateMetricBatch(metric_name, samples_in_batch, base_time);
                if (!batch_result.ok()) {
                    errors++;
                    continue;
                }
                
                // Send via DoPut
                auto descriptor = arrow::flight::FlightDescriptor::Path({metric_name});
                
                arrow::flight::FlightCallOptions call_options;
                auto put_result = client->DoPut(call_options, descriptor, (*batch_result)->schema());
                if (!put_result.ok()) {
                    errors++;
                    continue;
                }
                
                auto status = put_result->writer->WriteRecordBatch(**batch_result);
                if (!status.ok()) {
                    errors++;
                    continue;
                }
                
                status = put_result->writer->DoneWriting();
                if (!status.ok()) {
                    errors++;
                    continue;
                }
                
                total_samples += samples_in_batch;
            }
        });
    }
    
    for (auto& t : workers) {
        t.join();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    double rate = (double)total_samples / (duration_ms / 1000.0);
    
    std::cout << "\nResults:\n"
              << "  Time: " << duration_ms << " ms\n"
              << "  Total Samples: " << total_samples << "\n"
              << "  Errors: " << errors << "\n"
              << "  Rate: " << rate << " samples/sec\n";
    
    return 0;
}
