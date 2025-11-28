#include <gtest/gtest.h>
#include <vector>
#include <string>
#include "tsdb/core/result.h"

using namespace tsdb::core;

TEST(ResultTest, BasicConstruction) {
    // Test 1: Direct construction with value
    std::vector<std::string> vec1{"a", "b", "c"};
    Result<std::vector<std::string>> r1(std::move(vec1));
    
    std::cout << "Test 1 - Direct construction:" << std::endl;
    std::cout << "  r1.ok() = " << r1.ok() << std::endl;
    EXPECT_TRUE(r1.ok());
    
    // Test 2: Return from function
    auto r2 = []() -> Result<std::vector<std::string>> {
        return Result<std::vector<std::string>>(
            std::vector<std::string>{"x", "y", "z"}
        );
    }();
    
    std::cout << "Test 2 - Return from lambda:" << std::endl;
    std::cout << "  r2.ok() = " << r2.ok() << std::endl;
    EXPECT_TRUE(r2.ok());
    
    // Test 3: Inline construction like MockStorage
    auto r3 = Result<std::vector<std::string>>(
        std::vector<std::string>{"job", "instance"}
    );
    
    std::cout << "Test 3 - Inline construction:" << std::endl;
    std::cout << "  r3.ok() = " << r3.ok() << std::endl;
    EXPECT_TRUE(r3.ok());
}
