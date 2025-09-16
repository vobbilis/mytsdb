#include <iostream>
#include <memory>
#include "tsdb/core/types.h"
#include "tsdb/core/config.h"
#include "tsdb/storage/storage_impl.h"
#include <filesystem>

int main() {
    try {
        // Create temporary directory
        auto test_dir = std::filesystem::temp_directory_path() / "tsdb_debug_test";
        std::filesystem::create_directories(test_dir);
        
        // Create storage config
        core::StorageConfig config;
        config.data_dir = test_dir.string();
        config.block_size = 4096;
        config.max_blocks_per_series = 1000;
        config.cache_size_bytes = 1024 * 1024;
        config.block_duration = 3600 * 1000;
        config.retention_period = 7 * 24 * 3600 * 1000;
        config.enable_compression = true;
        config.background_config.enable_auto_compaction = false;
        config.background_config.enable_auto_cleanup = false;
        config.background_config.enable_metrics_collection = false;
        
        // Create storage
        auto storage = std::make_unique<StorageImpl>();
        auto init_result = storage->init(config);
        if (!init_result.ok()) {
            std::cerr << "Failed to initialize storage: " << init_result.error() << std::endl;
            return 1;
        }
        
        std::cout << "Storage initialized successfully" << std::endl;
        
        // Test 1: Read with empty labels
        std::cout << "Testing read with empty labels..." << std::endl;
        core::Labels empty_labels;
        auto read_result = storage->read(empty_labels, 0, 1000);
        std::cout << "Read with empty labels result: " << (read_result.ok() ? "OK" : "ERROR") << std::endl;
        
        // Test 2: Write a series
        std::cout << "Testing write..." << std::endl;
        core::Labels labels;
        labels.add("__name__", "test_metric");
        labels.add("instance", "host1");
        
        core::TimeSeries series(labels);
        series.add_sample(core::Sample(1000, 1.0));
        
        auto write_result = storage->write(series);
        if (!write_result.ok()) {
            std::cerr << "Write failed: " << write_result.error() << std::endl;
            return 1;
        }
        std::cout << "Write successful" << std::endl;
        
        // Test 3: Read with invalid time range
        std::cout << "Testing read with invalid time range..." << std::endl;
        auto invalid_range_result = storage->read(series.labels(), 2000, 1000);
        std::cout << "Read with invalid range result: " << (invalid_range_result.ok() ? "OK" : "ERROR") << std::endl;
        
        // Test 4: Close storage
        std::cout << "Closing storage..." << std::endl;
        auto close_result = storage->close();
        if (!close_result.ok()) {
            std::cerr << "Close failed: " << close_result.error() << std::endl;
            return 1;
        }
        std::cout << "Close successful" << std::endl;
        
        // Cleanup
        storage.reset();
        std::filesystem::remove_all(test_dir);
        
        std::cout << "All tests completed successfully!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
}
