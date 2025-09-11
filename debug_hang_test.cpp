#include <iostream>
#include <chrono>
#include <thread>
#include "tsdb/storage/storage_impl.h"
#include "tsdb/core/config.h"
#include "tsdb/core/types.h"

int main() {
    std::cout << "=== DEBUG: Starting minimal StorageImpl test ===" << std::endl;
    
    try {
        std::cout << "Step 1: Creating configuration..." << std::endl;
        tsdb::core::StorageConfig config;
        config.data_dir = "./debug_test_data";
        
        std::cout << "Step 2: Creating StorageImpl..." << std::endl;
        auto storage = std::make_unique<tsdb::storage::StorageImpl>(config);
        
        std::cout << "Step 3: Initializing StorageImpl..." << std::endl;
        auto init_result = storage->init(config);
        if (!init_result.ok()) {
            std::cerr << "FAILED: Storage initialization failed: " << init_result.error().what() << std::endl;
            return 1;
        }
        std::cout << "SUCCESS: StorageImpl initialized" << std::endl;
        
        std::cout << "Step 4: Creating simple test series..." << std::endl;
        
        // Create labels first
        tsdb::core::Labels labels;
        labels.add("__name__", "debug_test");
        labels.add("test", "minimal");
        
        // Create series with labels
        tsdb::core::TimeSeries series(labels);
        
        // Add a few samples
        for (int i = 0; i < 5; ++i) {
            series.add_sample(1000 + i, 42.0 + i);
        }
        
        std::cout << "Step 5: Writing series to storage..." << std::endl;
        std::cout << "  5a: About to call storage->write()..." << std::endl;
        std::cout << "  5a1: storage pointer = " << storage.get() << std::endl;
        std::cout << "  5a2: series samples count = " << series.size() << std::endl;
        std::cout.flush();
        auto write_result = storage->write(series);
        std::cout << "  5b: storage->write() returned" << std::endl;
        if (!write_result.ok()) {
            std::cerr << "FAILED: Write failed: " << write_result.error().what() << std::endl;
            return 1;
        }
        std::cout << "SUCCESS: Series written" << std::endl;
        
        std::cout << "Step 6: Reading series from storage..." << std::endl;
        tsdb::core::Labels query_labels;
        query_labels.add("__name__", "debug_test");
        query_labels.add("test", "minimal");
        
        auto read_result = storage->read(query_labels, 1000, 1010);
        if (!read_result.ok()) {
            std::cerr << "FAILED: Read failed: " << read_result.error().what() << std::endl;
            return 1;
        }
        std::cout << "SUCCESS: Series read, samples count: " << read_result.value().size() << std::endl;
        
        std::cout << "Step 7: Closing storage..." << std::endl;
        storage->close();
        std::cout << "SUCCESS: Storage closed" << std::endl;
        
        std::cout << "=== DEBUG: All steps completed successfully ===" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "EXCEPTION: " << e.what() << std::endl;
        return 1;
    }
}
