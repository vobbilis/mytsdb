#include <gtest/gtest.h>
#include "tsdb/core/error.h"
#include <string>

namespace tsdb {
namespace core {
namespace {

TEST(ErrorTest, Construction) {
    Error error("Invalid input", Error::Code::INVALID_ARGUMENT);
    EXPECT_EQ(error.code(), Error::Code::INVALID_ARGUMENT);
    EXPECT_EQ(error.what(), std::string("Invalid input"));
}

TEST(ErrorTest, CopyConstruction) {
    Error original("Resource not found", Error::Code::NOT_FOUND);
    Error copy(original);
    
    EXPECT_EQ(copy.code(), original.code());
    EXPECT_EQ(copy.what(), original.what());
}

TEST(ErrorTest, MoveConstruction) {
    Error original("Internal error", Error::Code::INTERNAL);
    Error moved(std::move(original));
    
    EXPECT_EQ(moved.code(), Error::Code::INTERNAL);
    EXPECT_EQ(moved.what(), std::string("Internal error"));
}

TEST(ErrorTest, Assignment) {
    Error error1("Invalid", Error::Code::INVALID_ARGUMENT);
    Error error2("Not found", Error::Code::NOT_FOUND);
    
    error1 = error2;
    EXPECT_EQ(error1.code(), Error::Code::NOT_FOUND);
    EXPECT_EQ(error1.what(), std::string("Not found"));
}

TEST(ErrorTest, MoveAssignment) {
    Error error1("Invalid", Error::Code::INVALID_ARGUMENT);
    Error error2("Not found", Error::Code::NOT_FOUND);
    
    error1 = std::move(error2);
    EXPECT_EQ(error1.code(), Error::Code::NOT_FOUND);
    EXPECT_EQ(error1.what(), std::string("Not found"));
}

TEST(ErrorTest, Comparison) {
    Error error1("Invalid", Error::Code::INVALID_ARGUMENT);
    Error error2("Invalid", Error::Code::INVALID_ARGUMENT);
    Error error3("Not found", Error::Code::NOT_FOUND);
    Error error4("Invalid", Error::Code::NOT_FOUND);
    
    EXPECT_EQ(error1.code(), error2.code());
    EXPECT_NE(error1.code(), error3.code());
    EXPECT_NE(error1.code(), error4.code());
}

TEST(ErrorTest, ErrorCodeValues) {
    // Test all error codes are distinct
    EXPECT_NE(Error::Code::UNKNOWN, Error::Code::INVALID_ARGUMENT);
    EXPECT_NE(Error::Code::INVALID_ARGUMENT, Error::Code::NOT_FOUND);
    EXPECT_NE(Error::Code::NOT_FOUND, Error::Code::ALREADY_EXISTS);
    EXPECT_NE(Error::Code::ALREADY_EXISTS, Error::Code::TIMEOUT);
    EXPECT_NE(Error::Code::TIMEOUT, Error::Code::RESOURCE_EXHAUSTED);
    EXPECT_NE(Error::Code::RESOURCE_EXHAUSTED, Error::Code::INTERNAL);
}

TEST(ErrorTest, EmptyMessage) {
    Error error("", Error::Code::UNKNOWN);
    EXPECT_EQ(error.code(), Error::Code::UNKNOWN);
    EXPECT_EQ(error.what(), std::string(""));
}

TEST(ErrorTest, LongMessage) {
    std::string long_message(1000, 'x');
    Error error(long_message, Error::Code::INTERNAL);
    EXPECT_EQ(error.what(), long_message);
}

TEST(ErrorTest, SpecialCharacters) {
    Error error("Special chars: \n\t\r\"'\\", Error::Code::INVALID_ARGUMENT);
    EXPECT_EQ(error.what(), std::string("Special chars: \n\t\r\"'\\"));
}

TEST(ErrorTest, SpecificErrorTypes) {
    InvalidArgumentError invalid_arg("Invalid argument");
    EXPECT_EQ(invalid_arg.code(), Error::Code::INVALID_ARGUMENT);
    EXPECT_EQ(invalid_arg.what(), std::string("Invalid argument"));
    
    NotFoundError not_found("Resource not found");
    EXPECT_EQ(not_found.code(), Error::Code::NOT_FOUND);
    EXPECT_EQ(not_found.what(), std::string("Resource not found"));
    
    AlreadyExistsError already_exists("Resource already exists");
    EXPECT_EQ(already_exists.code(), Error::Code::ALREADY_EXISTS);
    EXPECT_EQ(already_exists.what(), std::string("Resource already exists"));
    
    TimeoutError timeout("Operation timed out");
    EXPECT_EQ(timeout.code(), Error::Code::TIMEOUT);
    EXPECT_EQ(timeout.what(), std::string("Operation timed out"));
    
    ResourceExhaustedError resource_exhausted("Resource exhausted");
    EXPECT_EQ(resource_exhausted.code(), Error::Code::RESOURCE_EXHAUSTED);
    EXPECT_EQ(resource_exhausted.what(), std::string("Resource exhausted"));
    
    InternalError internal("Internal error");
    EXPECT_EQ(internal.code(), Error::Code::INTERNAL);
    EXPECT_EQ(internal.what(), std::string("Internal error"));
}

} // namespace
} // namespace core
} // namespace tsdb 