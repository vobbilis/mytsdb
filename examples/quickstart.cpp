#include "tsdb/storage/storage_impl.h"
#include "tsdb/core/types.h"
#include <iostream>
#include <ctime>

using namespace tsdb;

int main() {
    std::cout << "=== MyTSDB Quick Start Example ===" << std::endl;
    
    // Configure storage
    core::StorageConfig config = core::StorageConfig::Default();
    config.data_dir = "./tsdb_data";
    
    std::cout << "Creating storage with data_dir: " << config.data_dir << std::endl;
    
    // Create storage instance
    storage::StorageImpl storage(config);
    
    // Initialize
    std::cout << "Initializing storage..." << std::endl;
    auto init_result = storage.init(config);
    if (!init_result.ok()) {
        std::cerr << "Init failed: " << init_result.error() << std::endl;
        return 1;
    }
    std::cout << "✅ Storage initialized" << std::endl;
    
    // Create time series
    core::Labels labels;
    labels.add("__name__", "cpu_usage");
    labels.add("host", "server1");
    core::TimeSeries series(labels);
    series.add_sample(std::time(nullptr) * 1000, 0.75);
    
    std::cout << "Writing time series with " << series.samples().size() << " samples..." << std::endl;
    
    // Write
    auto write_result = storage.write(series);
    if (!write_result.ok()) {
        std::cerr << "Write failed: " << write_result.error() << std::endl;
        return 1;
    }
    std::cout << "✅ Write successful" << std::endl;
    
    // Read
    std::cout << "Reading time series..." << std::endl;
    auto read_result = storage.read(labels, 0, std::time(nullptr) * 1000);
    if (read_result.ok()) {
        std::cout << "✅ Read " << read_result.value().size() << " samples" << std::endl;
        for (const auto& sample : read_result.value().samples()) {
            std::cout << "  Timestamp: " << sample.timestamp() 
                      << ", Value: " << sample.value() << std::endl;
        }
    } else {
        std::cerr << "Read failed: " << read_result.error() << std::endl;
    }
    
    // Close
    std::cout << "Closing storage..." << std::endl;
    storage.close();
    std::cout << "✅ Quick start complete!" << std::endl;
    
    return 0;
}

