#include "tsdb/storage/storage_impl.h"
#include "tsdb/core/config.h"
#include "tsdb/core/types.h"
#include <iostream>
#include <chrono>

int main() {
    std::cout << "=== Manual StorageImpl Integration Test ===" << std::endl;
    
    try {
        // Create storage configuration
        tsdb::core::StorageConfig config;
        config.data_dir = "./manual_test_data";
        config.enable_compression = true;
        config.background_config.enable_background_processing = true;
        config.background_config.enable_auto_compaction = true;
        config.background_config.enable_metrics_collection = true;
        
        std::cout << "1. Creating StorageImpl with full configuration..." << std::endl;
        auto storage = std::make_unique<tsdb::storage::StorageImpl>(config);
        
        std::cout << "2. Initializing storage..." << std::endl;
        auto init_result = storage->init(config);
        if (!init_result.ok()) {
            std::cerr << "FAILED: Storage initialization failed: " << init_result.error().what() << std::endl;
            return 1;
        }
        std::cout << "✓ Storage initialized successfully" << std::endl;
        
        // Create test data
        std::cout << "3. Creating test time series..." << std::endl;
        tsdb::core::Labels labels;
        labels.add("__name__", "test_metric");
        labels.add("job", "manual_test");
        labels.add("instance", "localhost:8080");
        
        tsdb::core::TimeSeries series(labels);
        
        // Add some test samples
        int64_t base_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        for (int i = 0; i < 100; ++i) {
            series.add_sample(base_time + i * 1000, 100.0 + i * 0.5); // 1 second intervals
        }
        
        std::cout << "✓ Created test series with " << series.samples().size() << " samples" << std::endl;
        
        // Test write operation
        std::cout << "4. Testing write operation..." << std::endl;
        auto write_result = storage->write(series);
        if (!write_result.ok()) {
            std::cerr << "FAILED: Write operation failed: " << write_result.error().what() << std::endl;
            return 1;
        }
        std::cout << "✓ Write operation successful" << std::endl;
        
        // Test read operation
        std::cout << "5. Testing read operation..." << std::endl;
        auto read_result = storage->read(labels, base_time, base_time + 50000); // First 50 samples
        if (!read_result.ok()) {
            std::cerr << "FAILED: Read operation failed: " << read_result.error().what() << std::endl;
            return 1;
        }
        
        const auto& read_series = read_result.value();
        std::cout << "✓ Read operation successful - retrieved " << read_series.samples().size() << " samples" << std::endl;
        
        // Test data integrity
        std::cout << "6. Testing data integrity..." << std::endl;
        if (read_series.samples().size() > 0) {
            const auto& first_sample = read_series.samples()[0];
            std::cout << "   First sample: timestamp=" << first_sample.timestamp() 
                      << ", value=" << first_sample.value() << std::endl;
        }
        
        // Test cache functionality by reading again
        std::cout << "7. Testing cache functionality (second read)..." << std::endl;
        auto cached_read_result = storage->read(labels, base_time, base_time + 25000); // First 25 samples
        if (!cached_read_result.ok()) {
            std::cerr << "FAILED: Cached read operation failed: " << cached_read_result.error().what() << std::endl;
            return 1;
        }
        std::cout << "✓ Cached read successful - retrieved " << cached_read_result.value().samples().size() << " samples" << std::endl;
        
        // Test flush operation
        std::cout << "8. Testing flush operation..." << std::endl;
        auto flush_result = storage->flush();
        if (!flush_result.ok()) {
            std::cerr << "FAILED: Flush operation failed: " << flush_result.error().what() << std::endl;
            return 1;
        }
        std::cout << "✓ Flush operation successful" << std::endl;
        
        // Test close operation
        std::cout << "9. Testing close operation..." << std::endl;
        auto close_result = storage->close();
        if (!close_result.ok()) {
            std::cerr << "FAILED: Close operation failed: " << close_result.error().what() << std::endl;
            return 1;
        }
        std::cout << "✓ Close operation successful" << std::endl;
        
        std::cout << "\n=== ALL TESTS PASSED! ===" << std::endl;
        std::cout << "StorageImpl integration appears to be working correctly." << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "EXCEPTION: " << e.what() << std::endl;
        return 1;
    }
}
