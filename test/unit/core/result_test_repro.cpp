#include "gtest/gtest.h"
#include "tsdb/core/result.h"
#include "tsdb/core/types.h"
#include <iostream>

namespace tsdb {
namespace core {

TEST(ResultReproTest, ErrorResultDestruction) {
    std::cout << "Creating error result..." << std::endl;
    auto result = Result<TimeSeries>::error("Test error");
    
    std::cout << "Checking ok()..." << std::endl;
    EXPECT_FALSE(result.ok());
    
    std::cout << "Checking error message..." << std::endl;
    EXPECT_EQ(result.error(), "Test error");
    
    std::cout << "Destructing result..." << std::endl;
}

TEST(ResultReproTest, MoveErrorResult) {
    std::cout << "Creating error result..." << std::endl;
    auto result1 = Result<TimeSeries>::error("Test error");
    
    std::cout << "Moving result..." << std::endl;
    auto result2 = std::move(result1);
    
    std::cout << "Checking result2..." << std::endl;
    EXPECT_FALSE(result2.ok());
    EXPECT_EQ(result2.error(), "Test error");
    
    std::cout << "Destructing results..." << std::endl;
}

} // namespace core
} // namespace tsdb
