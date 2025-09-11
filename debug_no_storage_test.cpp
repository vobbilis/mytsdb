#include <iostream>
#include "tsdb/core/config.h"

int main() {
    std::cout << "=== NO STORAGE TEST ===" << std::endl;
    
    try {
        std::cout << "Step 1: Creating basic config..." << std::endl;
        tsdb::core::StorageConfig config;
        config.data_dir = "./no_storage_test_data";
        
        std::cout << "Step 2: Config created successfully" << std::endl;
        
        std::cout << "=== NO STORAGE TEST: SUCCESS ===" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "EXCEPTION: " << e.what() << std::endl;
        return 1;
    }
}
