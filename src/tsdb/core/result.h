#ifndef TSDB_CORE_RESULT_H_
#define TSDB_CORE_RESULT_H_

#include <string>
#include <variant>
#include <type_traits>
#include "tsdb/core/error.h"

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
    // Success constructor
    Result(const T& value) : data_(value) {}
    Result(T&& value) : data_(std::move(value)) {}
    
    // Error constructors
    Result(const Error& error) : data_(error) {}
    Result(Error&& error) : data_(std::move(error)) {}
    
    // Static factory methods
    static Result<T> error(const std::string& message) {
        return Result<T>(Error(message));
    }
    
    // Status checks
    bool ok() const { return std::holds_alternative<T>(data_); }
    bool has_error() const { return std::holds_alternative<Error>(data_); }
    
    // Value access
    const T& value() const& {
        if (!ok()) {
            throw std::runtime_error("Attempting to access value of error result");
        }
        return std::get<T>(data_);
    }
    
    T&& value() && {
        if (!ok()) {
            throw std::runtime_error("Attempting to access value of error result");
        }
        return std::move(std::get<T>(data_));
    }
    
    // Error access
    std::string error() const {
        if (!has_error()) {
            throw std::runtime_error("Attempting to access error of ok result");
        }
        return std::get<Error>(data_).what();
    }
    
private:
    std::variant<T, Error> data_;
};

/**
 * @brief Specialization for void results
 */
template<>
class Result<void> {
public:
    Result() : error_(nullptr) {}
    Result(const Error& error) : error_(new Error(error)) {}
    Result(Error&& error) : error_(new Error(std::move(error))) {}
    
    static Result<void> error(const std::string& message) {
        return Result<void>(Error(message));
    }
    
    bool ok() const { return error_ == nullptr; }
    bool has_error() const { return error_ != nullptr; }
    
    std::string error() const {
        if (!has_error()) {
            throw std::runtime_error("Attempting to access error of ok result");
        }
        return error_->what();
    }
    
private:
    std::unique_ptr<Error> error_;
};

} // namespace core
} // namespace tsdb

#endif // TSDB_CORE_RESULT_H_ 