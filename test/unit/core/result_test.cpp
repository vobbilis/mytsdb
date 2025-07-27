#include <gtest/gtest.h>
#include "tsdb/core/result.h"
#include "tsdb/core/error.h"
#include <string>
#include <vector>

namespace tsdb {
namespace core {
namespace {

TEST(ResultTest, SuccessConstruction) {
    Result<int> result(42);
    EXPECT_TRUE(result.ok());
    EXPECT_FALSE(result.has_error());
    EXPECT_EQ(result.value(), 42);
}

TEST(ResultTest, ErrorConstruction) {
    auto result = Result<int>::error("Invalid input");
    EXPECT_FALSE(result.ok());
    EXPECT_TRUE(result.has_error());
    EXPECT_EQ(result.error(), "Invalid input");
}

TEST(ResultTest, StringResult) {
    Result<std::string> result("test string");
    EXPECT_TRUE(result.ok());
    EXPECT_EQ(result.value(), "test string");
}

TEST(ResultTest, VectorResult) {
    std::vector<int> vec = {1, 2, 3, 4, 5};
    Result<std::vector<int>> result(vec);
    EXPECT_TRUE(result.ok());
    EXPECT_EQ(result.value().size(), 5);
    EXPECT_EQ(result.value()[0], 1);
}

TEST(ResultTest, ErrorResult) {
    auto result = Result<std::string>::error("Resource not found");
    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.error(), "Resource not found");
}

TEST(ResultTest, MoveConstruction) {
    Result<std::string> original("moved string");
    Result<std::string> moved(std::move(original));
    
    EXPECT_TRUE(moved.ok());
    EXPECT_EQ(moved.value(), "moved string");
    // Original should be in a valid but unspecified state
}

TEST(ResultTest, MoveAssignment) {
    Result<std::string> original("original string");
    Result<std::string> assigned = std::move(original);
    
    EXPECT_TRUE(assigned.ok());
    EXPECT_EQ(assigned.value(), "original string");
}

TEST(ResultTest, VoidResult) {
    Result<void> result;
    EXPECT_TRUE(result.ok());
    EXPECT_FALSE(result.has_error());
}

TEST(ResultTest, VoidErrorResult) {
    auto result = Result<void>::error("Internal error");
    EXPECT_FALSE(result.ok());
    EXPECT_TRUE(result.has_error());
    EXPECT_EQ(result.error(), "Internal error");
}

TEST(ResultTest, MoveValue) {
    Result<std::string> result("test value");
    EXPECT_TRUE(result.ok());
    
    std::string value = std::move(result).value();
    EXPECT_EQ(value, "test value");
}

TEST(ResultTest, ErrorCodeTypes) {
    auto invalid_arg = Result<int>::error("Invalid argument");
    auto not_found = Result<int>::error("Not found");
    auto timeout = Result<int>::error("Timeout");
    
    EXPECT_FALSE(invalid_arg.ok());
    EXPECT_FALSE(not_found.ok());
    EXPECT_FALSE(timeout.ok());
    
    EXPECT_EQ(invalid_arg.error(), "Invalid argument");
    EXPECT_EQ(not_found.error(), "Not found");
    EXPECT_EQ(timeout.error(), "Timeout");
}

TEST(ResultTest, ErrorConstructor) {
    Error error("Custom error", Error::Code::INVALID_ARGUMENT);
    Result<int> result(error);
    EXPECT_FALSE(result.ok());
    EXPECT_TRUE(result.has_error());
    EXPECT_EQ(result.error(), "Custom error");
}

TEST(ResultTest, ExceptionOnValueAccess) {
    auto result = Result<int>::error("Error occurred");
    EXPECT_THROW(result.value(), std::runtime_error);
}

TEST(ResultTest, ExceptionOnErrorAccess) {
    Result<int> result(42);
    EXPECT_THROW(result.error(), std::runtime_error);
}

} // namespace
} // namespace core
} // namespace tsdb 