#ifndef TSDB_CORE_RESULT_H_
#define TSDB_CORE_RESULT_H_

#include <string>
#include <optional>
#include <type_traits>
#include <memory>
#include <stdexcept>
#include <vector>
#include "tsdb/core/error.h"
#include <iostream>

namespace tsdb {
namespace core {

/**
 * @brief Result type for operations that can fail
 * 
 * Usage:
 * ```
 * Result<int> foo() {
 *     if (error_condition) {
 *         return Result<int>::error("error message");
 *     }
 *     return Result<int>(42);
 * }
 * 
 * auto result = foo();
 * if (result.ok()) {
 *     int value = result.value();
 * } else {
 *     std::string error = result.error();
 * }
 * ```
 */
template<typename T>
class Result {
public:
    Result(T value) : value_(std::move(value)), error_msg_(std::nullopt) {}
    
    // Constructor for error (internal use or compatibility)
    explicit Result(std::unique_ptr<Error> error) : value_(), error_msg_(error ? std::make_optional(std::string(error->what())) : std::nullopt) {}
    
    struct ErrorTag {};
    // Constructor for error message
    explicit Result(std::string error_msg, ErrorTag) : value_(), error_msg_(std::move(error_msg)) {}
    
    // Move constructor
    Result(Result&& other) noexcept 
        : value_(std::move(other.value_)), error_msg_(std::move(other.error_msg_)) {}
    
    // Move assignment
    Result& operator=(Result&& other) noexcept {
        if (this != &other) {
            value_ = std::move(other.value_);
            error_msg_ = std::move(other.error_msg_);
        }
        return *this;
    }
    
    // Delete copy constructor and assignment (Result should be move-only)
    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;
    
    bool ok() const { return !error_msg_.has_value(); }
    std::string error() const { 
        if (!error_msg_) {
            throw std::runtime_error("Attempting to access error of ok result");
        }
        return *error_msg_;
    }
    const T& value() const { return value_; }
    T&& take_value() { return std::move(value_); }
    
    static Result<T> error(const std::string& message) {
        return Result<T>(message, ErrorTag{});
    }
    
    ~Result() {
        // Logging removed
    }
    
private:
    T value_;
    std::optional<std::string> error_msg_;
};

/**
 * @brief Specialization for void results
 */
template<>
class Result<void> {
public:
    Result() : error_msg_(std::nullopt) {}
    explicit Result(std::unique_ptr<Error> error) : error_msg_(error ? std::make_optional(std::string(error->what())) : std::nullopt) {}
    
    struct ErrorTag {};
    explicit Result(std::string error_msg, ErrorTag) : error_msg_(std::move(error_msg)) {}
    // Keep old constructor for compatibility if needed, but disambiguate
    explicit Result(std::string error_msg) : error_msg_(std::move(error_msg)) {} // void result doesn't have T constructor, so this is fine?
    // Actually, Result<void> has no T constructor. So Result(string) is fine.
    // But for consistency, let's use ErrorTag in static error() method.
    
    bool ok() const { return !error_msg_.has_value(); }
    std::string error() const { 
        if (!error_msg_) {
            throw std::runtime_error("Attempting to access error of ok result");
        }
        return *error_msg_;
    }
    
    static Result<void> error(const std::string& message) {
        return Result<void>(message, ErrorTag{});
    }
    
private:
    std::optional<std::string> error_msg_;
};

// Common template instantiations declarations
template class Result<std::vector<uint8_t>>;
template class Result<std::string>;

} // namespace core
} // namespace tsdb

#endif // TSDB_CORE_RESULT_H_ 