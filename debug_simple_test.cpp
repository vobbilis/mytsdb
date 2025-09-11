#include <iostream>
#include "tsdb/storage/storage_impl.h"
#include "tsdb/core/config.h"

int main() {
    std::cout << "=== SIMPLE TEST: Testing StorageImpl without write ===" << std::endl;
    
    try {
        std::cout << "Step 1: Creating configuration..." << std::endl;
        tsdb::core::StorageConfig config;
        config.data_dir = "./simple_test_data";
        
        std::cout << "Step 2: Creating StorageImpl..." << std::endl;
        auto storage = std::make_unique<tsdb::storage::StorageImpl>(config);
        
        std::cout << "Step 3: Initializing StorageImpl..." << std::endl;
        auto init_result = storage->init(config);
        if (!init_result.ok()) {
            std::cerr << "FAILED: Storage initialization failed: " << init_result.error().what() << std::endl;
            return 1;
        }
        std::cout << "SUCCESS: StorageImpl initialized" << std::endl;
        
        std::cout << "Step 4: Calling flush (simple operation)..." << std::endl;
        auto flush_result = storage->flush();
        if (!flush_result.ok()) {
            std::cerr << "FAILED: Flush failed: " << flush_result.error().what() << std::endl;
            return 1;
        }
        std::cout << "SUCCESS: Flush completed" << std::endl;
        
        std::cout << "Step 5: Explicitly calling close()..." << std::endl;
        std::cout << "  5a: About to call storage->close()..." << std::endl;
        std::cout.flush();
        auto close_result = storage->close();
        std::cout << "  5b: storage->close() returned" << std::endl;
        if (!close_result.ok()) {
            std::cerr << "FAILED: Close failed: " << close_result.error().what() << std::endl;
            return 1;
        }
        std::cout << "SUCCESS: Storage closed explicitly" << std::endl;
        
        std::cout << "Step 6: Resetting storage pointer (destructor)..." << std::endl;
        std::cout.flush();
        storage.reset();
        std::cout << "SUCCESS: Storage destructor completed" << std::endl;
        
        std::cout << "=== SIMPLE TEST: All steps completed successfully ===" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "EXCEPTION: " << e.what() << std::endl;
        return 1;
    }
}
