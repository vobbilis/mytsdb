#include <opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_factory.h>
#include <opentelemetry/metrics/provider.h>
#include <opentelemetry/sdk/metrics/meter_provider.h>
#include <chrono>
#include <thread>
#include <random>
#include <iostream>

int main() {
    try {
        // Configure the exporter
        opentelemetry::exporter::metrics::OtlpGrpcExporterOptions options;
        options.endpoint = "localhost:4317";
        auto exporter = opentelemetry::exporter::metrics::OtlpGrpcExporterFactory::Create(options);
        
        // Create meter provider
        auto provider = opentelemetry::sdk::metrics::MeterProviderFactory::Create(
            std::move(exporter));
        
        // Set the global meter provider
        opentelemetry::metrics::Provider::SetMeterProvider(provider);
        
        // Get a meter
        auto meter = provider->GetMeter("example_meter");
        
        // Create instruments
        auto counter = meter->CreateCounter<int64_t>("example_counter",
                                                   "An example counter",
                                                   "requests");
        
        auto gauge = meter->CreateGauge<double>("example_gauge",
                                              "An example gauge",
                                              "temperature");
        
        auto histogram = meter->CreateHistogram<double>("example_histogram",
                                                     "An example histogram",
                                                     "latency",
                                                     "ms");
        
        // Create random number generators
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<> latency_dist(50.0, 10.0);  // mean=50ms, std=10ms
        std::uniform_real_distribution<> temp_dist(20.0, 30.0);  // 20-30°C
        
        // Record metrics in a loop
        std::cout << "Recording metrics... Press Ctrl+C to stop" << std::endl;
        
        while (true) {
            // Record counter
            counter->Add(1, {{"endpoint", "/api"}});
            
            // Record gauge
            gauge->Set(temp_dist(gen), {{"location", "datacenter"}});
            
            // Record histogram
            histogram->Record(std::max(0.0, latency_dist(gen)),
                            {{"operation", "request"}});
            
            // Sleep for 100ms
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
} 