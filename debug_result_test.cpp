#include "tsdb/core/result.h"
#include "tsdb/core/error.h"
#include <iostream>

int main() {
    std::cout << "Testing core::Result error access..." << std::endl;
    
    // Test 1: Create a failed result
    std::cout << "Creating failed result..." << std::endl;
    auto failed_result = tsdb::core::Result<int>::error("Test error message");
    
    std::cout << "Result ok: " << (failed_result.ok() ? "true" : "false") << std::endl;
    std::cout << "Result has_error: " << (failed_result.has_error() ? "true" : "false") << std::endl;
    
    // Test 2: Try to access error
    std::cout << "About to access error..." << std::endl;
    try {
        std::string error_msg = failed_result.error();
        std::cout << "Error message: " << error_msg << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Exception caught: " << e.what() << std::endl;
    }
    
    std::cout << "Test completed successfully" << std::endl;
    return 0;
}
