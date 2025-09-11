#include <iostream>
#include "tsdb/storage/storage_impl.h"
#include "tsdb/storage/storage.h"
#include "tsdb/core/config.h"

int main() {
    std::cout << "=== VIRTUAL DISPATCH TEST ===" << std::endl;
    
    try {
        std::cout << "Step 1: Creating StorageImpl directly..." << std::endl;
        tsdb::core::StorageConfig config;
        config.data_dir = "./virtual_test_data";
        
        // Create StorageImpl directly (not through base class pointer)
        tsdb::storage::StorageImpl storage_direct(config);
        
        std::cout << "Step 2: Initializing StorageImpl directly..." << std::endl;
        auto init_result = storage_direct.init(config);
        if (!init_result.ok()) {
            std::cerr << "FAILED: Direct storage initialization failed: " << init_result.error().what() << std::endl;
            return 1;
        }
        std::cout << "SUCCESS: Direct StorageImpl initialized" << std::endl;
        
        std::cout << "Step 3: Calling close() directly..." << std::endl;
        std::cout.flush();
        auto close_result = storage_direct.close();
        std::cout << "Step 3b: Direct close() returned" << std::endl;
        
        if (!close_result.ok()) {
            std::cerr << "FAILED: Direct close failed: " << close_result.error().what() << std::endl;
            return 1;
        }
        std::cout << "SUCCESS: Direct storage closed" << std::endl;
        
        std::cout << "=== VIRTUAL DISPATCH TEST: SUCCESS ===" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "EXCEPTION: " << e.what() << std::endl;
        return 1;
    }
}
